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
/* Solve patience games.  Prioritized breadth-first search.  Simple breadth-
first uses exponential memory.  Here the work queue is kept sorted to give
priority to positions with more cards out, so the solution found is not
guaranteed to be the shortest, but it'll be better than with a depth-first
search. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "instance.h"
#include "fnv.h"

#include "inline.h"
#include "bool.h"

/* We can't free the stored piles in the trees, but we can free some of the
fcs_pats_position_t structs.  We have to be careful, though, because there are many
threads running through the game tree starting from the queued positions.
The nchild element keeps track of descendents, and when there are none left
in the parent we can free it too after solve() returns and we get called
recursively (rec == TRUE). */

/* We don't really free anything here, we just push it onto a
   freelist (using the queue member), so we can use it again later.
*/


static GCC_INLINE void free_position_non_recursive(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const pos)
{
    pos->queue = soft_thread->freed_positions;
    soft_thread->freed_positions = pos;
    pos->parent->nchild--;
}

static GCC_INLINE void free_position_recursive(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t *pos)
{
    do
    {
        pos->queue = soft_thread->freed_positions;
        soft_thread->freed_positions = pos;
        pos = pos->parent;
        if (! pos)
        {
            return;
        }
        pos->nchild--;
    } while (pos->nchild == 0);
}

/* Like strcpy() but returns the length of the string. */
static GCC_INLINE int strecpy(char *dest, const char *src)
{
    int i = 0;
    while ((*dest++ = *src++) != '\0') {
        i++;
    }

    return i;
}

/* Test the current position to see if it's new (or better).  If it is, save
it, along with the pointer to its parent and the move we used to get here. */

/* Return the position on the head of the queue, or NULL if there isn't one. */

static GCC_INLINE void unpack_position(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const pos)
{
    DECLARE_STACKS();

    /* Get the Out cells from the cluster number. */

    {
        int packed_foundations = pos->cluster;

        fcs_state_t * s_ptr = &(soft_thread->current_pos.s);
        fcs_set_foundation(*s_ptr, 0, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 1, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 2, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 3, packed_foundations & 0xF);
    }

    {
        u_char c;
        u_char *p;
        fcs_pats__bucket_list_t *l;

        /* Unpack bytes p into pile numbers j.
           p         p         p
           +--------+----:----+--------+
           |76543210|7654:3210|76543210|
           +--------+----:----+--------+
           j             j
           */

        c = 0;
        p = (u_char *)(pos->node) + sizeof(fcs_pats__tree_t);
        fcs_bool_t k = FALSE;
        for (int w = 0; w < LOCAL_STACKS_NUM ; w++)
        {
            int i;
            if (k)
            {
                i = (c & 0xF) << 8;
                i |= *p++;
            }
            else
            {
                i = *p++ << 4;
                c = *p++;
                i |= (c >> 4) & 0xF;
            }
            k = !k;
            soft_thread->current_pos.stack_ids[w] = i;
            l = soft_thread->bucket_from_pile_lookup[i];
            fcs_cards_column_t w_col = fcs_state_get_col(soft_thread->current_pos.s, w);
            i = strecpy(w_col+1, (char *)(l->pile));
            fcs_col_len(w_col) = i;
            soft_thread->current_pos.stack_hashes[w] = l->hash;
        }
    }

    /* soft_thread->current_pos.freecells cells. */

    {
        u_char * p = ( ((u_char *)pos) + sizeof(fcs_pats_position_t) );
        for (int i = 0 ; i < LOCAL_FREECELLS_NUM ; i++) {
            fcs_freecell_card(soft_thread->current_pos.s, i) = *(p++);
        }
    }
}

static GCC_INLINE fcs_pats_position_t * dequeue_position(fcs_pats_thread_t * const soft_thread)
{
    /* This is a kind of prioritized round robin.  We make sweeps
    through the queues, starting at the highest priority and
    working downwards; each time through the sweeps get longer.
    That way the highest priority queues get serviced the most,
    but we still get lots of low priority action (instead of
    ignoring it completely). */

    fcs_bool_t last = FALSE;
    do {
        soft_thread->dequeue__qpos--;
        if (soft_thread->dequeue__qpos < soft_thread->dequeue__minpos) {
            if (last) {
                return NULL;
            }
            soft_thread->dequeue__qpos = soft_thread->max_queue_idx;
            soft_thread->dequeue__minpos--;
            if (soft_thread->dequeue__minpos < 0) {
                soft_thread->dequeue__minpos = soft_thread->max_queue_idx;
            }
            if (soft_thread->dequeue__minpos == 0) {
                last = TRUE;
            }
        }
    } while (soft_thread->queue_head[soft_thread->dequeue__qpos] == NULL);

    fcs_pats_position_t * const pos = soft_thread->queue_head[soft_thread->dequeue__qpos];
    soft_thread->queue_head[soft_thread->dequeue__qpos] = pos->queue;
#ifdef DEBUG
    soft_thread->Inq[soft_thread->dequeue__qpos]--;
#endif

    /* Decrease soft_thread->max_queue_idx if that queue emptied. */

    while (soft_thread->queue_head[soft_thread->dequeue__qpos] == NULL && soft_thread->dequeue__qpos == soft_thread->max_queue_idx && soft_thread->max_queue_idx > 0) {
        soft_thread->max_queue_idx--;
        soft_thread->dequeue__qpos--;
        if (soft_thread->dequeue__qpos < soft_thread->dequeue__minpos) {
            soft_thread->dequeue__minpos = soft_thread->dequeue__qpos;
        }
    }

    /* Unpack the position into the work arrays. */

    unpack_position(soft_thread, pos);

#ifdef DEBUG
soft_thread->num_positions_in_clusters[pos->cluster]--;
#endif
    return pos;
}

fcs_pats_position_t *fc_solve_pats__new_position(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const parent, const fcs_pats__move_t * const m)
{
    DECLARE_STACKS();
    int t, cluster;
    u_char *p;
    fcs_pats_position_t *pos;

    /* Search the list of stored positions.  If this position is found,
    then ignore it and return (unless this position is better). */
    const int depth = (parent ? (parent->depth + 1) : 0);

    fcs_pats__tree_t *node;
    const fcs_pats__insert_code_t verdict = fc_solve_pats__insert(soft_thread, &cluster, depth, &node);
    if (verdict == FCS_PATS__INSERT_CODE_NEW)
    {
        soft_thread->num_checked_states++;
    }
    else if (verdict != FCS_PATS__INSERT_CODE_FOUND_BETTER)
    {
        return NULL;
    }

    /* A new or better position.  fc_solve_pats__insert() already stashed it in the
    tree, we just have to wrap a fcs_pats_position_t struct around it, and link it
    into the move stack.  Store the temp cells after the fcs_pats_position_t. */

    if (soft_thread->freed_positions)
    {
        p = (u_char *)soft_thread->freed_positions;
        soft_thread->freed_positions = soft_thread->freed_positions->queue;
    }
    else
    {
        p = fc_solve_pats__new_from_block(
            soft_thread,
            soft_thread->position_size
        );
        if (p == NULL)
        {
            return NULL;
        }
    }

    pos = (fcs_pats_position_t *)p;
    pos->queue = NULL;
    pos->parent = parent;
    pos->node = node;
    pos->move = *m;                 /* struct copy */
    pos->cluster = cluster;
    pos->depth = depth;
    pos->nchild = 0;

    p += sizeof(fcs_pats_position_t);
    int i = 0;
    for (t = 0; t < LOCAL_FREECELLS_NUM; t++) {
        *p++ = fcs_freecell_card(soft_thread->current_pos.s, t);
        if (fcs_freecell_card(soft_thread->current_pos.s, t) != fc_solve_empty_card) {
            i++;
        }
    }
    pos->num_cards_in_freecells = i;

    return pos;
}

/* Hash the whole layout.  This is called once, at the start. */
static GCC_INLINE const fcs_bool_t check_for_exceeded(fcs_pats_thread_t * const soft_thread)
{
    return
    (
        (soft_thread->status == FCS_PATS__NOSOL)
            && (soft_thread->max_num_checked_states >= 0)
            && (soft_thread->num_checked_states >= soft_thread->max_num_checked_states)
    );
}

/* Generate all the successors to a position and either queue them or
recursively solve them.  Return whether any of the child nodes, or their
descendents, were queued or not (if not, the position can be freed). */

static GCC_INLINE void wrap_up_solve(fcs_pats_thread_t * const soft_thread, const enum FC_SOLVE_PATS__MYDIR mydir)
{
    soft_thread->curr_solve_dir = mydir;
}

static GCC_INLINE void freecell_solver_pats__make_move(fcs_pats_thread_t * const soft_thread, const fcs_pats__move_t * const m)
{
    fcs_card_t card;

    const int from = m->from;
    const int to = m->to;

    /* Remove from pile. */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL) {
        card = fcs_freecell_card(soft_thread->current_pos.s, from);
        fcs_empty_freecell(soft_thread->current_pos.s, from);
    } else {
        fcs_cards_column_t from_col = fcs_state_get_col(soft_thread->current_pos.s, from);
        fcs_col_pop_card(from_col, card);
        fc_solve_pats__hashpile(soft_thread, from);
    }

    /* Add to pile. */

    if (m->totype == FCS_PATS__TYPE_FREECELL) {
        fcs_freecell_card(soft_thread->current_pos.s, to) = card;
    } else if (m->totype == FCS_PATS__TYPE_WASTE) {
        fcs_cards_column_t to_col = fcs_state_get_col(soft_thread->current_pos.s, to);
        fcs_col_push_card(to_col, card);
        fc_solve_pats__hashpile(soft_thread, to);
    } else {
        fcs_increment_foundation(soft_thread->current_pos.s, to);
    }
}

static GCC_INLINE void fc_solve_pats__undo_move(fcs_pats_thread_t * const soft_thread, const fcs_pats__move_t * const m)
{
    const typeof(m->from) from = m->from;
    const typeof(m->to) to = m->to;

    /* Remove from 'to' pile. */

    fcs_card_t card;
    if (m->totype == FCS_PATS__TYPE_FREECELL) {
        card = fcs_freecell_card(soft_thread->current_pos.s, to);
        fcs_empty_freecell(soft_thread->current_pos.s, to);
    } else if (m->totype == FCS_PATS__TYPE_WASTE) {
        fcs_cards_column_t to_col = fcs_state_get_col(soft_thread->current_pos.s, to);
        fcs_col_pop_card(to_col, card);
        fc_solve_pats__hashpile(soft_thread, to);
    } else {
        card = fcs_make_card(fcs_foundation_value(soft_thread->current_pos.s, to), to);
        fcs_foundation_value(soft_thread->current_pos.s, to)--;
    }

    /* Add to 'from' pile. */

    if (m->fromtype == FCS_PATS__TYPE_FREECELL) {
        fcs_freecell_card(soft_thread->current_pos.s, from) = card;
    } else {
        fcs_cards_column_t from_col = fcs_state_get_col(soft_thread->current_pos.s, from);
        fcs_col_push_card(from_col, card);
        fc_solve_pats__hashpile(soft_thread, from);
    }
}

static GCC_INLINE int solve(fcs_pats_thread_t * const soft_thread, fcs_bool_t * const is_finished)
{
    /* short for Depth */
#define D (soft_thread->curr_solve_depth)
    /* Short for Level */
#define L (soft_thread->solve_stack[D])
#define UP_L (soft_thread->solve_stack[D+1])
    typeof(soft_thread->curr_solve_dir) mydir = soft_thread->curr_solve_dir;

    *is_finished = FALSE;
    while (D >= 0)
    {

    if (check_for_exceeded(soft_thread))
    {
        wrap_up_solve(soft_thread, mydir);
        return TRUE;
    }

    typeof(L.parent) parent = L.parent;
    /* If we've won already (or failed), we just go through the motions
    but always return FALSE from any position.  This enables the cleanup
    of the move stack and eventual destruction of the position store. */

    if ((soft_thread->status != FCS_PATS__NOSOL)
        || (parent->node->depth < parent->depth))
    {
        L.q = FALSE;
        D--;
        mydir = FC_SOLVE_PATS__DOWN;
        continue;
    }

    /* Generate an array of all the moves we can make. */
    int nmoves;
    if (! L.mp0)
    {
        L.mp0 =
            fc_solve_pats__get_moves(soft_thread, parent, &nmoves);
        if (! L.mp0)
        {
            L.q = FALSE;
            D--;
            mydir = FC_SOLVE_PATS__DOWN;
            continue;
        }
        L.mp_end = L.mp0 + (parent->nchild = nmoves);
        L.mp = L.mp0;
        L.q = FALSE;
    }
    else
    {
        nmoves = L.mp_end - L.mp0;
    }

    /* Make each move and either solve or queue the result. */

    if (mydir == FC_SOLVE_PATS__DOWN)
    {
        if (UP_L.q)
        {
            L.q = TRUE;
        }
        else
        {
            free_position_non_recursive(soft_thread, L.pos);
        }
        fc_solve_pats__undo_move(soft_thread, L.mp);
        mydir = FC_SOLVE_PATS__UP;
        L.mp++;
    }

    if (L.mp == L.mp_end)
    {
        fc_solve_pats__free_array(soft_thread, L.mp0, fcs_pats__move_t, nmoves);
        L.mp0 = NULL;
        D--;
        mydir = FC_SOLVE_PATS__DOWN;
        continue;
    }
    else
    {
        freecell_solver_pats__make_move(soft_thread, L.mp);

        /* Calculate indices for the new piles. */
        fc_solve_pats__sort_piles(soft_thread);

        /* See if this is a new position. */
        L.pos = fc_solve_pats__new_position(soft_thread, parent, L.mp);
        if (! L.pos)
        {
            parent->nchild--;
            fc_solve_pats__undo_move(soft_thread, L.mp);
            L.mp++;
            mydir = FC_SOLVE_PATS__UP;
        }
        else
        {
            /* If this position is in a new cluster, a card went out.
               Don't queue it, just keep going.  A larger cutoff can also
               force a recursive call, which can help speed things up (but
               reduces the quality of solutions).  Otherwise, save it for
               later. */

            if (L.pos->cluster != parent->cluster || nmoves < soft_thread->cutoff)
            {
                if (D+1 >= soft_thread->max_solve_depth)
                {
                    soft_thread->max_solve_depth += FCS_PATS__SOLVE_LEVEL_GROW_BY;
                    soft_thread->solve_stack = SREALLOC(soft_thread->solve_stack, soft_thread->max_solve_depth);
                }
                UP_L.parent = L.pos;
                UP_L.mp0 = NULL;
                D++;
                mydir = FC_SOLVE_PATS__UP;
            }
            else
            {
                fc_solve_pats__queue_position(soft_thread, L.pos, (L.mp)->pri);
                fc_solve_pats__undo_move(soft_thread, L.mp);
                L.q = TRUE;
                L.mp++;
                mydir = FC_SOLVE_PATS__UP;
            }
        }
    }

    /* Return true if this position needs to be kept around. */
    }
    *is_finished = TRUE;
    wrap_up_solve(soft_thread, mydir);
    typeof(UP_L.q) ret = UP_L.q;
    return ret;
#undef L
#undef UP_L
#undef D
}

DLLEXPORT void fc_solve_pats__do_it(fcs_pats_thread_t * const soft_thread)
{
    /* Solve it. */

    while (1)
    {
        if (! soft_thread->curr_solve_pos)
        {
            fcs_pats_position_t * const pos = dequeue_position(soft_thread);
            if (! pos)
            {
                break;
            }
            soft_thread->solve_stack[0].parent = soft_thread->curr_solve_pos = pos;
            soft_thread->solve_stack[0].mp0 = NULL;
            soft_thread->curr_solve_depth = 0;
            soft_thread->curr_solve_dir = FC_SOLVE_PATS__UP;
        }
        fcs_bool_t is_finished;
        if (! solve(soft_thread, &is_finished) )
        {
            free_position_recursive(soft_thread, soft_thread->curr_solve_pos);
        }
        if (is_finished)
        {
            soft_thread->curr_solve_pos = NULL;
        }

        if (check_for_exceeded(soft_thread))
        {
            soft_thread->status = FCS_PATS__FAIL;
#ifdef FCS_PATSOLVE__WITH_FAIL_REASON
            soft_thread->fail_reason = FCS_PATS__FAIL_CHECKED_STATES;
#endif
            break;
        }
    }
}



/* Save positions for consideration later.  pri is the priority of the move
that got us here.  The work queue is kept sorted by priority (simply by
having separate queues). */

void fc_solve_pats__queue_position(fcs_pats_thread_t * const soft_thread, fcs_pats_position_t * const pos, int pri)
{
    double x;

    /* In addition to the priority of a move, a position gets an
    additional priority depending on the number of cards out.  We use a
    "queue squashing function" to map num_cards_out to priority.  */

    const int num_cards_out = fcs_foundation_value(soft_thread->current_pos.s, 0) + fcs_foundation_value(soft_thread->current_pos.s, 1) + fcs_foundation_value(soft_thread->current_pos.s, 2) + fcs_foundation_value(soft_thread->current_pos.s, 3);

    /* y_param[0] * nout^2 + y_param[1] * nout + y_param[2] */

    const typeof(soft_thread->pats_solve_params.y[0]) * const y_param
        = soft_thread->pats_solve_params.y;

    x = (y_param[0] * num_cards_out + y_param[1]) * num_cards_out + y_param[2];
    {
        /*
         * GCC gives a warning with some flags if we cast the result
         * of floor to an int directly. As a result, we need to use
         * an intermediate variable.
         * */
        const double rounded_x = (floor(x + .5));
        pri += (int)rounded_x;
    }

    if (pri < 0) {
        pri = 0;
    } else if (pri >= FC_SOLVE_PATS__NUM_QUEUES) {
        pri = FC_SOLVE_PATS__NUM_QUEUES - 1;
    }
    if (pri > soft_thread->max_queue_idx) {
        soft_thread->max_queue_idx = pri;
    }

    /* We always dequeue from the head.  Here we either stick the move
    at the head or tail of the queue, depending on whether we're
    pretending it's a stack or a queue. */

    pos->queue = NULL;
    if (soft_thread->queue_head[pri] == NULL) {
        soft_thread->queue_head[pri] = pos;
        soft_thread->queue_tail[pri] = pos;
    } else {
        if (soft_thread->to_stack) {
            pos->queue = soft_thread->queue_head[pri];
            soft_thread->queue_head[pri] = pos;
        } else {
            soft_thread->queue_tail[pri]->queue = pos;
            soft_thread->queue_tail[pri] = pos;
        }
    }
#ifdef DEBUG
soft_thread->Inq[pri]++;
soft_thread->num_positions_in_clusters[pos->cluster]++;
#endif
}


/* Unpack a compact position rep.  soft_thread->current_pos.freecells cells must be restored from
 * the array following the fcs_pats_position_t struct. */





