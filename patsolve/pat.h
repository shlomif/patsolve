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

#ifndef FC_SOLVE_PATSOLVE_PAT_H
#define FC_SOLVE_PATSOLVE_PAT_H

#include <stdio.h>
#include <sys/types.h>
#include "util.h"
#include "config.h"
#include "tree.h"
#include "param.h"
#include "inline.h"
#include "bool.h"

/* A card is represented as (suit << 4) + rank. */

typedef u_char card_t;

#define PS_DIAMOND 0x00         /* red */
#define PS_CLUB    0x10         /* black */
#define PS_HEART   0x20         /* red */

#define PS_SPADE   0x30         /* black */
#define PS_COLOR   0x10         /* black if set */
#define PS_SUIT    0x30         /* mask both suit bits */

#define NONE    0
#define PS_ACE  1
#define PS_KING 13

static GCC_INLINE card_t fcs_pats_card_rank(card_t card)
{
    return (card & 0xF);
}

static GCC_INLINE card_t fcs_pats_card_suit(card_t card)
{
    return (card >> 4);
}

static GCC_INLINE card_t fcs_pats_card_color(card_t card)
{
    return (card & PS_COLOR);
}

/* Some macros used in get_possible_moves(). */

/* The following macro implements
   (Same_suit ? (suit(a) == suit(b)) : (color(a) != color(b)))
*/
static GCC_INLINE fcs_bool_t fcs_pats_is_suitable(card_t a, card_t b, card_t suit_mask, card_t suit_val)
{
    return (((a ^ b) & suit_mask) == suit_val);
}

static GCC_INLINE fcs_bool_t fcs_pats_is_king_only(fcs_bool_t not_king_only, card_t card)
{
    return (not_king_only || fcs_pats_card_rank(card) == PS_KING);
}

/* Represent a move. */

typedef struct {
    card_t card;            /* the card we're moving */
    u_char from;            /* from pile number */
    u_char to;              /* to pile number */
    u_char fromtype;        /* O, T, or W */
    u_char totype;
    card_t srccard;         /* card we're uncovering */
    card_t destcard;        /* card we're moving to */
    signed char pri;        /* move priority (low priority == low value) */
} MOVE;

#define O_TYPE 1                /* pile types */
#define T_TYPE 2
#define W_TYPE 3

#define MAXTPILES       8       /* max number of piles */
#define MAXWPILES      13

/* Position information.  We store a compact representation of the position;
Temp cells are stored separately since they don't have to be compared.
We also store the move that led to this position from the parent, as well
as a pointers back to the parent, and the btree of all positions examined so
far. */

typedef struct pos {
    struct pos *queue;      /* next position in the queue */
    struct pos *parent;     /* point back up the move stack */
    TREE *node;             /* compact position rep.'s tree node */
    MOVE move;              /* move that got us here from the parent */
    unsigned short cluster; /* the cluster this node is in */
    short depth;            /* number of moves so far */
    u_char ntemp;           /* number of cards in T */
    u_char nchild;          /* number of child nodes left */
} POSITION;

/* suits of the output piles */
extern const card_t fc_solve_pats__output_suits[4];

/* Temp storage for possible moves. */

#define MAXMOVES 64             /* > max # moves from any position */

enum inscode { NEW, FOUND, FOUND_BETTER, ERR };

enum fc_solve_pats__status_code_enum {
    FAIL = -1,
    WIN = 0,
    NOSOL = 1
};

typedef enum fc_solve_pats__status_code_enum fc_solve_pats__status_code_t;

/* Memory. */

typedef struct block {
    u_char *block;
    u_char *ptr;
    int remain;
    struct block *next;
} BLOCK;

#define BLOCKSIZE (32 * 4096)

typedef struct fcs_pats__treelist_struct {
    TREE *tree;
    int cluster;
    struct fcs_pats__treelist_struct *next;
} fcs_pats__treelist_t;


#define FC_SOLVE_BUCKETLIST_NBUCKETS 4093           /* the largest 12 bit prime */
#define NPILES  4096            /* a 12 bit code */

typedef struct bucketlist {
    u_char *pile;           /* 0 terminated copy of the pile */
    u_int32_t hash;         /* the pile's hash code */
    int pilenum;            /* the unique id for this pile */
    struct bucketlist *next;
} BUCKETLIST;

/* Statistics. */

#define FC_SOLVE_PATS__NUM_QUEUES 100

struct fc_solve_soft_thread_struct
{
    long remaining_memory;
    int bytes_per_pile;
    POSITION *queue_head[FC_SOLVE_PATS__NUM_QUEUES]; /* separate queue for each priority */
    POSITION *queue_tail[FC_SOLVE_PATS__NUM_QUEUES]; /* positions are added here */
    int Maxq;
#if DEBUG
    int Clusternum[0x10000];
    int Inq[FC_SOLVE_PATS__NUM_QUEUES];
#endif
    /* game parameters */
    int Same_suit;
    fcs_bool_t King_only;
    /* the numbers we're actually using */
    int Nwpiles;
    int Ntpiles;

    card_t Suit_mask;
    card_t Suit_val;

    POSITION *Freepos;       /* position freelist */

    /* Work arrays. */

    card_t T[MAXTPILES];     /* one card in each temp cell */
    card_t W[MAXWPILES][52]; /* the workspace */
    card_t *Wp[MAXWPILES];   /* point to the top card of each work pile */
    /* The number of cards in each column. */
    int columns_lens[MAXWPILES];
    int Widx[MAXWPILES];     /* used to keep the piles sorted */
    int Widxi[MAXWPILES];    /* inverse of the above */

    card_t O[4];             /* output piles store only the rank or NONE */

    /* Every different pile has a hash and a unique id. */

    u_int32_t Whash[MAXWPILES];
    int Wpilenum[MAXWPILES];

    /* Temp storage for possible moves. */

    MOVE Possible[MAXMOVES];


    /* Statistics. */

    int Total_positions;
    int Total_generated;

    int Xparam[NXPARAM];
    double Yparam[NYPARAM];
    int Posbytes;

    BUCKETLIST *Bucketlist[FC_SOLVE_BUCKETLIST_NBUCKETS];
    int Pilenum;                    /* the next pile number to be assigned */
    /* reverse lookup for unpack to get the bucket
                       from the pile */
    BUCKETLIST *Pilebucket[NPILES];
    int Treebytes;
    int Interactive; /* interactive mode. */
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

    #define FCS_PATS__TREE_LIST_NUM_BUCKETS 499    /* a prime */
    fcs_pats__treelist_t *tree_list[FCS_PATS__TREE_LIST_NUM_BUCKETS];
    BLOCK *my_block;

    fcs_bool_t is_quiet;
};

typedef struct fc_solve_soft_thread_struct fc_solve_soft_thread_t;

/* Prototypes. */

extern int fc_solve_pats__insert(fc_solve_soft_thread_t * soft_thread, int *cluster, int d, TREE **node);
extern void fc_solve_pats__do_it(fc_solve_soft_thread_t *);
extern void fc_solve_pats__print_card(card_t card, FILE *);
extern void freecell_solver_pats__make_move(fc_solve_soft_thread_t * soft_thread, MOVE *);
extern void fc_solve_pats__undo_move(fc_solve_soft_thread_t * soft_thread, MOVE *);
extern MOVE *fc_solve_pats__get_moves(fc_solve_soft_thread_t * soft_thread, POSITION *, int *);
extern void fc_solve_pats__init_clusters(fc_solve_soft_thread_t * soft_thread);
extern u_char *fc_solve_pats__new_from_block(fc_solve_soft_thread_t * soft_thread, size_t);
extern void fc_solve_pats__sort_piles(fc_solve_soft_thread_t * soft_thread);


/* Initialize the hash buckets. */

static GCC_INLINE void fc_solve_pats__init_buckets(fc_solve_soft_thread_t * soft_thread)
{
    int i;

    /* Packed positions need 3 bytes for every 2 piles. */

    i = soft_thread->Nwpiles * 3;
    i >>= 1;
    i += soft_thread->Nwpiles & 0x1;
    soft_thread->bytes_per_pile = i;

    memset(soft_thread->Bucketlist, 0, sizeof(soft_thread->Bucketlist));
    soft_thread->Pilenum = 0;
    soft_thread->Treebytes = sizeof(TREE) + soft_thread->bytes_per_pile;

    /* In order to keep the TREE structure aligned, we need to add
    up to 7 bytes on Alpha or 3 bytes on Intel -- but this is still
    better than storing the TREE nodes and keys separately, as that
    requires a pointer.  On Intel for -f Treebytes winds up being
    a multiple of 8 currently anyway so it doesn't matter. */

//#define ALIGN_BITS 0x3
#define ALIGN_BITS 0x7
    if (soft_thread->Treebytes & ALIGN_BITS) {
        soft_thread->Treebytes |= ALIGN_BITS;
        soft_thread->Treebytes++;
    }
    soft_thread->Posbytes = sizeof(POSITION) + soft_thread->Ntpiles;
    if (soft_thread->Posbytes & ALIGN_BITS) {
        soft_thread->Posbytes |= ALIGN_BITS;
        soft_thread->Posbytes++;
    }
}

/* A function and some macros for allocating memory. */

extern void * fc_solve_pats__malloc(fc_solve_soft_thread_t * soft_thread, size_t s);

#define fc_solve_pats__new(soft_thread, type) ((type *)fc_solve_pats__malloc(soft_thread, sizeof(type)))

static GCC_INLINE void fc_solve_pats__release(fc_solve_soft_thread_t * const soft_thread, void * const ptr, const size_t count_freed)
{
    free(ptr);

    soft_thread->remaining_memory += count_freed;
}

#define fc_solve_pats__free_ptr(soft_thread, ptr, type) fc_solve_pats__release((soft_thread), (ptr), sizeof(type))

#define fc_solve_pats__new_array(soft_thread, type, size) ((type *)fc_solve_pats__malloc(soft_thread, (size) * sizeof(type)))
#define fc_solve_pats__free_array(soft_thread, ptr, type, size) fc_solve_pats__release((soft_thread), (ptr), ((size)*sizeof(type)))

static GCC_INLINE void fc_solve_pats__free_buckets(fc_solve_soft_thread_t * soft_thread)
{
    int i, j;
    BUCKETLIST *l, *n;

    for (i = 0; i < FC_SOLVE_BUCKETLIST_NBUCKETS; i++) {
        l = soft_thread->Bucketlist[i];
        while (l) {
            n = l->next;
            j = strlen((const char *)l->pile);    /* @@@ use block? */
            fc_solve_pats__free_array(soft_thread, l->pile, u_char, j + 1);
            fc_solve_pats__free_ptr(soft_thread, l, BUCKETLIST);
            l = n;
        }
    }
}

static GCC_INLINE void fc_solve_pats__free_blocks(fc_solve_soft_thread_t * soft_thread)
{
    BLOCK *b, *next;

    b = soft_thread->my_block;
    while (b) {
        next = b->next;
        fc_solve_pats__free_array(soft_thread, b->block, u_char, BLOCKSIZE);
        fc_solve_pats__free_ptr(soft_thread, b, BLOCK);
        b = next;
    }
}

extern void fc_solve_pats__hash_layout(fc_solve_soft_thread_t * soft_thread);

static GCC_INLINE void fc_solve_pats__free_clusters(fc_solve_soft_thread_t * soft_thread)
{
    int i;
    fcs_pats__treelist_t *l, *n;

    for (i = 0; i < FCS_PATS__TREE_LIST_NUM_BUCKETS; i++) {
        l = soft_thread->tree_list[i];
        while (l) {
            n = l->next;
            fc_solve_pats__free_ptr(soft_thread, l, fcs_pats__treelist_t);
            l = n;
        }
    }
}

extern void fc_solve_msg(const char *msg, ...);

#if DEBUG
static GCC_INLINE void fc_solve_pats__print_queue(fc_solve_soft_thread_t * soft_thread)
{
    fc_solve_msg("Maxq %d\n", soft_thread->Maxq);
    int n = 0;
    for (int i = 0; i <= soft_thread->Maxq; i++) {
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

#endif /* #ifndef FC_SOLVE_PATSOLVE_PAT_H */
