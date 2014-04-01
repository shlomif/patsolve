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
#include "pat.h"
#include "fnv.h"
#include "util.h"

#include "inline.h"

static int solve(fc_solve_soft_thread_t *, fcs_pats_position_t *);
static void free_position(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos, int);
static void queue_position(fc_solve_soft_thread_t *, fcs_pats_position_t *, int);
static fcs_pats_position_t *dequeue_position(fc_solve_soft_thread_t *);

/* Test the current position to see if it's new (or better).  If it is, save
it, along with the pointer to its parent and the move we used to get here. */

static fcs_pats_position_t *new_position(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *parent, fcs_pats__move_t *m)
{
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
        soft_thread->Total_positions++;
    }
    else if (verdict != FCS_PATS__INSERT_CODE_FOUND_BETTER)
    {
        return NULL;
    }

    /* A new or better position.  fc_solve_pats__insert() already stashed it in the
    tree, we just have to wrap a fcs_pats_position_t struct around it, and link it
    into the move stack.  Store the temp cells after the fcs_pats_position_t. */

    if (soft_thread->Freepos) {
        p = (u_char *)soft_thread->Freepos;
        soft_thread->Freepos = soft_thread->Freepos->queue;
    } else {
        p = fc_solve_pats__new_from_block(soft_thread, soft_thread->Posbytes);
        if (p == NULL) {
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
    for (t = 0; t < soft_thread->Ntpiles; t++) {
        *p++ = soft_thread->T[t];
        if (soft_thread->T[t] != NONE) {
            i++;
        }
    }
    pos->ntemp = i;

    return pos;
}

void fc_solve_pats__do_it(fc_solve_soft_thread_t * soft_thread)
{
    int i, q;
    fcs_pats_position_t *pos;
    fcs_pats__move_t m;

    /* Init the queues. */

    for (i = 0; i < FC_SOLVE_PATS__NUM_QUEUES; i++) {
        soft_thread->queue_head[i] = NULL;
    }
    soft_thread->Maxq = 0;
#if DEBUG
memset(soft_thread->Clusternum, 0, sizeof(soft_thread->Clusternum));
memset(soft_thread->Inq, 0, sizeof(soft_thread->Inq));
#endif

    /* Queue the initial position to get started. */

    fc_solve_pats__hash_layout(soft_thread);
    fc_solve_pats__sort_piles(soft_thread);
    m.card = NONE;
    pos = new_position(soft_thread, NULL, &m);
    if (pos == NULL) {
        return;
    }
    queue_position(soft_thread, pos, 0);

    /* Solve it. */

    while ((pos = dequeue_position(soft_thread)) != NULL) {
        q = solve(soft_thread, pos);
        if (!q) {
            free_position(soft_thread, pos, TRUE);
        }
    }
}

/* Generate all the successors to a position and either queue them or
recursively solve them.  Return whether any of the child nodes, or their
descendents, were queued or not (if not, the position can be freed). */

static int solve(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *parent)
{
    int i, nmoves, q, qq;
    fcs_pats__move_t *mp, *mp0;
    fcs_pats_position_t *pos;

    /* If we've won already (or failed), we just go through the motions
    but always return FALSE from any position.  This enables the cleanup
    of the move stack and eventual destruction of the position store. */

    if (soft_thread->status != NOSOL) {
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

static void free_position(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos, int rec)
{
    /* We don't really free anything here, we just push it onto a
    freelist (using the queue member), so we can use it again later. */

    if (!rec) {
        pos->queue = soft_thread->Freepos;
        soft_thread->Freepos = pos;
        pos->parent->nchild--;
    } else {
        do {
            pos->queue = soft_thread->Freepos;
            soft_thread->Freepos = pos;
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

static void queue_position(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos, int pri)
{
    int nout;
    double x;

    /* In addition to the priority of a move, a position gets an
    additional priority depending on the number of cards out.  We use a
    "queue squashing function" to map nout to priority.  */

    nout = soft_thread->O[0] + soft_thread->O[1] + soft_thread->O[2] + soft_thread->O[3];

    /* soft_thread->Yparam[0] * nout^2 + soft_thread->Yparam[1] * nout + soft_thread->Yparam[2] */

    x = (soft_thread->Yparam[0] * nout + soft_thread->Yparam[1]) * nout + soft_thread->Yparam[2];
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
    if (pri > soft_thread->Maxq) {
        soft_thread->Maxq = pri;
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
#if DEBUG
soft_thread->Inq[pri]++;
soft_thread->Clusternum[pos->cluster]++;
#endif
}

/* Like strcpy() but return the length of the string. */
static GCC_INLINE int strecpy(u_char *dest, u_char *src)
{
    int i;

    i = 0;
    while ((*dest++ = *src++) != '\0') {
        i++;
    }

    return i;
}

/* Unpack a compact position rep.  soft_thread->T cells must be restored from
 * the array following the fcs_pats_position_t struct. */

static GCC_INLINE void unpack_position(fc_solve_soft_thread_t * soft_thread, fcs_pats_position_t *pos)
{

    /* Get the Out cells from the cluster number. */

    {
        int packed_foundations = pos->cluster;
        typeof(soft_thread->O[0]) * const O = soft_thread->O;
        O[0] = packed_foundations & 0xF;
        packed_foundations >>= 4;
        O[1] = packed_foundations & 0xF;
        packed_foundations >>= 4;
        O[2] = packed_foundations & 0xF;
        packed_foundations >>= 4;
        O[3] = packed_foundations & 0xF;
    }

    {
        int w;
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

        w = c = 0;
        p = (u_char *)(pos->node) + sizeof(fcs_pats__tree_t);
        fcs_bool_t k = FALSE;
        const typeof(soft_thread->Nwpiles) Nwpiles = soft_thread->Nwpiles;
        while (w < Nwpiles) {
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
            soft_thread->Wpilenum[w] = i;
            l = soft_thread->Pilebucket[i];
            i = strecpy(soft_thread->W[w], l->pile);
            soft_thread->Wp[w] = &soft_thread->W[w][i - 1];
            soft_thread->columns_lens[w] = i;
            soft_thread->Whash[w] = l->hash;
            w++;
        }
    }

    /* soft_thread->T cells. */

    {
        u_char * p = (u_char *)pos;
        p += sizeof(fcs_pats_position_t);
        const typeof(soft_thread->Ntpiles) Ntpiles = soft_thread->Ntpiles;
        for (int i = 0; i < Ntpiles; i++) {
            soft_thread->T[i] = *p++;
        }
    }
}

/* Return the position on the head of the queue, or NULL if there isn't one. */

static fcs_pats_position_t *dequeue_position(fc_solve_soft_thread_t * soft_thread)
{
    int last;
    fcs_pats_position_t *pos;
    static int qpos = 0;
    static int minpos = 0;

    /* This is a kind of prioritized round robin.  We make sweeps
    through the queues, starting at the highest priority and
    working downwards; each time through the sweeps get longer.
    That way the highest priority queues get serviced the most,
    but we still get lots of low priority action (instead of
    ignoring it completely). */

    last = FALSE;
    do {
        qpos--;
        if (qpos < minpos) {
            if (last) {
                return NULL;
            }
            qpos = soft_thread->Maxq;
            minpos--;
            if (minpos < 0) {
                minpos = soft_thread->Maxq;
            }
            if (minpos == 0) {
                last = TRUE;
            }
        }
    } while (soft_thread->queue_head[qpos] == NULL);

    pos = soft_thread->queue_head[qpos];
    soft_thread->queue_head[qpos] = pos->queue;
#if DEBUG
    soft_thread->Inq[qpos]--;
#endif

    /* Decrease soft_thread->Maxq if that queue emptied. */

    while (soft_thread->queue_head[qpos] == NULL && qpos == soft_thread->Maxq && soft_thread->Maxq > 0) {
        soft_thread->Maxq--;
        qpos--;
        if (qpos < minpos) {
            minpos = qpos;
        }
    }

    /* Unpack the position into the work arrays. */

    unpack_position(soft_thread, pos);

#if DEBUG
soft_thread->Clusternum[pos->cluster]--;
#endif
    return pos;
}
