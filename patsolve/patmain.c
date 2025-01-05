// This file is part of patsolve. It is subject to the license terms in
// the LICENSE file found in the top-level directory of this distribution
// and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
// part of patsolve, including this file, may be copied, modified, propagated,
// or distributed except according to the terms contained in the COPYING file.
//
// Copyright (c) 2002 Tom Holroyd
// Main().  Parse args, read the position, and call the solver.
#include "pat.h"
#include "range_solvers_gen_ms_boards.h"

#include "print_layout.h"
#include "print_card.h"
#include "read_layout.h"
#include "pats__play.h"
#include "pats__print_msg.h"

static const char Usage[] =
    "usage: %s [-s|f] [-k|a] [-w<n>] [-t<n>] [-E] [-S] [-q|v] [layout]\n"
    "-s Seahaven (same suit), -f Freecell (red/black)\n"
    "-k only Kings start a pile, -a any card starts a pile\n"
    "-w<n> number of work piles, -t<n> number of free cells\n"
    "-E don't exit after one solution; continue looking for better ones\n"
    "-S speed mode; find a solution quickly, rather than a good solution\n"
    "-q quiet, -v verbose\n"
    "-s implies -aw10 -t4, -f implies -aw8 -t4\n";

static inline void trace_solution(
    fcs_pats_thread *const soft_thread, FILE *const out, const bool is_quiet)
{
    /* Go back up the chain of parents and store the moves
    in reverse order. */
    const_AUTO(num_moves, soft_thread->num_moves_to_win);
    const_SLOT(moves_to_win, soft_thread);
    const_AUTO(moves_end, moves_to_win + num_moves);

    for (var_PTR(move_ptr, moves_to_win); move_ptr < moves_end; move_ptr++)
    {
        fc_solve_pats__print_card(move_ptr->card, out);
        fputc(' ', out);
        switch (move_ptr->totype)
        {
        case FCS_PATS__TYPE_FREECELL:
            fprintf(out, "to temp\n");
            break;
        case FCS_PATS__TYPE_FOUNDATION:
            fprintf(out, "out\n");
            break;
        default:
            fprintf(out, "to ");
            if (fcs_card_is_empty(move_ptr->destcard))
            {
                fprintf(out, "empty pile");
            }
            else
            {
                fc_solve_pats__print_card(move_ptr->destcard, out);
            }
            fputc('\n', out);
            break;
        }
    }

    free(soft_thread->moves_to_win);
    soft_thread->moves_to_win = NULL;
    soft_thread->num_moves_to_win = 0;

    if (!is_quiet)
    {
        printf("A winner.\n");
        printf("%ld moves.\n", (long)num_moves);
#ifdef DEBUG
        printf(
            "%d positions generated.\n", soft_thread->num_states_in_collection);
        printf("%d unique positions.\n", soft_thread->num_checked_states);
        printf("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
    }
}

#include "read_state.h"
int main(int argc, char **argv)
{
    const long long start_board_idx =
        get_idx_from_env("PATSOLVE_START"); // for range solving
    const long long end_board_idx = get_idx_from_env("PATSOLVE_END");

    fcs_pats_thread soft_thread_struct__dont_use_directly;
    fcs_pats_thread *const soft_thread = &soft_thread_struct__dont_use_directly;

    fcs_instance instance_struct;
    bool is_quiet = false;
    fc_solve_pats__configure_soft_thread(soft_thread, &instance_struct, &argc,
        (const char ***)(&argv), &is_quiet);

    FILE *in_fh = stdin;
    if (argc && **argv != '-')
    {
        in_fh = fopen(*argv, "r");

        if (!in_fh)
        {
            fatalerr("Cannot open input file '%s' (for reading).", *argv);
        }
    }
    if (start_board_idx < 0)
    {
        // Read in the initial layout and play it.

        const fcs_user_state_str user_state = read_state(in_fh);
        fc_solve_pats__read_layout(soft_thread, user_state.s);
        if (!is_quiet)
        {
            fc_solve_pats__print_layout(soft_thread);
        }
        fc_solve_pats__play(soft_thread, is_quiet);
        const_AUTO(exit_code, (soft_thread->status));
        switch (exit_code)
        {
        case FCS_PATS__WIN: {
            FILE *const out = fopen("win", "w");
            if (!out)
            {
                fprintf(stderr, "%s\n", "Cannot open 'win' for writing.");
                exit(1);
            }
            trace_solution(soft_thread, out, is_quiet);
            fclose(out);
        }
        break;

        case FCS_PATS__FAIL:
            printf("%s\n", "Ran out of memory.");
            break;

        case FCS_PATS__NOSOL:
            printf("%s\n", "Failed to solve.");
            break;
        }
        fc_solve_pats__recycle_soft_thread(soft_thread);
        fc_solve_pats__destroy_soft_thread(soft_thread);

        return ((int)(exit_code));
    }
    else
    {
        fcs_state_string state_string;
        get_board__setup_string(state_string);
        // Range mode.  Play lots of consecutive games.
        for (long long board_num = start_board_idx; board_num < end_board_idx;
            ++board_num)
        {
            printf("#%ld\n", (long)board_num);
            get_board_l__without_setup(board_num, state_string);
            fc_solve_pats__read_layout(soft_thread, state_string);
            fc_solve_pats__play(soft_thread, is_quiet);
            switch (soft_thread->status)
            {
            case FCS_PATS__WIN:
                printf("#%ld - Won\n", (long)board_num);
                break;

            case FCS_PATS__FAIL:
                printf("#%ld - OutOfMem\n", (long)board_num);
                break;

            case FCS_PATS__NOSOL:
                printf("#%ld - Impossible\n", (long)board_num);
                break;
            }
            fc_solve_pats__recycle_soft_thread(soft_thread);
            fflush(stdout);
        }
        fc_solve_pats__destroy_soft_thread(soft_thread);

        return 0;
    }
}
