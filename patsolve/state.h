/* Copyright (c) 2000 Shlomi Fish
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
/*
 * state.h - header file for state functions and macros for Freecell Solver
 *
 */
#ifndef FC_SOLVE__STATE_H
#define FC_SOLVE__STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "config.h"

#include "fcs_move.h"
#include "fcs_limit.h"

#include "inline.h"
#include "bool.h"

#include "game_type_limit.h"

#include "internal_move_struct.h"
#include "indirect_buffer.h"
#include "fcs_dllexport.h"

#if MAX_NUM_INITIAL_CARDS_IN_A_STACK+12>(MAX_NUM_DECKS*52)
#define MAX_NUM_CARDS_IN_A_STACK (MAX_NUM_DECKS*52)
#else
#define MAX_NUM_CARDS_IN_A_STACK (MAX_NUM_INITIAL_CARDS_IN_A_STACK+12)
#endif

#ifndef FCS_MAX_NUM_SCANS_BUCKETS
#define FCS_MAX_NUM_SCANS_BUCKETS 4
#endif

/* How many ranks there are - Ace to King == 13. */
#define FCS_MAX_RANK 13

#define FCS_CHAR_BIT_SIZE_LOG2 3
#define MAX_NUM_SCANS (FCS_MAX_NUM_SCANS_BUCKETS * (sizeof(unsigned char) * 8))

#define is_scan_visited(ptr_state, scan_id) ((FCS_S_SCAN_VISITED(ptr_state))[(scan_id)>>FCS_CHAR_BIT_SIZE_LOG2] & (1 << ((scan_id)&((1<<(FCS_CHAR_BIT_SIZE_LOG2))-1))))
#define set_scan_visited(ptr_state, scan_id) { (FCS_S_SCAN_VISITED(ptr_state))[(scan_id)>>FCS_CHAR_BIT_SIZE_LOG2] |= (1 << ((scan_id)&((1<<(FCS_CHAR_BIT_SIZE_LOG2))-1))); }


#ifdef DEBUG_STATES

typedef struct
{
    short rank;
    char suit;
    char flags;
} fcs_card_t;

typedef struct
{
    int num_cards;
    fcs_card_t cards[MAX_NUM_CARDS_IN_A_STACK];
} fc_stack_t;

typedef fc_stack_t * fcs_cards_column_t;
typedef const fc_stack_t * fcs_const_cards_column_t;
typedef int fcs_state_foundation_t;

struct fcs_struct_state_t
{
    fc_stack_t stacks[MAX_NUM_STACKS];
    fcs_card_t freecells[MAX_NUM_FREECELLS];
    fcs_state_foundation_t foundations[MAX_NUM_DECKS*4];
};

typedef struct fcs_struct_state_t fcs_state_t;

typedef int fcs_locs_t;

static GCC_INLINE fcs_card_t fcs_make_card(const int rank, const int suit)
{
    fcs_card_t ret = { .rank = rank, .suit = suit, .flags = 0 };

    return ret;
}
static GCC_INLINE char fcs_card2char(const fcs_card_t card)
{
    return (char)(card.suit | (card.rank << 2));
}
static GCC_INLINE fcs_card_t fcs_char2card(unsigned char c)
{
    return fcs_make_card((c >> 2), (c & 0x3));
}

#define fcs_state_get_col(state, col_idx) \
    (&((state).stacks[(col_idx)]))

#define fcs_col_len(col) \
    ((col)->num_cards)

#define fcs_col_get_card(col, c) \
    ((col)->cards[(c)] )

#define fcs_card_rank(card) \
    ( (card).rank )

#define fcs_card_suit(card) \
    ((int)( (card).suit ))

#ifndef FCS_WITHOUT_CARD_FLIPPING
#define fcs_card_get_flipped(card) \
    ( (card).flags )
#endif

#define fcs_freecell_card(state, f) \
    ( (state).freecells[(f)] )

#define fcs_foundation_value(state, found) \
    ( (state).foundations[(found)] )

#define fcs_card_set_suit(card, d) \
    (card).suit = (d)

#define fcs_card_set_rank(card, num) \
    (card).rank = (num)

#ifndef FCS_WITHOUT_CARD_FLIPPING
#define fcs_card_set_flipped(card, flipped) \
    (card).flags = (flipped)
#endif

#elif defined(COMPACT_STATES)    /* #ifdef DEBUG_STATES */


typedef char fcs_card_t;
typedef fcs_card_t * fcs_cards_column_t;
typedef const fcs_card_t * fcs_const_cards_column_t;
typedef fcs_card_t fcs_state_foundation_t;
/*
 * Card:
 * Bits 0-3 - Card Number
 * Bits 4-5 - Deck
 *
 */

struct fcs_struct_state_t
{
    fcs_card_t data[MAX_NUM_STACKS*(MAX_NUM_CARDS_IN_A_STACK+1)+MAX_NUM_FREECELLS+4*MAX_NUM_DECKS];
};

/*
 * Stack: 0 - Number of cards
 *        1-19 - Cards
 * Stacks: stack_num*20 where stack_num >= 0 and
 *         stack_num <= (MAX_NUM_STACKS-1)
 * Bytes: (MAX_NUM_STACKS*20) to
 *      (MAX_NUM_STACKS*20+MAX_NUM_FREECELLS-1)
 *      are Freecells.
 * Bytes: (MAX_NUM_STACKS*20+MAX_NUM_FREECELLS) to
 *      MAX_NUM_STACKS*20+MAX_NUM_FREECELLS+3
 *      are Foundations.
 * */

/*  ===== Depracated Information =====
 * Stack: 0 - Number of cards 1-19 - Cards
 * Stacks: stack_num*20 where stack_num >= 0 and stack_num <= 7
 * Bytes 160-163 - Freecells
 * Bytes 164-167 - Decks
 */

typedef struct fcs_struct_state_t fcs_state_t;

typedef char fcs_locs_t;

#define fcs_state_get_col(state, col_idx) \
    ( (state).data + ((col_idx)*(MAX_NUM_CARDS_IN_A_STACK+1)) )


#define FCS_FREECELLS_OFFSET ((MAX_NUM_STACKS)*(MAX_NUM_CARDS_IN_A_STACK+1))

#define fcs_freecell_card(state, f) \
    ( (state).data[FCS_FREECELLS_OFFSET+(f)] )

#define FCS_FOUNDATIONS_OFFSET (((MAX_NUM_STACKS)*(MAX_NUM_CARDS_IN_A_STACK+1))+(MAX_NUM_FREECELLS))

#define fcs_foundation_value(state, d) \
    ( (state).data[FCS_FOUNDATIONS_OFFSET+(d)])

#elif defined(INDIRECT_STACK_STATES) /* #ifdef DEBUG_STATES
                                        #elif defined(COMPACT_STATES)
                                      */

typedef char fcs_card_t;
typedef fcs_card_t * fcs_cards_column_t;
typedef const fcs_card_t * fcs_const_cards_column_t;
typedef char fcs_state_foundation_t;

struct fcs_struct_state_t
{
    fcs_cards_column_t stacks[MAX_NUM_STACKS];
    fcs_card_t freecells[MAX_NUM_FREECELLS];
    fcs_state_foundation_t foundations[MAX_NUM_DECKS*4];
};

typedef struct fcs_struct_state_t fcs_state_t;

#define fcs_state_get_col(state, col_idx) \
    ( (state).stacks[(col_idx)] )



#define fcs_freecell_card(state, f) \
    ( (state).freecells[(f)] )

#define fcs_foundation_value(state, d) \
    ( (state).foundations[(d)] )


#define fcs_copy_stack(state_key, state_val, idx, buffer) \
    {     \
        if (! ((state_val).stacks_copy_on_write_flags & (1 << idx)))        \
        {          \
            fcs_cards_column_t copy_stack_col; \
                                    \
            (state_val).stacks_copy_on_write_flags |= (1 << idx);       \
            copy_stack_col = fcs_state_get_col((state_key), idx); \
            memcpy(&buffer[idx << 7], copy_stack_col, fcs_col_len(copy_stack_col)+1); \
            fcs_state_get_col((state_key), idx) = &buffer[idx << 7];     \
        }     \
    }

#define fcs_duplicate_state_extra(ptr_dest, ptr_src)  \
    {   \
        (ptr_dest)->info.stacks_copy_on_write_flags = 0; \
    }
#define fcs_duplicate_kv_state_extra(ptr_dest, ptr_src) \
{ \
    ptr_dest->stacks_copy_on_write_flags = 0; \
}

typedef char fcs_locs_t;

#else
#error DEBUG_STATES , COMPACT_STATES or INDIRECT_STACK_STATES are not defined.
#endif /* #ifdef DEBUG_STATES -
          #elif defined COMPACT_STATES -
          #elif defined INDIRECT_STACK_STATES
        */

/* These are macros that are common to all three _STATES types. */

/*
 * This macro determines if child can be placed above parent.
 *
 * The variable sequences_are_built_by has to be initialized to
 * the sequences_are_built_by member of the instance.
 *
 * */

#ifdef FCS_FREECELL_ONLY

#define fcs_is_parent_card(child, parent) \
    ((fcs_card_rank(child)+1 == fcs_card_rank(parent)) && \
            ((fcs_card_suit(child) & 0x1) != (fcs_card_suit(parent)&0x1)) \
    )

#else

#define fcs_is_parent_card(child, parent) \
    ((fcs_card_rank(child)+1 == fcs_card_rank(parent)) && \
    ((sequences_are_built_by == FCS_SEQ_BUILT_BY_RANK) ?   \
        1 :                                                          \
        ((sequences_are_built_by == FCS_SEQ_BUILT_BY_SUIT) ?   \
            (fcs_card_suit(child) == fcs_card_suit(parent)) :     \
            ((fcs_card_suit(child) & 0x1) != (fcs_card_suit(parent)&0x1))   \
        ))                \
    )

#endif

#define fcs_col_get_rank(col, card_idx) \
    fcs_card_rank(fcs_col_get_card((col), (card_idx)))

#define fcs_freecell_card_suit(state, f) \
    ( fcs_card_suit(fcs_freecell_card((state),(f))) )

#define fcs_increment_foundation(state, d) \
    ( (fcs_foundation_value((state), (d)))++)

#define fcs_set_foundation(state, found, value) \
    ( (fcs_foundation_value((state), (found))) = (fcs_state_foundation_t)(value) )

#define fcs_col_pop_top(col) \
    {       \
        fcs_col_get_card((col), (--fcs_col_len(col))) = fc_solve_empty_card;  \
    }

#define fcs_col_pop_card(col, into) \
    {   \
        into = fcs_col_get_card((col), (fcs_col_len(col)-1)); \
        fcs_col_pop_top(col); \
    }

#define fcs_col_push_card(col, from) \
{ \
  fcs_col_get_card((col), ((fcs_col_len(col))++)) = (from); \
}

#define fcs_col_push_col_card(dest_col, src_col, card_idx) \
    fcs_col_push_card((dest_col), fcs_col_get_card((src_col), (card_idx)))

#define fcs_card_is_empty(card) (fcs_card_rank(card) == 0)
#define fcs_card_is_valid(card) (fcs_card_rank(card) != 0)
#define fcs_freecell_is_empty(state, idx) (fcs_card_is_empty(fcs_freecell_card(state, idx)))

#if defined(COMPACT_STATES) || defined(DEBUG_STATES)

#define fcs_duplicate_state_extra(ptr_dest, ptr_src) \
    {}

#define fcs_duplicate_kv_state_extra(ptr_dest, ptr_src) \
    {}

#define fcs_copy_stack(state_key, state_val, idx, buffer) {}

#endif

#define fcs_put_card_in_freecell(state, f, card) \
    (fcs_freecell_card((state), (f)) = (card))

#define fcs_empty_freecell(state, f) \
    fcs_put_card_in_freecell((state), (f), fc_solve_empty_card)

#ifndef FCS_WITHOUT_CARD_FLIPPING
#define fcs_col_flip_card(col, c) \
    (fcs_card_set_flipped(fcs_col_get_card((col), (c)), 0))
#endif

/* These are macros that are common to COMPACT_STATES and
 * INDIRECT_STACK_STATES */
#if defined(COMPACT_STATES) || defined(INDIRECT_STACK_STATES)

static GCC_INLINE fcs_card_t fcs_make_card(const int rank, const int suit)
{
    return ( (((fcs_card_t)rank) << 2) | ((fcs_card_t)suit) );
}

#define fcs_card2char(c) (c)
#define fcs_char2card(c) (c)

#define fcs_col_len(col) \
    ( ((col)[0]) )

#define fcs_col_get_card(col, card_idx) \
    ((col)[(card_idx)+1])

#define fcs_card_rank(card) \
    ( (card) >> 2 )

#define fcs_card_suit(card) \
    ( (card) & 0x03 )

#ifndef FCS_WITHOUT_CARD_FLIPPING
#define fcs_card_get_flipped(card) \
    ( (card) >> 6 )
#endif

#define fcs_card_set_rank(card, num) \
    (card) = ((fcs_card_t)(((card)&0x03)|((num)<<2)));

#define fcs_card_set_suit(card, suit) \
    (card) = ((fcs_card_t)(((card)&0xFC)|(suit)));

#ifndef FCS_WITHOUT_CARD_FLIPPING
#define fcs_card_set_flipped(card, flipped) \
    (card) = ((fcs_card_t)(((card)&((fcs_card_t)0x3F))|((fcs_card_t)((flipped)<<6))))
#endif

#endif

struct fcs_state_keyval_pair_struct;

/*
 * NOTE: the order of elements here is intended to reduce framgmentation
 * and memory consumption. Namely:
 *
 * 1. Pointers come first.
 *
 * 2. ints (32-bit on most platform) come next.
 *
 * 3. chars come next.
 *
 * */
struct fcs_state_extra_info_struct
{
#ifdef FCS_RCS_STATES
    struct fcs_state_extra_info_struct * parent;
#else
    struct fcs_state_keyval_pair_struct * parent;
#endif
    fcs_move_stack_t * moves_to_parent;

#ifndef FCS_WITHOUT_DEPTH_FIELD
    int depth;
#endif

#ifndef FCS_WITHOUT_VISITED_ITER
    /*
     * The iteration in which this state was marked as visited
     * */
    fcs_int_limit_t visited_iter;
#endif

    /*
     * This is the number of direct children of this state which were not
     * yet declared as dead ends. Once this counter reaches zero, this
     * state too is declared as a dead end.
     *
     * It was converted to an unsigned short , because it is extremely
     * unlikely that a state will have more than 64K active children.
     * */
    unsigned short num_active_children;


    /*
     * This field contains global, scan-independant flags, which are used
     * from the FCS_VISITED_* enum below.
     *
     * FCS_VISITED_IN_SOLUTION_PATH - indicates that the state is in the
     * solution path found by the scan. (used by the optimization scan)
     *
     * FCS_VISITED_IN_OPTIMIZED_PATH - indicates that the state is in the
     * optimized solution path which is computed by the optimization scan.
     *
     * FCS_VISITED_DEAD_END - indicates that the state does not lead to
     * anywhere useful, and scans should not examine it in the first place.
     *
     * FCS_VISITED_GENERATED_BY_PRUNING - indicates that the state was
     * generated by pruning, so one can skip calling the pruning function
     * for it.
     * */
    fcs_game_limit_t visited;


    /*
     * This is a vector of flags - one for each scan. Each indicates whether
     * its scan has already visited this state
     * */
    unsigned char scan_visited[FCS_MAX_NUM_SCANS_BUCKETS];

#ifdef INDIRECT_STACK_STATES
    /*
     * A vector of flags that indicates which stacks were already copied.
     * */
    int stacks_copy_on_write_flags;
#endif
};

typedef struct
{
    /*
     * These contain the location of the original stacks and freecells
     * in the permutation of them. They are sorted by the canonization
     * function.
     * */
    fcs_locs_t stack_locs[MAX_NUM_STACKS];
    fcs_locs_t fc_locs[MAX_NUM_FREECELLS];
} fcs_state_locs_struct_t;

static GCC_INLINE void fc_solve_init_locs(fcs_state_locs_struct_t * locs)
{
    int i;
    for ( i=0 ; i<MAX_NUM_STACKS ; i++)
    {
        locs->stack_locs[i] = (fcs_locs_t)i;
    }
    for ( i=0 ; i<MAX_NUM_FREECELLS ; i++)
    {
        locs->fc_locs[i] = (fcs_locs_t)i;
    }
}

typedef struct fcs_state_extra_info_struct fcs_state_extra_info_t;

struct fcs_state_keyval_pair_struct
{
    union
    {
        struct
        {
            fcs_state_t s;
            fcs_state_extra_info_t info;
        };
        struct fcs_state_keyval_pair_struct * next;
    };
};

typedef struct fcs_state_keyval_pair_struct fcs_state_keyval_pair_t;

typedef struct {
    fcs_state_t * key;
    fcs_state_extra_info_t * val;
} fcs_kv_state_t;

static GCC_INLINE void
FCS_STATE_keyval_pair_to_kv(fcs_kv_state_t * ret, fcs_state_keyval_pair_t * s)
{
    ret->key = &(s->s);
    ret->val = &(s->info);
}

/*
 * This type is the struct that is collectible inside the hash.
 *
 * In FCS_RCS_STATES we only collect the extra_info's and the state themselves
 * are kept in an LRU cache because they can be calculated from the
 * extra_infos and the original state by applying the moves.
 *
 * */
#ifdef FCS_RCS_STATES

typedef fcs_state_extra_info_t fcs_collectible_state_t;
#define FCS_S_ACCESSOR(s, field) ((s)->field)

#define fcs_duplicate_state(ptr_dest, ptr_src) \
    { \
    *((ptr_dest)->key) = *((ptr_src)->key); \
    *((ptr_dest)->val) = *((ptr_src)->val); \
    fcs_duplicate_state_extra(((ptr_dest)->val), ((ptr_src)->val));   \
    }

#define fcs_duplicate_kv_state(x,y) fcs_duplicate_state(x,y)

#define FCS_STATE_keyval_pair_to_collectible(s) (&((s)->info))
#define FCS_STATE_kv_to_collectible(s) ((s)->val)

static GCC_INLINE void
FCS_STATE_collectible_to_kv(fcs_kv_state_t * ret, fcs_collectible_state_t * s)
{
    ret->val = s;
}

#else

typedef fcs_state_keyval_pair_t fcs_collectible_state_t;

#define FCS_S_ACCESSOR(s, field) (((s)->info).field)

#define fcs_duplicate_state(ptr_dest, ptr_src) \
    { \
    *(ptr_dest) = *(ptr_src); \
    fcs_duplicate_state_extra(ptr_dest, ptr_src);   \
    }


#define fcs_duplicate_kv_state(ptr_dest, ptr_src) \
    { \
    *((ptr_dest)->key) = *((ptr_src)->key); \
    *((ptr_dest)->val) = *((ptr_src)->val); \
    fcs_duplicate_kv_state_extra(((ptr_dest)->val), ((ptr_src)->val));   \
    }

#define FCS_STATE_keyval_pair_to_collectible(s) (s)
#define FCS_STATE_kv_to_collectible(s) ((fcs_collectible_state_t *)((s)->key))

#define FCS_STATE_collectible_to_kv(ret,s) FCS_STATE_keyval_pair_to_kv((ret),(s))

#endif

#define FCS_S_NEXT(s) FCS_S_ACCESSOR(s, parent)
#define FCS_S_PARENT(s) FCS_S_ACCESSOR(s, parent)
#define FCS_S_NUM_ACTIVE_CHILDREN(s) FCS_S_ACCESSOR(s, num_active_children)
#define FCS_S_MOVES_TO_PARENT(s) FCS_S_ACCESSOR(s, moves_to_parent)
#define FCS_S_VISITED(s) FCS_S_ACCESSOR(s, visited)

#define FCS_S_SCAN_VISITED(s) FCS_S_ACCESSOR(s, scan_visited)

#ifndef FCS_WITHOUT_DEPTH_FIELD
#define FCS_S_DEPTH(s) FCS_S_ACCESSOR(s, depth)
#endif

#ifndef FCS_WITHOUT_VISITED_ITER
#define FCS_S_VISITED_ITER(s) FCS_S_ACCESSOR(s, visited_iter)
#endif

typedef struct {
    fcs_state_t * key;
    fcs_state_extra_info_t * val;
    fcs_collectible_state_t * s;
    fcs_state_locs_struct_t locs;
} fcs_standalone_state_ptrs_t;

#ifdef DEBUG_STATES

extern fcs_card_t fc_solve_empty_card;
#define DEFINE_fc_solve_empty_card() \
    fcs_card_t fc_solve_empty_card = {0,0};

#elif defined(COMPACT_STATES) || defined (INDIRECT_STACK_STATES)

#define fc_solve_empty_card ((fcs_card_t)0)

#define DEFINE_fc_solve_empty_card() \
    ;

#endif

extern void fc_solve_canonize_state(
    fcs_kv_state_t * state_raw,
    int freecells_num,
    int stacks_num
    );

void fc_solve_canonize_state_with_locs(
    fcs_kv_state_t * state,
    fcs_state_locs_struct_t * locs,
    int freecells_num,
    int stacks_num);

#if (FCS_STATE_STORAGE != FCS_STATE_STORAGE_INDIRECT)

#if (FCS_STATE_STORAGE != FCS_STATE_STORAGE_LIBREDBLACK_TREE)
typedef void * fcs_compare_context_t;
#else
typedef const void * fcs_compare_context_t;
#endif

#if (FCS_STATE_STORAGE != FCS_STATE_STORAGE_INDIRECT)
static GCC_INLINE int fc_solve_state_compare(const void * s1, const void * s2)
{
    return memcmp(s1,s2,sizeof(fcs_state_t));
}
#endif

#if (FCS_STATE_STORAGE == FCS_STATE_STORAGE_GLIB_HASH)
extern int fc_solve_state_compare_equal(const void * s1, const void * s2);
#endif
extern int fc_solve_state_compare_with_context(const void * s1, const void * s2, fcs_compare_context_t context);

#else
extern int fc_solve_state_compare_indirect(const void * s1, const void * s2);
extern int fc_solve_state_compare_indirect_with_context(const void * s1, const void * s2, void * context);
#endif
enum
{
    FCS_USER_STATE_TO_C__SUCCESS = 0,
    FCS_USER_STATE_TO_C__PREMATURE_END_OF_INPUT
};


/*
 * This function converts an entire card from its string representations
 * (e.g: "AH", "KS", "8D"), to a fcs_card_t data type.
 * */
extern DLLEXPORT fcs_card_t fc_solve_card_user2perl(const char * str);

/*
 * Convert an entire card to its user representation.
 *
 * */
extern char * fc_solve_card_perl2user(
    fcs_card_t card,
    char * str,
    fcs_bool_t t
    );

/*
 * Converts a rank from its internal representation to a string.
 *
 * rank_idx - the rank
 * str - the string to output to.
 * rank_is_null - a pointer to a bool that indicates whether
 *      the card number is out of range or equal to zero
 * t - whether 10 should be printed as T or not.
 * */
extern char * fc_solve_p2u_rank(
    int rank_idx,
    char * str,
    fcs_bool_t * rank_is_null,
    fcs_bool_t t
#ifndef FCS_WITHOUT_CARD_FLIPPING
    , fcs_bool_t flipped
#endif
    );

/*
 * This function converts a card number from its user representation
 * (e.g: "A", "K", "9") to its card number that can be used by
 * the program.
 * */
extern DLLEXPORT int fc_solve_u2p_rank(const char * string);

/*
 * This function converts a string containing a suit letter (that is
 * one of H,S,D,C) into its suit ID.
 *
 * The suit letter may come somewhat after the beginning of the string.
 *
 * */
extern DLLEXPORT int fc_solve_u2p_suit(const char * deck);

#ifdef INDIRECT_STACK_STATES
#define fc_solve_state_init(state, stacks_num, indirect_stacks_buffer) \
    fc_solve_state_init_proto(state, stacks_num, indirect_stacks_buffer)
#else
#define fc_solve_state_init(state, stacks_num, indirect_stacks_buffer) \
    fc_solve_state_init_proto(state, stacks_num)
#endif

static GCC_INLINE void fc_solve_state_init_proto(
    fcs_state_keyval_pair_t * state,
    int stacks_num
    IND_BUF_T_PARAM(indirect_stacks_buffer)
    )
{
#if ((!defined(FCS_WITHOUT_CARD_FLIPPING)) || defined(INDIRECT_STACK_STATES))
    int i;
#endif


    memset(&(state->s), 0, sizeof(state->s));

#ifdef INDIRECT_STACK_STATES
    for(i=0;i<stacks_num;i++)
    {
        state->s.stacks[i] = &indirect_stacks_buffer[i << 7];
        memset(state->s.stacks[i], '\0', MAX_NUM_DECKS*52+1);
    }
    for(;i<MAX_NUM_STACKS;i++)
    {
        state->s.stacks[i] = NULL;
    }
#endif
    state->info.parent = NULL;
    state->info.moves_to_parent = NULL;
#ifndef FCS_WITHOUT_DEPTH_FIELD
    state->info.depth = 0;
#endif
    state->info.visited = 0;
#ifndef FCS_WITHOUT_VISITED_ITER
    state->info.visited_iter = 0;
#endif
    state->info.num_active_children = 0;
    memset(state->info.scan_visited, '\0', sizeof(state->info.scan_visited));
#ifdef INDIRECT_STACK_STATES
    state->info.stacks_copy_on_write_flags = 0;
#endif
}

static const char * const fc_solve_freecells_prefixes[] = { "FC:", "Freecells:", "Freecell:", NULL};

static const char * const fc_solve_foundations_prefixes[] = { "Decks:", "Deck:", "Founds:", "Foundations:", "Foundation:", "Found:", NULL};
#if defined(WIN32) && (!defined(HAVE_STRNCASECMP))
#ifndef strncasecmp
#define strncasecmp(a,b,c) (strnicmp((a),(b),(c)))
#endif
#endif

#ifdef INDIRECT_STACK_STATES
#define fc_solve_initial_user_state_to_c(string, out_state, freecells_num, stacks_num, decks_num, indirect_stacks_buffer) \
    fc_solve_initial_user_state_to_c_proto(string, out_state, freecells_num, stacks_num, decks_num, indirect_stacks_buffer)
#else
#define fc_solve_initial_user_state_to_c(string, out_state, freecells_num, stacks_num, decks_num, indirect_stacks_buffer) \
    fc_solve_initial_user_state_to_c_proto(string, out_state, freecells_num, stacks_num, decks_num)
#endif

static GCC_INLINE int fc_solve_initial_user_state_to_c_proto(
    const char * string,
    fcs_state_keyval_pair_t * out_state,
    int freecells_num,
    int stacks_num,
    int decks_num
    IND_BUF_T_PARAM(indirect_stacks_buffer)
    )
{
    int s,c;
    const char * str;
    fcs_card_t card;
    fcs_cards_column_t col;
    int first_line;

    const char * const * prefix;
    int decks_index[4];

    fc_solve_state_init(
        out_state,
        stacks_num,
        indirect_stacks_buffer
        );
    str = string;

    first_line = 1;

#define ret (out_state->s)
/* Handle the end of string - shouldn't happen */
#define handle_eos() \
    { \
        if ((*str) == '\0') \
        {  \
            return FCS_USER_STATE_TO_C__PREMATURE_END_OF_INPUT; \
        } \
    }

    for (s = 0 ; s < stacks_num ; s++)
    {
        /* Move to the next stack */
        if (!first_line)
        {
            while((*str) != '\n')
            {
                handle_eos();
                str++;
            }
            str++;
        }

        first_line = 0;

        for (prefix = fc_solve_freecells_prefixes ; (*prefix) ; prefix++)
        {
            if (!strncasecmp(str, (*prefix), strlen(*prefix)))
            {
                str += strlen(*prefix);
                break;
            }
        }

        if (*prefix)
        {
            for (c = 0 ; c < freecells_num ; c++)
            {
                fcs_empty_freecell(ret, c);
            }
            for (c = 0 ; c < freecells_num ; c++)
            {
                if (c!=0)
                {
                    while(
                            ((*str) != ' ') &&
                            ((*str) != '\t') &&
                            ((*str) != '\n') &&
                            ((*str) != '\r')
                         )
                    {
                        handle_eos();
                        str++;
                    }
                    if ((*str == '\n') || (*str == '\r'))
                    {
                        break;
                    }
                    str++;
                }

                while ((*str == ' ') || (*str == '\t'))
                {
                    str++;
                }
                if ((*str == '\r') || (*str == '\n'))
                    break;

                if ((*str == '*') || (*str == '-'))
                {
                    card = fc_solve_empty_card;
                }
                else
                {
                    card = fc_solve_card_user2perl(str);
                }

                fcs_put_card_in_freecell(ret, c, card);
            }

            while (*str != '\n')
            {
                handle_eos();
                str++;
            }
            s--;
            continue;
        }

        for(prefix = fc_solve_foundations_prefixes ; (*prefix) ; prefix++)
        {
            if (!strncasecmp(str, (*prefix), strlen(*prefix)))
            {
                str += strlen(*prefix);
                break;
            }
        }

        if (*prefix)
        {
            int d;

            for(d=0;d<decks_num*4;d++)
            {
                fcs_set_foundation(ret, d, 0);
            }

            for(d=0;d<4;d++)
            {
                decks_index[d] = 0;
            }
            while (1)
            {
                while((*str == ' ') || (*str == '\t'))
                    str++;
                if ((*str == '\n') || (*str == '\r'))
                    break;
                d = fc_solve_u2p_suit(str);
                str++;
                while (*str == '-')
                    str++;
                /* Workaround for fc_solve_u2p_rank's willingness
                 * to designate the string '0' as 10. */
                c = ((str[0] == '0') ? 0 : fc_solve_u2p_rank(str));
                while (
                        (*str != ' ') &&
                        (*str != '\t') &&
                        (*str != '\n') &&
                        (*str != '\r')
                      )
                {
                    handle_eos();
                    str++;
                }

                fcs_set_foundation(ret, (decks_index[d]*4+d), c);
                decks_index[d]++;
                if (decks_index[d] >= decks_num)
                {
                    decks_index[d] = 0;
                }
            }
            s--;
            continue;
        }

        /* Handle columns that start with an initial colon, so we can
         * input states from -p -t mid-play. */
        if ((*str) == ':')
        {
            str++;
        }

        col = fcs_state_get_col(ret, s);
        for(c=0 ; c < MAX_NUM_CARDS_IN_A_STACK ; c++)
        {
            /* Move to the next card */
            if (c!=0)
            {
                while(
                    ((*str) != ' ') &&
                    ((*str) != '\t') &&
                    ((*str) != '\n') &&
                    ((*str) != '\r')
                )
                {
                    handle_eos();
                    str++;
                }
                if ((*str == '\n') || (*str == '\r'))
                {
                    break;
                }
            }

            while ((*str == ' ') || (*str == '\t'))
            {
                str++;
            }
            handle_eos();
            if ((*str == '\n') || (*str == '\r'))
            {
                break;
            }
            card = fc_solve_card_user2perl(str);

            fcs_col_push_card(col, card);
        }
    }

    return FCS_USER_STATE_TO_C__SUCCESS;
}

#undef ret
#undef handle_eos

extern DLLEXPORT char * fc_solve_state_as_string(
    fcs_state_t * key,
    fcs_state_locs_struct_t * state_locs,
    int freecells_num,
    int stacks_num,
    int decks_num,
    fcs_bool_t parseable_output,
    fcs_bool_t canonized_order_output,
    fcs_bool_t display_10_as_t
    );

enum
{
    FCS_STATE_VALIDITY__OK = 0,
    FCS_STATE_VALIDITY__MISSING_CARD = 1,
    FCS_STATE_VALIDITY__EXTRA_CARD = 2,
    FCS_STATE_VALIDITY__EMPTY_SLOT = 3,
    FCS_STATE_VALIDITY__PREMATURE_END_OF_INPUT = 4
};

static GCC_INLINE int fc_solve_check_state_validity(
    fcs_state_keyval_pair_t * state_pair,
    int freecells_num,
    int stacks_num,
    int decks_num,
    fcs_card_t * misplaced_card)
{
    int cards[4][14];
    int c, s, d, f;
    int col_len;

    fcs_state_t * state;
    fcs_cards_column_t col;
    fcs_card_t card;

    state = &(state_pair->s);

    /* Initialize all cards to 0 */
    for(d=0;d<4;d++)
    {
        for(c=1;c<=13;c++)
        {
            cards[d][c] = 0;
        }
    }

    /* Mark the cards in the decks */
    for(d=0;d<decks_num*4;d++)
    {
        for(c=1;c<=fcs_foundation_value(*state, d);c++)
        {
            cards[d%4][c]++;
        }
    }

    /* Mark the cards in the freecells */
    for(f=0;f<freecells_num;f++)
    {
        card = fcs_freecell_card(*state, f);
        if (fcs_card_is_valid(card))
        {
            cards
                [fcs_card_suit(card)]
                [fcs_card_rank(card)] ++;
        }
    }

    /* Mark the cards in the stacks */
    for(s=0;s<stacks_num;s++)
    {
        col = fcs_state_get_col(*state, s);
        col_len = fcs_col_len(col);
        for(c=0;c<col_len;c++)
        {
            card = fcs_col_get_card(col,c);
            if (fcs_card_is_empty(card))
            {
                *misplaced_card = fc_solve_empty_card;
                return FCS_STATE_VALIDITY__EMPTY_SLOT;
            }
            cards
                [fcs_card_suit(card)]
                [fcs_card_rank(card)] ++;

        }
    }

    /* Now check if there are extra or missing cards */

    for(int suit_idx = 0; suit_idx < 4; suit_idx ++)
    {
        for (int rank = 1; rank <= FCS_MAX_RANK ; rank++)
        {
            if (cards[suit_idx][rank] != decks_num)
            {
                *misplaced_card = fcs_make_card(rank, suit_idx);
                return ((cards[suit_idx][rank] < decks_num)
                    ? FCS_STATE_VALIDITY__MISSING_CARD
                    : FCS_STATE_VALIDITY__EXTRA_CARD
                    )
                    ;
            }
        }
    }

    return FCS_STATE_VALIDITY__OK;
}

#undef state

#ifdef __cplusplus
}
#endif

enum
{
    FCS_VISITED_IN_SOLUTION_PATH = 0x1,
    FCS_VISITED_IN_OPTIMIZED_PATH = 0x2,
    FCS_VISITED_DEAD_END = 0x4,
    FCS_VISITED_ALL_TESTS_DONE = 0x8,
    FCS_VISITED_GENERATED_BY_PRUNING = 0x10,
};

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef DEBUG_STATES
#define FCS_WITH_CARD_COMPARE_LOOKUP
#endif

static GCC_INLINE int fc_solve_card_compare(
        const fcs_card_t c1,
        const fcs_card_t c2
        )
{
#ifdef FCS_WITH_CARD_COMPARE_LOOKUP
    return (c1)-(c2);
#else
    if (fcs_card_rank(c1) > fcs_card_rank(c2))
    {
        return 1;
    }
    else if (fcs_card_rank(c1) < fcs_card_rank(c2))
    {
        return -1;
    }
    else if (fcs_card_suit(c1) > fcs_card_suit(c2))
    {
        return 1;
    }
    else if (fcs_card_suit(c1) < fcs_card_suit(c2))
    {
        return -1;
    }
    else
    {
        return 0;
    }
#endif
}

#if defined(INDIRECT_STACK_STATES)
static GCC_INLINE int fc_solve_stack_compare_for_comparison(const void * const v_s1, const void * const v_s2)
{
    const fcs_card_t * const s1 = (const fcs_card_t * const)v_s1;
    const fcs_card_t * const s2 = (const fcs_card_t * const)v_s2;

    const int min_len = min(s1[0], s2[0]);
    {
        for(int a=1 ; a <= min_len ; a++)
        {
            int ret = fc_solve_card_compare(s1[a],s2[a]);
            if (ret != 0)
            {
                return ret;
            }
        }
    }
    /*
     * The reason I do the stack length comparisons after the card-by-card
     * comparison is to maintain correspondence with
     * fcs_stack_compare_for_stack_sort, and with the one card comparison
     * of the other state representation mechanisms.
     * */
    /* For some reason this code is faster than s1[0]-s2[0] */
    if (s1[0] < s2[0])
    {
        return -1;
    }
    else if (s1[0] > s2[0])
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

#endif

#endif /* FC_SOLVE__STATE_H */
