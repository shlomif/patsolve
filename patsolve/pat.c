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
/* Common routines & arrays. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "inline.h"

#include "pat.h"
#include "fnv.h"
#include "tree.h"

/* Names of the cards.  The ordering is defined in pat.h. */

const card_t fc_solve_pats__output_suits[4] = { PS_DIAMOND, PS_CLUB, PS_HEART, PS_SPADE };

static GCC_INLINE int get_possible_moves(fc_solve_soft_thread_t * soft_thread, int *, int *);
static void mark_irreversible(fc_solve_soft_thread_t * soft_thread, int n);
static void win(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos);
static GCC_INLINE int get_pilenum(fc_solve_soft_thread_t * soft_thread, int w);

/* Hash a pile. */

static GCC_INLINE void hashpile(fc_solve_soft_thread_t * soft_thread, int w)
{
    soft_thread->current_pos.stacks[w][soft_thread->current_pos.columns_lens[w]] = 0;
    soft_thread->current_pos.stack_hashes[w] = fnv_hash_str(soft_thread->current_pos.stacks[w]);

    /* Invalidate this pile's id.  We'll calculate it later. */

    soft_thread->current_pos.stack_ids[w] = -1;
}

/* Hash the whole layout.  This is called once, at the start. */


void fc_solve_pats__hash_layout(fc_solve_soft_thread_t * soft_thread)
{
    DECLARE_STACKS();
    int w;

    for (w = 0; w < LOCAL_STACKS_NUM; w++) {
        hashpile(soft_thread, w);
    }
}

/* These two routines make and unmake moves. */

void freecell_solver_pats__make_move(fc_solve_soft_thread_t * soft_thread, fcs_pats__move_t *m)
{
    int from, to;
    card_t card;

    from = m->from;
    to = m->to;

    /* Remove from pile. */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL) {
        card = soft_thread->current_pos.freecells[from];
        soft_thread->current_pos.freecells[from] = NONE;
    } else {
        card = *(soft_thread->current_pos.stack_ptrs[from]--);
        soft_thread->current_pos.columns_lens[from]--;
        hashpile(soft_thread, from);
    }

    /* Add to pile. */

    if (m->totype == FCS_PATS__TYPE_FREECELL) {
        soft_thread->current_pos.freecells[to] = card;
    } else if (m->totype == FCS_PATS__TYPE_WASTE) {
        *++soft_thread->current_pos.stack_ptrs[to] = card;
        soft_thread->current_pos.columns_lens[to]++;
        hashpile(soft_thread, to);
    } else {
        soft_thread->current_pos.foundations[to]++;
    }
}

void fc_solve_pats__undo_move(fc_solve_soft_thread_t * soft_thread, fcs_pats__move_t *m)
{
    int from, to;
    card_t card;

    from = m->from;
    to = m->to;

    /* Remove from 'to' pile. */

    if (m->totype == FCS_PATS__TYPE_FREECELL) {
        card = soft_thread->current_pos.freecells[to];
        soft_thread->current_pos.freecells[to] = NONE;
    } else if (m->totype == FCS_PATS__TYPE_WASTE) {
        card = *(soft_thread->current_pos.stack_ptrs[to]--);
        soft_thread->current_pos.columns_lens[to]--;
        hashpile(soft_thread, to);
    } else {
        card = fcs_pats_make_card(soft_thread->current_pos.foundations[to], fc_solve_pats__output_suits[to]);
        soft_thread->current_pos.foundations[to]--;
    }

    /* Add to 'from' pile. */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL) {
        soft_thread->current_pos.freecells[from] = card;
    } else {
        *++soft_thread->current_pos.stack_ptrs[from] = card;
        soft_thread->current_pos.columns_lens[from]++;
        hashpile(soft_thread, from);
    }
}

/* This prune applies only to Seahaven in -k mode: if we're putting a card
onto a soft_thread->current_pos.stacks pile, and if that pile already has LOCAL_FREECELLS_NUM+1 cards of this suit in
a row, and if there is a smaller card of the same suit below the run, then
the position is unsolvable.  This cuts out a lot of useless searching, so
it's worth checking.  */

static int prune_seahaven(fc_solve_soft_thread_t * soft_thread, fcs_pats__move_t *mp)
{
    const fc_solve_instance_t * const instance = soft_thread->instance;
    const fcs_game_type_params_t game_params = instance->game_params;
    int i, j, w, r, s;

    if (!(GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) || !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) || mp->totype != FCS_PATS__TYPE_WASTE) {
        return FALSE;
    }
    w = mp->to;

    /* Count the number of cards below this card. */

    j = 0;
    r = fcs_pats_card_rank(mp->card) + 1;
    s = fcs_pats_card_suit(mp->card);
    for (i = soft_thread->current_pos.columns_lens[w] - 1; i >= 0; i--) {
        if (fcs_pats_card_suit(soft_thread->current_pos.stacks[w][i]) == s && fcs_pats_card_rank(soft_thread->current_pos.stacks[w][i]) == r + j) {
            j++;
        }
    }
    if (j < LOCAL_FREECELLS_NUM + 1) {
        return FALSE;
    }

    /* If there's a smaller card of this suit in the pile, we can prune
    the move. */

    j = soft_thread->current_pos.columns_lens[w];
    r -= 1;
    for (i = 0; i < j; i++) {
        if ( (fcs_pats_card_suit(soft_thread->current_pos.stacks[w][i]) == s)
            && (fcs_pats_card_rank(soft_thread->current_pos.stacks[w][i]) < r) ) {
            return TRUE;
        }
    }

    return FALSE;
}

/* This utility routine is used to check if a card is ever moved in
a sequence of moves. */

static GCC_INLINE int cardmoved(card_t card, fcs_pats__move_t **mpp, int j)
{
    int i;

    for (i = 0; i < j; i++) {
        if (mpp[i]->card == card) {
            return TRUE;
        }
    }
    return FALSE;
}

/* This utility routine is used to check if a card is ever used as a
destination in a sequence of moves. */

static GCC_INLINE int cardisdest(card_t card, fcs_pats__move_t **mpp, int j)
{
    int i;

    for (i = 0; i < j; i++) {
        if (mpp[i]->destcard == card) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Prune redundant moves, if we can prove that they really are redundant. */

#define MAXPREVMOVE 4   /* Increasing this beyond 4 doesn't do much. */

static int prune_redundant(fc_solve_soft_thread_t * soft_thread, fcs_pats__move_t *mp, fcs_pats_position_t *pos0)
{
    DECLARE_STACKS();
    int i, j;
    int zerot;
    fcs_pats__move_t *m, *prev[MAXPREVMOVE];
    fcs_pats_position_t *pos;

    /* Don't move the same card twice in a row. */

    pos = pos0;
    if (pos->depth == 0) {
        return FALSE;
    }
    m = &pos->move;
    if (m->card == mp->card) {
        return TRUE;
    }

    /* The check above is the simplest case of the more general strategy
    of searching the move stack (up the chain of parents) to prove that
    the current move is redundant.  To do that, we need some more data. */

    pos = pos->parent;
    if (pos->depth == 0) {
        return FALSE;
    }
    prev[0] = m;
    j = -1;
    for (i = 1; i < MAXPREVMOVE; i++) {

        /* Make a list of the last few moves. */

        prev[i] = &pos->move;

        /* Locate the last time we moved this card. */

        if (m->card == mp->card) {
            j = i;
            break;
        }

        /* Keep going up the move stack, looking for this
        card, until we run out of moves (or patience). */

        pos = pos->parent;
        if (pos->depth == 0) {
            return FALSE;
        }
    }

    /* If we haven't moved this card recently, assume this isn't
    a redundant move. */

    if (j < 0) {
        return FALSE;
    }

    /* If the number of empty soft_thread->current_pos.freecells cells
     * ever goes to zero, from prev[0] to
    prev[j-1], there may be a dependency.  We also want to know if there
    were any empty soft_thread->current_pos.freecells cells on move prev[j]. */

    zerot = 0;
    pos = pos0;
    for (i = 0; i < j; i++) {
        zerot |= (pos->ntemp == LOCAL_FREECELLS_NUM);
        pos = pos->parent;
    }

    /* Now, prev[j] (m) is a move involving the same card as the current
    move.  See if the current move inverts that move.  There are several
    cases. */

    /* soft_thread->current_pos.freecells -> soft_thread->current_pos.stacks, ..., soft_thread->current_pos.stacks -> soft_thread->current_pos.freecells */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL && m->totype == FCS_PATS__TYPE_WASTE &&
        mp->fromtype == FCS_PATS__TYPE_WASTE && mp->totype == FCS_PATS__TYPE_FREECELL) {

        /* If the number of soft_thread->current_pos.freecells cells goes to zero, we have a soft_thread->current_pos.freecells
        dependency, and we can't prune. */

        if (zerot) {
            return FALSE;
        }

        /* If any intervening move used this card as a destination,
        we have a dependency and we can't prune. */

        if (cardisdest(mp->card, prev, j)) {
            return FALSE;
        }

        return TRUE;
    }

    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.freecells, ..., soft_thread->current_pos.freecells -> soft_thread->current_pos.stacks */
    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks, ..., soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks */

    if ((m->fromtype == FCS_PATS__TYPE_WASTE && m->totype == FCS_PATS__TYPE_FREECELL &&
         mp->fromtype == FCS_PATS__TYPE_FREECELL && mp->totype == FCS_PATS__TYPE_WASTE) ||
        (m->fromtype == FCS_PATS__TYPE_WASTE && m->totype == FCS_PATS__TYPE_WASTE &&
         mp->fromtype == FCS_PATS__TYPE_WASTE && mp->totype == FCS_PATS__TYPE_WASTE)) {

        /* If we're not putting the card back where we found it,
        it's not an inverse. */

        if (m->srccard != mp->destcard) {
            return FALSE;
        }

        /* If any of the intervening moves either moves the card
        that was uncovered or uses it as a destination (including
        NONE), there is a dependency. */

        if (cardmoved(mp->destcard, prev, j) ||
            cardisdest(mp->destcard, prev, j)) {
            return FALSE;
        }

        return TRUE;
    }

    /* These are not inverse prunes, we're taking a shortcut. */

    /* soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks, ..., soft_thread->current_pos.stacks -> soft_thread->current_pos.freecells */

    if (m->fromtype == FCS_PATS__TYPE_WASTE && m->totype == FCS_PATS__TYPE_WASTE &&
        mp->fromtype == FCS_PATS__TYPE_WASTE && mp->totype == FCS_PATS__TYPE_FREECELL) {

        /* If we could have moved the card to soft_thread->current_pos.freecells on the
        first move, prune.  There are other cases, but they
        are more complicated. */

        if (pos->ntemp != LOCAL_FREECELLS_NUM && !zerot) {
            return TRUE;
        }

        return FALSE;
    }

    /* soft_thread->current_pos.freecells -> soft_thread->current_pos.stacks, ..., soft_thread->current_pos.stacks -> soft_thread->current_pos.stacks */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL && m->totype == FCS_PATS__TYPE_WASTE &&
        mp->fromtype == FCS_PATS__TYPE_WASTE && mp->totype == FCS_PATS__TYPE_WASTE) {

        /* We can prune these moves as long as the intervening
        moves don't touch mp->destcard. */

        if (cardmoved(mp->destcard, prev, j) ||
            cardisdest(mp->destcard, prev, j)) {
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}

/* Move prioritization.  Given legal, pruned moves, there are still some
that are a waste of time, especially in the endgame where there are lots of
possible moves, but few productive ones.  Note that we also prioritize
positions when they are added to the queue. */

#define NNEED 8

static void prioritize(fc_solve_soft_thread_t * soft_thread, fcs_pats__move_t *mp0, int n)
{
    DECLARE_STACKS();
    int i, j, s, w, pile[NNEED], npile;
    card_t card, need[4];
    fcs_pats__move_t *mp;

    /* There are 4 cards that we "need": the next cards to go out.  We
    give higher priority to the moves that remove cards from the piles
    containing these cards. */

    for (i = 0; i < NNEED; i++) {
        pile[i] = -1;
    }
    npile = 0;

    for (s = 0; s < 4; s++) {
        need[s] = NONE;
        if (soft_thread->current_pos.foundations[s] == NONE) {
            need[s] = fcs_pats_make_card(PS_ACE, fc_solve_pats__output_suits[s]);
        } else if (soft_thread->current_pos.foundations[s] != PS_KING) {
            need[s] = fcs_pats_make_card(soft_thread->current_pos.foundations[s] + 1, fc_solve_pats__output_suits[s]);
        }
    }

    /* Locate the needed cards.  There's room for optimization here,
    like maybe an array that keeps track of every card; if maintaining
    such an array is not too expensive. */

    for (w = 0; w < LOCAL_STACKS_NUM; w++) {
        j = soft_thread->current_pos.columns_lens[w];
        for (i = 0; i < j; i++) {
            card = soft_thread->current_pos.stacks[w][i];
            s = fcs_pats_card_suit(card);

            /* Save the locations of the piles containing
            not only the card we need next, but the card
            after that as well. */

            if (need[s] != NONE &&
                (card == need[s] || card == fcs_pats_next_card(need[s]))) {
                pile[npile++] = w;
                if (npile == NNEED) {
                    break;
                }
            }
        }
        if (npile == NNEED) {
            break;
        }
    }

    /* Now if any of the moves remove a card from any of the piles
    listed in pile[], bump their priority.  Likewise, if a move
    covers a card we need, decrease its priority.  These priority
    increments and decrements were determined empirically. */

    for (i = 0, mp = mp0; i < n; i++, mp++) {
        if (mp->card != NONE) {
            if (mp->fromtype == FCS_PATS__TYPE_WASTE) {
                w = mp->from;
                for (j = 0; j < npile; j++) {
                    if (w == pile[j]) {
                        mp->pri += soft_thread->pats_solve_params.x[0];
                    }
                }
                if (soft_thread->current_pos.columns_lens[w] > 1) {
                    card = soft_thread->current_pos.stacks[w][soft_thread->current_pos.columns_lens[w] - 2];
                    for (s = 0; s < 4; s++) {
                        if (card == need[s]) {
                            mp->pri += soft_thread->pats_solve_params.x[1];
                            break;
                        }
                    }
                }
            }
            if (mp->totype == FCS_PATS__TYPE_WASTE) {
                for (j = 0; j < npile; j++) {
                    if (mp->to == pile[j]) {
                        mp->pri -= soft_thread->pats_solve_params.x[2];
                    }
                }
            }
        }
    }
}

/* Generate an array of the moves we can make from this position. */

fcs_pats__move_t *fc_solve_pats__get_moves(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos, int *nmoves)
{
    int i, n, alln, o, a, numout;
    fcs_pats__move_t *mp, *mp0;

    /* Fill in the soft_thread->possible_moves array. */

    alln = n = get_possible_moves(soft_thread, &a, &numout);

    if (!a) {

        /* Throw out some obviously bad (non-auto)moves. */

        for (i = 0, mp = soft_thread->possible_moves; i < alln; i++, mp++) {

            /* Special prune for Seahaven -k. */

            if (prune_seahaven(soft_thread, mp)) {
                mp->card = NONE;
                n--;
                continue;
            }

            /* Prune redundant moves. */

            if (prune_redundant(soft_thread, mp, pos)) {
                mp->card = NONE;
                n--;
            }
        }

        /* Mark any irreversible moves. */

        mark_irreversible(soft_thread, n);
    }

    /* No moves?  Maybe we won. */

    if (n == 0) {
        for (o = 0; o < 4; o++) {
            if (soft_thread->current_pos.foundations[o] != PS_KING) {
                break;
            }
        }

        if (o == 4) {

            /* Report the win. */

            win(soft_thread, pos);

            if (soft_thread->Noexit) {
                soft_thread->num_solutions++;
                return NULL;
            }
            soft_thread->status = WIN;

            return NULL;
        }

        /* We lost. */

        return NULL;
    }

    /* Prioritize these moves.  Automoves don't get queued, so they
    don't need a priority. */

    if (!a) {
        prioritize(soft_thread, soft_thread->possible_moves, alln);
    }

    /* Now copy to safe storage and return.  Non-auto moves out get put
    at the end.  Queueing them isn't a good idea because they are still
    good moves, even if they didn't pass the automove test.  So we still
    do the recursive solve() on them, but only after queueing the other
    moves. */

    mp = mp0 = fc_solve_pats__new_array(soft_thread, fcs_pats__move_t, n);
    if (mp == NULL) {
        return NULL;
    }
    *nmoves = n;
    i = 0;
    if (a || numout == 0) {
        for (i = 0; i < alln; i++) {
            if (soft_thread->possible_moves[i].card != NONE) {
                *mp = soft_thread->possible_moves[i];      /* struct copy */
                mp++;
            }
        }
    } else {
        for (i = numout; i < alln; i++) {
            if (soft_thread->possible_moves[i].card != NONE) {
                *mp = soft_thread->possible_moves[i];      /* struct copy */
                mp++;
            }
        }
        for (i = 0; i < numout; i++) {
            if (soft_thread->possible_moves[i].card != NONE) {
                *mp = soft_thread->possible_moves[i];      /* struct copy */
                mp++;
            }
        }
    }

    return mp0;
}

/* Automove logic.  Freecell games must avoid certain types of automoves. */

static GCC_INLINE int good_automove(fc_solve_soft_thread_t * soft_thread, int o, int r)
{
    const fc_solve_instance_t * const instance = soft_thread->instance;
    int i;

    if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) || r <= 2) {
        return TRUE;
    }

    /* Check the Out piles of opposite color. */

    for (i = 1 - (o & 1); i < 4; i += 2) {
        if (soft_thread->current_pos.foundations[i] < r - 1) {

#if 1   /* Raymond's Rule */
            /* Not all the N-1's of opposite color are out
            yet.  We can still make an automove if either
            both N-2's are out or the other same color N-3
            is out (Raymond's rule).  Note the re-use of
            the loop variable i.  We return here and never
            make it back to the outer loop. */

            for (i = 1 - (o & 1); i < 4; i += 2) {
                if (soft_thread->current_pos.foundations[i] < r - 2) {
                    return FALSE;
                }
            }
            if (soft_thread->current_pos.foundations[(o + 2) & 3] < r - 3) {
                return FALSE;
            }

            return TRUE;
#else   /* Horne's Rule */
            return FALSE;
#endif
        }
    }

    return TRUE;
}

/* Get the possible moves from a position, and store them in soft_thread->possible_moves[]. */

static GCC_INLINE int get_possible_moves(fc_solve_soft_thread_t * soft_thread, int *a, int *numout)
{
    const fc_solve_instance_t * const instance = soft_thread->instance;
    DECLARE_STACKS();
    int i, n, t, w, o, empty, emptyw;
    card_t card;
    fcs_pats__move_t *mp;

    /* Check for moves from soft_thread->current_pos.stacks to soft_thread->current_pos.foundations. */

    n = 0;
    mp = soft_thread->possible_moves;
    for (w = 0; w < LOCAL_STACKS_NUM; w++) {
        if (soft_thread->current_pos.columns_lens[w] > 0) {
            card = *soft_thread->current_pos.stack_ptrs[w];
            o = fcs_pats_card_suit(card);
            const card_t found_o = soft_thread->current_pos.foundations[o];
            empty = (found_o == NONE);
            if (fcs_pats_card_rank(card) == found_o + 1)
            {
                mp->card = card;
                mp->from = w;
                mp->fromtype = FCS_PATS__TYPE_WASTE;
                mp->to = o;
                mp->totype = FCS_PATS__TYPE_FOUNDATION;
                mp->srccard = NONE;
                if (soft_thread->current_pos.columns_lens[w] > 1) {
                    mp->srccard = soft_thread->current_pos.stack_ptrs[w][-1];
                }
                mp->destcard = NONE;
                mp->pri = 0;    /* unused */
                n++;
                mp++;

                /* If it's an automove, just do it. */

                if (good_automove(soft_thread, o, fcs_pats_card_rank(card))) {
                    *a = TRUE;
                    if (n != 1) {
                        soft_thread->possible_moves[0] = mp[-1];
                        return 1;
                    }
                    return n;
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to soft_thread->current_pos.foundations. */

    for (t = 0; t < LOCAL_FREECELLS_NUM; t++) {
        if (soft_thread->current_pos.freecells[t] != NONE) {
            card = soft_thread->current_pos.freecells[t];
            o = fcs_pats_card_suit(card);
            empty = (soft_thread->current_pos.foundations[o] == NONE);
            if ((empty && (fcs_pats_card_rank(card) == PS_ACE)) ||
                (!empty && (fcs_pats_card_rank(card) == soft_thread->current_pos.foundations[o] + 1))) {
                mp->card = card;
                mp->from = t;
                mp->fromtype = FCS_PATS__TYPE_FREECELL;
                mp->to = o;
                mp->totype = FCS_PATS__TYPE_FOUNDATION;
                mp->srccard = NONE;
                mp->destcard = NONE;
                mp->pri = 0;    /* unused */
                n++;
                mp++;

                /* If it's an automove, just do it. */

                if (good_automove(soft_thread, o, fcs_pats_card_rank(card))) {
                    *a = TRUE;
                    if (n != 1) {
                        soft_thread->possible_moves[0] = mp[-1];
                        return 1;
                    }
                    return n;
                }
            }
        }
    }

    /* No more automoves, but remember if there were any moves out. */

    *a = FALSE;
    *numout = n;

    /* Check for moves from non-singleton soft_thread->current_pos.stacks cells to one of any
    empty soft_thread->current_pos.stacks cells. */

    const fcs_bool_t not_King_only = (! ((INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY)));

    emptyw = -1;
    for (w = 0; w < LOCAL_STACKS_NUM; w++) {
        if (soft_thread->current_pos.columns_lens[w] == 0) {
            emptyw = w;
            break;
        }
    }
    if (emptyw >= 0) {
        for (i = 0; i < LOCAL_STACKS_NUM; i++) {
            if (i == emptyw) {
                continue;
            }
            if (soft_thread->current_pos.columns_lens[i] > 1 &&
                fcs_pats_is_king_only(not_King_only, *(soft_thread->current_pos.stack_ptrs[i]))) {
                card = *soft_thread->current_pos.stack_ptrs[i];
                mp->card = card;
                mp->from = i;
                mp->fromtype = FCS_PATS__TYPE_WASTE;
                mp->to = emptyw;
                mp->totype = FCS_PATS__TYPE_WASTE;
                mp->srccard = soft_thread->current_pos.stack_ptrs[i][-1];
                mp->destcard = NONE;
                mp->pri = soft_thread->pats_solve_params.x[3];
                n++;
                mp++;
            }
        }
    }

    const card_t game_variant_suit_mask = instance->game_variant_suit_mask;
    const card_t game_variant_desired_suit_value = instance->game_variant_desired_suit_value;
    /* Check for moves from soft_thread->current_pos.stacks to non-empty soft_thread->current_pos.stacks cells. */

    for (i = 0; i < LOCAL_STACKS_NUM; i++) {
        if (soft_thread->current_pos.columns_lens[i] > 0) {
            card = *soft_thread->current_pos.stack_ptrs[i];
            for (w = 0; w < LOCAL_STACKS_NUM; w++) {
                if (i == w) {
                    continue;
                }
                if (soft_thread->current_pos.columns_lens[w] > 0 &&
                    (fcs_pats_card_rank(card) == fcs_pats_card_rank(*soft_thread->current_pos.stack_ptrs[w]) - 1 &&
                     fcs_pats_is_suitable(card, *(soft_thread->current_pos.stack_ptrs[w]), game_variant_suit_mask, game_variant_desired_suit_value))) {
                    mp->card = card;
                    mp->from = i;
                    mp->fromtype = FCS_PATS__TYPE_WASTE;
                    mp->to = w;
                    mp->totype = FCS_PATS__TYPE_WASTE;
                    mp->srccard = NONE;
                    if (soft_thread->current_pos.columns_lens[i] > 1) {
                        mp->srccard = soft_thread->current_pos.stack_ptrs[i][-1];
                    }
                    mp->destcard = *soft_thread->current_pos.stack_ptrs[w];
                    mp->pri = soft_thread->pats_solve_params.x[4];
                    n++;
                    mp++;
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to non-empty soft_thread->current_pos.stacks cells. */

    for (t = 0; t < LOCAL_FREECELLS_NUM; t++) {
        card = soft_thread->current_pos.freecells[t];
        if (card != NONE) {
            for (w = 0; w < LOCAL_STACKS_NUM; w++) {
                if (soft_thread->current_pos.columns_lens[w] > 0 &&
                    (fcs_pats_card_rank(card) == fcs_pats_card_rank(*soft_thread->current_pos.stack_ptrs[w]) - 1 &&
                     fcs_pats_is_suitable(card, *(soft_thread->current_pos.stack_ptrs[w]), game_variant_suit_mask, game_variant_desired_suit_value))) {
                    mp->card = card;
                    mp->from = t;
                    mp->fromtype = FCS_PATS__TYPE_FREECELL;
                    mp->to = w;
                    mp->totype = FCS_PATS__TYPE_WASTE;
                    mp->srccard = NONE;
                    mp->destcard = *soft_thread->current_pos.stack_ptrs[w];
                    mp->pri = soft_thread->pats_solve_params.x[5];
                    n++;
                    mp++;
                }
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.freecells to one of any empty soft_thread->current_pos.stacks cells. */

    if (emptyw >= 0) {
        for (t = 0; t < LOCAL_FREECELLS_NUM; t++) {
            card = soft_thread->current_pos.freecells[t];
            if (card != NONE && fcs_pats_is_king_only(not_King_only, card)) {
                mp->card = card;
                mp->from = t;
                mp->fromtype = FCS_PATS__TYPE_FREECELL;
                mp->to = emptyw;
                mp->totype = FCS_PATS__TYPE_WASTE;
                mp->srccard = NONE;
                mp->destcard = NONE;
                mp->pri = soft_thread->pats_solve_params.x[6];
                n++;
                mp++;
            }
        }
    }

    /* Check for moves from soft_thread->current_pos.stacks to one of any empty soft_thread->current_pos.freecells cells. */

    for (t = 0; t < LOCAL_FREECELLS_NUM; t++) {
        if (soft_thread->current_pos.freecells[t] == NONE) {
            break;
        }
    }
    if (t < LOCAL_FREECELLS_NUM) {
        for (w = 0; w < LOCAL_STACKS_NUM; w++) {
            if (soft_thread->current_pos.columns_lens[w] > 0) {
                card = *soft_thread->current_pos.stack_ptrs[w];
                mp->card = card;
                mp->from = w;
                mp->fromtype = FCS_PATS__TYPE_WASTE;
                mp->to = t;
                mp->totype = FCS_PATS__TYPE_FREECELL;
                mp->srccard = NONE;
                if (soft_thread->current_pos.columns_lens[w] > 1) {
                    mp->srccard = soft_thread->current_pos.stack_ptrs[w][-1];
                }
                mp->destcard = NONE;
                mp->pri = soft_thread->pats_solve_params.x[7];
                n++;
                mp++;
            }
        }
    }

    return n;
}

/* Moves that can't be undone get slightly higher priority, since it means
we are moving a card for the first time. */

static GCC_INLINE fcs_bool_t is_irreversible_move(
    const card_t game_variant_suit_mask,
    const card_t game_variant_desired_suit_value,
    const fcs_bool_t King_only,
    const fcs_pats__move_t * const mp
)
{
    if (mp->totype == FCS_PATS__TYPE_FOUNDATION)
    {
        return TRUE;
    }
    else if (mp->fromtype == FCS_PATS__TYPE_WASTE)
    {
        const card_t srccard = mp->srccard;
        if (srccard != NONE)
        {
            const card_t card = mp->card;
            if (
                ( fcs_pats_card_rank(card) !=
                  fcs_pats_card_rank(srccard) - 1
                )
                ||
                !fcs_pats_is_suitable(card, srccard,
                    game_variant_suit_mask,
                    game_variant_desired_suit_value
                )
            ) {
                return TRUE;
            }
        }
        /* TODO : This is probably a bug because mp->card probably cannot be
         * PS_KING - only PS_KING bitwise-ORed with some other value.
         * */
        else if (King_only && mp->card != fcs_pats_make_card(PS_KING, 0))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void mark_irreversible(fc_solve_soft_thread_t * const soft_thread, int n)
{
    const fc_solve_instance_t * const instance = soft_thread->instance;
    int i;
    fcs_pats__move_t *mp;

    const card_t game_variant_suit_mask = instance->game_variant_suit_mask;
    const card_t game_variant_desired_suit_value = instance->game_variant_desired_suit_value;
    const fcs_bool_t King_only = (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY);
    const typeof(soft_thread->pats_solve_params.x[8]) x_param_8 = soft_thread->pats_solve_params.x[8];

    for (i = 0, mp = soft_thread->possible_moves; i < n; i++, mp++) {
        if (is_irreversible_move(
                game_variant_suit_mask,
                game_variant_desired_suit_value,
                King_only,
                mp))
        {
            mp->pri += x_param_8;
        }
    }
}

/* Comparison function for sorting the soft_thread->current_pos.stacks piles. */

static GCC_INLINE int wcmp(fc_solve_soft_thread_t * soft_thread, int a, int b)
{
    if (soft_thread->pats_solve_params.x[9] < 0) {
        return soft_thread->current_pos.stack_ids[b] - soft_thread->current_pos.stack_ids[a];       /* newer piles first */
    } else {
        return soft_thread->current_pos.stack_ids[a] - soft_thread->current_pos.stack_ids[b];       /* older piles first */
    }
}

#if 0
static GCC_INLINE int wcmp(int a, int b)
{
    if (soft_thread->pats_solve_params.x[9] < 0) {
        return soft_thread->current_pos.columns_lens[b] - soft_thread->current_pos.columns_lens[a];       /* longer piles first */
    } else {
        return soft_thread->current_pos.columns_lens[a] - soft_thread->current_pos.columns_lens[b];       /* shorter piles first */
    }
}
#endif

/* Sort the piles, to remove the physical differences between logically
equivalent layouts.  Assume it's already mostly sorted.  */

void fc_solve_pats__sort_piles(fc_solve_soft_thread_t * soft_thread)
{
    DECLARE_STACKS();
    int w, i, j;

    /* Make sure all the piles have id numbers. */

    for (w = 0; w < LOCAL_STACKS_NUM; w++) {
        if (soft_thread->current_pos.stack_ids[w] < 0) {
            soft_thread->current_pos.stack_ids[w] = get_pilenum(soft_thread, w);
            if (soft_thread->current_pos.stack_ids[w] < 0) {
                return;
            }
        }
    }

    /* Sort them. */

    /* TODO : Add a temp pointer to column_idxs */

    soft_thread->current_pos.column_idxs[0] = 0;
    w = 0;
    for (i = 1; i < LOCAL_STACKS_NUM; i++) {
        if (wcmp(soft_thread, soft_thread->current_pos.column_idxs[w], i) < 0) {
            w++;
            soft_thread->current_pos.column_idxs[w] = i;
        } else {
            for (j = w; j >= 0; --j) {
                soft_thread->current_pos.column_idxs[j + 1] = soft_thread->current_pos.column_idxs[j];
                if (j == 0 || wcmp(soft_thread, soft_thread->current_pos.column_idxs[j - 1], i) < 0) {
                    soft_thread->current_pos.column_idxs[j] = i;
                    break;
                }
            }
            w++;
        }
    }

#if 0
    /* Compute the inverse. */

    for (i = 0; i < LOCAL_STACKS_NUM; i++) {
        soft_thread->current_pos.column_inv_idxs[soft_thread->current_pos.column_idxs[i]] = i;
    }
#endif
}

/* Win.  Print out the move stack. */

static void win(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos)
{
    int i, nmoves;
    FILE *out;
    fcs_pats_position_t *p;
    fcs_pats__move_t *mp, **mpp, **mpp0;

    /* Go back up the chain of parents and store the moves
    in reverse order. */

    i = 0;
    for (p = pos; p->parent; p = p->parent) {
        i++;
    }
    nmoves = i;
    mpp0 = fc_solve_pats__new_array(soft_thread, fcs_pats__move_t *, nmoves);
    if (mpp0 == NULL) {
        return; /* how sad, so close... */
    }
    mpp = mpp0 + nmoves - 1;
    for (p = pos; p->parent; p = p->parent) {
        *mpp-- = &p->move;
    }

    /* Now print them out in the correct order. */

    out = fopen("win", "w");

    if (! out)
    {
        fprintf(stderr, "%s\n", "Cannot open 'win' for writing.");
        exit(1);
    }
    for (i = 0, mpp = mpp0; i < nmoves; i++, mpp++) {
        mp = *mpp;
        fc_solve_pats__print_card(mp->card, out);
        if (mp->totype == FCS_PATS__TYPE_FREECELL) {
            fprintf(out, "to temp\n");
        } else if (mp->totype == FCS_PATS__TYPE_FOUNDATION) {
            fprintf(out, "out\n");
        } else {
            fprintf(out, "to ");
            if (mp->destcard == NONE) {
                fprintf(out, "empty pile");
            } else {
                fc_solve_pats__print_card(mp->destcard, out);
            }
            fputc('\n', out);
        }
    }
    fclose(out);
    fc_solve_pats__free_array(soft_thread, mpp0, fcs_pats__move_t *, nmoves);

    if (!soft_thread->is_quiet) {
        printf("A winner.\n");
        printf("%d moves.\n", nmoves);
#ifdef DEBUG
        printf("%d positions generated.\n", soft_thread->num_states_in_collection);
        printf("%d unique positions.\n", soft_thread->num_checked_states);
        printf("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
    }
}

/* For each pile, return a unique identifier.  Although there are a
large number of possible piles, generally fewer than 1000 different
piles appear in any given game.  We'll use the pile's hash to find
a hash bucket that contains a short list of piles, along with their
identifiers. */

static GCC_INLINE int get_pilenum(fc_solve_soft_thread_t * soft_thread, int w)
{
    int pilenum;
    fcs_pats__bucket_list_t *l, *last;

    /* For a given pile, get its unique pile id.  If it doesn't have
    one, add it to the appropriate list and give it one.  First, get
    the hash bucket. */

    const int bucket = soft_thread->current_pos.stack_hashes[w] % FC_SOLVE_BUCKETLIST_NBUCKETS;

    /* Look for the pile in this bucket. */

    last = NULL;
    for (l = soft_thread->buckets_list[bucket]; l; l = l->next) {
        if (l->hash == soft_thread->current_pos.stack_hashes[w] &&
            strncmp((const char *)l->pile, (const char *)soft_thread->current_pos.stacks[w], soft_thread->current_pos.columns_lens[w]) == 0) {
            break;
        }
        last = l;
    }

    /* If we didn't find it, make a new one and add it to the list. */

    if (l == NULL) {
        if (soft_thread->next_pile_idx == FC_SOLVE__MAX_NUM_PILES) {
            fc_solve_msg("Ran out of pile numbers!");
            return -1;
        }
        l = fc_solve_pats__new(soft_thread, fcs_pats__bucket_list_t);
        if (l == NULL) {
            return -1;
        }
        l->pile = fc_solve_pats__new_array(soft_thread, u_char, soft_thread->current_pos.columns_lens[w] + 1);
        if (l->pile == NULL) {
            fc_solve_pats__free_ptr(soft_thread, l, fcs_pats__bucket_list_t);
            return -1;
        }

        /* Store the new pile along with its hash.  Maintain
        a reverse mapping so we can unpack the piles swiftly. */

        strncpy((char*)l->pile, (const char *)soft_thread->current_pos.stacks[w], soft_thread->current_pos.columns_lens[w] + 1);
        l->hash = soft_thread->current_pos.stack_hashes[w];
        l->pilenum = pilenum = soft_thread->next_pile_idx++;
        l->next = NULL;
        if (last == NULL) {
            soft_thread->buckets_list[bucket] = l;
        } else {
            last->next = l;
        }
        soft_thread->bucket_from_pile_lookup[pilenum] = l;
    }

    return l->pilenum;
}

