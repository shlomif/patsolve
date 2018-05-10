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

static inline void fc_solve_pats__read_layout(
    fcs_pats_thread *const soft_thread, const char *const input_s)
{
#if !defined(HARD_CODED_NUM_STACKS)
    const_SLOT(game_params, soft_thread->instance);
#endif

    fcs_state_keyval_pair kv;
    fc_solve_initial_user_state_to_c(input_s, &kv, LOCAL_FREECELLS_NUM,
        LOCAL_STACKS_NUM, 1, soft_thread->current_pos.indirect_stacks_buffer);
    soft_thread->current_pos.s = kv.s;
}
