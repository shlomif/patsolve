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

#include "pat.h"
#include "inline.h"

static const char *const fc_solve_pats__Ranks_string = " A23456789TJQK";
static const char *const fc_solve_pats__Suits_string = "HCDS";

static GCC_INLINE void fc_solve_pats__print_card(
    const fcs_card_t card, FILE *const out_fh)
{
    if (fcs_card_rank(card) != fc_solve_empty_card)
    {
        fprintf(out_fh, "%c%c",
            fc_solve_pats__Ranks_string[(int)fcs_card_rank(card)],
            fc_solve_pats__Suits_string[(int)fcs_card_suit(card)]);
    }
}
