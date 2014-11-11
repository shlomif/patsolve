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

static char *Progname = NULL;


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

static void GCC_INLINE trace_solution(fcs_pats_thread_t * soft_thread, FILE * out)
{
    fcs_pats_position_t *pos = soft_thread->win_pos;

    int i, nmoves;
    fcs_pats_position_t *p;
    fcs_pats__move_t *mp, **mpp, **mpp0;

    /* Go back up the chain of parents and store the moves
    in reverse order. */

    i = 0;
    for (p = pos; p->parent; p = p->parent) {
        i++;
    }
    nmoves = i;
    mpp0 = fc_solve_pats__new_array(soft_thread, fcs_pats__move_t *, nmoves);
    if (mpp0 == NULL) {
        return; /* how sad, so close... */
    }
    mpp = mpp0 + nmoves - 1;
    for (p = pos; p->parent; p = p->parent) {
        *mpp-- = &p->move;
    }

    /* Now print them out in the correct order. */

    for (i = 0, mpp = mpp0; i < nmoves; i++, mpp++) {
        mp = *mpp;
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
    fc_solve_pats__free_array(soft_thread, mpp0, fcs_pats__move_t *, nmoves);

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

static void fc_solve_pats__configure_soft_thread(
    fcs_pats_thread_t * soft_thread,
    fc_solve_instance_t * instance,
    int * argc_ptr,
    char * * * argv_ptr
)
{
    int argc = *argc_ptr;
    char * * argv = *argv_ptr;

    fc_solve_pats__init_soft_thread(soft_thread, instance);
    /* Default variation. */
    instance->game_params.game_flags = 0;
    instance->game_params.game_flags |= FCS_SEQ_BUILT_BY_ALTERNATE_COLOR;
    instance->game_params.game_flags |= FCS_ES_FILLED_BY_ANY_CARD << 2;
    INSTANCE_DECKS_NUM = 1;
    INSTANCE_STACKS_NUM = 10;
    INSTANCE_FREECELLS_NUM = 4;

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
                INSTANCE_STACKS_NUM = 10;
                INSTANCE_FREECELLS_NUM = 4;
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
                INSTANCE_STACKS_NUM = 8;
                INSTANCE_FREECELLS_NUM = 4;
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
                INSTANCE_STACKS_NUM = atoi(curr_arg);
                curr_arg = NULL;
                break;

            case 't':
                INSTANCE_FREECELLS_NUM = atoi(curr_arg);
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

int main(int argc, char **argv)
{
    u_int64_t gn;
    const int start_game_idx = get_idx_from_env("PATSOLVE_START");         /* for range solving */
    const int end_game_idx = get_idx_from_env("PATSOLVE_END");

    fcs_pats_thread_t soft_thread_struct__dont_use_directly;
    fcs_pats_thread_t * soft_thread = &soft_thread_struct__dont_use_directly;

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
#ifdef DEBUG
    Init_mem_remain = soft_thread->remaining_memory;
#endif
    if (start_game_idx < 0) {

        /* Read in the initial layout and play it. */

        char board_string[4096];
        memset(board_string, '\0', sizeof(board_string));
        fread(board_string, sizeof(board_string[0]), COUNT(board_string)-1, infile);
        fc_solve_pats__read_layout(soft_thread, board_string);
        if (!soft_thread->is_quiet) {
            fc_solve_pats__print_layout(soft_thread);
        }
        fc_solve_pats__play(soft_thread);

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
        fc_solve_pats__recycle_soft_thread(soft_thread);
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
    }

    return ((int)(soft_thread->status));
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
