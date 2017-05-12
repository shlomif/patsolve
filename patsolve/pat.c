/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */

/* Common routines & arrays. */

#include "alloc_wrap.h"
#include "count.h"

#include "instance.h"
#include "fnv.h"
#include "tree.h"

/* Names of the cards.  The ordering is defined in pat.h. */

static inline int calc_empty_col_idx(
    fcs_pats_thread_t *const soft_thread, const int stacks_num)
{
    for (int w = 0; w < stacks_num; w++)
    {
        if (!fcs_col_len(fcs_state_get_col(soft_thread->current_pos.s, w)))
        {
            return w;
        }
    }
    return -1;
}

/* Automove logic.  Freecell games must avoid certain types of automoves. */
static inline fcs_bool_t good_automove(
    fcs_pats_thread_t *const soft_thread, const int o, const int r)
{
#ifndef FCS_FREECELL_ONLY
    const fc_solve_instance_t *const instance = soft_thread->instance;
#endif

    if (
#ifndef FCS_FREECELL_ONLY
        (GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
            FCS_SEQ_BUILT_BY_SUIT) ||
#endif
        r <= 2)
    {
        return TRUE;
    }

    /* Check the Out piles of opposite color. */

    for (int i = 1 - (o & 1); i < 4; i += 2)
    {
        if (fcs_foundation_value(soft_thread->current_pos.s, i) < r - 1)
        {

#if 1 /* Raymond's Rule */
            /* Not all the N-1's of opposite color are out
            yet.  We can still make an automove if either
            both N-2's are out or the other same color N-3
            is out (Raymond's rule).  Note the re-use of
            the loop variable i.  We return here and never
            make it back to the outer loop. */

            for (int j = 1 - (o & 1); j < 4; j += 2)
            {
                if (fcs_foundation_value(soft_thread->current_pos.s, j) < r - 2)
                {
                    return FALSE;
                }
            }
            return (fcs_foundation_value(
                        soft_thread->current_pos.s, ((o + 2) & 3)) >= r - 3);
#else /* Horne's Rule */
            return FALSE;
#endif
        }
    }

    return TRUE;
}
/* Get the possible moves from a position, and store them in
 * soft_thread->possible_moves[]. */

static inline int get_possible_moves(fcs_pats_thread_t *const soft_thread,
    fcs_bool_t *const a, int *const num_cards_out)
{
#ifndef FCS_FREECELL_ONLY
    const fc_solve_instance_t *const instance = soft_thread->instance;
#endif
    DECLARE_STACKS();

/* Check for moves from soft_thread->current_pos.stacks to
 * soft_thread->current_pos.foundations. */

#define NUM_MOVES (move_ptr - soft_thread->possible_moves)
    typeof(soft_thread->possible_moves[0]) *move_ptr =
        soft_thread->possible_moves;
    for (int w = 0; w < LOCAL_STACKS_NUM; ++w)
    {
        const fcs_cards_column_t col =
            fcs_state_get_col(soft_thread->current_pos.s, w);
        const int col_len = fcs_col_len(col);
        if (col_len > 0)
        {
            const fcs_card_t card = fcs_col_get_card(col, col_len - 1);
            const int o = fcs_card_suit(card);
            if (fcs_card_rank(card) ==
                fcs_foundation_value(soft_thread->current_pos.s, o) + 1)
            {
                *(move_ptr++) = (typeof(*move_ptr)){.card = card,
                    .from = w,
                    .fromtype = FCS_PATS__TYPE_WASTE,
                    .to = o,
                    .totype = FCS_PATS__TYPE_FOUNDATION,
                    .srccard =
                        ((col_len > 1) ? fcs_col_get_card(col, col_len - 2)
                                       : fc_solve_empty_card),
                    .destcard = fc_solve_empty_card,
                    .pri = 0}; /* unused */
                /* If it's an automove, just do it. */
                if (good_automove(soft_thread, o, fcs_card_rank(card)))
                {
                    *a = TRUE;
                    if (NUM_MOVES != 1)
                    {
                        soft_thread->possible_moves[0] = move_ptr[-1];
                    }
                    return 1;
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to
     * soft_thread->current_pos.foundations. */

    for (int t = 0; t < LOCAL_FREECELLS_NUM; t++)
    {
        if (fcs_freecell_card(soft_thread->current_pos.s, t) !=
            fc_solve_empty_card)
        {
            const fcs_card_t card =
                fcs_freecell_card(soft_thread->current_pos.s, t);
            const int o = fcs_card_suit(card);
            if (fcs_card_rank(card) ==
                fcs_foundation_value(soft_thread->current_pos.s, o) + 1)
            {
                *(move_ptr++) = (typeof(*move_ptr)){.card = card,
                    .from = t,
                    .fromtype = FCS_PATS__TYPE_FREECELL,
                    .to = o,
                    .totype = FCS_PATS__TYPE_FOUNDATION,
                    .srccard = fc_solve_empty_card,
                    .destcard = fc_solve_empty_card,
                    .pri = 0}; /* unused */
                /* If it's an automove, just do it. */
                if (good_automove(soft_thread, o, fcs_card_rank(card)))
                {
                    *a = TRUE;
                    if (NUM_MOVES != 1)
                    {
                        soft_thread->possible_moves[0] = move_ptr[-1];
                    }
                    return 1;
                }
            }
        }
    }

    /* No more automoves, but remember if there were any moves out. */

    *a = FALSE;
    *num_cards_out = NUM_MOVES;

    /* Check for moves from non-singleton soft_thread->current_pos.stacks cells
    to one of any
    empty soft_thread->current_pos.stacks cells. */

    const fcs_bool_t not_King_only =
#ifndef FCS_FREECELL_ONLY
        (!((INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY)));
#else
        TRUE;
#endif

    const int empty_col_idx = calc_empty_col_idx(soft_thread, LOCAL_STACKS_NUM);
    const fcs_bool_t has_empty_col = (empty_col_idx >= 0);
    if (has_empty_col)
    {
        for (int i = 0; i < LOCAL_STACKS_NUM; i++)
        {
            const fcs_cards_column_t i_col =
                fcs_state_get_col(soft_thread->current_pos.s, i);
            const int i_col_len = fcs_col_len(i_col);
            if (i_col_len > 1)
            {
                const fcs_card_t card = fcs_col_get_card(i_col, i_col_len - 1);
                if (fcs_pats_is_king_only(not_King_only, card))
                {
                    *(move_ptr++) = (typeof(*move_ptr)){
                        .card = card,
                        .from = i,
                        .fromtype = FCS_PATS__TYPE_WASTE,
                        .to = empty_col_idx,
                        .totype = FCS_PATS__TYPE_WASTE,
                        .srccard = fcs_col_get_card(i_col, i_col_len - 2),
                        .destcard = fc_solve_empty_card,
                        .pri = soft_thread->pats_solve_params.x[3],
                    };
                }
            }
        }
    }

#ifndef FCS_FREECELL_ONLY
    const fcs_card_t game_variant_suit_mask = instance->game_variant_suit_mask;
    const fcs_card_t game_variant_desired_suit_value =
        instance->game_variant_desired_suit_value;
#endif
    /* Check for moves from soft_thread->current_pos.stacks to non-empty
     * soft_thread->current_pos.stacks cells. */

    for (int i = 0; i < LOCAL_STACKS_NUM; i++)
    {
        const fcs_cards_column_t i_col =
            fcs_state_get_col(soft_thread->current_pos.s, i);
        const int i_col_len = fcs_col_len(i_col);
        if (i_col_len > 0)
        {
            const fcs_card_t card = fcs_col_get_card(i_col, i_col_len - 1);
            for (int w = 0; w < LOCAL_STACKS_NUM; w++)
            {
                if (i == w)
                {
                    continue;
                }
                const fcs_cards_column_t w_col =
                    fcs_state_get_col(soft_thread->current_pos.s, w);
                if (fcs_col_len(w_col) > 0)
                {
                    const fcs_card_t w_card =
                        fcs_col_get_card(w_col, fcs_col_len(w_col) - 1);
                    if (fcs_card_rank(card) == fcs_card_rank(w_card) - 1 &&
                        fcs_pats_is_suitable(card, w_card
#ifndef FCS_FREECELL_ONLY
                            ,
                            game_variant_suit_mask,
                            game_variant_desired_suit_value
#endif
                            ))
                    {
                        *(move_ptr++) = (typeof(*move_ptr)){.card = card,
                            .from = i,
                            .fromtype = FCS_PATS__TYPE_WASTE,
                            .to = w,
                            .totype = FCS_PATS__TYPE_WASTE,
                            .srccard =
                                ((i_col_len > 1)
                                        ? fcs_col_get_card(i_col, i_col_len - 2)
                                        : fc_solve_empty_card),
                            .destcard = w_card,
                            .pri = soft_thread->pats_solve_params.x[4]};
                    }
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to non-empty
     * soft_thread->current_pos.stacks cells. */

    for (int t = 0; t < LOCAL_FREECELLS_NUM; t++)
    {
        const fcs_card_t card =
            fcs_freecell_card(soft_thread->current_pos.s, t);
        if (card != fc_solve_empty_card)
        {
            for (int w = 0; w < LOCAL_STACKS_NUM; w++)
            {
                fcs_cards_column_t w_col =
                    fcs_state_get_col(soft_thread->current_pos.s, w);
                if (fcs_col_len(w_col) > 0)
                {
                    const fcs_card_t w_card =
                        fcs_col_get_card(w_col, fcs_col_len(w_col) - 1);
                    if ((fcs_card_rank(card) == fcs_card_rank(w_card) - 1 &&
                            fcs_pats_is_suitable(card, w_card
#ifndef FCS_FREECELL_ONLY
                                ,
                                game_variant_suit_mask,
                                game_variant_desired_suit_value
#endif
                                )))
                    {
                        *(move_ptr++) = (typeof(*move_ptr)){.card = card,
                            .from = t,
                            .fromtype = FCS_PATS__TYPE_FREECELL,
                            .to = w,
                            .totype = FCS_PATS__TYPE_WASTE,
                            .srccard = fc_solve_empty_card,
                            .destcard = w_card,
                            .pri = soft_thread->pats_solve_params.x[5]};
                    }
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to one of any
     * empty soft_thread->current_pos.stacks cells. */

    if (has_empty_col)
    {
        for (int t = 0; t < LOCAL_FREECELLS_NUM; t++)
        {
            const fcs_card_t card =
                fcs_freecell_card(soft_thread->current_pos.s, t);
            if (card != fc_solve_empty_card &&
                fcs_pats_is_king_only(not_King_only, card))
            {
                *(move_ptr++) = (typeof(*move_ptr)){.card = card,
                    .from = t,
                    .fromtype = FCS_PATS__TYPE_FREECELL,
                    .to = empty_col_idx,
                    .totype = FCS_PATS__TYPE_WASTE,
                    .srccard = fc_solve_empty_card,
                    .destcard = fc_solve_empty_card,
                    .pri = soft_thread->pats_solve_params.x[6]};
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.stacks to one of any empty
     * soft_thread->current_pos.freecells cells. */

    {
        int t;
        for (t = 0; t < LOCAL_FREECELLS_NUM; t++)
        {
            if (fcs_freecell_card(soft_thread->current_pos.s, t) ==
                fc_solve_empty_card)
            {
                break;
            }
        }
        if (t < LOCAL_FREECELLS_NUM)
        {
            for (int w = 0; w < LOCAL_STACKS_NUM; w++)
            {
                fcs_cards_column_t w_col =
                    fcs_state_get_col(soft_thread->current_pos.s, w);
                const_AUTO(w_col_len, fcs_col_len(w_col));
                if (w_col_len > 0)
                {
                    *(move_ptr++) = (typeof(*move_ptr)){
                        .card = fcs_col_get_card(w_col, w_col_len - 1),
                        .from = w,
                        .fromtype = FCS_PATS__TYPE_WASTE,
                        .to = t,
                        .totype = FCS_PATS__TYPE_FREECELL,
                        .srccard = ((w_col_len > 1)
                                        ? fcs_col_get_card(w_col, w_col_len - 2)
                                        : fc_solve_empty_card),
                        .destcard = fc_solve_empty_card,
                        .pri = soft_thread->pats_solve_params.x[7],
                    };
                }
            }
        }
    }
    return NUM_MOVES;
}

/* Moves that can't be undone get slightly higher priority, since it means
we are moving a card for the first time. */

static inline fcs_bool_t is_irreversible_move(
#ifndef FCS_FREECELL_ONLY
    const fcs_card_t game_variant_suit_mask,
    const fcs_card_t game_variant_desired_suit_value,
#endif
    const fcs_bool_t King_only, const fcs_pats__move_t *const move_ptr)
{
    if (move_ptr->totype == FCS_PATS__TYPE_FOUNDATION)
    {
        return TRUE;
    }
    else if (move_ptr->fromtype == FCS_PATS__TYPE_WASTE)
    {
        const fcs_card_t srccard = move_ptr->srccard;
        if (srccard != fc_solve_empty_card)
        {
            const fcs_card_t card = move_ptr->card;
            if ((fcs_card_rank(card) != fcs_card_rank(srccard) - 1) ||
                !fcs_pats_is_suitable(card, srccard
#ifndef FCS_FREECELL_ONLY
                    ,
                    game_variant_suit_mask, game_variant_desired_suit_value
#endif
                    ))
            {
                return TRUE;
            }
        }
        /* TODO : This is probably a bug because move_ptr->card probably cannot
         * be
         * FCS_PATS__KING - only FCS_PATS__KING bitwise-ORed with some other
         * value.
         * */
        else if (King_only &&
                 move_ptr->card != fcs_make_card(FCS_PATS__KING, 0))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static inline void mark_irreversible(
    fcs_pats_thread_t *const soft_thread, const int n)
{
#ifndef FCS_FREECELL_ONLY
    const fc_solve_instance_t *const instance = soft_thread->instance;

    const fcs_card_t game_variant_suit_mask = instance->game_variant_suit_mask;
    const fcs_card_t game_variant_desired_suit_value =
        instance->game_variant_desired_suit_value;
#endif
    const fcs_bool_t King_only =
#ifndef FCS_FREECELL_ONLY
        (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY);
#else
        FALSE;
#endif

    const typeof(soft_thread->pats_solve_params.x[8]) x_param_8 =
        soft_thread->pats_solve_params.x[8];

    fcs_pats__move_t *move_ptr = soft_thread->possible_moves;
    const fcs_pats__move_t *const moves_end = move_ptr + n;
    for (; move_ptr < moves_end; ++move_ptr)
    {
        if (is_irreversible_move(
#ifndef FCS_FREECELL_ONLY
                game_variant_suit_mask, game_variant_desired_suit_value,
#endif
                King_only, move_ptr))
        {
            move_ptr->pri += x_param_8;
        }
    }
}

/* For each pile, return a unique identifier.  Although there are a
large number of possible piles, generally fewer than 1000 different
piles appear in any given game.  We'll use the pile's hash to find
a hash bucket that contains a short list of piles, along with their
identifiers. */

static inline int get_pilenum(fcs_pats_thread_t *const soft_thread, const int w)
{

    /* For a given pile, get its unique pile id.  If it doesn't have
    one, add it to the appropriate list and give it one.  First, get
    the hash bucket. */

    const int bucket =
        soft_thread->current_pos.stack_hashes[w] % FC_SOLVE_BUCKETLIST_NBUCKETS;

    /* Look for the pile in this bucket. */

    fcs_pats__bucket_list_t *list_iter = soft_thread->buckets_list[bucket],
                            *last_item = NULL;
    const fcs_cards_column_t w_col =
        fcs_state_get_col(soft_thread->current_pos.s, w);
    const int w_col_len = fcs_col_len(w_col);
    const char *w_col_data = (const char *)w_col + 1;
    for (; list_iter; list_iter = list_iter->next)
    {
        if (list_iter->hash == soft_thread->current_pos.stack_hashes[w])
        {
            if (memcmp((const char *)list_iter->pile, w_col_data, w_col_len) ==
                0)
            {
                break;
            }
        }
        last_item = list_iter;
    }

    /* If we didn't find it, make a new one and add it to the list. */

    if (!list_iter)
    {
        if (soft_thread->next_pile_idx == FC_SOLVE__MAX_NUM_PILES)
        {
#if 0
            fc_solve_msg("Ran out of pile numbers!");
#endif
            return -1;
        }
        list_iter = fc_solve_pats__new(soft_thread, fcs_pats__bucket_list_t);
        if (list_iter == NULL)
        {
            return -1;
        }
        list_iter->pile =
            fc_solve_pats__new_array(soft_thread, u_char, w_col_len + 1);
        if (list_iter->pile == NULL)
        {
            fc_solve_pats__free_ptr(
                soft_thread, list_iter, fcs_pats__bucket_list_t);
            return -1;
        }

        /* Store the new pile along with its hash.  Maintain
        a reverse mapping so we can unpack the piles swiftly. */

        memcpy((char *)list_iter->pile, w_col_data, w_col_len + 1);
        list_iter->hash = soft_thread->current_pos.stack_hashes[w];
        list_iter->next = NULL;
        if (last_item)
        {
            last_item->next = list_iter;
        }
        else
        {
            soft_thread->buckets_list[bucket] = list_iter;
        }
        soft_thread->bucket_from_pile_lookup[list_iter->pilenum =
                                                 soft_thread->next_pile_idx++] =
            list_iter;
    }

    return list_iter->pilenum;
}

/* Win.  Print out the move stack. */

static inline void win(
    fcs_pats_thread_t *const soft_thread, fcs_pats_position_t *const pos)
{
    if (soft_thread->moves_to_win)
    {
        free(soft_thread->moves_to_win);
        soft_thread->moves_to_win = NULL;
    }

    size_t num_moves = 0;
    for (fcs_pats_position_t *p = pos; p->parent; p = p->parent)
    {
        ++num_moves;
    }
    typeof(soft_thread->moves_to_win) moves_to_win =
        SMALLOC(moves_to_win, num_moves);
    if (!moves_to_win)
    {
        return; /* how sad, so close... */
    }
    var_AUTO(moves_ptr, moves_to_win + num_moves);
    for (fcs_pats_position_t *p = pos; p->parent; p = p->parent)
    {
        *(--moves_ptr) = (p->move);
    }

    soft_thread->moves_to_win = moves_to_win;
    soft_thread->num_moves_to_win = num_moves;
}

/* These two routines make and unmake moves. */

#ifndef FCS_FREECELL_ONLY
/* This prune applies only to Seahaven in -k mode: if we're putting a card
onto a soft_thread->current_pos.stacks pile, and if that pile already has
LOCAL_FREECELLS_NUM+1 cards of this suit in
a row, and if there is a smaller card of the same suit below the run, then
the position is unsolvable.  This cuts out a lot of useless searching, so
it's worth checking.  */

static inline int prune_seahaven(fcs_pats_thread_t *const soft_thread,
    const fcs_pats__move_t *const move_ptr)
{
    const fc_solve_instance_t *const instance = soft_thread->instance;
    const_SLOT(game_params, instance);

    if (!(GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
            FCS_SEQ_BUILT_BY_SUIT) ||
        !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) ||
        move_ptr->totype != FCS_PATS__TYPE_WASTE)
    {
        return FALSE;
    }

    /* Count the number of cards below this card. */

    const int r = fcs_card_rank(move_ptr->card) + 1;
    const int s = fcs_card_suit(move_ptr->card);
    const fcs_cards_column_t col =
        fcs_state_get_col(soft_thread->current_pos.s, move_ptr->to);
    const int len = fcs_col_len(col);

    {
        int j = 0;
        for (int i = len - 1; i >= 0; i--)
        {
            fcs_card_t card = fcs_col_get_card(col, i);
            if (fcs_card_suit(card) == s && fcs_card_rank(card) == r + j)
            {
                j++;
            }
        }
        if (j < LOCAL_FREECELLS_NUM + 1)
        {
            return FALSE;
        }
    }

    /* If there's a smaller card of this suit in the pile, we can prune
    the move. */
    const int r_minus = r - 1;
    for (int i = 0; i < len; i++)
    {
        fcs_card_t card = fcs_col_get_card(col, i);
        if ((fcs_card_suit(card) == s) && (fcs_card_rank(card) < r_minus))
        {
            return TRUE;
        }
    }

    return FALSE;
}
#endif

/* This utility routine is used to check if a card is ever moved in
a sequence of moves. */

static inline int was_card_moved(
    const fcs_card_t card, fcs_pats__move_t *const *const moves, const int j)
{
    for (int i = 0; i < j; i++)
    {
        if (moves[i]->card == card)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* This utility routine is used to check if a card is ever used as a
destination in a sequence of moves. */

static inline int is_card_dest(
    const fcs_card_t card, fcs_pats__move_t *const *const moves, const int j)
{
    for (int i = 0; i < j; i++)
    {
        if (moves[i]->destcard == card)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static inline int was_card_moved_or_dest(
    const fcs_card_t card, fcs_pats__move_t *const *const moves, const int j)
{
    return (was_card_moved(card, moves, j) || is_card_dest(card, moves, j));
}

/* Prune redundant moves, if we can prove that they really are redundant. */

#define MAX_PREVIOUS_MOVES 4 /* Increasing this beyond 4 doesn't do much. */

static inline int prune_redundant(fcs_pats_thread_t *const soft_thread,
    const fcs_pats__move_t *const move_ptr, fcs_pats_position_t *const pos0)
{
    DECLARE_STACKS();
    int j;
    fcs_pats__move_t *m, *prev[MAX_PREVIOUS_MOVES];

    /* Don't move the same card twice in a row. */
    fcs_pats_position_t *pos = pos0;
    if (pos->depth == 0)
    {
        return FALSE;
    }
    m = &pos->move;
    if (m->card == move_ptr->card)
    {
        return TRUE;
    }

    /* The check above is the simplest case of the more general strategy
    of searching the move stack (up the chain of parents) to prove that
    the current move is redundant.  To do that, we need some more data. */

    pos = pos->parent;
    if (pos->depth == 0)
    {
        return FALSE;
    }
    prev[0] = m;
    j = -1;
    for (int i = 1; i < MAX_PREVIOUS_MOVES; i++)
    {

        /* Make a list of the last few moves. */

        prev[i] = &pos->move;

        /* Locate the last time we moved this card. */

        if (m->card == move_ptr->card)
        {
            j = i;
            break;
        }

        /* Keep going up the move stack, looking for this
        card, until we run out of moves (or patience). */

        pos = pos->parent;
        if (pos->depth == 0)
        {
            return FALSE;
        }
    }

    /* If we haven't moved this card recently, assume this isn't
    a redundant move. */

    if (j < 0)
    {
        return FALSE;
    }

    /* If the number of empty soft_thread->current_pos.freecells cells
     * ever goes to zero, from prev[0] to
    prev[j-1], there may be a dependency.  We also want to know if there
    were any empty soft_thread->current_pos.freecells cells on move prev[j]. */

    fcs_bool_t was_all_freecells_occupied = FALSE;
    pos = pos0;
    for (int i = 0; i < j; i++)
    {
        was_all_freecells_occupied =
            was_all_freecells_occupied ||
            (pos->num_cards_in_freecells == LOCAL_FREECELLS_NUM);
        pos = pos->parent;
    }

    /* Now, prev[j] (m) is a move involving the same card as the current
    move.  See if the current move inverts that move.  There are several
    cases. */

    /* soft_thread->current_pos.freecells -> soft_thread->current_pos.stacks,
     * ..., soft_thread->current_pos.stacks ->
     * soft_thread->current_pos.freecells */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL &&
        m->totype == FCS_PATS__TYPE_WASTE &&
        move_ptr->fromtype == FCS_PATS__TYPE_WASTE &&
        move_ptr->totype == FCS_PATS__TYPE_FREECELL)
    {

        /* If the number of soft_thread->current_pos.freecells cells goes to
        zero, we have a soft_thread->current_pos.freecells
        dependency, and we can't prune. */

        if (was_all_freecells_occupied)
        {
            return FALSE;
        }

        /* If any intervening move used this card as a destination,
        we have a dependency and we can't prune. */

        if (is_card_dest(move_ptr->card, prev, j))
        {
            return FALSE;
        }

        return TRUE;
    }

    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.freecells,
     * ..., soft_thread->current_pos.freecells ->
     * soft_thread->current_pos.stacks */
    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks, ...,
     * soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks */

    if ((m->fromtype == FCS_PATS__TYPE_WASTE &&
            m->totype == FCS_PATS__TYPE_FREECELL &&
            move_ptr->fromtype == FCS_PATS__TYPE_FREECELL &&
            move_ptr->totype == FCS_PATS__TYPE_WASTE) ||
        (m->fromtype == FCS_PATS__TYPE_WASTE &&
            m->totype == FCS_PATS__TYPE_WASTE &&
            move_ptr->fromtype == FCS_PATS__TYPE_WASTE &&
            move_ptr->totype == FCS_PATS__TYPE_WASTE))
    {

        /* If we're not putting the card back where we found it,
        it's not an inverse. */

        if (m->srccard != move_ptr->destcard)
        {
            return FALSE;
        }

        /* If any of the intervening moves either moves the card
        that was uncovered or uses it as a destination (including
        fc_solve_empty_card), there is a dependency. */

        if (was_card_moved_or_dest(move_ptr->destcard, prev, j))
        {
            return FALSE;
        }

        return TRUE;
    }

    /* These are not inverse prunes, we're taking a shortcut. */

    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks, ...,
     * soft_thread->current_pos.stacks -> soft_thread->current_pos.freecells */

    if (m->fromtype == FCS_PATS__TYPE_WASTE &&
        m->totype == FCS_PATS__TYPE_WASTE &&
        move_ptr->fromtype == FCS_PATS__TYPE_WASTE &&
        move_ptr->totype == FCS_PATS__TYPE_FREECELL)
    {

        /* If we could have moved the card to soft_thread->current_pos.freecells
        on the
        first move, prune.  There are other cases, but they
        are more complicated. */

        if (pos->num_cards_in_freecells != LOCAL_FREECELLS_NUM &&
            !was_all_freecells_occupied)
        {
            return TRUE;
        }

        return FALSE;
    }

    /* soft_thread->current_pos.freecells -> soft_thread->current_pos.stacks,
     * ..., soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks
     */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL &&
        m->totype == FCS_PATS__TYPE_WASTE &&
        move_ptr->fromtype == FCS_PATS__TYPE_WASTE &&
        move_ptr->totype == FCS_PATS__TYPE_WASTE)
    {

        /* We can prune these moves as long as the intervening
        moves don't touch move_ptr->destcard. */

        if (was_card_moved_or_dest(move_ptr->destcard, prev, j))
        {
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}

/*
 * Next card in rank.
 * */
static inline fcs_card_t fcs_pats_next_card(const fcs_card_t card)
{
    return card + (1 << 2);
}

/* Move prioritization.  Given legal, pruned moves, there are still some
that are a waste of time, especially in the endgame where there are lots of
possible moves, but few productive ones.  Note that we also prioritize
positions when they are added to the queue. */

#define NUM_PRIORITIZE_PILES 8

static inline void prioritize(fcs_pats_thread_t *const soft_thread,
    fcs_pats__move_t *const moves_start, const int n)
{
    DECLARE_STACKS();
    int pile[NUM_PRIORITIZE_PILES];

    /* There are 4 cards that we "need": the next cards to go out.  We
    give higher priority to the moves that remove cards from the piles
    containing these cards. */

    for (size_t i = 0; i < COUNT(pile); i++)
    {
        pile[i] = -1;
    }

    fcs_card_t needed_cards[FCS_NUM_SUITS];
    for (int suit = 0; suit < FCS_NUM_SUITS; suit++)
    {
        needed_cards[suit] = fc_solve_empty_card;
        const fcs_card_t rank =
            fcs_foundation_value(soft_thread->current_pos.s, suit);
        if (rank != FCS_PATS__KING)
        {
            needed_cards[suit] = fcs_make_card(rank + 1, suit);
        }
    }

    /* Locate the needed cards.  There's room for optimization here,
    like maybe an array that keeps track of every card; if maintaining
    such an array is not too expensive. */

    int num_piles = 0;

    for (int w = 0; w < LOCAL_STACKS_NUM; w++)
    {
        fcs_cards_column_t col =
            fcs_state_get_col(soft_thread->current_pos.s, w);
        const int len = fcs_col_len(col);
        for (int i = 0; i < len; i++)
        {
            const fcs_card_t card = fcs_col_get_card(col, i);
            const int suit = fcs_card_suit(card);

            /* Save the locations of the piles containing
            not only the card we need next, but the card
            after that as well. */

            if (needed_cards[suit] != fc_solve_empty_card &&
                (card == needed_cards[suit] ||
                    card == fcs_pats_next_card(needed_cards[suit])))
            {
                pile[num_piles++] = w;
                if (num_piles == COUNT(pile))
                {
                    goto end_of_stacks;
                }
            }
        }
    }

end_of_stacks:;

    /* Now if any of the moves remove a card from any of the piles
    listed in pile[], bump their priority.  Likewise, if a move
    covers a card we need, decrease its priority.  These priority
    increments and decrements were determined empirically. */

    const typeof(moves_start) moves_end = moves_start + n;
    for (fcs_pats__move_t *move_ptr = moves_start; move_ptr < moves_end;
         move_ptr++)
    {
        if (move_ptr->card != fc_solve_empty_card)
        {
            if (move_ptr->fromtype == FCS_PATS__TYPE_WASTE)
            {
                const int w = move_ptr->from;
                for (int j = 0; j < num_piles; j++)
                {
                    if (w == pile[j])
                    {
                        move_ptr->pri += soft_thread->pats_solve_params.x[0];
                    }
                }
                fcs_cards_column_t col =
                    fcs_state_get_col(soft_thread->current_pos.s, w);

                if (fcs_col_len(col) > 1)
                {
                    const fcs_card_t card =
                        fcs_col_get_card(col, fcs_col_len(col) - 2);
                    if (card == needed_cards[(int)fcs_card_suit(card)])
                    {
                        move_ptr->pri += soft_thread->pats_solve_params.x[1];
                    }
                }
            }
            if (move_ptr->totype == FCS_PATS__TYPE_WASTE)
            {
                for (int j = 0; j < num_piles; j++)
                {
                    if (move_ptr->to == pile[j])
                    {
                        move_ptr->pri -= soft_thread->pats_solve_params.x[2];
                    }
                }
            }
        }
    }
}

static inline fcs_bool_t is_win(fcs_pats_thread_t *const soft_thread)
{
    for (int o = 0; o < 4; o++)
    {
        if (fcs_foundation_value(soft_thread->current_pos.s, o) !=
            FCS_PATS__KING)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Generate an array of the moves we can make from this position. */

fcs_pats__move_t *fc_solve_pats__get_moves(fcs_pats_thread_t *const soft_thread,
    fcs_pats_position_t *const pos, int *const num_moves)
{
    int num_cards_out = 0;

    /* Fill in the soft_thread->possible_moves array. */

    fcs_bool_t a;
    const int total_num_moves =
        get_possible_moves(soft_thread, &a, &num_cards_out);
    int n = total_num_moves;

    if (!a)
    {

        /* Throw out some obviously bad (non-auto)moves. */
        typeof(soft_thread->possible_moves[0]) *move_ptr =
            soft_thread->possible_moves;
        const typeof(move_ptr) moves_end = move_ptr + total_num_moves;
        for (; move_ptr < moves_end; move_ptr++)
        {

#ifndef FCS_FREECELL_ONLY
            /* Special prune for Seahaven -k. */

            if (prune_seahaven(soft_thread, move_ptr))
            {
                move_ptr->card = fc_solve_empty_card;
                n--;
                continue;
            }
#endif

            /* Prune redundant moves. */

            if (prune_redundant(soft_thread, move_ptr, pos))
            {
                move_ptr->card = fc_solve_empty_card;
                n--;
            }
        }

        /* Mark any irreversible moves. */

        mark_irreversible(soft_thread, n);
    }

    /* No moves?  Maybe we won. */

    if (n == 0)
    {
        if (is_win(soft_thread))
        {
            /* Report the win. */
            win(soft_thread, pos);

            if (soft_thread->dont_exit_on_sol)
            {
                soft_thread->num_solutions++;
            }
            else
            {
                soft_thread->status = FCS_PATS__WIN;
            }
        }

        /* We lost. */
        return NULL;
    }

    /* Prioritize these moves.  Automoves don't get queued, so they
    don't need a priority. */

    if (!a)
    {
        prioritize(soft_thread, soft_thread->possible_moves, total_num_moves);
    }

    /* Now copy to safe storage and return.  Non-auto moves out get put
    at the end.  Queueing them isn't a good idea because they are still
    good moves, even if they didn't pass the automove test.  So we still
    do the recursive solve() on them, but only after queueing the other
    moves. */

    {
        fcs_pats__move_t *move_ptr, *moves_start;
        move_ptr = moves_start =
            fc_solve_pats__new_array(soft_thread, fcs_pats__move_t, n);
        if (move_ptr == NULL)
        {
            return NULL;
        }
        *num_moves = n;
        if (a || num_cards_out == 0)
        {
            for (int i = 0; i < total_num_moves; i++)
            {
                if (soft_thread->possible_moves[i].card != fc_solve_empty_card)
                {
                    *move_ptr =
                        soft_thread->possible_moves[i]; /* struct copy */
                    move_ptr++;
                }
            }
        }
        else
        {
            for (int i = num_cards_out; i < total_num_moves; i++)
            {
                if (soft_thread->possible_moves[i].card != fc_solve_empty_card)
                {
                    *move_ptr =
                        soft_thread->possible_moves[i]; /* struct copy */
                    move_ptr++;
                }
            }
            for (int i = 0; i < num_cards_out; i++)
            {
                if (soft_thread->possible_moves[i].card != fc_solve_empty_card)
                {
                    *move_ptr = soft_thread->possible_moves[i];
                    ++move_ptr;
                }
            }
        }

        return moves_start;
    }
}

/* Comparison function for sorting the soft_thread->current_pos.stacks piles. */

static inline int wcmp(
    fcs_pats_thread_t *const soft_thread, const int a, const int b)
{
    if (soft_thread->pats_solve_params.x[9] < 0)
    {
        return soft_thread->current_pos.stack_ids[b] -
               soft_thread->current_pos.stack_ids[a]; /* newer piles first */
    }
    else
    {
        return soft_thread->current_pos.stack_ids[a] -
               soft_thread->current_pos.stack_ids[b]; /* older piles first */
    }
}

/* Sort the piles, to remove the physical differences between logically
equivalent layouts.  Assume it's already mostly sorted.  */

void fc_solve_pats__sort_piles(fcs_pats_thread_t *const soft_thread)
{
    DECLARE_STACKS();
    /* Make sure all the piles have id numbers. */

    for (int stack_idx = 0; stack_idx < LOCAL_STACKS_NUM; stack_idx++)
    {
        if ((soft_thread->current_pos.stack_ids[stack_idx] < 0) &&
            ((soft_thread->current_pos.stack_ids[stack_idx] =
                     get_pilenum(soft_thread, stack_idx)) < 0))
        {
            return;
        }
    }

    /* Sort them. */
    soft_thread->current_pos.column_idxs[0] = 0;
    for (int i = 1; i < LOCAL_STACKS_NUM; i++)
    {
        const typeof(i) w = i - 1;
        if (wcmp(soft_thread, soft_thread->current_pos.column_idxs[w], i) < 0)
        {
            soft_thread->current_pos.column_idxs[i] = i;
        }
        else
        {
            for (int j = w; j >= 0; --j)
            {
                soft_thread->current_pos.column_idxs[j + 1] =
                    soft_thread->current_pos.column_idxs[j];
                if (j == 0 ||
                    wcmp(soft_thread,
                        soft_thread->current_pos.column_idxs[j - 1], i) < 0)
                {
                    soft_thread->current_pos.column_idxs[j] = i;
                    break;
                }
            }
        }
    }

#if 0
    /* Compute the inverse. */

    for (i = 0; i < LOCAL_STACKS_NUM; i++) {
        soft_thread->current_pos.column_inv_idxs[soft_thread->current_pos.column_idxs[i]] = i;
    }
#endif
}
