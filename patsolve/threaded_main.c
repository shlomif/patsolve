// This file is part of patsolve. It is subject to the license terms in
// the LICENSE file found in the top-level directory of this distribution
// and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
// part of patsolve, including this file, may be copied, modified, propagated,
// or distributed except according to the terms contained in the COPYING file.
//
// Copyright (c) 2002 Tom Holroyd
#include <stdarg.h>
#include <pthread.h>

#include "rinutils/portable_time.h"

#include "pat.h"
#include "range_solvers_gen_ms_boards.h"

#include "print_layout.h"
#include "read_layout.h"
#include "pats__play.h"
#include "pats__print_msg.h"
#include "print_time.h"

static const char Usage[] =
    "SYNOPSIS:\n"
    "\n"
    "    PATSOLVE_START=1 PATSOLVE_END=32000 threaded-pats -f -S\n"
    "\n"
    "Usage: %s [-s|f] [-k|a] [-w<n>] [-t<n>] [-E] [-S] [-q|v] [layout]\n"
    "-s Seahaven (same suit), -f Freecell (red/black)\n"
    "-k only Kings start a pile, -a any card starts a pile\n"
    "-w<n> number of work piles, -t<n> number of free cells\n"
    "-E don't exit after one solution; continue looking for better ones\n"
    "-S speed mode; find a solution quickly, rather than a good solution\n"
    "-q quiet, -v verbose\n"
    "-s implies -aw10 -t4, -f implies -aw8 -t4\n";

static const pthread_mutex_t initial_mutex_constant = PTHREAD_MUTEX_INITIALIZER;

static long long next_board_num;
long long end_board_idx, past_end_board;
static pthread_mutex_t next_board_num_lock;

long long total_num_iters = 0;
static pthread_mutex_t total_num_iters_lock;
static fcs_int_limit_t update_total_num_iters_threshold = 1000000;
const long long board_num_step = 32;
const long long stop_at = 100;
int context_argc;
char **context_argv;

static void *worker_thread(void *context)
{
    fcs_pats_thread soft_thread_struct__dont_use_directly;
    fcs_pats_thread *const soft_thread = &soft_thread_struct__dont_use_directly;

    int argc = context_argc;
    char **argv = context_argv;

    fcs_instance instance_struct;
    bool is_quiet = false;
    fc_solve_pats__configure_soft_thread(soft_thread, &(instance_struct), &argc,
        (const char ***)(&argv), &is_quiet);

    long long board_num;
    fcs_int_limit_t total_num_iters_temp = 0;
    fcs_state_string state_string;
    get_board__setup_string(state_string);
    do
    {
        pthread_mutex_lock(&next_board_num_lock);
        board_num = next_board_num;
        const long long proposed_quota_end = (next_board_num += board_num_step);
        pthread_mutex_unlock(&next_board_num_lock);

        const long long quota_end = min(proposed_quota_end, past_end_board);

        for (; board_num < quota_end; ++board_num)
        {
            get_board_l__without_setup(board_num, state_string);

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
    if (argc > 1 && ((!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help"))))
    {
        USAGE();
        exit(0);
    }
    next_board_num_lock = initial_mutex_constant;
    total_num_iters_lock = initial_mutex_constant;
    next_board_num = get_idx_from_env("PATSOLVE_START");
    past_end_board = (end_board_idx = get_idx_from_env("PATSOLVE_END")) + 1;

    int num_workers = 4;
    fc_solve_print_started_at();
    context_argc = argc;
    context_argv = argv;
    pthread_t workers[num_workers];
    for (int idx = 0; idx < num_workers; idx++)
    {
        const int check =
            pthread_create(&workers[idx], NULL, worker_thread, NULL);
        if (check)
        {
            fprintf(stderr,
                "Worker Thread No. %d Initialization failed "
                "with error %d!\n",
                idx, check);
            exit(-1);
        }
    }

    // Wait for all threads to finish.
    for (int idx = 0; idx < num_workers; idx++)
    {
        pthread_join(workers[idx], NULL);
    }
    fc_solve_print_finished(total_num_iters);

    return 0;
}
