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

static inline void fc_solve_pats__print_layout(
    fcs_pats_thread *const soft_thread)
{
    FCS_ON_NOT_FC_ONLY(fcs_instance *const instance = soft_thread->instance);

    fcs_state_locs_struct locs;
    fc_solve_init_locs(&locs);
    char s[1000];
    fc_solve_state_as_string(s, &(soft_thread->current_pos.s), &(locs),
        INSTANCE_FREECELLS_NUM, INSTANCE_STACKS_NUM, INSTANCE_DECKS_NUM, TRUE,
        FALSE, TRUE);

    fprintf(stderr, "%s\n---\n", s);
}
