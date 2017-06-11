/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
// Deal Microsoft Freecell / FreeCell-Pro deals.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned long long LONG;
typedef uint32_t UINT;
typedef int CARD;

#define NUM_CARDS 52

static LONG seedx;

static inline void srandp(UINT s) { seedx = (LONG)s; }

static inline UINT randp(void)
{
    seedx = seedx * 214013L + 2531011L;
    return (seedx >> 16) & 0xffff;
}

static inline void srando(UINT s) { seedx = (LONG)s; }

static inline UINT rando(void)
{
    seedx = seedx * 214013L + 2531011L;
    return (seedx >> 16) & 0x7fff;
}

static const char Rank[] = "A23456789TJQK";
static const char Suit[] = "CDHS";

int main(int argc, char **argv)
{
    int j, c;
    // num_left_cards is the cards left to be chosen in shuffle
    int num_left_cards = NUM_CARDS;
    CARD deck[NUM_CARDS];
    CARD pos[10][10];

    size_t stacks_num = 8;
    if (argc > 2 && argv[1][0] == 's')
    {
        stacks_num = 10;
        argv++;
        argc--;
    }

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s number\n", argv[0]);
        exit(1);
    }
    const LONG gnGameNumber = strtoul(argv[1], NULL, 10);

    memset(pos, 0, sizeof(pos));
    for (int i = 0; i < NUM_CARDS; i++)
    {
        deck[i] = i + 1;
    }

    if (gnGameNumber < 0x100000000)
    {
        srando((UINT)gnGameNumber);
    }
    else
    {
        srandp((UINT)(gnGameNumber - 0x100000000));
    }

    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (gnGameNumber < 0x100000000)
        {
            if (gnGameNumber < 0x80000000)
            {
                j = rando() % num_left_cards;
            }
            else
            {
                j = (rando() | 0x8000) % num_left_cards;
            }
        }
        else
        {
            j = (randp() + 1) % num_left_cards;
        }
        pos[i % stacks_num][i / stacks_num] = deck[j];
        deck[j] = deck[--num_left_cards];
        if (stacks_num == 10 && i == 49)
        {
            break;
        }
    }
    for (size_t i = 0; i < stacks_num; i++)
    {
        j = 0;
        while (pos[i][j])
        {
            c = pos[i][j] - 1;
            j++;
            printf("%c%c ", Rank[c / 4], Suit[c % 4]);
        }
        putchar('\n');
    }
    // leftover cards to temp
    c = -1;
    for (size_t i = 0; i < 4; i++)
    {
        if (num_left_cards)
        {
            j = --num_left_cards;
            c = deck[j] - 1;
            printf("%c%c ", Rank[c / 4], Suit[c % 4]);
        }
    }
    if (c >= 0)
        putchar('\n');
    return 0;
}
