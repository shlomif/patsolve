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

#include "pat.h"

static GCC_INLINE void fc_solve_pats__print_layout(
    fcs_pats_thread_t *const soft_thread)
{
#ifndef FCS_FREECELL_ONLY
    fc_solve_instance_t *const instance = soft_thread->instance;
#endif

    fcs_state_locs_struct_t locs;
    fc_solve_init_locs(&locs);
    char s[1000];
    fc_solve_state_as_string(s, &(soft_thread->current_pos.s), &(locs),
        INSTANCE_FREECELLS_NUM, INSTANCE_STACKS_NUM, INSTANCE_DECKS_NUM, TRUE,
        FALSE, TRUE);

    fprintf(stderr, "%s\n---\n", s);
}
