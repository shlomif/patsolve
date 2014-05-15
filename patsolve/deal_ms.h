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
/*
 * Deal Microsoft Freecell Cards.
 */

#ifndef FC_SOLVE_PATSOLVE__DEAL_MS_H
#define FC_SOLVE_PATSOLVE__DEAL_MS_H

#include "pat.h"

#include "inline.h"

#ifdef WIN32
typedef unsigned __int64 LONG;
typedef unsigned __int32 UINT;
typedef int CARD;
#else
typedef u_int64_t LONG;
typedef u_int32_t UINT;
typedef int CARD;
#endif

#define NUM_CARDS 52

static GCC_INLINE void fc_solve_pats__srand(LONG * seedx_ptr, UINT s)
{
    *(seedx_ptr) = (LONG) s;
}

static GCC_INLINE UINT fc_solve_pats__rand(LONG * seedx_ptr)
{
    *(seedx_ptr) = *(seedx_ptr) * 214013L + 2531011L;
    return (((*seedx_ptr) >> 16) & 0xffff);
}


static GCC_INLINE UINT fc_solve_pats__game_num_rand(LONG * seedx_ptr, LONG gnGameNumber)
{
    UINT ret = fc_solve_pats__rand(seedx_ptr);
    if (gnGameNumber < 0x100000000LL)
    {
        if (gnGameNumber < 0x80000000)
        {
            return ret;
        }
        else
        {
            return (ret | 0x8000);
        }
    }
    else
    {
        return ret + 1;
    }
}

static GCC_INLINE void fc_solve_pats__deal_ms(fc_solve_soft_thread_t * soft_thread, LONG gnGameNumber)
{
#if !defined(HARD_CODED_NUM_FREECELLS)
    const fcs_game_type_params_t game_params = soft_thread->instance->game_params;
#endif

    static const int fc_solve_pats__msdeal_suits[] = { PS_CLUB, PS_DIAMOND, PS_HEART, PS_SPADE };
    int i, j, c;
    int wLeft = NUM_CARDS;  // cards left to be chosen in shuffle
    CARD deck[NUM_CARDS];
    CARD pos[MAX_NUM_STACKS][NUM_CARDS+1];
    LONG seedx;

    memset(pos, 0, sizeof(pos));
    for (i = 0; i < NUM_CARDS; i++) {
        deck[i] = i + 1;
    }

    if (gnGameNumber < 0x100000000LL) {
        fc_solve_pats__srand(&seedx, (UINT) gnGameNumber);
    } else {
        fc_solve_pats__srand(&seedx, (UINT) (gnGameNumber - 0x100000000LL));
    }

    for (i = 0; i < NUM_CARDS; i++) {
        j = fc_solve_pats__game_num_rand(&seedx, gnGameNumber) % wLeft;
        pos[i % LOCAL_STACKS_NUM][i / LOCAL_STACKS_NUM] = deck[j];
        deck[j] = deck[--wLeft];
        if (LOCAL_STACKS_NUM == 10 && i == 49) {
            break;
        }
    }
    for (i = 0; i < LOCAL_STACKS_NUM; i++) {
        j = 0;
        while (pos[i][j]) {
            c = pos[i][j] - 1;
            soft_thread->current_pos.stacks[i][j] = fc_solve_pats__msdeal_suits[c % 4] + (c / 4) + 1;
            j++;
        }
        soft_thread->current_pos.stack_ptrs[i] = &soft_thread->current_pos.stacks[i][j - 1];
        soft_thread->current_pos.columns_lens[i] = j;
    }
    /* leftover cards to temp */
    for (i = 0; i < MAX_NUM_FREECELLS; i++) {
        soft_thread->current_pos.freecells[i] = 0;
        if (wLeft) {
            j = --wLeft;
            c = deck[j] - 1;
            soft_thread->current_pos.freecells[i] = fc_solve_pats__msdeal_suits[c % 4] + (c / 4) + 1;
        }
    }
    for (i = 0; i < 4; i++) {
        soft_thread->current_pos.foundations[i] = 0;
    }
}

#endif /*  FC_SOLVE_PATSOLVE__DEAL_MS_H  */
