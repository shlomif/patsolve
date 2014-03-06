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

const char Rank[] = " A23456789TJQK";
const char Suit[] = "DCHS";

const card_t Osuit[4] = { PS_DIAMOND, PS_CLUB, PS_HEART, PS_SPADE };

static int get_possible_moves(fc_solve_soft_thread_t * soft_thread, int *, int *);
static void mark_irreversible(fc_solve_soft_thread_t * soft_thread, int n);
static void win(fc_solve_soft_thread_t * soft_thread, POSITION *pos);
static GCC_INLINE int get_pilenum(fc_solve_soft_thread_t * soft_thread, int w);

/* Hash a pile. */

static GCC_INLINE void hashpile(fc_solve_soft_thread_t * soft_thread, int w)
{
    soft_thread->W[w][soft_thread->Wlen[w]] = 0;
    soft_thread->Whash[w] = fnv_hash_str(soft_thread->W[w]);

    /* Invalidate this pile's id.  We'll calculate it later. */

    soft_thread->Wpilenum[w] = -1;
}

/* Hash the whole layout.  This is called once, at the start. */

void hash_layout(fc_solve_soft_thread_t * soft_thread)
{
    int w;

    for (w = 0; w < soft_thread->Nwpiles; w++) {
        hashpile(soft_thread, w);
    }
}

/* These two routines make and unmake moves. */

void freecell_solver_pats__make_move(fc_solve_soft_thread_t * soft_thread, MOVE *m)
{
    int from, to;
    card_t card;

    from = m->from;
    to = m->to;

    /* Remove from pile. */

    if (m->fromtype == T_TYPE) {
        card = soft_thread->T[from];
        soft_thread->T[from] = NONE;
    } else {
        card = *soft_thread->Wp[from]--;
        soft_thread->Wlen[from]--;
        hashpile(soft_thread, from);
    }

    /* Add to pile. */

    if (m->totype == T_TYPE) {
        soft_thread->T[to] = card;
    } else if (m->totype == W_TYPE) {
        *++soft_thread->Wp[to] = card;
        soft_thread->Wlen[to]++;
        hashpile(soft_thread, to);
    } else {
        soft_thread->O[to]++;
    }
}

void undo_move(fc_solve_soft_thread_t * soft_thread, MOVE *m)
{
    int from, to;
    card_t card;

    from = m->from;
    to = m->to;

    /* Remove from 'to' pile. */

    if (m->totype == T_TYPE) {
        card = soft_thread->T[to];
        soft_thread->T[to] = NONE;
    } else if (m->totype == W_TYPE) {
        card = *soft_thread->Wp[to]--;
        soft_thread->Wlen[to]--;
        hashpile(soft_thread, to);
    } else {
        card = soft_thread->O[to] + Osuit[to];
        soft_thread->O[to]--;
    }

    /* Add to 'from' pile. */

    if (m->fromtype == T_TYPE) {
        soft_thread->T[from] = card;
    } else {
        *++soft_thread->Wp[from] = card;
        soft_thread->Wlen[from]++;
        hashpile(soft_thread, from);
    }
}

/* This prune applies only to Seahaven in -k mode: if we're putting a card
onto a soft_thread->W pile, and if that pile already has soft_thread->Ntpiles+1 cards of this suit in
a row, and if there is a smaller card of the same suit below the run, then
the position is unsolvable.  This cuts out a lot of useless searching, so
it's worth checking.  */

static int prune_seahaven(fc_solve_soft_thread_t * soft_thread, MOVE *mp)
{
    int i, j, w, r, s;

    if (!soft_thread->Same_suit || !soft_thread->King_only || mp->totype != W_TYPE) {
        return FALSE;
    }
    w = mp->to;

    /* Count the number of cards below this card. */

    j = 0;
    r = fcs_pats_card_rank(mp->card) + 1;
    s = fcs_pats_card_suit(mp->card);
    for (i = soft_thread->Wlen[w] - 1; i >= 0; i--) {
        if (fcs_pats_card_suit(soft_thread->W[w][i]) == s && fcs_pats_card_rank(soft_thread->W[w][i]) == r + j) {
            j++;
        }
    }
    if (j < soft_thread->Ntpiles + 1) {
        return FALSE;
    }

    /* If there's a smaller card of this suit in the pile, we can prune
    the move. */

    j = soft_thread->Wlen[w];
    r -= 1;
    for (i = 0; i < j; i++) {
        if ( (fcs_pats_card_suit(soft_thread->W[w][i]) == s)
            && (fcs_pats_card_rank(soft_thread->W[w][i]) < r) ) {
            return TRUE;
        }
    }

    return FALSE;
}

/* This utility routine is used to check if a card is ever moved in
a sequence of moves. */

static GCC_INLINE int cardmoved(card_t card, MOVE **mpp, int j)
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

static GCC_INLINE int cardisdest(card_t card, MOVE **mpp, int j)
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

static int prune_redundant(fc_solve_soft_thread_t * soft_thread, MOVE *mp, POSITION *pos0)
{
    int i, j;
    int zerot;
    MOVE *m, *prev[MAXPREVMOVE];
    POSITION *pos;

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

    /* If the number of empty soft_thread->T cells ever goes to zero, from prev[0] to
    prev[j-1], there may be a dependency.  We also want to know if there
    were any empty soft_thread->T cells on move prev[j]. */

    zerot = 0;
    pos = pos0;
    for (i = 0; i < j; i++) {
        zerot |= (pos->ntemp == soft_thread->Ntpiles);
        pos = pos->parent;
    }

    /* Now, prev[j] (m) is a move involving the same card as the current
    move.  See if the current move inverts that move.  There are several
    cases. */

    /* soft_thread->T -> soft_thread->W, ..., soft_thread->W -> soft_thread->T */

    if (m->fromtype == T_TYPE && m->totype == W_TYPE &&
        mp->fromtype == W_TYPE && mp->totype == T_TYPE) {

        /* If the number of soft_thread->T cells goes to zero, we have a soft_thread->T
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

    /* soft_thread->W -> soft_thread->T, ..., soft_thread->T -> soft_thread->W */
    /* soft_thread->W -> soft_thread->W, ..., soft_thread->W -> soft_thread->W */

    if ((m->fromtype == W_TYPE && m->totype == T_TYPE &&
         mp->fromtype == T_TYPE && mp->totype == W_TYPE) ||
        (m->fromtype == W_TYPE && m->totype == W_TYPE &&
         mp->fromtype == W_TYPE && mp->totype == W_TYPE)) {

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

    /* soft_thread->W -> soft_thread->W, ..., soft_thread->W -> soft_thread->T */

    if (m->fromtype == W_TYPE && m->totype == W_TYPE &&
        mp->fromtype == W_TYPE && mp->totype == T_TYPE) {

        /* If we could have moved the card to soft_thread->T on the
        first move, prune.  There are other cases, but they
        are more complicated. */

        if (pos->ntemp != soft_thread->Ntpiles && !zerot) {
            return TRUE;
        }

        return FALSE;
    }

    /* soft_thread->T -> soft_thread->W, ..., soft_thread->W -> soft_thread->W */

    if (m->fromtype == T_TYPE && m->totype == W_TYPE &&
        mp->fromtype == W_TYPE && mp->totype == W_TYPE) {

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

static void prioritize(fc_solve_soft_thread_t * soft_thread, MOVE *mp0, int n)
{
    int i, j, s, w, pile[NNEED], npile;
    card_t card, need[4];
    MOVE *mp;

    /* There are 4 cards that we "need": the next cards to go out.  We
    give higher priority to the moves that remove cards from the piles
    containing these cards. */

    for (i = 0; i < NNEED; i++) {
        pile[i] = -1;
    }
    npile = 0;

    for (s = 0; s < 4; s++) {
        need[s] = NONE;
        if (soft_thread->O[s] == NONE) {
            need[s] = Osuit[s] + PS_ACE;
        } else if (soft_thread->O[s] != PS_KING) {
            need[s] = Osuit[s] + soft_thread->O[s] + 1;
        }
    }

    /* Locate the needed cards.  There's room for optimization here,
    like maybe an array that keeps track of every card; if maintaining
    such an array is not too expensive. */

    for (w = 0; w < soft_thread->Nwpiles; w++) {
        j = soft_thread->Wlen[w];
        for (i = 0; i < j; i++) {
            card = soft_thread->W[w][i];
            s = fcs_pats_card_suit(card);

            /* Save the locations of the piles containing
            not only the card we need next, but the card
            after that as well. */

            if (need[s] != NONE &&
                (card == need[s] || card == need[s] + 1)) {
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
            if (mp->fromtype == W_TYPE) {
                w = mp->from;
                for (j = 0; j < npile; j++) {
                    if (w == pile[j]) {
                        mp->pri += soft_thread->Xparam[0];
                    }
                }
                if (soft_thread->Wlen[w] > 1) {
                    card = soft_thread->W[w][soft_thread->Wlen[w] - 2];
                    for (s = 0; s < 4; s++) {
                        if (card == need[s]) {
                            mp->pri += soft_thread->Xparam[1];
                            break;
                        }
                    }
                }
            }
            if (mp->totype == W_TYPE) {
                for (j = 0; j < npile; j++) {
                    if (mp->to == pile[j]) {
                        mp->pri -= soft_thread->Xparam[2];
                    }
                }
            }
        }
    }
}

/* Generate an array of the moves we can make from this position. */

MOVE *get_moves(fc_solve_soft_thread_t * soft_thread, POSITION *pos, int *nmoves)
{
    int i, n, alln, o, a, numout;
    MOVE *mp, *mp0;

    /* Fill in the soft_thread->Possible array. */

    alln = n = get_possible_moves(soft_thread, &a, &numout);

    if (!a) {

        /* Throw out some obviously bad (non-auto)moves. */

        for (i = 0, mp = soft_thread->Possible; i < alln; i++, mp++) {

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
            if (soft_thread->O[o] != PS_KING) {
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
            soft_thread->Status = WIN;

            if (soft_thread->Interactive) {
                exit(0);
            }

            return NULL;
        }

        /* We lost. */

        return NULL;
    }

    /* Prioritize these moves.  Automoves don't get queued, so they
    don't need a priority. */

    if (!a) {
        prioritize(soft_thread, soft_thread->Possible, alln);
    }

    /* Now copy to safe storage and return.  Non-auto moves out get put
    at the end.  Queueing them isn't a good idea because they are still
    good moves, even if they didn't pass the automove test.  So we still
    do the recursive solve() on them, but only after queueing the other
    moves. */

    mp = mp0 = new_array(soft_thread, MOVE, n);
    if (mp == NULL) {
        return NULL;
    }
    *nmoves = n;
    i = 0;
    if (a || numout == 0) {
        for (i = 0; i < alln; i++) {
            if (soft_thread->Possible[i].card != NONE) {
                *mp = soft_thread->Possible[i];      /* struct copy */
                mp++;
            }
        }
    } else {
        for (i = numout; i < alln; i++) {
            if (soft_thread->Possible[i].card != NONE) {
                *mp = soft_thread->Possible[i];      /* struct copy */
                mp++;
            }
        }
        for (i = 0; i < numout; i++) {
            if (soft_thread->Possible[i].card != NONE) {
                *mp = soft_thread->Possible[i];      /* struct copy */
                mp++;
            }
        }
    }

    return mp0;
}

/* Automove logic.  Freecell games must avoid certain types of automoves. */

static GCC_INLINE int good_automove(fc_solve_soft_thread_t * soft_thread, int o, int r)
{
    int i;

    if (soft_thread->Same_suit || r <= 2) {
        return TRUE;
    }

    /* Check the Out piles of opposite color. */

    for (i = 1 - (o & 1); i < 4; i += 2) {
        if (soft_thread->O[i] < r - 1) {

#if 1   /* Raymond's Rule */
            /* Not all the N-1's of opposite color are out
            yet.  We can still make an automove if either
            both N-2's are out or the other same color N-3
            is out (Raymond's rule).  Note the re-use of
            the loop variable i.  We return here and never
            make it back to the outer loop. */

            for (i = 1 - (o & 1); i < 4; i += 2) {
                if (soft_thread->O[i] < r - 2) {
                    return FALSE;
                }
            }
            if (soft_thread->O[(o + 2) & 3] < r - 3) {
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

/* Get the possible moves from a position, and store them in soft_thread->Possible[]. */

static int get_possible_moves(fc_solve_soft_thread_t * soft_thread, int *a, int *numout)
{
    int i, n, t, w, o, empty, emptyw;
    card_t card;
    MOVE *mp;

    /* Check for moves from soft_thread->W to soft_thread->O. */

    n = 0;
    mp = soft_thread->Possible;
    for (w = 0; w < soft_thread->Nwpiles; w++) {
        if (soft_thread->Wlen[w] > 0) {
            card = *soft_thread->Wp[w];
            o = fcs_pats_card_suit(card);
            empty = (soft_thread->O[o] == NONE);
            if ((empty && (fcs_pats_card_rank(card) == PS_ACE)) ||
                (!empty && (fcs_pats_card_rank(card) == soft_thread->O[o] + 1))) {
                mp->card = card;
                mp->from = w;
                mp->fromtype = W_TYPE;
                mp->to = o;
                mp->totype = O_TYPE;
                mp->srccard = NONE;
                if (soft_thread->Wlen[w] > 1) {
                    mp->srccard = soft_thread->Wp[w][-1];
                }
                mp->destcard = NONE;
                mp->pri = 0;    /* unused */
                n++;
                mp++;

                /* If it's an automove, just do it. */

                if (good_automove(soft_thread, o, fcs_pats_card_rank(card))) {
                    *a = TRUE;
                    if (n != 1) {
                        soft_thread->Possible[0] = mp[-1];
                        return 1;
                    }
                    return n;
                }
            }
        }
    }

    /* Check for moves from soft_thread->T to soft_thread->O. */

    for (t = 0; t < soft_thread->Ntpiles; t++) {
        if (soft_thread->T[t] != NONE) {
            card = soft_thread->T[t];
            o = fcs_pats_card_suit(card);
            empty = (soft_thread->O[o] == NONE);
            if ((empty && (fcs_pats_card_rank(card) == PS_ACE)) ||
                (!empty && (fcs_pats_card_rank(card) == soft_thread->O[o] + 1))) {
                mp->card = card;
                mp->from = t;
                mp->fromtype = T_TYPE;
                mp->to = o;
                mp->totype = O_TYPE;
                mp->srccard = NONE;
                mp->destcard = NONE;
                mp->pri = 0;    /* unused */
                n++;
                mp++;

                /* If it's an automove, just do it. */

                if (good_automove(soft_thread, o, fcs_pats_card_rank(card))) {
                    *a = TRUE;
                    if (n != 1) {
                        soft_thread->Possible[0] = mp[-1];
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

    /* Check for moves from non-singleton soft_thread->W cells to one of any
    empty soft_thread->W cells. */

    emptyw = -1;
    for (w = 0; w < soft_thread->Nwpiles; w++) {
        if (soft_thread->Wlen[w] == 0) {
            emptyw = w;
            break;
        }
    }
    if (emptyw >= 0) {
        for (i = 0; i < soft_thread->Nwpiles; i++) {
            if (i == emptyw) {
                continue;
            }
            if (soft_thread->Wlen[i] > 1 && fcs_pats_is_king_only(*soft_thread->Wp[i])) {
                card = *soft_thread->Wp[i];
                mp->card = card;
                mp->from = i;
                mp->fromtype = W_TYPE;
                mp->to = emptyw;
                mp->totype = W_TYPE;
                mp->srccard = soft_thread->Wp[i][-1];
                mp->destcard = NONE;
                mp->pri = soft_thread->Xparam[3];
                n++;
                mp++;
            }
        }
    }

    /* Check for moves from soft_thread->W to non-empty soft_thread->W cells. */

    for (i = 0; i < soft_thread->Nwpiles; i++) {
        if (soft_thread->Wlen[i] > 0) {
            card = *soft_thread->Wp[i];
            for (w = 0; w < soft_thread->Nwpiles; w++) {
                if (i == w) {
                    continue;
                }
                if (soft_thread->Wlen[w] > 0 &&
                    (fcs_pats_card_rank(card) == fcs_pats_card_rank(*soft_thread->Wp[w]) - 1 &&
                     fcs_pats_is_suitable(card, *soft_thread->Wp[w]))) {
                    mp->card = card;
                    mp->from = i;
                    mp->fromtype = W_TYPE;
                    mp->to = w;
                    mp->totype = W_TYPE;
                    mp->srccard = NONE;
                    if (soft_thread->Wlen[i] > 1) {
                        mp->srccard = soft_thread->Wp[i][-1];
                    }
                    mp->destcard = *soft_thread->Wp[w];
                    mp->pri = soft_thread->Xparam[4];
                    n++;
                    mp++;
                }
            }
        }
    }

    /* Check for moves from soft_thread->T to non-empty soft_thread->W cells. */

    for (t = 0; t < soft_thread->Ntpiles; t++) {
        card = soft_thread->T[t];
        if (card != NONE) {
            for (w = 0; w < soft_thread->Nwpiles; w++) {
                if (soft_thread->Wlen[w] > 0 &&
                    (fcs_pats_card_rank(card) == fcs_pats_card_rank(*soft_thread->Wp[w]) - 1 &&
                     fcs_pats_is_suitable(card, *soft_thread->Wp[w]))) {
                    mp->card = card;
                    mp->from = t;
                    mp->fromtype = T_TYPE;
                    mp->to = w;
                    mp->totype = W_TYPE;
                    mp->srccard = NONE;
                    mp->destcard = *soft_thread->Wp[w];
                    mp->pri = soft_thread->Xparam[5];
                    n++;
                    mp++;
                }
            }
        }
    }

    /* Check for moves from soft_thread->T to one of any empty soft_thread->W cells. */

    if (emptyw >= 0) {
        for (t = 0; t < soft_thread->Ntpiles; t++) {
            card = soft_thread->T[t];
            if (card != NONE && fcs_pats_is_king_only(card)) {
                mp->card = card;
                mp->from = t;
                mp->fromtype = T_TYPE;
                mp->to = emptyw;
                mp->totype = W_TYPE;
                mp->srccard = NONE;
                mp->destcard = NONE;
                mp->pri = soft_thread->Xparam[6];
                n++;
                mp++;
            }
        }
    }

    /* Check for moves from soft_thread->W to one of any empty soft_thread->T cells. */

    for (t = 0; t < soft_thread->Ntpiles; t++) {
        if (soft_thread->T[t] == NONE) {
            break;
        }
    }
    if (t < soft_thread->Ntpiles) {
        for (w = 0; w < soft_thread->Nwpiles; w++) {
            if (soft_thread->Wlen[w] > 0) {
                card = *soft_thread->Wp[w];
                mp->card = card;
                mp->from = w;
                mp->fromtype = W_TYPE;
                mp->to = t;
                mp->totype = T_TYPE;
                mp->srccard = NONE;
                if (soft_thread->Wlen[w] > 1) {
                    mp->srccard = soft_thread->Wp[w][-1];
                }
                mp->destcard = NONE;
                mp->pri = soft_thread->Xparam[7];
                n++;
                mp++;
            }
        }
    }

    return n;
}

/* Moves that can't be undone get slightly higher priority, since it means
we are moving a card for the first time. */

static void mark_irreversible(fc_solve_soft_thread_t * soft_thread, int n)
{
    int i, irr;
    card_t card, srccard;
    MOVE *mp;

    for (i = 0, mp = soft_thread->Possible; i < n; i++, mp++) {
        irr = FALSE;
        if (mp->totype == O_TYPE) {
            irr = TRUE;
        } else if (mp->fromtype == W_TYPE) {
            srccard = mp->srccard;
            if (srccard != NONE) {
                card = mp->card;
                if (fcs_pats_card_rank(card) != fcs_pats_card_rank(srccard) - 1 ||
                    !fcs_pats_is_suitable(card, srccard)) {
                    irr = TRUE;
                }
            } else if (soft_thread->King_only && mp->card != PS_KING) {
                irr = TRUE;
            }
        }
        if (irr) {
            mp->pri += soft_thread->Xparam[8];
        }
    }
}

/* Test the current position to see if it's new (or better).  If it is, save
it, along with the pointer to its parent and the move we used to get here. */

POSITION *new_position(fc_solve_soft_thread_t * soft_thread, POSITION *parent, MOVE *m)
{
    int i, t, depth, cluster;
    u_char *p;
    POSITION *pos;
    TREE *node;

    /* Search the list of stored positions.  If this position is found,
    then ignore it and return (unless this position is better). */

    if (parent == NULL) {
        depth = 0;
    } else {
        depth = parent->depth + 1;
    }
    i = insert(soft_thread, &cluster, depth, &node);
    if (i == NEW) {
        soft_thread->Total_positions++;
    } else if (i != FOUND_BETTER) {
        return NULL;
    }

    /* A new or better position.  insert() already stashed it in the
    tree, we just have to wrap a POSITION struct around it, and link it
    into the move stack.  Store the temp cells after the POSITION. */

    if (soft_thread->Freepos) {
        p = (u_char *)soft_thread->Freepos;
        soft_thread->Freepos = soft_thread->Freepos->queue;
    } else {
        p = new_from_block(soft_thread, soft_thread->Posbytes);
        if (p == NULL) {
            return NULL;
        }
    }

    pos = (POSITION *)p;
    pos->queue = NULL;
    pos->parent = parent;
    pos->node = node;
    pos->move = *m;                 /* struct copy */
    pos->cluster = cluster;
    pos->depth = depth;
    pos->nchild = 0;

    p += sizeof(POSITION);
    i = 0;
    for (t = 0; t < soft_thread->Ntpiles; t++) {
        *p++ = soft_thread->T[t];
        if (soft_thread->T[t] != NONE) {
            i++;
        }
    }
    pos->ntemp = i;

    return pos;
}

/* Comparison function for sorting the soft_thread->W piles. */

static GCC_INLINE int wcmp(fc_solve_soft_thread_t * soft_thread, int a, int b)
{
    if (soft_thread->Xparam[9] < 0) {
        return soft_thread->Wpilenum[b] - soft_thread->Wpilenum[a];       /* newer piles first */
    } else {
        return soft_thread->Wpilenum[a] - soft_thread->Wpilenum[b];       /* older piles first */
    }
}

#if 0
static GCC_INLINE int wcmp(int a, int b)
{
    if (soft_thread->Xparam[9] < 0) {
        return soft_thread->Wlen[b] - soft_thread->Wlen[a];       /* longer piles first */
    } else {
        return soft_thread->Wlen[a] - soft_thread->Wlen[b];       /* shorter piles first */
    }
}
#endif

/* Sort the piles, to remove the physical differences between logically
equivalent layouts.  Assume it's already mostly sorted.  */

void pilesort(fc_solve_soft_thread_t * soft_thread)
{
    int w, i, j;

    /* Make sure all the piles have id numbers. */

    for (w = 0; w < soft_thread->Nwpiles; w++) {
        if (soft_thread->Wpilenum[w] < 0) {
            soft_thread->Wpilenum[w] = get_pilenum(soft_thread, w);
            if (soft_thread->Wpilenum[w] < 0) {
                return;
            }
        }
    }

    /* Sort them. */

    soft_thread->Widx[0] = 0;
    w = 0;
    for (i = 1; i < soft_thread->Nwpiles; i++) {
        if (wcmp(soft_thread, soft_thread->Widx[w], i) < 0) {
            w++;
            soft_thread->Widx[w] = i;
        } else {
            for (j = w; j >= 0; --j) {
                soft_thread->Widx[j + 1] = soft_thread->Widx[j];
                if (j == 0 || wcmp(soft_thread, soft_thread->Widx[j - 1], i) < 0) {
                    soft_thread->Widx[j] = i;
                    break;
                }
            }
            w++;
        }
    }

    /* Compute the inverse. */

    for (i = 0; i < soft_thread->Nwpiles; i++) {
        soft_thread->Widxi[soft_thread->Widx[i]] = i;
    }
}
/* Compact position representation.  The position is stored as an
array with the following format:
    pile0# pile1# ... pileN# (N = soft_thread->Nwpiles)
where each pile number is packed into 12 bits (so 2 piles take 3 bytes).
Positions in this format are unique can be compared with memcmp().  The soft_thread->O
cells are encoded as a cluster number: no two positions with different
cluster numbers can ever be the same, so we store different clusters in
different trees.  */

TREE *pack_position(fc_solve_soft_thread_t * soft_thread)
{
    int j, k, w;
    u_char *p;
    TREE *node;

    /* Allocate space and store the pile numbers.  The tree node
    will get filled in later, by insert_node(). */

    p = new_from_block(soft_thread, soft_thread->Treebytes);
    if (p == NULL) {
        return NULL;
    }
    node = (TREE *)p;
    p += sizeof(TREE);

    /* Pack the pile numers j into bytes p.
               j             j
        +--------+----:----+--------+
        |76543210|7654:3210|76543210|
        +--------+----:----+--------+
            p         p         p
    */

    k = 0;
    for (w = 0; w < soft_thread->Nwpiles; w++) {
        j = soft_thread->Wpilenum[soft_thread->Widx[w]];
        switch (k) {
        case 0:
            *p++ = j >> 4;
            *p = (j & 0xF) << 4;
            k = 1;
            break;
        case 1:
            *p++ |= j >> 8;         /* j is positive */
            *p++ = j & 0xFF;
            k = 0;
            break;
        }
    }

    return node;
}

/* Like strcpy() but return the length of the string. */
static int strecpy(u_char *dest, u_char *src)
{
    int i;

    i = 0;
    while ((*dest++ = *src++) != '\0') {
        i++;
    }

    return i;
}

/* Unpack a compact position rep.  soft_thread->T cells must be restored from the
array following the POSITION struct. */

void unpack_position(fc_solve_soft_thread_t * soft_thread, POSITION *pos)
{
    int i, k, w;
    u_char c, *p;
    BUCKETLIST *l;

    /* Get the Out cells from the cluster number. */

    k = pos->cluster;
    soft_thread->O[0] = k & 0xF;
    k >>= 4;
    soft_thread->O[1] = k & 0xF;
    k >>= 4;
    soft_thread->O[2] = k & 0xF;
    k >>= 4;
    soft_thread->O[3] = k & 0xF;

    /* Unpack bytes p into pile numbers j.
            p         p         p
        +--------+----:----+--------+
        |76543210|7654:3210|76543210|
        +--------+----:----+--------+
               j             j
    */

    k = w = i = c = 0;
    p = (u_char *)(pos->node) + sizeof(TREE);
    while (w < soft_thread->Nwpiles) {
        switch (k) {
        case 0:
            i = *p++ << 4;
            c = *p++;
            i |= (c >> 4) & 0xF;
            k = 1;
            break;
        case 1:
            i = (c & 0xF) << 8;
            i |= *p++;
            k = 0;
            break;
        }
        soft_thread->Wpilenum[w] = i;
        l = soft_thread->Pilebucket[i];
        i = strecpy(soft_thread->W[w], l->pile);
        soft_thread->Wp[w] = &soft_thread->W[w][i - 1];
        soft_thread->Wlen[w] = i;
        soft_thread->Whash[w] = l->hash;
        w++;
    }

    /* soft_thread->T cells. */

    p = (u_char *)pos;
    p += sizeof(POSITION);
    for (i = 0; i < soft_thread->Ntpiles; i++) {
        soft_thread->T[i] = *p++;
    }
}

/* Win.  Print out the move stack. */

static void win(fc_solve_soft_thread_t * soft_thread, POSITION *pos)
{
    int i, nmoves;
    FILE *out;
    POSITION *p;
    MOVE *mp, **mpp, **mpp0;

    /* Go back up the chain of parents and store the moves
    in reverse order. */

    i = 0;
    for (p = pos; p->parent; p = p->parent) {
        i++;
    }
    nmoves = i;
    mpp0 = new_array(soft_thread, MOVE *, nmoves);
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
        printcard(mp->card, out);
        if (mp->totype == T_TYPE) {
            fprintf(out, "to temp\n");
        } else if (mp->totype == O_TYPE) {
            fprintf(out, "out\n");
        } else {
            fprintf(out, "to ");
            if (mp->destcard == NONE) {
                fprintf(out, "empty pile");
            } else {
                printcard(mp->destcard, out);
            }
            fputc('\n', out);
        }
    }
    fclose(out);
    free_array(soft_thread, mpp0, MOVE *, nmoves);

    if (!Quiet) {
        printf("A winner.\n");
        printf("%d moves.\n", nmoves);
#if DEBUG
        printf("%d positions generated.\n", soft_thread->Total_generated);
        printf("%d unique positions.\n", soft_thread->Total_positions);
        printf("Mem_remain = %ld\n", soft_thread->Mem_remain);
#endif
    }
}

/* Initialize the hash buckets. */

void init_buckets(fc_solve_soft_thread_t * soft_thread)
{
    int i;

    /* Packed positions need 3 bytes for every 2 piles. */

    i = soft_thread->Nwpiles * 3;
    i >>= 1;
    i += soft_thread->Nwpiles & 0x1;
    soft_thread->Pilebytes = i;

    memset(soft_thread->Bucketlist, 0, sizeof(soft_thread->Bucketlist));
    soft_thread->Pilenum = 0;
    soft_thread->Treebytes = sizeof(TREE) + soft_thread->Pilebytes;

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

/* For each pile, return a unique identifier.  Although there are a
large number of possible piles, generally fewer than 1000 different
piles appear in any given game.  We'll use the pile's hash to find
a hash bucket that contains a short list of piles, along with their
identifiers. */

static GCC_INLINE int get_pilenum(fc_solve_soft_thread_t * soft_thread, int w)
{
    int bucket, pilenum;
    BUCKETLIST *l, *last;

    /* For a given pile, get its unique pile id.  If it doesn't have
    one, add it to the appropriate list and give it one.  First, get
    the hash bucket. */

    bucket = soft_thread->Whash[w] % FC_SOLVE_BUCKETLIST_NBUCKETS;

    /* Look for the pile in this bucket. */

    last = NULL;
    for (l = soft_thread->Bucketlist[bucket]; l; l = l->next) {
        if (l->hash == soft_thread->Whash[w] &&
            strncmp((const char *)l->pile, (const char *)soft_thread->W[w], soft_thread->Wlen[w]) == 0) {
            break;
        }
        last = l;
    }

    /* If we didn't find it, make a new one and add it to the list. */

    if (l == NULL) {
        if (soft_thread->Pilenum == NPILES) {
            fc_solve_msg("Ran out of pile numbers!");
            return -1;
        }
        l = new(soft_thread, BUCKETLIST);
        if (l == NULL) {
            return -1;
        }
        l->pile = new_array(soft_thread, u_char, soft_thread->Wlen[w] + 1);
        if (l->pile == NULL) {
            free_ptr(soft_thread, l, BUCKETLIST);
            return -1;
        }

        /* Store the new pile along with its hash.  Maintain
        a reverse mapping so we can unpack the piles swiftly. */

        strncpy((char*)l->pile, (const char *)soft_thread->W[w], soft_thread->Wlen[w] + 1);
        l->hash = soft_thread->Whash[w];
        l->pilenum = pilenum = soft_thread->Pilenum++;
        l->next = NULL;
        if (last == NULL) {
            soft_thread->Bucketlist[bucket] = l;
        } else {
            last->next = l;
        }
        soft_thread->Pilebucket[pilenum] = l;
    }

    return l->pilenum;
}

void free_buckets(fc_solve_soft_thread_t * soft_thread)
{
    int i, j;
    BUCKETLIST *l, *n;

    for (i = 0; i < FC_SOLVE_BUCKETLIST_NBUCKETS; i++) {
        l = soft_thread->Bucketlist[i];
        while (l) {
            n = l->next;
            j = strlen((const char *)l->pile);    /* @@@ use block? */
            free_array(soft_thread, l->pile, u_char, j + 1);
            free_ptr(soft_thread, l, BUCKETLIST);
            l = n;
        }
    }
}
