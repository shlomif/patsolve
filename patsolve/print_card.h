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

#ifndef FC_SOLVE_PATSOLVE_PRINT_CARD_H
#define FC_SOLVE_PATSOLVE_PRINT_CARD_H

#include "pat.h"
#include "inline.h"

static const char * const fc_solve_pats__Ranks_string = " A23456789TJQK";
static const char * const fc_solve_pats__Suits_string = "HCDS";

static GCC_INLINE void fc_solve_pats__print_card(const fcs_card_t card, FILE * const out_fh)
{
    if (fcs_card_rank(card) != fc_solve_empty_card) {
        fprintf(out_fh, "%c%c",
            fc_solve_pats__Ranks_string[(int)fcs_card_rank(card)],
            fc_solve_pats__Suits_string[(int)fcs_card_suit(card)]);
    }
}

#endif /* FC_SOLVE_PATSOLVE_PRINT_CARD_H */
