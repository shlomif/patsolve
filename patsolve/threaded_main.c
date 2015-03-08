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
#include <pthread.h>

#include "inline.h"
#include "count.h"
#include "portable_int64.h"
#include "portable_time.h"
#include "alloc_wrap.h"

#include "pat.h"
#include "tree.h"
#include "range_solvers_gen_ms_boards.h"
#include "state.h"

#include "print_layout.h"
#include "read_layout.h"
#include "pats__play.h"

/* Just print a message. */

static void print_msg(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
}

static const char Usage[] =
  "usage: %s [-s|f] [-k|a] [-w<n>] [-t<n>] [-E] [-S] [-q|v] [layout]\n"
  "-s Seahaven (same suit), -f Freecell (red/black)\n"
  "-k only Kings start a pile, -a any card starts a pile\n"
  "-w<n> number of work piles, -t<n> number of free cells\n"
  "-E don't exit after one solution; continue looking for better ones\n"
  "-S speed mode; find a solution quickly, rather than a good solution\n"
  "-q quiet, -v verbose\n"
  "-s implies -aw10 -t4, -f implies -aw8 -t4\n";
#define USAGE() print_msg(Usage, Progname)


#ifdef DEBUG
long Init_mem_remain;
#endif

static const char *Progname = NULL;

static GCC_INLINE void pats__recycle_soft_thread(
    fcs_pats_thread_t * soft_thread
)
{
    fc_solve_pats__free_buckets(soft_thread);
    fc_solve_pats__free_clusters(soft_thread);
    fc_solve_pats__free_blocks(soft_thread);
    soft_thread->freed_positions = NULL;
}

/* Print a message and exit. */
static void fatalerr(const char *msg, ...)
{
    va_list ap;

    if (Progname) {
        fprintf(stderr, "%s: ", Progname);
    }
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);

    exit(1);
}


static void set_param(fcs_pats_thread_t * soft_thread, int pnum)
{
    soft_thread->pats_solve_params = freecell_solver_pats__x_y_params_preset[pnum];
    soft_thread->cutoff = soft_thread->pats_solve_params.x[FC_SOLVE_PATS__NUM_X_PARAM - 1];
}


#ifdef DEBUG
#ifdef HANDLE_SIG_QUIT
static void quit(fcs_pats_thread_t * soft_thread, int sig)
{
    int i, c;

    fc_solve_pats__print_queue(soft_thread);
    c = 0;
    for (i = 0; i <= 0xFFFF; i++) {
        if (soft_thread->num_positions_in_clusters[i]) {
            print_msg("%04X: %6d", i, soft_thread->num_positions_in_clusters[i]);
            c++;
            if (c % 5 == 0) {
                c = 0;
                print_msg("\n");
            } else {
                print_msg("\t");
            }
        }
    }
    if (c != 0) {
        print_msg("\n");
    }
    fc_solve_pats__print_layout(soft_thread);

#ifdef HANDLE_SIG_QUIT
    signal(SIGQUIT, quit);
#endif
}
#endif

#endif

/* Read a layout file.  Format is one pile per line, bottom to top (visible
card).  Temp cells and Out on the last two lines, if any. */

static GCC_INLINE fcs_bool_t str_fgets(char * line, const int len, char * * input_s)
{
    char * end = strchr(*input_s, '\n');
    if (! end)
    {
        end = *input_s + strlen(*input_s);
    }

    if (end - (*input_s) >= len)
    {
        end = (*input_s) + len-1;
    }

    fcs_bool_t ret = ((*input_s) != end);
    if (ret)
    {
        strncpy(line, *input_s, end-(*input_s));
        line[end-(*input_s)] = '\0';
    }
    else
    {
        line[0] = '\0';
    }

    (*input_s) = (((*end) == '\0') ? end : end+1);

    return ret;
}


static int freecell_solver_user_set_sequences_are_built_by_type(
    fc_solve_instance_t * instance,
    int sequences_are_built_by
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

#if 0
static int freecell_solver_user_set_sequence_move(
    fc_solve_instance_t * instance,
    int unlimited_sequence_move
    )
{
#ifndef FCS_FREECELL_ONLY
    instance->game_params.game_flags &= (~(1 << 4));
    instance->game_params.game_flags |=
        ((unlimited_sequence_move != 0)<< 4);

#endif
    return 0;
}
#endif

static int freecell_solver_user_set_empty_stacks_filled_by(
    fc_solve_instance_t * instance,
    int empty_stacks_fill
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

static void fc_solve_pats__configure_soft_thread(
    fcs_pats_thread_t * soft_thread,
    int * argc_ptr,
    const char * * * argv_ptr
)
{
    int argc = *argc_ptr;
    const char * * argv = *argv_ptr;

    soft_thread->is_quiet = FALSE;      /* print entertaining messages, else exit(Status); */
    soft_thread->Noexit = FALSE;
    soft_thread->to_stack = FALSE;
    soft_thread->cutoff = 1;
    soft_thread->remaining_memory = (50 * 1000 * 1000);
    soft_thread->freed_positions = NULL;
    /* Default variation. */

    typeof (soft_thread->instance) instance = soft_thread->instance;

#ifndef FCS_FREECELL_ONLY
    instance->game_params.game_flags = 0;
    instance->game_params.game_flags |= FCS_SEQ_BUILT_BY_ALTERNATE_COLOR;
    instance->game_params.game_flags |= FCS_ES_FILLED_BY_ANY_CARD << 2;
    INSTANCE_STACKS_NUM = 10;
    INSTANCE_FREECELLS_NUM = 4;
#endif

    Progname = *argv;
#ifdef DEBUG
#ifdef HANDLE_SIG_QUIT
    signal(SIGQUIT, quit);
#endif
    print_msg("sizeof(POSITION) = %d\n", sizeof(POSITION));
#endif

    /* Parse args twice.  Once to get the operating mode, and the
    next for other options. */

    typeof(argc) argc0 = argc;
    typeof(argv) argv0 = argv;
    const char * curr_arg;
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
                    if (i < 0 || i > LastParam) {
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

    instance->game_variant_suit_mask = FCS_PATS__COLOR;
    instance->game_variant_desired_suit_value = FCS_PATS__COLOR;
    if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT)) {
        instance->game_variant_suit_mask = FCS_PATS__SUIT;
        instance->game_variant_desired_suit_value = 0;
    }

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

static GCC_INLINE int get_idx_from_env(const char * name)
{
    const char * s = getenv(name);
    if (!s)
    {
        return -1;
    }
    else
    {
        return atoi(s);
    }
}

static const pthread_mutex_t initial_mutex_constant =
    PTHREAD_MUTEX_INITIALIZER
    ;

static int next_board_num;
static pthread_mutex_t next_board_num_lock;

fcs_int64_t total_num_iters = 0;
static pthread_mutex_t total_num_iters_lock;

typedef struct {
    int argc;
    char * * argv;
    int arg;
    int stop_at;
    int end_board;
    int board_num_step;
    int update_total_num_iters_threshold;
} context_t;

typedef struct
{
    fcs_pats_thread_t soft_thread_struct__dont_use_directly;
    fc_solve_instance_t instance_struct;
} pack_item_t;

static void * worker_thread(void * void_context)
{
    /* 52 cards of 3 chars (suit+rank+whitespace) each,
     * plus 8 newlines, plus one '\0' terminator*/

    const context_t * const context = (const context_t * const)void_context;

    pack_item_t user;
    fcs_pats_thread_t * soft_thread = &user.soft_thread_struct__dont_use_directly;
    soft_thread->instance = &(user.instance_struct);

    int argc = context->argc;
    char * * argv = context->argv;

    fc_solve_pats__configure_soft_thread(
        soft_thread,
        &argc,
        (const char * * *)(&argv)
    );

    int board_num;
    const int end_board = context->end_board;
    const int board_num_step = context->board_num_step;
    const int update_total_num_iters_threshold = context->update_total_num_iters_threshold;
    const int past_end_board = end_board+1;
    fcs_portable_time_t mytime;
    fcs_int_limit_t total_num_iters_temp = 0;
    const int stop_at = context->stop_at;
    do
    {
        pthread_mutex_lock(&next_board_num_lock);
        board_num = next_board_num;
        const int proposed_quota_end = (next_board_num += board_num_step);
        pthread_mutex_unlock(&next_board_num_lock);

        const int quota_end = min(proposed_quota_end, past_end_board);

        for ( ; board_num < quota_end ; board_num++ )
        {
            fcs_state_string_t board_string;
            get_board(board_num, board_string);

            fc_solve_pats__read_layout(soft_thread, board_string);
            fc_solve_pats__play(soft_thread);
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
                fcs_int64_t total_num_iters_copy;

                pthread_mutex_lock(&total_num_iters_lock);
                total_num_iters_copy = (total_num_iters += total_num_iters_temp);
                pthread_mutex_unlock(&total_num_iters_lock);
                total_num_iters_temp = 0;

                FCS_PRINT_REACHED_BOARD(
                    mytime,
                    board_num,
                    total_num_iters_copy
                );
                fflush(stdout);
            }

            pats__recycle_soft_thread(soft_thread);
        }
    } while (board_num <= end_board);

    pthread_mutex_lock(&total_num_iters_lock);
    total_num_iters += total_num_iters_temp;
    pthread_mutex_unlock(&total_num_iters_lock);

    return NULL;
}

int main(int argc, char **argv)
{
    next_board_num_lock = initial_mutex_constant;
    total_num_iters_lock = initial_mutex_constant;
    const int start_game_idx = get_idx_from_env("PATSOLVE_START");         /* for range solving */
    const int end_game_idx = get_idx_from_env("PATSOLVE_END");

#ifdef DEBUG
    Init_mem_remain = soft_thread->remaining_memory;
#endif
    {
        int board_num_step = 1;
        int update_total_num_iters_threshold = 1000000;
        int num_workers = 4;
        int stop_at = 100;

        next_board_num = start_game_idx;

        fcs_portable_time_t mytime;
        FCS_PRINT_STARTED_AT(mytime);
        context_t context = {.argc = argc, .argv = argv,
            .stop_at = stop_at, .end_board = end_game_idx,
            .board_num_step = board_num_step,
            .update_total_num_iters_threshold = update_total_num_iters_threshold,
        };

        pthread_t * const workers = SMALLOC(workers, num_workers);
        for ( int idx = 0 ; idx < num_workers ; idx++)
        {
            const int check = pthread_create(
                &workers[idx],
                NULL,
                worker_thread,
                &context
            );
            if (check)
            {
                fprintf(stderr,
                    "Worker Thread No. %d Initialization failed with error %d!\n",
                    idx, check
                );
                exit(-1);
            }
        }

        /* Wait for all threads to finish. */
        for( int idx = 0 ; idx < num_workers ; idx++)
        {
            pthread_join(workers[idx], NULL);
        }

        FCS_PRINT_FINISHED(mytime, total_num_iters);

        free(workers);
    }

    return 0;
}

#if 0
void print_move(MOVE *mp)
{
  fc_solve_pats__print_card(mp->card, stderr);
  if (mp->totype == T_TYPE) {
   print_msg("to temp (%d)\n", mp->pri);
  } else if (mp->totype == O_TYPE) {
   print_msg("out (%d)\n", mp->pri);
  } else {
   print_msg("to ");
   if (mp->destcard == NONE) {
    print_msg("empty pile (%d)", mp->pri);
   } else {
    fc_solve_pats__print_card(mp->destcard, stderr);
    print_msg("(%d)", mp->pri);
   }
   fputc('\n', stderr);
  }
  fc_solve_pats__print_layout(soft_thread);
}
#endif
