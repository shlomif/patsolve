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

static const int freecell_solver_user_set_sequences_are_built_by_type(
    fc_solve_instance_t * const instance,
    const int sequences_are_built_by
    )
{
#ifndef FCS_FREECELL_ONLY
    if ((sequences_are_built_by < 0) || (sequences_are_built_by > 2))
    {
        return 1;
    }

    instance->game_params.game_flags &= (~0x3);
    instance->game_params.game_flags |= sequences_are_built_by;

#endif

    return 0;
}

static const int freecell_solver_user_set_empty_stacks_filled_by(
    fc_solve_instance_t * const instance,
    const int empty_stacks_fill
    )
{

#ifndef FCS_FREECELL_ONLY
    if ((empty_stacks_fill < 0) || (empty_stacks_fill > 2))
    {
        return 1;
    }

    instance->game_params.game_flags &= (~(0x3 << 2));
    instance->game_params.game_flags |=
        (empty_stacks_fill << 2);
#endif

    return 0;
}

static GCC_INLINE const int get_idx_from_env(const char * const name)
{
    const char * const s = getenv(name);
    if (!s)
    {
        return -1;
    }
    else
    {
        return atoi(s);
    }
}


static GCC_INLINE void pats__init_soft_thread_and_instance(
    fcs_pats_thread_t * const soft_thread,
    fc_solve_instance_t * const instance
)
{
    fc_solve_pats__init_soft_thread(soft_thread, instance);
#ifndef FCS_FREECELL_ONLY
    instance->game_params.game_flags = 0;
    instance->game_params.game_flags |= FCS_SEQ_BUILT_BY_ALTERNATE_COLOR;
    instance->game_params.game_flags |= FCS_ES_FILLED_BY_ANY_CARD << 2;
    INSTANCE_DECKS_NUM = 1;
    INSTANCE_STACKS_NUM = 10;
    INSTANCE_FREECELLS_NUM = 4;
#endif
}

#endif /* FC_SOLVE_PATSOLVE_PATS_PLAY_H */
