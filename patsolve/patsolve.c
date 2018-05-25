/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */

/* Solve patience games.  Prioritized breadth-first search.  Simple breadth-
first uses exponential memory.  Here the work queue is kept sorted to give
priority to positions with more cards out, so the solution found is not
guaranteed to be the shortest, but it'll be better than with a depth-first
search. */

#include <math.h>
#include "instance.h"

/* We can't free the stored piles in the trees, but we can free some of the
fcs_pats_position structs.  We have to be careful, though, because there are
many
threads running through the game tree starting from the queued positions.
The num_childs element keeps track of descendents, and when there are none left
in the parent we can free it too after solve() returns and we get called
recursively (rec == TRUE). */

/* We don't really free anything here, we just push it onto a
   freelist (using the queue member), so we can use it again later.
*/

static inline void free_position_non_recursive(
    fcs_pats_thread *const soft_thread, fcs_pats_position *const pos)
{
    pos->queue = soft_thread->freed_positions;
    soft_thread->freed_positions = pos;
    pos->parent->num_childs--;
}

static inline void free_position_recursive(
    fcs_pats_thread *const soft_thread, fcs_pats_position *pos)
{
    do
    {
        pos->queue = soft_thread->freed_positions;
        soft_thread->freed_positions = pos;
        pos = pos->parent;
        if (!pos)
        {
            return;
        }
        pos->num_childs--;
    } while (pos->num_childs == 0);
}

/* Like strcpy() but returns the length of the string. */
static inline int strecpy(char *dest, const char *src)
{
    int i = 0;
    while ((*dest++ = *src++) != '\0')
    {
        i++;
    }

    return i;
}

/* Test the current position to see if it's new (or better).  If it is, save
it, along with the pointer to its parent and the move we used to get here. */

/* Return the position on the head of the queue, or NULL if there isn't one. */

static inline void unpack_position(
    fcs_pats_thread *const soft_thread, fcs_pats_position *const pos)
{
    DECLARE_STACKS();

    /* Get the Out cells from the cluster number. */

    {
        int packed_foundations = pos->cluster;

        fcs_state *s_ptr = &(soft_thread->current_pos.s);
        fcs_set_foundation(*s_ptr, 0, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 1, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 2, packed_foundations & 0xF);
        packed_foundations >>= 4;
        fcs_set_foundation(*s_ptr, 3, packed_foundations & 0xF);
    }

    {
        /* Unpack bytes p into pile numbers j.
           p         p         p
           +--------+----:----+--------+
           |76543210|7654:3210|76543210|
           +--------+----:----+--------+
           j             j
           */
        unsigned char c = 0;
        unsigned char *p =
            (unsigned char *)(pos->node) + sizeof(fcs_pats__tree);
        bool k = FALSE;
        for (int w = 0; w < LOCAL_STACKS_NUM; w++)
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
            var_AUTO(l, soft_thread->bucket_from_pile_lookup[i]);
            var_AUTO(w_col, fcs_state_get_col(soft_thread->current_pos.s, w));
            i = strecpy(w_col + 1, (char *)(l->pile));
            fcs_col_len(w_col) = i;
            soft_thread->current_pos.stack_hashes[w] = l->hash;
        }
    }

    /* soft_thread->current_pos.freecells cells. */
#if MAX_NUM_FREECELLS > 0
    {
        unsigned char *p = (((unsigned char *)pos) + sizeof(fcs_pats_position));
        for (int i = 0; i < LOCAL_FREECELLS_NUM; i++)
        {
            fcs_freecell_card(soft_thread->current_pos.s, i) = *(p++);
        }
    }
#endif
}

static inline fcs_pats_position *dequeue_position(
    fcs_pats_thread *const soft_thread)
{
    /* This is a kind of prioritized round robin.  We make sweeps
    through the queues, starting at the highest priority and
    working downwards; each time through the sweeps get longer.
    That way the highest priority queues get serviced the most,
    but we still get lots of low priority action (instead of
    ignoring it completely). */

    bool is_last = FALSE;
    do
    {
        if ((--soft_thread->dequeue__qpos) < soft_thread->dequeue__minpos)
        {
            if (is_last)
            {
                return NULL;
            }
            soft_thread->dequeue__qpos = soft_thread->max_queue_idx;
            if ((--soft_thread->dequeue__minpos) < 0)
            {
                soft_thread->dequeue__minpos = soft_thread->max_queue_idx;
            }
            if (soft_thread->dequeue__minpos == 0)
            {
                is_last = TRUE;
            }
        }
    } while (soft_thread->queue_head[soft_thread->dequeue__qpos] == NULL);

    fcs_pats_position *const pos =
        soft_thread->queue_head[soft_thread->dequeue__qpos];
    soft_thread->queue_head[soft_thread->dequeue__qpos] = pos->queue;
#ifdef DEBUG
    --soft_thread->num_positions_in_queue[soft_thread->dequeue__qpos];
#endif

    /* Decrease soft_thread->max_queue_idx if that queue emptied. */
    while (soft_thread->queue_head[soft_thread->dequeue__qpos] == NULL &&
           soft_thread->dequeue__qpos == soft_thread->max_queue_idx &&
           soft_thread->max_queue_idx > 0)
    {
        --soft_thread->max_queue_idx;
        if ((--soft_thread->dequeue__qpos) < soft_thread->dequeue__minpos)
        {
            soft_thread->dequeue__minpos = soft_thread->dequeue__qpos;
        }
    }

    /* Unpack the position into the work arrays. */
    unpack_position(soft_thread, pos);

#ifdef DEBUG
    --soft_thread->num_positions_in_clusters[pos->cluster];
#endif
    return pos;
}

fcs_pats_position *fc_solve_pats__new_position(
    fcs_pats_thread *const soft_thread, fcs_pats_position *const parent,
    const fcs_pats__move *const m)
{
    DECLARE_STACKS();
    int cluster;
    unsigned char *p;

    /* Search the list of stored positions.  If this position is found,
    then ignore it and return (unless this position is better). */
    const int depth = (parent ? (parent->depth + 1) : 0);

    fcs_pats__tree *node;
    const fcs_pats__insert_code verdict =
        fc_solve_pats__insert(soft_thread, &cluster, depth, &node);
    if (verdict == FCS_PATS__INSERT_CODE_NEW)
    {
        soft_thread->num_checked_states++;
    }
    else if (verdict != FCS_PATS__INSERT_CODE_FOUND_BETTER)
    {
        return NULL;
    }

    /* A new or better position.  fc_solve_pats__insert() already stashed it in
    the tree, we just have to wrap a fcs_pats_position struct around it, and
    link it into the move stack.  Store the temp cells after the
    fcs_pats_position. */
    if (soft_thread->freed_positions)
    {
        p = (unsigned char *)soft_thread->freed_positions;
        soft_thread->freed_positions = soft_thread->freed_positions->queue;
    }
    else
    {
        p = fc_solve_pats__new_from_block(
            soft_thread, soft_thread->position_size);
        if (p == NULL)
        {
            return NULL;
        }
    }

    var_AUTO(pos, (fcs_pats_position *)p);
    pos->queue = NULL;
    pos->parent = parent;
    pos->node = node;
    pos->move = *m; /* struct copy */
    pos->cluster = cluster;
    pos->depth = depth;
    pos->num_childs = 0;

    p += sizeof(fcs_pats_position);
    int i = 0;
#if MAX_NUM_FREECELLS > 0
    for (int t = 0; t < LOCAL_FREECELLS_NUM; t++)
    {
        *p++ = fcs_freecell_card(soft_thread->current_pos.s, t);
        if (!fcs_freecell_is_empty(soft_thread->current_pos.s, t))
        {
            ++i;
        }
    }
#endif
    pos->num_cards_in_freecells = i;

    return pos;
}

// Hash the whole layout.  This is called once, at the start.
static inline bool check_for_exceeded(fcs_pats_thread *const soft_thread)
{
    return ((soft_thread->status == FCS_PATS__NOSOL) &&
            (soft_thread->max_num_checked_states >= 0) &&
            (soft_thread->num_checked_states >=
                soft_thread->max_num_checked_states));
}

/* Generate all the successors to a position and either queue them or
recursively solve them.  Return whether any of the child nodes, or their
descendents, were queued or not (if not, the position can be freed). */

static inline void wrap_up_solve(
    fcs_pats_thread *const soft_thread, const enum FC_SOLVE_PATS__MYDIR mydir)
{
    soft_thread->curr_solve_dir = mydir;
}

static inline void freecell_solver_pats__make_move(
    fcs_pats_thread *const soft_thread, const fcs_pats__move *const m)
{
    fcs_card card;
    const_SLOT(from, m);
    const_SLOT(to, m);

#if MAX_NUM_FREECELLS > 0
    // Remove from pile.
    if (m->fromtype == FCS_PATS__TYPE_FREECELL)
    {
        card = fcs_freecell_card(soft_thread->current_pos.s, from);
        fcs_empty_freecell(soft_thread->current_pos.s, from);
    }
    else
#endif
    {
        var_AUTO(from_col, fcs_state_get_col(soft_thread->current_pos.s, from));
        fcs_col_pop_card(from_col, card);
        fc_solve_pats__hashpile(soft_thread, from);
    }

    // Add to pile.

    switch (m->totype)
    {
    case FCS_PATS__TYPE_FREECELL:
#if MAX_NUM_FREECELLS > 0
        fcs_freecell_card(soft_thread->current_pos.s, to) = card;
#endif
        break;
    case FCS_PATS__TYPE_WASTE:
        fcs_state_push(&soft_thread->current_pos.s, to, card);
        fc_solve_pats__hashpile(soft_thread, to);
        break;
    default:
        fcs_increment_foundation(soft_thread->current_pos.s, to);
        break;
    }
}

static inline void fc_solve_pats__undo_move(
    fcs_pats_thread *const soft_thread, const fcs_pats__move *const m)
{
    const_SLOT(from, m);
    const_SLOT(to, m);
    // Remove from 'to' pile.
    fcs_card card;
    switch (m->totype)
    {
#if MAX_NUM_FREECELLS > 0
    case FCS_PATS__TYPE_FREECELL:
        card = fcs_freecell_card(soft_thread->current_pos.s, to);
        fcs_empty_freecell(soft_thread->current_pos.s, to);
        break;
#endif
    case FCS_PATS__TYPE_WASTE:
        card = fcs_state_pop_col_card(&soft_thread->current_pos.s, to);
        fc_solve_pats__hashpile(soft_thread, to);
        break;
    default:
        card = fcs_make_card(
            fcs_foundation_value(soft_thread->current_pos.s, to), to);
        --fcs_foundation_value(soft_thread->current_pos.s, to);
        break;
    }
    // Add to 'from' pile.

#if MAX_NUM_FREECELLS > 0
    if (m->fromtype == FCS_PATS__TYPE_FREECELL)
    {
        fcs_freecell_card(soft_thread->current_pos.s, from) = card;
    }
    else
#endif
    {
        fcs_state_push(&soft_thread->current_pos.s, from, card);
        fc_solve_pats__hashpile(soft_thread, from);
    }
}

static inline int solve(
    fcs_pats_thread *const soft_thread, bool *const is_finished)
{
#define DEPTH (soft_thread->curr_solve_depth)
#define LEVEL (soft_thread->solve_stack[DEPTH])
#define UP_LEVEL (soft_thread->solve_stack[DEPTH + 1])
    var_AUTO(mydir, soft_thread->curr_solve_dir);

    *is_finished = FALSE;
    while (DEPTH >= 0)
    {
        if (check_for_exceeded(soft_thread))
        {
            wrap_up_solve(soft_thread, mydir);
            return TRUE;
        }

        var_S_SLOT(parent, LEVEL);
        /* If we've won already (or failed), we just go through the motions
        but always return FALSE from any position.  This enables the cleanup
        of the move stack and eventual destruction of the position store. */

        if ((soft_thread->status != FCS_PATS__NOSOL) ||
            (parent->node->depth < parent->depth))
        {
            LEVEL.q = FALSE;
            --DEPTH;
            mydir = FC_SOLVE_PATS__DOWN;
            continue;
        }

        // Generate an array of all the moves we can make.
        int num_moves;
        if (!LEVEL.moves_start)
        {
            LEVEL.moves_start =
                fc_solve_pats__get_moves(soft_thread, parent, &num_moves);
            if (!LEVEL.moves_start)
            {
                LEVEL.q = FALSE;
                --DEPTH;
                mydir = FC_SOLVE_PATS__DOWN;
                continue;
            }
            LEVEL.moves_end =
                LEVEL.moves_start + (parent->num_childs = num_moves);
            LEVEL.move_ptr = LEVEL.moves_start;
            LEVEL.q = FALSE;
        }
        else
        {
            num_moves = LEVEL.moves_end - LEVEL.moves_start;
        }

        // Make each move and either solve or queue the result.
        if (mydir == FC_SOLVE_PATS__DOWN)
        {
            if (UP_LEVEL.q)
            {
                LEVEL.q = TRUE;
            }
            else
            {
                free_position_non_recursive(soft_thread, LEVEL.pos);
            }
            fc_solve_pats__undo_move(soft_thread, LEVEL.move_ptr);
            LEVEL.move_ptr++;
        }

        if (LEVEL.move_ptr == LEVEL.moves_end)
        {
            fc_solve_pats__free_array(
                soft_thread, LEVEL.moves_start, fcs_pats__move, num_moves);
            LEVEL.moves_start = NULL;
            --DEPTH;
            mydir = FC_SOLVE_PATS__DOWN;
            continue;
        }
        else
        {
            freecell_solver_pats__make_move(soft_thread, LEVEL.move_ptr);

            // Calculate indices for the new piles.
            fc_solve_pats__sort_piles(soft_thread);

            // See if this is a new position.
            LEVEL.pos = fc_solve_pats__new_position(
                soft_thread, parent, LEVEL.move_ptr);
            if (!LEVEL.pos)
            {
                parent->num_childs--;
                fc_solve_pats__undo_move(soft_thread, LEVEL.move_ptr);
                LEVEL.move_ptr++;
                mydir = FC_SOLVE_PATS__UP;
            }
            else
            {
                /* If this position is in a new cluster, a card went out.
                   Don't queue it, just keep going.  A larger cutoff can also
                   force a recursive call, which can help speed things up (but
                   reduces the quality of solutions).  Otherwise, save it for
                   later. */

                if (LEVEL.pos->cluster != parent->cluster ||
                    num_moves < soft_thread->num_moves_to_cut_off)
                {
                    if (DEPTH + 1 >= soft_thread->max_solve_depth)
                    {
                        soft_thread->max_solve_depth +=
                            FCS_PATS__SOLVE_LEVEL_GROW_BY;
                        soft_thread->solve_stack =
                            SREALLOC(soft_thread->solve_stack,
                                soft_thread->max_solve_depth);
                    }
                    UP_LEVEL.parent = LEVEL.pos;
                    UP_LEVEL.moves_start = NULL;
                    ++DEPTH;
                    mydir = FC_SOLVE_PATS__UP;
                }
                else
                {
                    fc_solve_pats__queue_position(
                        soft_thread, LEVEL.pos, (LEVEL.move_ptr)->pri);
                    fc_solve_pats__undo_move(soft_thread, LEVEL.move_ptr);
                    LEVEL.q = TRUE;
                    LEVEL.move_ptr++;
                    mydir = FC_SOLVE_PATS__UP;
                }
            }
        }
    }
    *is_finished = TRUE;
    wrap_up_solve(soft_thread, mydir);
    return UP_LEVEL.q;
#undef LEVEL
#undef UP_LEVEL
#undef DEPTH
}

DLLEXPORT void fc_solve_pats__do_it(fcs_pats_thread *const soft_thread)
{
    while (1)
    {
        if (!soft_thread->curr_solve_pos)
        {
            fcs_pats_position *const pos = dequeue_position(soft_thread);
            if (!pos)
            {
                break;
            }
            soft_thread->solve_stack[0].parent = soft_thread->curr_solve_pos =
                pos;
            soft_thread->solve_stack[0].moves_start = NULL;
            soft_thread->curr_solve_depth = 0;
            soft_thread->curr_solve_dir = FC_SOLVE_PATS__UP;
        }
        bool is_finished;
        if (!solve(soft_thread, &is_finished))
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

void fc_solve_pats__queue_position(
    fcs_pats_thread *const soft_thread, fcs_pats_position *const pos, int pri)
{
    /* In addition to the priority of a move, a position gets an
    additional priority depending on the number of cards out.  We use a
    "queue squashing function" to map num_cards_out to priority.  */
    const int num_cards_out =
        fcs_foundation_value(soft_thread->current_pos.s, 0) +
        fcs_foundation_value(soft_thread->current_pos.s, 1) +
        fcs_foundation_value(soft_thread->current_pos.s, 2) +
        fcs_foundation_value(soft_thread->current_pos.s, 3);

    /* y_param[0] * nout^2 + y_param[1] * nout + y_param[2] */
    const_PTR(y_param, soft_thread->pats_solve_params.y);
    const double x =
        (y_param[0] * num_cards_out + y_param[1]) * num_cards_out + y_param[2];
    /*
     * GCC gives a warning with some flags if we cast the result
     * of floor to an int directly. As a result, we need to use
     * an intermediate variable.
     * */
    const double rounded_x = (floor(x + .5));
    pri += (int)rounded_x;

    if (pri < 0)
    {
        pri = 0;
    }
    else if (pri >= FC_SOLVE_PATS__NUM_QUEUES)
    {
        pri = FC_SOLVE_PATS__NUM_QUEUES - 1;
    }
    if (pri > soft_thread->max_queue_idx)
    {
        soft_thread->max_queue_idx = pri;
    }

    /* We always dequeue from the head.  Here we either stick the move
    at the head or tail of the queue, depending on whether we're
    pretending it's a stack or a queue. */

    pos->queue = NULL;
    if (soft_thread->queue_head[pri] == NULL)
    {
        soft_thread->queue_head[pri] = pos;
        soft_thread->queue_tail[pri] = pos;
    }
    else
    {
        if (soft_thread->to_stack)
        {
            pos->queue = soft_thread->queue_head[pri];
            soft_thread->queue_head[pri] = pos;
        }
        else
        {
            soft_thread->queue_tail[pri]->queue = pos;
            soft_thread->queue_tail[pri] = pos;
        }
    }
#ifdef DEBUG
    ++soft_thread->num_positions_in_queue[pri];
    ++soft_thread->num_positions_in_clusters[pos->cluster];
#endif
}
