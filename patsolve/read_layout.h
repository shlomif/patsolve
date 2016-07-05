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

#pragma once

#include "config.h"
#include "inline.h"

#include "instance.h"
#include "pat.h"

static GCC_INLINE void fc_solve_pats__read_layout(
    fcs_pats_thread_t *const soft_thread, const char *const input_s)
{
#if !defined(HARD_CODED_NUM_STACKS)
    const fcs_game_type_params_t game_params =
        soft_thread->instance->game_params;
#endif

    fcs_state_keyval_pair_t kv;
    fc_solve_initial_user_state_to_c(input_s, &kv, LOCAL_FREECELLS_NUM,
        LOCAL_STACKS_NUM, 1, soft_thread->current_pos.indirect_stacks_buffer);
    soft_thread->current_pos.s = kv.s;
}
