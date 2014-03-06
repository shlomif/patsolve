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
#define fcs_pats_is_suitable(a, b) ((((a) ^ (b)) & soft_thread->Suit_mask) == soft_thread->Suit_val)

#define fcs_pats_is_king_only(card) (!soft_thread->King_only || fcs_pats_card_rank(card) == PS_KING)

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
extern const card_t Osuit[4];         /* suits of the output piles */

/* Temp storage for possible moves. */

#define MAXMOVES 64             /* > max # moves from any position */

enum inscode { NEW, FOUND, FOUND_BETTER, ERR };

/* Command line flags. */

extern int Quiet;

enum statuscode { FAIL = -1, WIN = 0, NOSOL = 1 };

/* Memory. */

typedef struct block {
    u_char *block;
    u_char *ptr;
    int remain;
    struct block *next;
} BLOCK;

#define BLOCKSIZE (32 * 4096)

typedef struct treelist {
    TREE *tree;
    int cluster;
    struct treelist *next;
} TREELIST;


#define FC_SOLVE_BUCKETLIST_NBUCKETS 4093           /* the largest 12 bit prime */
#define NPILES  4096            /* a 12 bit code */

typedef struct bucketlist {
    u_char *pile;           /* 0 terminated copy of the pile */
    u_int32_t hash;         /* the pile's hash code */
    int pilenum;            /* the unique id for this pile */
    struct bucketlist *next;
} BUCKETLIST;

/* Statistics. */

#define NQUEUES 100

struct fc_solve_soft_thread_struct
{
    long Mem_remain;
    int Pilebytes;
    POSITION *Qhead[NQUEUES]; /* separate queue for each priority */
    POSITION *Qtail[NQUEUES]; /* positions are added here */
    int Maxq;
#if DEBUG
    int Clusternum[0x10000];
    int Inq[NQUEUES];
#endif
    /* game parameters */
    int Same_suit;
    int King_only;
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
    int Wlen[MAXWPILES];     /* the number of cards in each pile */
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
    int Status;
};

typedef struct fc_solve_soft_thread_struct fc_solve_soft_thread_t;

/* Prototypes. */

extern int insert(fc_solve_soft_thread_t * soft_thread, int *cluster, int d, TREE **node);
extern void doit(fc_solve_soft_thread_t *);
extern void read_layout(fc_solve_soft_thread_t * soft_thread, FILE *);
extern void printcard(card_t card, FILE *);
extern void print_layout(fc_solve_soft_thread_t * soft_thread);
extern void freecell_solver_pats__make_move(fc_solve_soft_thread_t * soft_thread, MOVE *);
extern void undo_move(fc_solve_soft_thread_t * soft_thread, MOVE *);
extern MOVE *get_moves(fc_solve_soft_thread_t * soft_thread, POSITION *, int *);
extern POSITION *new_position(fc_solve_soft_thread_t * soft_thread, POSITION *parent, MOVE *m);
extern void unpack_position(fc_solve_soft_thread_t * soft_thread, POSITION *);
extern TREE *pack_position(fc_solve_soft_thread_t * soft_thread);
extern void init_buckets(fc_solve_soft_thread_t * soft_thread);
extern void init_clusters(fc_solve_soft_thread_t * soft_thread);
extern u_char *new_from_block(fc_solve_soft_thread_t * soft_thread, size_t);
extern void pilesort(fc_solve_soft_thread_t * soft_thread);
extern void msdeal(fc_solve_soft_thread_t * soft_thread, u_int64_t);

/* A function and some macros for allocating memory. */

extern void *new_(fc_solve_soft_thread_t * soft_thread, size_t s);

#define new(soft_thread, type) (type *)new_(soft_thread, sizeof(type))
#define free_ptr(soft_thread, ptr, type) free(ptr); (soft_thread)->Mem_remain += sizeof(type)

#define new_array(soft_thread, type, size) (type *)new_(soft_thread, (size) * sizeof(type))
#define free_array(soft_thread, ptr, type, size) free(ptr); \
           (soft_thread)->Mem_remain += (size) * sizeof(type)

extern void free_buckets(fc_solve_soft_thread_t * soft_thread);
extern void free_clusters(fc_solve_soft_thread_t * soft_thread);
extern void free_blocks(fc_solve_soft_thread_t * soft_thread);
extern void hash_layout(fc_solve_soft_thread_t * soft_thread);

#endif /* #ifndef FC_SOLVE_PATSOLVE_PAT_H */
