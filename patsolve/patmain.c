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
#include "pat.h"
#include "tree.h"

#include "inline.h"

static const char Usage[] =
  "usage: %s [-s|f] [-k|a] [-w<n>] [-t<n>] [-E] [-S] [-q|v] [layout]\n"
  "-s Seahaven (same suit), -f Freecell (red/black)\n"
  "-k only Kings start a pile, -a any card starts a pile\n"
  "-w<n> number of work piles, -t<n> number of free cells\n"
  "-E don't exit after one solution; continue looking for better ones\n"
  "-S speed mode; find a solution quickly, rather than a good solution\n"
  "-q quiet, -v verbose\n"
  "-s implies -aw10 -t4, -f implies -aw8 -t4\n";
#define USAGE() fc_solve_msg(Usage, Progname)

int Quiet = FALSE;      /* print entertaining messages, else exit(Status); */

#if DEBUG
long Init_mem_remain;
#endif

static char *Progname = NULL;

/* Print a message and exit. */
static void fatalerr(char *msg, ...)
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

static void print_layout(fc_solve_soft_thread_t * soft_thread)
{
    int i, t, w, o;

    for (w = 0; w < soft_thread->Nwpiles; w++) {
        for (i = 0; i < soft_thread->Wlen[w]; i++) {
            fc_solve_pats__print_card(soft_thread->W[w][i], stderr);
        }
        fputc('\n', stderr);
    }
    for (t = 0; t < soft_thread->Ntpiles; t++) {
        fc_solve_pats__print_card(soft_thread->T[t], stderr);
    }
    fputc('\n', stderr);
    for (o = 0; o < 4; o++) {
        fc_solve_pats__print_card(soft_thread->O[o] + Osuit[o], stderr);
    }
    fprintf(stderr, "\n---\n");
}
void set_param(fc_solve_soft_thread_t * soft_thread, int pnum)
{
    const int *x;
    const double *y;
    int i;

    x = XYparam[pnum].x;
    y = XYparam[pnum].y;
    for (i = 0; i < NXPARAM; i++) {
        soft_thread->Xparam[i] = x[i];
    }
    soft_thread->cutoff = soft_thread->Xparam[NXPARAM - 1];
    for (i = 0; i < NYPARAM; i++) {
        soft_thread->Yparam[i] = y[i];
    }
}

#if DEBUG
extern int Clusternum[];

void quit(fc_solve_soft_thread_t * soft_thread, int sig)
{
    int i, c;
    extern void print_queue(fc_solve_soft_thread_t * soft_thread);

    print_queue(soft_thread);
    c = 0;
    for (i = 0; i <= 0xFFFF; i++) {
        if (soft_thread->Clusternum[i]) {
            fc_solve_msg("%04X: %6d", i, soft_thread->Clusternum[i]);
            c++;
            if (c % 5 == 0) {
                c = 0;
                fc_solve_msg("\n");
            } else {
                fc_solve_msg("\t");
            }
        }
    }
    if (c != 0) {
        fc_solve_msg("\n");
    }
    print_layout(soft_thread);

#if 0
    signal(SIGQUIT, quit);
#endif
}
#endif

void play(fc_solve_soft_thread_t * soft_thread);

/* Read a layout file.  Format is one pile per line, bottom to top (visible
card).  Temp cells and Out on the last two lines, if any. */

static GCC_INLINE void read_layout(fc_solve_soft_thread_t * soft_thread, FILE *infile)
{
    int w, i, total;
    char buf[100];
    card_t out[4];
    int parse_pile(char *s, card_t *w, int size);

    /* Read the workspace. */

    w = 0;
    total = 0;
    while (fgets(buf, 100, infile)) {
        i = parse_pile(buf, soft_thread->W[w], 52);
        soft_thread->Wp[w] = &soft_thread->W[w][i - 1];
        soft_thread->Wlen[w] = i;
        w++;
        total += i;
        if (w == soft_thread->Nwpiles) {
            break;
        }
    }
    if (w != soft_thread->Nwpiles) {
        fatalerr("not enough piles in input file");
    }

    /* Temp cells may have some cards too. */

    for (i = 0; i < soft_thread->Ntpiles; i++) {
        soft_thread->T[i] = NONE;
    }
    if (total != 52) {
        fgets(buf, 100, infile);
        total += parse_pile(buf, soft_thread->T, soft_thread->Ntpiles);
    }

    /* Output piles, if any. */

    for (i = 0; i < 4; i++) {
        soft_thread->O[i] = out[i] = NONE;
    }
    if (total != 52) {
        fgets(buf, 100, infile);
        parse_pile(buf, out, 4);
        for (i = 0; i < 4; i++) {
            if (out[i] != NONE) {
                total +=
                    (
                        soft_thread->O[fcs_pats_card_suit(out[i])]
                            = fcs_pats_card_rank(out[i])
                    );
            }
        }
    }

    if (total != 52) {
        fatalerr("not enough cards");
    }
}

int main(int argc, char **argv)
{
    int i, c, argc0;
    char *curr_arg, **argv0;
    u_int64_t gn;
    FILE *infile;
    int Sgame = -1;         /* for range solving */
    int Egame = -1;

    fc_solve_soft_thread_t soft_thread_struct;
    fc_solve_soft_thread_t * soft_thread;

    soft_thread = &soft_thread_struct;

    soft_thread->Interactive = TRUE;
    soft_thread->Noexit = FALSE;
    soft_thread->to_stack = FALSE;
    soft_thread->cutoff = 1;
    soft_thread->Mem_remain = (50 * 1000 * 1000);
    soft_thread->Freepos = NULL;
    /* Default variation. */
    soft_thread_struct.Same_suit = SAME_SUIT;
    soft_thread_struct.King_only = KING_ONLY;
    soft_thread_struct.Nwpiles = NWPILES;
    soft_thread_struct.Ntpiles = NTPILES;


    Progname = *argv;
#if DEBUG
#if 0
    signal(SIGQUIT, quit);
#endif
    fc_solve_msg("sizeof(POSITION) = %d\n", sizeof(POSITION));
#endif

    /* Parse args twice.  Once to get the operating mode, and the
    next for other options. */

    argc0 = argc;
    argv0 = argv;
    while (--argc > 0 && **++argv == '-' && *(curr_arg = 1 + *argv)) {

        /* Scan current argument until a flag indicates that the rest
        of the argument isn't flags (curr_arg = NULL), or until
        the end of the argument is reached (if it is all flags). */

        while (curr_arg != NULL && (c = *curr_arg++) != '\0') {
            switch (c) {

            case 's':
                soft_thread_struct.Same_suit = TRUE;
                soft_thread_struct.King_only = FALSE;
                soft_thread_struct.Nwpiles = 10;
                soft_thread_struct.Ntpiles = 4;
                break;

            case 'f':
                soft_thread_struct.Same_suit = FALSE;
                soft_thread_struct.King_only = FALSE;
                soft_thread_struct.Nwpiles = 8;
                soft_thread_struct.Ntpiles = 4;
                break;

            case 'k':
                soft_thread_struct.King_only = TRUE;
                break;

            case 'a':
                soft_thread_struct.King_only = FALSE;
                break;

            case 'S':
                soft_thread->to_stack = TRUE;
                break;

            case 'w':
                soft_thread_struct.Nwpiles = atoi(curr_arg);
                curr_arg = NULL;
                break;

            case 't':
                soft_thread_struct.Ntpiles = atoi(curr_arg);
                curr_arg = NULL;
                break;

            case 'X':
                argv += NXPARAM - 1;
                argc -= NXPARAM - 1;
                curr_arg = NULL;
                break;

            case 'Y':
                argv += NYPARAM;
                argc -= NYPARAM;
                curr_arg = NULL;
                break;

            default:
                break;
            }
        }
    }

    /* Set parameters. */

    if (!soft_thread_struct.Same_suit && !soft_thread_struct.King_only && !soft_thread->to_stack) {
        set_param(soft_thread, FreecellBest);
    } else if (!soft_thread_struct.Same_suit && !soft_thread_struct.King_only && soft_thread->to_stack) {
        set_param(soft_thread, FreecellSpeed);
    } else if (soft_thread_struct.Same_suit && !soft_thread_struct.King_only && !soft_thread->to_stack) {
        set_param(soft_thread, SeahavenBest);
    } else if (soft_thread_struct.Same_suit && !soft_thread_struct.King_only && soft_thread->to_stack) {
        set_param(soft_thread, SeahavenSpeed);
    } else if (soft_thread_struct.Same_suit && soft_thread_struct.King_only && !soft_thread->to_stack) {
        set_param(soft_thread, SeahavenKing);
    } else if (soft_thread_struct.Same_suit && soft_thread_struct.King_only && soft_thread->to_stack) {
        set_param(soft_thread, SeahavenKingSpeed);
    } else {
        set_param(soft_thread, 0);   /* default */
    }

    /* Now get the other args, and allow overriding the parameters. */

    argc = argc0;
    argv = argv0;
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
                soft_thread->Mem_remain = atol(curr_arg) * 1000000;
                curr_arg = NULL;
                break;

            case 'v':
                Quiet = FALSE;
                break;

            case 'q':
                Quiet = TRUE;
                break;

            case 'N':
                Sgame = atoi(curr_arg);
                Egame = atoi(argv[1]);
                curr_arg = NULL;
                argv++;
                argc--;
                break;

            case 'X':

                /* use -c for the last X param */

                for (i = 0; i < NXPARAM - 1; i++) {
                    soft_thread->Xparam[i] = atoi(argv[i + 1]);
                }
                argv += NXPARAM - 1;
                argc -= NXPARAM - 1;
                curr_arg = NULL;
                break;

            case 'Y':
                for (i = 0; i < NYPARAM; i++) {
                    soft_thread->Yparam[i] = atof(argv[i + 1]);
                }
                argv += NYPARAM;
                argc -= NYPARAM;
                curr_arg = NULL;
                break;

            case 'P':
                i = atoi(curr_arg);
                if (i < 0 || i > LastParam) {
                    fatalerr("invalid parameter code");
                }
                set_param(soft_thread, i);
                curr_arg = NULL;
                break;

            default:
                fc_solve_msg("%s: unknown flag -%c\n", Progname, c);
                USAGE();
                exit(1);
            }
        }
    }

    if (soft_thread->to_stack && soft_thread->Noexit) {
        fatalerr("-S and -E may not be used together.");
    }
    if (soft_thread->Mem_remain < BLOCKSIZE * 2) {
        fatalerr("-M too small.");
    }
    if (soft_thread->Nwpiles > MAXWPILES) {
        fatalerr("too many w piles (max %d)", MAXWPILES);
    }
    if (soft_thread->Ntpiles > MAXTPILES) {
        fatalerr("too many t piles (max %d)", MAXTPILES);
    }

    /* Process the named file, or stdin if no file given.
    The name '-' also specifies stdin. */

    infile = stdin;
    if (argc && **argv != '-') {
        infile = fopen(*argv, "r");

        if (! infile)
        {
            fatalerr("Cannot open input file '%s' (for reading).", *argv);
        }
    }

    /* Initialize the suitable() macro variables. */

    soft_thread->Suit_mask = PS_COLOR;
    soft_thread->Suit_val = PS_COLOR;
    if (soft_thread_struct.Same_suit) {
        soft_thread->Suit_mask = PS_SUIT;
        soft_thread->Suit_val = 0;
    }

    /* Announce which variation this is. */

    if (!Quiet) {
        printf(soft_thread_struct.Same_suit ? "Seahaven; " : "Freecell; ");
        if (soft_thread_struct.King_only) {
            printf("only Kings are allowed to start a pile.\n");
        } else {
            printf("any card may start a pile.\n");
        }
        printf("%d work piles, %d temp cells.\n", soft_thread->Nwpiles, soft_thread->Ntpiles);
    }

#if DEBUG
Init_mem_remain = soft_thread->Mem_remain;
#endif
    if (Sgame < 0) {

        /* Read in the initial layout and play it. */

        read_layout(soft_thread, infile);
        if (!Quiet) {
            print_layout(soft_thread);
        }
        play(&soft_thread_struct);
    } else {

        /* Range mode.  Play lots of consecutive games. */

        soft_thread->Interactive = FALSE;
        for (gn = Sgame; gn < Egame; gn++) {
            printf("#%ld\n", (long)gn);
            msdeal(soft_thread, gn);
            play(&soft_thread_struct);
            fflush(stdout);
        }
    }

    return soft_thread->Status;
}

void play(fc_solve_soft_thread_t * soft_thread)
{
    /* Initialize the hash tables. */

    fc_solve_pats__init_buckets(soft_thread);
    fc_solve_pats__init_clusters(soft_thread);

    /* Reset stats. */

    soft_thread->Total_positions = 0;
    soft_thread->Total_generated = 0;
    soft_thread->num_solutions = 0;

    soft_thread->Status = NOSOL;

    /* Go to it. */

    fc_solve_pats__do_it(soft_thread);
    if (soft_thread->Status != WIN && !Quiet) {
        if (soft_thread->Status == FAIL) {
            printf("Out of memory.\n");
        } else if (soft_thread->Noexit && soft_thread->num_solutions > 0) {
            printf("No shorter solutions.\n");
        } else {
            printf("No solution.\n");
        }
#if DEBUG
        printf("%d positions generated.\n", soft_thread->Total_generated);
        printf("%d unique positions.\n", soft_thread->Total_positions);
        printf("Mem_remain = %ld\n", soft_thread->Mem_remain);
#endif
    }
    if (!soft_thread->Interactive) {
        free_buckets(soft_thread);
        free_clusters(soft_thread);
        free_blocks(soft_thread);
        soft_thread->Freepos = NULL;
    }
#if DEBUG
if (soft_thread->Mem_remain != Init_mem_remain) {
 fc_solve_msg("Mem_remain = %ld\n", soft_thread->Mem_remain);
}
#endif
}


int parse_pile(char *s, card_t *w, int size)
{
    int i;
    card_t rank, suit;

    i = 0;
    rank = suit = NONE;
    while (i < size && *s && *s != '\n' && *s != '\r') {
        while (*s == ' ') s++;
        *s = toupper(*s);
        if (*s == 'A') rank = 1;
        else if (*s >= '2' && *s <= '9') rank = *s - '0';
        else if (*s == 'T') rank = 10;
        else if (*s == 'J') rank = 11;
        else if (*s == 'Q') rank = 12;
        else if (*s == 'K') rank = 13;
        else fatalerr("bad card %c%c\n", s[0], s[1]);
        s++;
        *s = toupper(*s);
        if (*s == 'C') suit = PS_CLUB;
        else if (*s == 'D') suit = PS_DIAMOND;
        else if (*s == 'H') suit = PS_HEART;
        else if (*s == 'S') suit = PS_SPADE;
        else fatalerr("bad card %c%c\n", s[-1], s[0]);
        s++;
        *w++ = suit + rank;
        i++;
        while (*s == ' ') s++;
    }
    return i;
}

static const char * const fc_solve_pats__Ranks_string = " A23456789TJQK";
static const char * const fc_solve_pats__Suits_string = "DCHS";

void fc_solve_pats__print_card(card_t card, FILE *outfile)
{
    if (fcs_pats_card_rank(card) == NONE) {
        fprintf(outfile, "   ");
    } else {
        fprintf(outfile, "%c%c ",
            fc_solve_pats__Ranks_string[fcs_pats_card_rank(card)],
            fc_solve_pats__Suits_string[fcs_pats_card_suit(card)]);
    }
}


#if 0
void print_move(MOVE *mp)
{
  fc_solve_pats__print_card(mp->card, stderr);
  if (mp->totype == T_TYPE) {
   fc_solve_msg("to temp (%d)\n", mp->pri);
  } else if (mp->totype == O_TYPE) {
   fc_solve_msg("out (%d)\n", mp->pri);
  } else {
   fc_solve_msg("to ");
   if (mp->destcard == NONE) {
    fc_solve_msg("empty pile (%d)", mp->pri);
   } else {
    fc_solve_pats__print_card(mp->destcard, stderr);
    fc_solve_msg("(%d)", mp->pri);
   }
   fputc('\n', stderr);
  }
  print_layout(soft_thread);
}
#endif
