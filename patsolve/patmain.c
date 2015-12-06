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
/* Main().  Parse args, read the position, and call the solver. */

#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>

#include "inline.h"
#include "count.h"

#include "pat.h"
#include "tree.h"
#include "range_solvers_gen_ms_boards.h"
#include "state.h"

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

static GCC_INLINE void trace_solution(fcs_pats_thread_t * const soft_thread, FILE * const out)
{
    /* Go back up the chain of parents and store the moves
    in reverse order. */

    const typeof(soft_thread->num_moves_to_win) nmoves = soft_thread->num_moves_to_win;
    const typeof(soft_thread->moves_to_win) mpp0 = soft_thread->moves_to_win;
    const typeof(mpp0) mp_end = mpp0 + nmoves;

    for (const typeof(*mpp0) * mp = mpp0; mp < mp_end; mp++) {
        fc_solve_pats__print_card(mp->card, out);
        fputc(' ', out);
        if (mp->totype == FCS_PATS__TYPE_FREECELL) {
            fprintf(out, "to temp\n");
        } else if (mp->totype == FCS_PATS__TYPE_FOUNDATION) {
            fprintf(out, "out\n");
        } else {
            fprintf(out, "to ");
            if (mp->destcard == fc_solve_empty_card) {
                fprintf(out, "empty pile");
            } else {
                fc_solve_pats__print_card(mp->destcard, out);
            }
            fputc('\n', out);
        }
    }

    free (soft_thread->moves_to_win);
    soft_thread->moves_to_win = NULL;
    soft_thread->num_moves_to_win = 0;

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

static GCC_INLINE void fc_solve_pats__configure_soft_thread(
    fcs_pats_thread_t * const soft_thread,
    fc_solve_instance_t * const instance,
    int * const argc_ptr,
    char * * * const argv_ptr
)
{
    int argc = *argc_ptr;
    char * * argv = *argv_ptr;

    fc_solve_pats__init_soft_thread(soft_thread, instance);
    /* Default variation. */
#ifndef FCS_FREECELL_ONLY
    instance->game_params.game_flags = 0;
    instance->game_params.game_flags |= FCS_SEQ_BUILT_BY_ALTERNATE_COLOR;
    instance->game_params.game_flags |= FCS_ES_FILLED_BY_ANY_CARD << 2;
    INSTANCE_DECKS_NUM = 1;
    INSTANCE_STACKS_NUM = 10;
    INSTANCE_FREECELLS_NUM = 4;
#endif

    Progname = *argv;

    /* Parse args twice.  Once to get the operating mode, and the
    next for other options. */

    typeof(argc) argc0 = argc;
    typeof(argv) argv0 = argv;
    char * curr_arg;
    while (--argc > 0 && **++argv == '-' && *(curr_arg = 1 + *argv)) {

        /* Scan current argument until a flag indicates that the rest
        of the argument isn't flags (curr_arg = NULL), or until
        the end of the argument is reached (if it is all flags). */

        int c;
        while (curr_arg != NULL && (c = *curr_arg++) != '\0') {
            switch (c) {

            case 's':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance,
                    FCS_ES_FILLED_BY_ANY_CARD
                );
                freecell_solver_user_set_sequences_are_built_by_type(
                    instance,
                    FCS_SEQ_BUILT_BY_SUIT
                );
#ifndef FCS_FREECELL_ONLY
                INSTANCE_STACKS_NUM = 10;
                INSTANCE_FREECELLS_NUM = 4;
#endif
                break;

            case 'f':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance,
                    FCS_ES_FILLED_BY_ANY_CARD
                );
                freecell_solver_user_set_sequences_are_built_by_type(
                    instance,
                    FCS_SEQ_BUILT_BY_ALTERNATE_COLOR
                );
#ifndef FCS_FREECELL_ONLY
                INSTANCE_STACKS_NUM = 8;
                INSTANCE_FREECELLS_NUM = 4;
#endif
                break;

            case 'k':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance,
                    FCS_ES_FILLED_BY_KINGS_ONLY
                );
                break;

            case 'a':
                freecell_solver_user_set_empty_stacks_filled_by(
                    instance,
                    FCS_ES_FILLED_BY_ANY_CARD
                );
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

    /* Set parameters. */

    if (!(GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && !soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__FreecellBest);
    } else if (!(GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__FreecellSpeed);
    } else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && !soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__SeahavenBest);
    } else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && !(INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__SeahavenSpeed);
    } else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && !soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__SeahavenKing);
    } else if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) && (INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY) && soft_thread->to_stack) {
        set_param(soft_thread, FC_SOLVE_PATS__PARAM_PRESET__SeahavenKingSpeed);
    } else {
        set_param(soft_thread, 0);   /* default */
    }

    /* Now get the other args, and allow overriding the parameters. */

    argc = argc0;
    argv = argv0;
    int c;
    while (--argc > 0 && **++argv == '-' && *(curr_arg = 1 + *argv)) {
        while (curr_arg != NULL && (c = *curr_arg++) != '\0') {
            switch (c) {

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
                soft_thread->Noexit = TRUE;
                break;

            case 'c':
                soft_thread->cutoff = atoi(curr_arg);
                curr_arg = NULL;
                break;

            case 'M':
                soft_thread->remaining_memory = atol(curr_arg) * 1000000;
                curr_arg = NULL;
                break;

            case 'v':
                soft_thread->is_quiet = FALSE;
                break;

            case 'q':
                soft_thread->is_quiet = TRUE;
                break;

            case 'X':

                /* use -c for the last X param */

                for (int i = 0; i < FC_SOLVE_PATS__NUM_X_PARAM - 1; i++) {
                    soft_thread->pats_solve_params.x[i] = atoi(argv[i + 1]);
                }
                argv += FC_SOLVE_PATS__NUM_X_PARAM - 1;
                argc -= FC_SOLVE_PATS__NUM_X_PARAM - 1;
                curr_arg = NULL;
                break;

            case 'Y':
                for (int i = 0; i < FC_SOLVE_PATS__NUM_Y_PARAM; i++) {
                    soft_thread->pats_solve_params.y[i] = atof(argv[i + 1]);
                }
                argv += FC_SOLVE_PATS__NUM_Y_PARAM;
                argc -= FC_SOLVE_PATS__NUM_Y_PARAM;
                curr_arg = NULL;
                break;

            case 'P':
                {
                    int i = atoi(curr_arg);
                    if (i < 0 || i > FC_SOLVE_PATS__PARAM_PRESET__LastParam) {
                        fatalerr("invalid parameter code");
                    }
                    set_param(soft_thread, i);
                    curr_arg = NULL;
                }
                break;

            default:
                print_msg("%s: unknown flag -%c\n", Progname, c);
                USAGE();
                exit(1);
            }
        }
    }
#if !defined(HARD_CODED_NUM_STACKS) || !defined(HARD_CODED_NUM_FREECELLS)
    const fcs_game_type_params_t game_params = soft_thread->instance->game_params;
#endif

    if (soft_thread->to_stack && soft_thread->Noexit) {
        fatalerr("-S and -E may not be used together.");
    }
    if (soft_thread->remaining_memory < BLOCKSIZE * 2) {
        fatalerr("-M too small.");
    }
    if (LOCAL_STACKS_NUM > MAX_NUM_STACKS) {
        fatalerr("too many w piles (max %d)", MAX_NUM_STACKS);
    }
    if (LOCAL_FREECELLS_NUM > MAX_NUM_FREECELLS) {
        fatalerr("too many t piles (max %d)", MAX_NUM_FREECELLS);
    }

    /* Process the named file, or stdin if no file given.
    The name '-' also specifies stdin. */


    /* Initialize the suitable() macro variables. */

#ifndef FCS_FREECELL_ONLY
    instance->game_variant_suit_mask = FCS_PATS__COLOR;
    instance->game_variant_desired_suit_value = FCS_PATS__COLOR;
    if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT)) {
        instance->game_variant_suit_mask = FCS_PATS__SUIT;
        instance->game_variant_desired_suit_value = 0;
    }
#endif

    /* Announce which variation this is. */

    if (!soft_thread->is_quiet) {
        printf("%s", (GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT) ? "Seahaven; " : "Freecell; ");
        if ((INSTANCE_EMPTY_STACKS_FILL == FCS_ES_FILLED_BY_KINGS_ONLY)) {
            printf("%s", "only Kings are allowed to start a pile.\n");
        } else {
            printf("%s", "any card may start a pile.\n");
        }
        printf("%d work piles, %d temp cells.\n", LOCAL_STACKS_NUM, LOCAL_FREECELLS_NUM);
    }

    *argc_ptr = argc;
    *argv_ptr = argv;

    return;
}


int main(int argc, char **argv)
{
    u_int64_t gn;
    const int start_game_idx = get_idx_from_env("PATSOLVE_START");         /* for range solving */
    const int end_game_idx = get_idx_from_env("PATSOLVE_END");

    fcs_pats_thread_t soft_thread_struct__dont_use_directly;
    fcs_pats_thread_t * const soft_thread = &soft_thread_struct__dont_use_directly;

    fc_solve_instance_t instance_struct;

    fc_solve_pats__configure_soft_thread(
        soft_thread,
        &instance_struct,
        &argc,
        &argv
    );

    FILE * infile = stdin;
    if (argc && **argv != '-') {
        infile = fopen(*argv, "r");

        if (! infile)
        {
            fatalerr("Cannot open input file '%s' (for reading).", *argv);
        }
    }
    if (start_game_idx < 0) {

        /* Read in the initial layout and play it. */

        char board_string[4096];
        memset(board_string, '\0', sizeof(board_string));
        fread(board_string, sizeof(board_string[0]), COUNT(board_string)-1, infile);
        fc_solve_pats__read_layout(soft_thread, board_string);
        if (!soft_thread->is_quiet) {
            fc_solve_pats__print_layout(soft_thread);
        }
#if 0
        fc_solve_pats__play(soft_thread);
#else
        fc_solve_pats__before_play(soft_thread);

        do
        {
            soft_thread->max_num_checked_states = soft_thread->num_checked_states + 50;
            soft_thread->status = FCS_PATS__NOSOL;
            fc_solve_pats__do_it(soft_thread);
        } while (soft_thread->status == FCS_PATS__FAIL);
#endif
        if (soft_thread->status == FCS_PATS__WIN)
        {
            FILE * out = fopen("win", "w");
            if (! out)
            {
                fprintf(stderr, "%s\n", "Cannot open 'win' for writing.");
                exit(1);
            }
            trace_solution(soft_thread, out);

            fclose(out);
        }
        else if (soft_thread->status == FCS_PATS__FAIL)
        {
            printf("%s\n", "Ran out of memory.");
        }
        else if (soft_thread->status == FCS_PATS__NOSOL)
        {
            printf("%s\n", "Failed to solve.");
        }
        int ret = ((int)(soft_thread->status));
        fc_solve_pats__recycle_soft_thread(soft_thread);
        fc_solve_pats__destroy_soft_thread(soft_thread);

        return ret;
    }
    else
    {

        /* Range mode.  Play lots of consecutive games. */

        for (gn = start_game_idx; gn < end_game_idx; gn++) {
            printf("#%ld\n", (long)gn);
            char board_string[4096];
            memset(board_string, '\0', sizeof(board_string));
            get_board_l(gn, board_string);
            fc_solve_pats__read_layout(soft_thread, board_string);
            fc_solve_pats__play(soft_thread);
            fc_solve_pats__recycle_soft_thread(soft_thread);
            fflush(stdout);
        }
        fc_solve_pats__destroy_soft_thread(soft_thread);

        return 0;
    }
}

