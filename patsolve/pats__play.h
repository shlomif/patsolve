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

#ifndef FC_SOLVE_PATSOLVE_PATS_PLAY_H
#define FC_SOLVE_PATSOLVE_PATS_PLAY_H

#include "config.h"
#include "pat.h"
#include "inline.h"

#ifdef DEBUG
#include "msg.h"
#endif

static GCC_INLINE void fc_solve_pats__before_play(
    fcs_pats_thread_t * soft_thread
)
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

static GCC_INLINE void fc_solve_pats__play(fcs_pats_thread_t * const soft_thread)
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

static void set_param(fcs_pats_thread_t * const soft_thread, const int param_num)
{
    soft_thread->pats_solve_params = freecell_solver_pats__x_y_params_preset[param_num];
    soft_thread->cutoff = soft_thread->pats_solve_params.x[FC_SOLVE_PATS__NUM_X_PARAM - 1];
}

#endif /* FC_SOLVE_PATSOLVE_PATS_PLAY_H */
