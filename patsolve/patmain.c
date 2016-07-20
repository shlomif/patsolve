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

static GCC_INLINE void trace_solution(
    fcs_pats_thread_t *const soft_thread, FILE *const out)
{
    /* Go back up the chain of parents and store the moves
    in reverse order. */

    const typeof(soft_thread->num_moves_to_win) nmoves =
        soft_thread->num_moves_to_win;
    const typeof(soft_thread->moves_to_win) mpp0 = soft_thread->moves_to_win;
    const typeof(mpp0) mp_end = mpp0 + nmoves;

    for (const typeof(*mpp0) *mp = mpp0; mp < mp_end; mp++)
    {
        fc_solve_pats__print_card(mp->card, out);
        fputc(' ', out);
        if (mp->totype == FCS_PATS__TYPE_FREECELL)
        {
            fprintf(out, "to temp\n");
        }
        else if (mp->totype == FCS_PATS__TYPE_FOUNDATION)
        {
            fprintf(out, "out\n");
        }
        else
        {
            fprintf(out, "to ");
            if (mp->destcard == fc_solve_empty_card)
            {
                fprintf(out, "empty pile");
            }
            else
            {
                fc_solve_pats__print_card(mp->destcard, out);
            }
            fputc('\n', out);
        }
    }

    free(soft_thread->moves_to_win);
    soft_thread->moves_to_win = NULL;
    soft_thread->num_moves_to_win = 0;

    if (!soft_thread->is_quiet)
    {
        printf("A winner.\n");
        printf("%d moves.\n", nmoves);
#ifdef DEBUG
        printf(
            "%d positions generated.\n", soft_thread->num_states_in_collection);
        printf("%d unique positions.\n", soft_thread->num_checked_states);
        printf("remaining_memory = %ld\n", soft_thread->remaining_memory);
#endif
    }
}

int main(int argc, char **argv)
{
    const long long start_game_idx =
        get_idx_from_env("PATSOLVE_START"); /* for range solving */
    const long long end_game_idx = get_idx_from_env("PATSOLVE_END");

    fcs_pats_thread_t soft_thread_struct__dont_use_directly;
    fcs_pats_thread_t *const soft_thread =
        &soft_thread_struct__dont_use_directly;

    fc_solve_instance_t instance_struct;

    fc_solve_pats__configure_soft_thread(
        soft_thread, &instance_struct, &argc, (const char ***)(&argv));

    FILE *in_fh = stdin;
    fcs_bool_t in_fh_should_be_closed = FALSE;
    if (argc && **argv != '-')
    {
        in_fh = fopen(*argv, "r");

        if (!in_fh)
        {
            fatalerr("Cannot open input file '%s' (for reading).", *argv);
        }
        else
        {
            in_fh_should_be_closed = TRUE;
        }
    }
    if (start_game_idx < 0)
    {

        /* Read in the initial layout and play it. */

        char board_string[4096];
        memset(board_string, '\0', sizeof(board_string));
        fread(board_string, sizeof(board_string[0]), COUNT(board_string) - 1,
            in_fh);
        if (in_fh_should_be_closed)
        {
            fclose(in_fh);
            in_fh = NULL;
            in_fh_should_be_closed = FALSE;
        }
        fc_solve_pats__read_layout(soft_thread, board_string);
        if (!soft_thread->is_quiet)
        {
            fc_solve_pats__print_layout(soft_thread);
        }
#if 0
        fc_solve_pats__play(soft_thread);
#else
        fc_solve_pats__before_play(soft_thread);

        do
        {
            soft_thread->max_num_checked_states =
                soft_thread->num_checked_states + 50;
            soft_thread->status = FCS_PATS__NOSOL;
            fc_solve_pats__do_it(soft_thread);
        } while (soft_thread->status == FCS_PATS__FAIL);
#endif
        if (soft_thread->status == FCS_PATS__WIN)
        {
            FILE *out = fopen("win", "w");
            if (!out)
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

        for (u_int64_t game_num = start_game_idx; game_num < end_game_idx;
             game_num++)
        {
            printf("#%ld\n", (long)game_num);
            char board_string[4096];
            memset(board_string, '\0', sizeof(board_string));
            get_board_l(game_num, board_string);
            fc_solve_pats__read_layout(soft_thread, board_string);
            fc_solve_pats__play(soft_thread);
            fc_solve_pats__recycle_soft_thread(soft_thread);
            fflush(stdout);
        }
        fc_solve_pats__destroy_soft_thread(soft_thread);

        return 0;
    }
}
