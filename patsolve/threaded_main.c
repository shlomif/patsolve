/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
/* Main().  Parse args, read the position, and call the solver. */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>

#include "count.h"
#include "portable_time.h"
#include "alloc_wrap.h"

#include "pat.h"
#include "tree.h"
#include "range_solvers_gen_ms_boards.h"
#include "state.h"

#include "print_layout.h"
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

static const pthread_mutex_t initial_mutex_constant = PTHREAD_MUTEX_INITIALIZER;

static long long next_board_num;
static pthread_mutex_t next_board_num_lock;

long long total_num_iters = 0;
static pthread_mutex_t total_num_iters_lock;

typedef struct
{
    int argc;
    char **argv;
    int arg;
    int stop_at;
    int end_board_idx;
    int board_num_step;
    int update_total_num_iters_threshold;
} context_t;

static void *worker_thread(void *const void_context)
{
    const context_t *const context = (const context_t *const)void_context;

    fcs_pats_thread_t soft_thread_struct__dont_use_directly;
    fcs_pats_thread_t *const soft_thread =
        &soft_thread_struct__dont_use_directly;

    int argc = context->argc;
    char **argv = context->argv;

    fc_solve_instance_t instance_struct;
    fcs_bool_t is_quiet = FALSE;
    fc_solve_pats__configure_soft_thread(soft_thread, &(instance_struct), &argc,
        (const char ***)(&argv), &is_quiet);

    long long board_num;
    const int end_board_idx = context->end_board_idx;
    const int board_num_step = context->board_num_step;
    const int update_total_num_iters_threshold =
        context->update_total_num_iters_threshold;
    const int past_end_board = end_board_idx + 1;
    fcs_int_limit_t total_num_iters_temp = 0;
    const int stop_at = context->stop_at;
    do
    {
        pthread_mutex_lock(&next_board_num_lock);
        board_num = next_board_num;
        const int proposed_quota_end = (next_board_num += board_num_step);
        pthread_mutex_unlock(&next_board_num_lock);

        const int quota_end = min(proposed_quota_end, past_end_board);

        for (; board_num < quota_end; board_num++)
        {
            fcs_state_string_t state_string;
            get_board(board_num, state_string);

            fc_solve_pats__read_layout(soft_thread, state_string);
            fc_solve_pats__play(soft_thread, is_quiet);
            fflush(stdout);

            total_num_iters_temp += soft_thread->num_checked_states;
            if (total_num_iters_temp >= update_total_num_iters_threshold)
            {
                pthread_mutex_lock(&total_num_iters_lock);
                total_num_iters += total_num_iters_temp;
                pthread_mutex_unlock(&total_num_iters_lock);
                total_num_iters_temp = 0;
            }

            if (board_num % stop_at == 0)
            {
                long long total_num_iters_copy;

                pthread_mutex_lock(&total_num_iters_lock);
                total_num_iters_copy =
                    (total_num_iters += total_num_iters_temp);
                pthread_mutex_unlock(&total_num_iters_lock);
                total_num_iters_temp = 0;

                fc_solve_print_reached(board_num, total_num_iters_copy);
            }

            fc_solve_pats__recycle_soft_thread(soft_thread);
        }
    } while (board_num <= end_board_idx);

    pthread_mutex_lock(&total_num_iters_lock);
    total_num_iters += total_num_iters_temp;
    pthread_mutex_unlock(&total_num_iters_lock);

    return NULL;
}

int main(int argc, char **argv)
{
    next_board_num_lock = initial_mutex_constant;
    total_num_iters_lock = initial_mutex_constant;
    const long long start_game_idx =
        get_idx_from_env("PATSOLVE_START"); /* for range solving */
    const long long end_game_idx = get_idx_from_env("PATSOLVE_END");

    {
        int board_num_step = 1;
        int update_total_num_iters_threshold = 1000000;
        int num_workers = 4;
        int stop_at = 100;

        next_board_num = start_game_idx;

        fc_solve_print_started_at();
        context_t context = {
            .argc = argc,
            .argv = argv,
            .stop_at = stop_at,
            .end_board_idx = end_game_idx,
            .board_num_step = board_num_step,
            .update_total_num_iters_threshold =
                update_total_num_iters_threshold,
        };

        pthread_t *const workers = SMALLOC(workers, num_workers);
        for (int idx = 0; idx < num_workers; idx++)
        {
            const int check =
                pthread_create(&workers[idx], NULL, worker_thread, &context);
            if (check)
            {
                fprintf(stderr, "Worker Thread No. %d Initialization failed "
                                "with error %d!\n",
                    idx, check);
                exit(-1);
            }
        }

        /* Wait for all threads to finish. */
        for (int idx = 0; idx < num_workers; idx++)
        {
            pthread_join(workers[idx], NULL);
        }
        fc_solve_print_finished(total_num_iters);
        free(workers);
    }

    return 0;
}
