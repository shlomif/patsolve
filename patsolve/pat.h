/* Copyright (c) 2002 Tom Holroyd
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/* Solve Freecell and Seahaven type patience (solitaire) games. */

#pragma once
#include <stdio.h>
#include <sys/types.h>

#include "config.h"

#include "portable_int32.h"
#include "game_type_params.h"
#include "fcs_enums.h"
#include "tree.h"
#include "param.h"
#include "inline.h"
#include "bool.h"
#include "fcs_dllexport.h"
#include "state.h"
#include "fnv.h"
#include "alloc_wrap.h"
#include "instance.h"

/* A card is represented as (suit << 4) + rank. */


#define FCS_PATS__COLOR     0x01         /* black if set */
#define FCS_PATS__SUIT      0x03         /* mask both suit bits */

#define FCS_PATS__KING 13



/* Some macros used in get_possible_moves(). */

/* The following macro implements
   (Same_suit ? (suit(a) == suit(b)) : (color(a) != color(b)))
*/
#ifdef FCS_FREECELL_ONLY
static GCC_INLINE fcs_bool_t fcs_pats_is_suitable(const fcs_card_t a, const fcs_card_t b)
{
    const fcs_card_t suit_mask = FCS_PATS__COLOR;
    const fcs_card_t suit_val = FCS_PATS__COLOR;
    return (((a ^ b) & suit_mask) == suit_val);
}
#else
static GCC_INLINE fcs_bool_t fcs_pats_is_suitable(const fcs_card_t a, const fcs_card_t b, const fcs_card_t suit_mask, const fcs_card_t suit_val)
{
    return (((a ^ b) & suit_mask) == suit_val);
}
#endif

static GCC_INLINE fcs_bool_t fcs_pats_is_king_only(const fcs_bool_t not_king_only, const fcs_card_t card)
{
    return (not_king_only || fcs_card_rank(card) == FCS_PATS__KING);
}

/* Represent a move. */

typedef struct {
    fcs_card_t card;            /* the card we're moving */
    u_char from;            /* from pile number */
    u_char to;              /* to pile number */
    u_char fromtype;        /* O, T, or W */
    u_char totype;
    fcs_card_t srccard;         /* card we're uncovering */
    fcs_card_t destcard;        /* card we're moving to */
    signed char pri;        /* move priority (low priority == low value) */
} fcs_pats__move_t;

/* Pile types */
#define FCS_PATS__TYPE_FOUNDATION 1
#define FCS_PATS__TYPE_FREECELL 2
#define FCS_PATS__TYPE_WASTE 3

/* Position information.  We store a compact representation of the position;
Temp cells are stored separately since they don't have to be compared.
We also store the move that led to this position from the parent, as well
as a pointers back to the parent, and the btree of all positions examined so
far. */

typedef struct fc_solve_pats__pos__struct {
    struct fc_solve_pats__pos__struct *queue;      /* next position in the queue */
    struct fc_solve_pats__pos__struct *parent;     /* point back up the move stack */
    fcs_pats__tree_t *node;             /* compact position rep.'s tree node */
    fcs_pats__move_t move;              /* move that got us here from the parent */
    unsigned short cluster; /* the cluster this node is in */
    short depth;            /* number of moves so far */
    u_char num_cards_in_freecells;           /* number of cards in T */
    u_char nchild;          /* number of child nodes left */
} fcs_pats_position_t;

/* Temp storage for possible moves. */

/* > max # moves from any position */
#define FCS_PATS__MAX_NUM_MOVES 64

typedef enum {
    FCS_PATS__INSERT_CODE_NEW,
    FCS_PATS__INSERT_CODE_FOUND,
    FCS_PATS__INSERT_CODE_FOUND_BETTER,
    FCS_PATS__INSERT_CODE_ERR
} fcs_pats__insert_code_t;

typedef enum
{
    FCS_PATS__FAIL = -1,
    FCS_PATS__WIN = 0,
    FCS_PATS__NOSOL = 1
} fc_solve_pats__status_code_t;

#ifdef FCS_PATSOLVE__WITH_FAIL_REASON
typedef enum {
    FCS_PATS__FAIL_CHECKED_STATES = 0,
    FCS_PATS__FAIL_OTHER = 1,
} fc_solve_pats__status_fail_reason_t;
#endif

/* Memory. */

typedef struct fcs_pats__block_struct {
    u_char *block;
    u_char *ptr;
    size_t remaining;
    struct fcs_pats__block_struct *next;
} fcs_pats__block_t;

#define FC_SOLVE__PATS__BLOCKSIZE (32 * 4096)

typedef struct fcs_pats__treelist_struct {
    fcs_pats__tree_t *tree;
    int cluster;
    struct fcs_pats__treelist_struct *next;
} fcs_pats__treelist_t;


#define FC_SOLVE_BUCKETLIST_NBUCKETS 4093           /* the largest 12 bit prime */
#define FC_SOLVE__MAX_NUM_PILES  4096            /* a 12 bit code */

typedef struct fcs_pats__bucket_list_struct {
    u_char *pile;           /* 0 terminated copy of the pile */
    u_int32_t hash;         /* the pile's hash code */
    int pilenum;            /* the unique id for this pile */
    struct fcs_pats__bucket_list_struct *next;
} fcs_pats__bucket_list_t;

/* Statistics. */

#define FC_SOLVE_PATS__NUM_QUEUES 100

#ifdef PATSOLVE_STANDALONE
struct fc_solve_instance_struct
{
    /* game parameters */
    fcs_game_type_params_t game_params;
#ifndef FCS_FREECELL_ONLY
    fcs_card_t game_variant_suit_mask;
    fcs_card_t game_variant_desired_suit_value;
#endif
};

typedef struct fc_solve_instance_struct fc_solve_instance_t;
#endif

typedef struct {
    fcs_pats_position_t * parent;
    int nmoves;
    fcs_pats__move_t * mp0, * mp_end, * mp;
    fcs_bool_t q;
    fcs_pats_position_t * pos;
} fcs_pats__solve_depth_t;

enum FC_SOLVE_PATS__MYDIR { FC_SOLVE_PATS__UP, FC_SOLVE_PATS__DOWN };

struct fc_solve__patsolve_thread_struct
{
    fc_solve_instance_t * instance;
    size_t remaining_memory;
    int bytes_per_pile;
    fcs_pats_position_t *queue_head[FC_SOLVE_PATS__NUM_QUEUES]; /* separate queue for each priority */
    fcs_pats_position_t *queue_tail[FC_SOLVE_PATS__NUM_QUEUES]; /* positions are added here */
    int max_queue_idx;
#ifdef DEBUG
    int num_positions_in_clusters[0x10000];
    int Inq[FC_SOLVE_PATS__NUM_QUEUES];
#endif
    fcs_pats_position_t *freed_positions;       /* position freelist */

    /* Work arrays. */

    struct
    {
        fcs_state_t s;
        DECLARE_IND_BUF_T(indirect_stacks_buffer);
        /* used to keep the piles sorted */
        int column_idxs[MAX_NUM_STACKS];
        /* column_inv_idxs are not needed or used. */
#if 0
        /* Inverse of column_idxs */
        int column_inv_idxs[MAX_NUM_STACKS];
#endif
        /* Every different pile has a hash and a unique id. */
        u_int32_t stack_hashes[MAX_NUM_STACKS];
        int stack_ids[MAX_NUM_STACKS];
    } current_pos;

    /* Temp storage for possible moves. */

    fcs_pats__move_t possible_moves[FCS_PATS__MAX_NUM_MOVES];


    /* Statistics. */

    int num_checked_states, max_num_checked_states;
    int num_states_in_collection;

    fcs_pats_xy_param_t pats_solve_params;

    int position_size;

    fcs_pats__bucket_list_t * buckets_list[FC_SOLVE_BUCKETLIST_NBUCKETS];
    /* The next pile number to be assigned. */
    int next_pile_idx;
    /* reverse lookup for unpack to get the bucket
                       from the pile */
    fcs_pats__bucket_list_t * bucket_from_pile_lookup[FC_SOLVE__MAX_NUM_PILES];
    int bytes_per_tree_node;
    int Noexit;     /* -E means don't exit */
    int num_solutions;             /* number of solutions found in -E mode */
    /* -S means stack, not queue, the moves to be done. This is a boolean
     * value.
     * Default should be FALSE.
     * */
    int to_stack;
    /* Switch between depth- and breadth-first. Default is "1".*/
    int cutoff;
    /* win, lose, or fail */
    fc_solve_pats__status_code_t status;
#ifdef FCS_PATSOLVE__WITH_FAIL_REASON
    fc_solve_pats__status_fail_reason_t fail_reason;
#endif

    #define FCS_PATS__TREE_LIST_NUM_BUCKETS 499    /* a prime */
    fcs_pats__treelist_t *tree_list[FCS_PATS__TREE_LIST_NUM_BUCKETS];
    fcs_pats__block_t *my_block;

    int dequeue__minpos, dequeue__qpos;

    fcs_bool_t is_quiet;
    fcs_pats__move_t * moves_to_win;
    int num_moves_to_win;

#define FCS_PATS__SOLVE_LEVEL_GROW_BY 16
    int curr_solve_depth, max_solve_depth;
    fcs_pats__solve_depth_t * solve_stack;
    fcs_pats_position_t * curr_solve_pos;
    enum FC_SOLVE_PATS__MYDIR curr_solve_dir;
};

typedef struct fc_solve__patsolve_thread_struct fcs_pats_thread_t;

/* Prototypes. */

extern fcs_pats__insert_code_t fc_solve_pats__insert(fcs_pats_thread_t * soft_thread, int *cluster, int d, fcs_pats__tree_t **node);
extern void fc_solve_pats__do_it(fcs_pats_thread_t *);
extern fcs_pats__move_t *fc_solve_pats__get_moves(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *, int *);
extern u_char *fc_solve_pats__new_from_block(fcs_pats_thread_t * soft_thread, size_t);
extern void fc_solve_pats__sort_piles(fcs_pats_thread_t * soft_thread);

extern fcs_pats__block_t * fc_solve_pats__new_block(fcs_pats_thread_t * const soft_thread);

/* Clusters are also stored in a hashed array. */

static GCC_INLINE void fc_solve_pats__init_clusters(fcs_pats_thread_t * const soft_thread)
{
    memset(soft_thread->tree_list, 0, sizeof(soft_thread->tree_list));
    soft_thread->my_block = fc_solve_pats__new_block(soft_thread);
}

/* Initialize the hash buckets. */

static GCC_INLINE void fc_solve_pats__init_buckets(fcs_pats_thread_t * const soft_thread)
{
#if !defined(HARD_CODED_NUM_STACKS) || !defined(HARD_CODED_NUM_FREECELLS)
    const fc_solve_instance_t * const instance = soft_thread->instance;
#endif
    const int stacks_num = INSTANCE_STACKS_NUM;
    const int freecells_num = INSTANCE_FREECELLS_NUM;

    /* Packed positions need 3 bytes for every 2 piles. */
    soft_thread->bytes_per_pile =
        (
            (
                (stacks_num * 3)
                >> 1
            )
            + (stacks_num & 0x1)
        )
        ;

    memset(soft_thread->buckets_list, 0, sizeof(soft_thread->buckets_list));
    soft_thread->next_pile_idx = 0;
    soft_thread->bytes_per_tree_node = sizeof(fcs_pats__tree_t) + soft_thread->bytes_per_pile;

    /* In order to keep the fcs_pats__tree_t structure aligned, we need to add
    up to 7 bytes on Alpha or 3 bytes on Intel -- but this is still
    better than storing the fcs_pats__tree_t nodes and keys separately, as that
    requires a pointer.  On Intel for -f bytes_per_tree_node winds up being
    a multiple of 8 currently anyway so it doesn't matter. */

//#define ALIGN_BITS 0x3
#define ALIGN_BITS 0x7
    if (soft_thread->bytes_per_tree_node & ALIGN_BITS) {
        soft_thread->bytes_per_tree_node |= ALIGN_BITS;
        soft_thread->bytes_per_tree_node++;
    }
    soft_thread->position_size = sizeof(fcs_pats_position_t) + freecells_num;
    if (soft_thread->position_size & ALIGN_BITS) {
        soft_thread->position_size |= ALIGN_BITS;
        soft_thread->position_size++;
    }
}

/* A function and some macros for allocating memory. */
/* Allocate some space and return a pointer to it.  See fc_solve_pats__new() .
 * */

static GCC_INLINE void * fc_solve_pats__malloc(fcs_pats_thread_t * const soft_thread, const size_t s)
{
    if (s > soft_thread->remaining_memory) {
#if 0
        fcs_pats_position_t *pos;

        /* Try to get some space back from the freelist. A vain hope. */

        while (soft_thread->freed_positions) {
            pos = soft_thread->freed_positions->queue;
            fc_solve_pats__free_array(soft_thread->freed_positions, u_char, sizeof(*pos) + LOCAL_FREECELLS_NUM);
            soft_thread->freed_positions = pos;
        }
        if (s > soft_thread->remaining_memory) {
            soft_thread->status = FCS_PATS__FAIL;
            return NULL;
        }
#else
        soft_thread->status = FCS_PATS__FAIL;
        return NULL;
#endif
    }

    void * const x = malloc(s);

    if (x == NULL) {
        soft_thread->status = FCS_PATS__FAIL;
        return NULL;
    }

    soft_thread->remaining_memory -= s;
    return x;
}

#define fc_solve_pats__new(soft_thread, type) ((type *)fc_solve_pats__malloc(soft_thread, sizeof(type)))

static GCC_INLINE void fc_solve_pats__release(fcs_pats_thread_t * const soft_thread, void * const ptr, const size_t count_freed)
{
    free(ptr);

    soft_thread->remaining_memory += count_freed;
}

#define fc_solve_pats__free_ptr(soft_thread, ptr, type) fc_solve_pats__release((soft_thread), (ptr), sizeof(type))

#define fc_solve_pats__new_array(soft_thread, type, size) ((type *)fc_solve_pats__malloc(soft_thread, (size) * sizeof(type)))
#define fc_solve_pats__free_array(soft_thread, ptr, type, size) fc_solve_pats__release((soft_thread), (ptr), ((size)*sizeof(type)))

static GCC_INLINE void fc_solve_pats__free_buckets(fcs_pats_thread_t * const soft_thread)
{
    for (int i = 0; i < FC_SOLVE_BUCKETLIST_NBUCKETS; i++) {
        typeof(soft_thread->buckets_list[i]) l = soft_thread->buckets_list[i];
        while (l) {
            typeof(l->next) n = l->next;
            fc_solve_pats__free_array(soft_thread, l->pile, u_char,
                strlen((const char *)l->pile) + 1
            );
            fc_solve_pats__free_ptr(soft_thread, l, fcs_pats__bucket_list_t);
            l = n;
        }
        soft_thread->buckets_list[i] = NULL;
    }
}

static GCC_INLINE void fc_solve_pats__free_blocks(fcs_pats_thread_t * const soft_thread)
{
    typeof(soft_thread->my_block) b = soft_thread->my_block;
    while (b) {
        typeof (b->next) next = b->next;
        fc_solve_pats__free_array(soft_thread, b->block, u_char, FC_SOLVE__PATS__BLOCKSIZE);
        fc_solve_pats__free_ptr(soft_thread, b, fcs_pats__block_t);
        b = next;
    }
    soft_thread->my_block = NULL;
}

static GCC_INLINE void fc_solve_pats__free_clusters(fcs_pats_thread_t * const soft_thread)
{
    for (int i = 0; i < FCS_PATS__TREE_LIST_NUM_BUCKETS; i++) {
        typeof(soft_thread->tree_list[i]) l = soft_thread->tree_list[i];
        while (l) {
            typeof(l) n = l->next;
            fc_solve_pats__free_ptr(soft_thread, l, fcs_pats__treelist_t);
            l = n;
        }
        soft_thread->tree_list[i] = NULL;
    }
}

static GCC_INLINE void fc_solve_pats__soft_thread_reset_helper(
    fcs_pats_thread_t * const soft_thread
)
{
    soft_thread->freed_positions = NULL;
    soft_thread->num_checked_states = 0;
    soft_thread->num_states_in_collection = 0;
    soft_thread->num_solutions = 0;

    soft_thread->status = FCS_PATS__NOSOL;

    soft_thread->dequeue__qpos = 0;
    soft_thread->dequeue__minpos = 0;

    soft_thread->curr_solve_depth = 0;
    soft_thread->curr_solve_pos = NULL;
}

static GCC_INLINE void fc_solve_pats__recycle_soft_thread(
    fcs_pats_thread_t * const soft_thread
)
{
    fc_solve_pats__free_buckets(soft_thread);
    fc_solve_pats__free_clusters(soft_thread);
    fc_solve_pats__free_blocks(soft_thread);

    if (soft_thread->moves_to_win)
    {
        free (soft_thread->moves_to_win);
        soft_thread->moves_to_win = NULL;
        soft_thread->num_moves_to_win = 0;
    }
    fc_solve_pats__soft_thread_reset_helper(soft_thread);
}

static GCC_INLINE void fc_solve_pats__init_soft_thread(
    fcs_pats_thread_t * const soft_thread,
    fc_solve_instance_t * const instance
)
{
    soft_thread->instance = instance;
    soft_thread->is_quiet = FALSE;      /* print entertaining messages, else exit(Status); */
    soft_thread->Noexit = FALSE;
    soft_thread->to_stack = FALSE;
    soft_thread->cutoff = 1;
    soft_thread->remaining_memory = (50 * 1000 * 1000);
    soft_thread->freed_positions = NULL;
    soft_thread->max_num_checked_states = -1;

    soft_thread->moves_to_win = NULL;
    soft_thread->num_moves_to_win = 0;

    fc_solve_pats__soft_thread_reset_helper(soft_thread);

    soft_thread->max_solve_depth = FCS_PATS__SOLVE_LEVEL_GROW_BY;
    soft_thread->solve_stack = SMALLOC(soft_thread->solve_stack, soft_thread->max_solve_depth);
}

static GCC_INLINE void fc_solve_pats__destroy_soft_thread(
    fcs_pats_thread_t * const soft_thread
)
{
    free (soft_thread->solve_stack);
    soft_thread->solve_stack = NULL;
    soft_thread->max_solve_depth = 0;
    soft_thread->curr_solve_depth = -1;
}

/* Hash a pile. */
static GCC_INLINE void fc_solve_pats__hashpile(fcs_pats_thread_t * const soft_thread, const int w)
{
    fcs_cards_column_t col = fcs_state_get_col(soft_thread->current_pos.s, w);
    fcs_col_get_card(col, (int)fcs_col_len(col)) = '\0';
    soft_thread->current_pos.stack_hashes[w] = fnv_hash_str((const u_char *)(col+1));

    /* Invalidate this pile's id.  We'll calculate it later. */

    soft_thread->current_pos.stack_ids[w] = -1;
}

extern fcs_pats_position_t *fc_solve_pats__new_position(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const parent, const fcs_pats__move_t * const m);

extern void fc_solve_pats__queue_position(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const pos, int pri);

#if !defined(HARD_CODED_NUM_STACKS)
#define DECLARE_STACKS() \
    const fcs_game_type_params_t game_params = soft_thread->instance->game_params
#else
#define DECLARE_STACKS() {}
#endif

static GCC_INLINE void fc_solve_pats__hash_layout(fcs_pats_thread_t * const soft_thread)
{
    DECLARE_STACKS();

    for (int w = 0; w < LOCAL_STACKS_NUM; w++) {
        fc_solve_pats__hashpile(soft_thread, w);
    }
}

static GCC_INLINE void fc_solve_pats__initialize_solving_process(
    fcs_pats_thread_t * const soft_thread
)
{
    /* Init the queues. */

    for (int i = 0; i < FC_SOLVE_PATS__NUM_QUEUES; i++) {
        soft_thread->queue_head[i] = NULL;
    }
    soft_thread->max_queue_idx = 0;
#ifdef DEBUG
memset(soft_thread->num_positions_in_clusters, 0, sizeof(soft_thread->num_positions_in_clusters));
memset(soft_thread->Inq, 0, sizeof(soft_thread->Inq));
#endif

    /* Queue the initial position to get started. */

    fc_solve_pats__hash_layout(soft_thread);
    fc_solve_pats__sort_piles(soft_thread);
    fcs_pats__move_t m;
    m.card = fc_solve_empty_card;
    fcs_pats_position_t * pos = fc_solve_pats__new_position(soft_thread, NULL, &m);
    if (pos == NULL) {
        return;
    }
    fc_solve_pats__queue_position(soft_thread, pos, 0);
}
#if 0
#ifdef DEBUG

#include "msg.h"
static GCC_INLINE void fc_solve_pats__print_queue(fcs_pats_thread_t * soft_thread)
{
    fc_solve_msg("max_queue_idx %d\n", soft_thread->max_queue_idx);
    int n = 0;
    for (int i = 0; i <= soft_thread->max_queue_idx; i++) {
        if (soft_thread->Inq[i]) {
            fc_solve_msg("Inq %2d %5d", i, soft_thread->Inq[i]);
            if (n & 1) {
                fc_solve_msg("\n");
            } else {
                fc_solve_msg("\t\t");
            }
            n++;
        }
    }
    fc_solve_msg("\n");
}
#endif
#endif
