/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */

#pragma once

#include "config.h"
#include "pat.h"
#include "pats__print_msg.h"

static inline void fc_solve_pats__before_play(fcs_pats_thread_t *soft_thread)
{
    fc_solve_pats__init_buckets(soft_thread);
    fc_solve_pats__init_clusters(soft_thread);

    // Reset stats.
    soft_thread->num_checked_states = 0;
    soft_thread->num_states_in_collection = 0;
    soft_thread->num_solutions = 0;
    soft_thread->status = FCS_PATS__NOSOL;

    fc_solve_pats__initialize_solving_process(soft_thread);
}

static inline void fc_solve_pats__play(
    fcs_pats_thread_t *const soft_thread, const fcs_bool_t is_quiet)
{
    fc_solve_pats__before_play(soft_thread);
    fc_solve_pats__do_it(soft_thread);
    if (soft_thread->status != FCS_PATS__WIN && !is_quiet)
    {
        if (soft_thread->status == FCS_PATS__FAIL)
        {
            printf("Out of memory.\n");
        }
        else if (soft_thread->dont_exit_on_sol &&
                 soft_thread->num_solutions > 0)
        {
            printf("No shorter solutions.\n");
        }
        else
        {
            printf("No solution.\n");
        }
#ifdef DEBUG
        printf(
            "%d positions generated.\n", soft_thread->num_states_in_collection);
        printf("%d unique positions.\n", soft_thread->num_checked_states);
        printf("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
    }
#ifdef DEBUG
    fc_solve_msg("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
}

static void set_param(fcs_pats_thread_t *const soft_thread, const int param_num)
{
    soft_thread->pats_solve_params =
        freecell_solver_pats__x_y_params_preset[param_num];
    fc_solve_pats__set_cut_off(soft_thread);
}

static const int freecell_solver_user_set_sequences_are_built_by_type(
    fc_solve_instance_t *const instance, const int sequences_are_built_by)
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
    fc_solve_instance_t *const instance, const int empty_stacks_fill)
{
#ifndef FCS_FREECELL_ONLY
    if ((empty_stacks_fill < 0) || (empty_stacks_fill > 2))
    {
        return 1;
    }

    instance->game_params.game_flags &= (~(0x3 << 2));
    instance->game_params.game_flags |= (empty_stacks_fill << 2);
#endif
    return 0;
}

static inline const long long get_idx_from_env(const char *const name)
{
    const char *const s = getenv(name);
    if (!s)
    {
        return -1;
    }
    else
    {
        return atoll(s);
    }
}

static inline void pats__init_soft_thread_and_instance(
    fcs_pats_thread_t *const soft_thread, fc_solve_instance_t *const instance)
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

static inline void fc_solve_pats__configure_soft_thread(
    fcs_pats_thread_t *const soft_thread, fc_solve_instance_t *const instance,
    int *const argc_ptr, const char ***const argv_ptr,
    fcs_bool_t *const is_quiet)
{
    int argc = *argc_ptr;
    const char **argv = *argv_ptr;

    pats__init_soft_thread_and_instance(soft_thread, instance);

    program_name = *argv;

    /* Parse args twice.  Once to get the operating mode, and the
    next for other options. */
    var_AUTO(argc0, argc);
    var_AUTO(argv0, argv);
    const char *curr_arg;
    while (--argc > 0 && **++argv == '-' && *(curr_arg = 1 + *argv))
    {
        /* Scan current argument until a flag indicates that the rest
        of the argument isn't flags (curr_arg = NULL), or until
        the end of the argument is reached (if it is all flags). */
        int c;
        while (curr_arg != NULL && (c = *curr_arg++) != '\0')
        {
            switch (c)
            {
            case 's':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance, FCS_ES_FILLED_BY_ANY_CARD);
                freecell_solver_user_set_sequences_are_built_by_type(
                    instance, FCS_SEQ_BUILT_BY_SUIT);
#ifndef FCS_FREECELL_ONLY
                INSTANCE_STACKS_NUM = 10;
                INSTANCE_FREECELLS_NUM = 4;
#endif
                break;

            case 'f':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance, FCS_ES_FILLED_BY_ANY_CARD);
                freecell_solver_user_set_sequences_are_built_by_type(
                    instance, FCS_SEQ_BUILT_BY_ALTERNATE_COLOR);
#ifndef FCS_FREECELL_ONLY
                INSTANCE_STACKS_NUM = 8;
                INSTANCE_FREECELLS_NUM = 4;
#endif
                break;

            case 'k':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance, FCS_ES_FILLED_BY_KINGS_ONLY);
                break;

            case 'a':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance, FCS_ES_FILLED_BY_ANY_CARD);
                break;

            case 'S':
                soft_thread->to_stack = TRUE;
                break;

            case 'w':
#ifndef FCS_FREECELL_ONLY
                INSTANCE_STACKS_NUM = atoi(curr_arg);
#endif
                curr_arg = NULL;
                break;

            case 't':
#ifndef FCS_FREECELL_ONLY
                INSTANCE_FREECELLS_NUM = atoi(curr_arg);
#endif
                curr_arg = NULL;
                break;

            case 'X':
                argv += FC_SOLVE_PATS__NUM_X_PARAM - 1;
                argc -= FC_SOLVE_PATS__NUM_X_PARAM - 1;
                curr_arg = NULL;
                break;

            case 'Y':
                argv += FC_SOLVE_PATS__NUM_Y_PARAM;
                argc -= FC_SOLVE_PATS__NUM_Y_PARAM;
                curr_arg = NULL;
                break;

            default:
                break;
            }
        }
    }

    // Set parameters.
    if (!(GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
            FCS_SEQ_BUILT_BY_SUIT) &&
        !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY))
    {
        set_param(soft_thread, soft_thread->to_stack
                                   ? FC_SOLVE_PATS__PARAM_PRESET__FreecellSpeed
                                   : FC_SOLVE_PATS__PARAM_PRESET__FreecellBest);
    }
    else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
                 FCS_SEQ_BUILT_BY_SUIT) &&
             !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY))
    {
        set_param(soft_thread, soft_thread->to_stack
                                   ? FC_SOLVE_PATS__PARAM_PRESET__SeahavenSpeed
                                   : FC_SOLVE_PATS__PARAM_PRESET__SeahavenBest);
    }
    else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
                 FCS_SEQ_BUILT_BY_SUIT) &&
             (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY))
    {
        set_param(
            soft_thread, soft_thread->to_stack
                             ? FC_SOLVE_PATS__PARAM_PRESET__SeahavenKingSpeed
                             : FC_SOLVE_PATS__PARAM_PRESET__SeahavenKing);
    }
    else
    {
        set_param(soft_thread, 0); // default
    }

    // Now get the other args, and allow overriding the parameters.
    argc = argc0;
    argv = argv0;
    int c;
    while (--argc > 0 && **++argv == '-' && *(curr_arg = 1 + *argv))
    {
        while (curr_arg != NULL && (c = *curr_arg++) != '\0')
        {
            switch (c)
            {
            case 's':
            case 'f':
            case 'k':
            case 'a':
            case 'S':
                break;

            case 'w':
            case 't':
                curr_arg = NULL;
                break;

            case 'E':
                soft_thread->dont_exit_on_sol = TRUE;
                break;

            case 'c':
                soft_thread->num_moves_to_cut_off = atoi(curr_arg);
                curr_arg = NULL;
                break;

            case 'M':
                soft_thread->remaining_memory = atol(curr_arg) * 1000000;
                curr_arg = NULL;
                break;

            case 'v':
                *is_quiet = FALSE;
                break;

            case 'q':
                *is_quiet = TRUE;
                break;

            case 'X':
                // use -c for the last X param
                for (int i = 0; i < FC_SOLVE_PATS__NUM_X_PARAM - 1; i++)
                {
                    soft_thread->pats_solve_params.x[i] = atoi(argv[i + 1]);
                }
                argv += FC_SOLVE_PATS__NUM_X_PARAM - 1;
                argc -= FC_SOLVE_PATS__NUM_X_PARAM - 1;
                curr_arg = NULL;
                break;

            case 'Y':
                for (int i = 0; i < FC_SOLVE_PATS__NUM_Y_PARAM; i++)
                {
                    soft_thread->pats_solve_params.y[i] = atof(argv[i + 1]);
                }
                argv += FC_SOLVE_PATS__NUM_Y_PARAM;
                argc -= FC_SOLVE_PATS__NUM_Y_PARAM;
                curr_arg = NULL;
                break;

            case 'P':
            {
                const_AUTO(i, atoi(curr_arg));
                if (i < 0 || i > FC_SOLVE_PATS__PARAM_PRESET__LastParam)
                {
                    fatalerr("invalid parameter code");
                }
                set_param(soft_thread, i);
                curr_arg = NULL;
            }
            break;

            default:
                fc_solve_msg("%s: unknown flag -%c\n", program_name, c);
                USAGE();
                exit(1);
            }
        }
    }
#if !defined(HARD_CODED_NUM_STACKS) || !defined(HARD_CODED_NUM_FREECELLS)
    const_SLOT(game_params, soft_thread->instance);
#endif

    if (soft_thread->to_stack && soft_thread->dont_exit_on_sol)
    {
        fatalerr("-S and -E may not be used together.");
    }
    if (soft_thread->remaining_memory < (FC_SOLVE__PATS__BLOCKSIZE * 2))
    {
        fatalerr("-M too small.");
    }
    if (LOCAL_STACKS_NUM > MAX_NUM_STACKS)
    {
        fatalerr("too many w piles (max %d)", MAX_NUM_STACKS);
    }
    if (LOCAL_FREECELLS_NUM > MAX_NUM_FREECELLS)
    {
        fatalerr("too many t piles (max %d)", MAX_NUM_FREECELLS);
    }

/* Process the named file, or stdin if no file given.
The name '-' also specifies stdin. */

#ifndef FCS_FREECELL_ONLY
    instance->game_variant_suit_mask = FCS_PATS__COLOR;
    instance->game_variant_desired_suit_value = FCS_PATS__COLOR;
    if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
            FCS_SEQ_BUILT_BY_SUIT))
    {
        instance->game_variant_suit_mask = FCS_PATS__SUIT;
        instance->game_variant_desired_suit_value = 0;
    }
#endif

    // Announce which variation this is.
    if (!*(is_quiet))
    {
        printf("%s", (GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) ==
                         FCS_SEQ_BUILT_BY_SUIT)
                         ? "Seahaven; "
                         : "Freecell; ");
        if (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY)
        {
            printf("%s", "only Kings are allowed to start a pile.\n");
        }
        else
        {
            printf("%s", "any card may start a pile.\n");
        }
        printf("%d work piles, %d temp cells.\n", LOCAL_STACKS_NUM,
            LOCAL_FREECELLS_NUM);
    }

    *argc_ptr = argc;
    *argv_ptr = argv;
}
