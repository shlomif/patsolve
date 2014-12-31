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
#include "util.h"

#include "inline.h"
#include "bool.h"

static int solve(fcs_pats_thread_t *, fcs_pats_position_t *);
static void free_position(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *pos, int);
static void queue_position(fcs_pats_thread_t *, fcs_pats_position_t *, int);

/* Like strcpy() but return the length of the string. */
static GCC_INLINE int strecpy(char *dest, char *src)
{
    int i;

    i = 0;
    while ((*dest++ = *src++) != '\0') {
        i++;
    }

    return i;
}

/* Test the current position to see if it's new (or better).  If it is, save
it, along with the pointer to its parent and the move we used to get here. */

/* Return the position on the head of the queue, or NULL if there isn't one. */

static GCC_INLINE void unpack_position(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *pos)
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
        u_char * p = (u_char *)pos;
        p += sizeof(fcs_pats_position_t);
        const typeof(LOCAL_FREECELLS_NUM) Ntpiles = LOCAL_FREECELLS_NUM;
        for (int i = 0; i < Ntpiles; i++) {
            fcs_freecell_card(soft_thread->current_pos.s, i) = *p++;
        }
    }
}

static GCC_INLINE fcs_pats_position_t * dequeue_position(fcs_pats_thread_t * soft_thread)
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

static fcs_pats_position_t *new_position(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *parent, fcs_pats__move_t *m)
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
    pos->ntemp = i;

    return pos;
}

extern void fc_solve_pats__initialize_solving_process(
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
    fcs_pats_position_t * pos = new_position(soft_thread, NULL, &m);
    if (pos == NULL) {
        return;
    }
    queue_position(soft_thread, pos, 0);
}

DLLEXPORT void fc_solve_pats__do_it(fcs_pats_thread_t * soft_thread)
{
    fcs_pats_position_t *pos;

    /* Solve it. */

    while ((pos = dequeue_position(soft_thread)) != NULL) {
        if (! solve(soft_thread, pos) ) {
            free_position(soft_thread, pos, TRUE);
        }

        if ((soft_thread->status == FCS_PATS__NOSOL)
            && (soft_thread->max_num_checked_states >= 0)
            && (soft_thread->num_checked_states >= soft_thread->max_num_checked_states))
        {
            soft_thread->status = FCS_PATS__FAIL;
            soft_thread->fail_reason = FCS_PATS__FAIL_CHECKED_STATES;
            break;
        }
    }
}

/* Generate all the successors to a position and either queue them or
recursively solve them.  Return whether any of the child nodes, or their
descendents, were queued or not (if not, the position can be freed). */

static int solve(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *parent)
{
    int i, nmoves, q, qq;
    fcs_pats__move_t *mp, *mp0;
    fcs_pats_position_t *pos;

    /* If we've won already (or failed), we just go through the motions
    but always return FALSE from any position.  This enables the cleanup
    of the move stack and eventual destruction of the position store. */

    if (soft_thread->status != FCS_PATS__NOSOL) {
        return FALSE;
    }

    /* If the position was found again in the tree by a shorter
    path, prune this path. */

    if (parent->node->depth < parent->depth) {
        return FALSE;
    }

    /* Generate an array of all the moves we can make. */

    if (!(mp0 = fc_solve_pats__get_moves(soft_thread, parent, &nmoves))) {
        return FALSE;
    }
    parent->nchild = nmoves;

    /* Make each move and either solve or queue the result. */

    q = FALSE;
    for (i = 0, mp = mp0; i < nmoves; i++, mp++) {
        freecell_solver_pats__make_move(soft_thread, mp);

        /* Calculate indices for the new piles. */

        fc_solve_pats__sort_piles(soft_thread);

        /* See if this is a new position. */

        if ((pos = new_position(soft_thread, parent, mp)) == NULL) {
            fc_solve_pats__undo_move(soft_thread, mp);
            parent->nchild--;
            continue;
        }

        /* If this position is in a new cluster, a card went out.
        Don't queue it, just keep going.  A larger cutoff can also
        force a recursive call, which can help speed things up (but
        reduces the quality of solutions).  Otherwise, save it for
        later. */

        if (pos->cluster != parent->cluster || nmoves < soft_thread->cutoff) {
            qq = solve(soft_thread, pos);
            fc_solve_pats__undo_move(soft_thread, mp);
            if (!qq) {
                free_position(soft_thread, pos, FALSE);
            }
            q |= qq;
        } else {
            queue_position(soft_thread, pos, mp->pri);
            fc_solve_pats__undo_move(soft_thread, mp);
            q = TRUE;
        }
    }
    fc_solve_pats__free_array(soft_thread, mp0, fcs_pats__move_t, nmoves);

    /* Return true if this position needs to be kept around. */

    return q;
}

/* We can't free the stored piles in the trees, but we can free some of the
fcs_pats_position_t structs.  We have to be careful, though, because there are many
threads running through the game tree starting from the queued positions.
The nchild element keeps track of descendents, and when there are none left
in the parent we can free it too after solve() returns and we get called
recursively (rec == TRUE). */

static void free_position(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *pos, int rec)
{
    /* We don't really free anything here, we just push it onto a
    freelist (using the queue member), so we can use it again later. */

    if (!rec) {
        pos->queue = soft_thread->freed_positions;
        soft_thread->freed_positions = pos;
        pos->parent->nchild--;
    } else {
        do {
            pos->queue = soft_thread->freed_positions;
            soft_thread->freed_positions = pos;
            pos = pos->parent;
            if (pos == NULL) {
                return;
            }
            pos->nchild--;
        } while (pos->nchild == 0);
    }
}

/* Save positions for consideration later.  pri is the priority of the move
that got us here.  The work queue is kept sorted by priority (simply by
having separate queues). */

static void queue_position(fcs_pats_thread_t * soft_thread, fcs_pats_position_t *pos, int pri)
{
    int nout;
    double x;

    /* In addition to the priority of a move, a position gets an
    additional priority depending on the number of cards out.  We use a
    "queue squashing function" to map nout to priority.  */

    nout = fcs_foundation_value(soft_thread->current_pos.s, 0) + fcs_foundation_value(soft_thread->current_pos.s, 1) + fcs_foundation_value(soft_thread->current_pos.s, 2) + fcs_foundation_value(soft_thread->current_pos.s, 3);

    /* y_param[0] * nout^2 + y_param[1] * nout + y_param[2] */

    const typeof(soft_thread->pats_solve_params.y[0]) * const y_param
        = soft_thread->pats_solve_params.y;

    x = (y_param[0] * nout + y_param[1]) * nout + y_param[2];
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



DLLEXPORT void fc_solve_pats__before_play(fcs_pats_thread_t * soft_thread)
{
    /* Initialize the hash tables. */

    fc_solve_pats__init_buckets(soft_thread);
    fc_solve_pats__init_clusters(soft_thread);

    /* Reset stats. */

    soft_thread->num_checked_states = 0;
    soft_thread->num_states_in_collection = 0;
    soft_thread->num_solutions = 0;

    soft_thread->status = FCS_PATS__NOSOL;

    /* Go to it. */

    fc_solve_pats__initialize_solving_process(soft_thread);
}

DLLEXPORT void fc_solve_pats__play(fcs_pats_thread_t * soft_thread)
{
    fc_solve_pats__before_play(soft_thread);
    fc_solve_pats__do_it(soft_thread);
    if (soft_thread->status != FCS_PATS__WIN && !soft_thread->is_quiet)
    {
        if (soft_thread->status == FCS_PATS__FAIL)
        {
            printf("Out of memory.\n");
        }
        else if (soft_thread->Noexit && soft_thread->num_solutions > 0)
        {
            printf("No shorter solutions.\n");
        }
        else
        {
            printf("No solution.\n");
        }
#ifdef DEBUG
        printf("%d positions generated.\n", soft_thread->num_states_in_collection);
        printf("%d unique positions.\n", soft_thread->num_checked_states);
        printf("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
    }
#ifdef DEBUG
    fc_solve_msg("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
}

static const char * const fc_solve_pats__Ranks_string = " A23456789TJQK";
static const char * const fc_solve_pats__Suits_string = "HCDS";

DLLEXPORT void fc_solve_pats__print_card(const fcs_card_t card, FILE * out_fh)
{
    if (fcs_card_rank(card) != fc_solve_empty_card) {
        fprintf(out_fh, "%c%c",
            fc_solve_pats__Ranks_string[(int)fcs_card_rank(card)],
            fc_solve_pats__Suits_string[(int)fcs_card_suit(card)]);
    }
}
