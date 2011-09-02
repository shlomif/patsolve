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
 * TODO : Add a description of this file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "pat.h"

#ifdef WIN32
typedef void VOID;
typedef unsigned __int64 LONG;
typedef unsigned __int32 UINT;
typedef int CARD;
#else
typedef void VOID;
typedef u_int64_t LONG;
typedef u_int32_t UINT;
typedef int CARD;
#endif

#define NUM_CARDS 52

static LONG seedx;

static VOID srandp(UINT s)
{
	seedx = (LONG) s;
}

static UINT randp()
{
	seedx = seedx * 214013L + 2531011L;
	return (seedx >> 16) & 0xffff;
}

static VOID srando(UINT s)
{
	seedx = (LONG) s;
}

static UINT rando()
{
	seedx = seedx * 214013L + 2531011L;
	return (seedx >> 16) & 0x7fff;
}

#define PS_DIAMOND 0x00         /* red */
#define PS_CLUB    0x10         /* black */
#define PS_HEART   0x20         /* red */
#define PS_SPADE   0x30         /* black */
static int msdeal_Suit[] = { PS_CLUB, PS_DIAMOND, PS_HEART, PS_SPADE };

#define MAXTPILES       8       /* max number of piles */
#define MAXWPILES      13
extern card_t W[MAXWPILES][52]; /* the workspace */
extern card_t *Wp[MAXWPILES];   /* point to the top card of each work pile */
extern int Wlen[MAXWPILES];     /* the number of cards in each pile */
extern card_t T[MAXTPILES];     /* one card in each temp cell */
extern card_t O[4];             /* output piles store only the rank or NONE */

void msdeal(fc_solve_soft_thread_t * soft_thread, LONG gnGameNumber)
{
	int i, j, c;
	int wLeft = NUM_CARDS;  // cards left to be chosen in shuffle
	CARD deck[NUM_CARDS];
	CARD pos[MAXWPILES][NUM_CARDS+1];

	memset(pos, 0, sizeof(pos));
	for (i = 0; i < NUM_CARDS; i++) {
		deck[i] = i + 1;
	}

	if (gnGameNumber < 0x100000000LL) {
		srando((UINT) gnGameNumber);
	} else {
		srandp((UINT) (gnGameNumber - 0x100000000LL));
	}

	for (i = 0; i < NUM_CARDS; i++) {
		if (gnGameNumber < 0x100000000LL) {
			if (gnGameNumber < 0x80000000) {
				j = rando() % wLeft;
			} else {
				j = (rando() | 0x8000) % wLeft;
			}
		} else {
			j = (randp() + 1) % wLeft;
		}
		pos[i % soft_thread->Nwpiles][i / soft_thread->Nwpiles] = deck[j];
		deck[j] = deck[--wLeft];
		if (soft_thread->Nwpiles == 10 && i == 49) {
			break;
		}
	}
	for (i = 0; i < soft_thread->Nwpiles; i++) {
		j = 0;
		while (pos[i][j]) {
			c = pos[i][j] - 1;
			W[i][j] = msdeal_Suit[c % 4] + (c / 4) + 1;
			j++;
		}
		Wp[i] = &W[i][j - 1];
		Wlen[i] = j;
	}
	/* leftover cards to temp */
	for (i = 0; i < MAXTPILES; i++) {
		T[i] = 0;
		if (wLeft) {
			j = --wLeft;
			c = deck[j] - 1;
			T[i] = msdeal_Suit[c % 4] + (c / 4) + 1;
		}
	}
	for (i = 0; i < 4; i++) {
		O[i] = 0;
	}
}
