#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

typedef u_int64_t LONG;
typedef void VOID;
typedef u_int32_t UINT;
typedef int CARD;

#define NUM_CARDS 52

LONG seedx;

VOID srandp(UINT s)
{
	seedx = (LONG) s;
}

UINT randp()
{
	seedx = seedx * 214013L + 2531011L;
	return (seedx >> 16) & 0xffff;
}

VOID srando(UINT s)
{
	seedx = (LONG) s;
}

UINT rando()
{
	seedx = seedx * 214013L + 2531011L;
	return (seedx >> 16) & 0x7fff;
}

char Rank[] = "A23456789TJQK";
char Suit[] = "CDHS";

int Nwpiles = 8;

int main(int argc, char **argv)
{
	int i, j, c;
	int wLeft = NUM_CARDS;  // cards left to be chosen in shuffle
	CARD deck[NUM_CARDS];
	CARD pos[10][10];
	LONG gnGameNumber;

	if (argc > 2 && argv[1][0] == 's') {
		Nwpiles = 10;
		argv++;
		argc--;
	}

	if (argc != 2) {
		fprintf(stderr, "usage: %s number\n", argv[0]);
		exit(1);
	}
	gnGameNumber = strtoul(argv[1], NULL, 10);

	memset(pos, 0, sizeof(pos));
	for (i = 0; i < NUM_CARDS; i++) {
		deck[i] = i + 1;
	}

	if (gnGameNumber < 0x100000000) {
		srando((UINT) gnGameNumber);
	} else {
		srandp((UINT) (gnGameNumber - 0x100000000));
	}
	for (i = 0; i < NUM_CARDS; i++) {
		if (gnGameNumber < 0x100000000) {
			if (gnGameNumber < 0x80000000) {
				j = rando() % wLeft;
			} else {
				j = (rando() | 0x8000) % wLeft;
			}
		} else {
			j = (randp() + 1) % wLeft;
		}
		pos[i % Nwpiles][i / Nwpiles] = deck[j];
		deck[j] = deck[--wLeft];
		if (Nwpiles == 10 && i == 49) {
			break;
		}
	}
	for (i = 0; i < Nwpiles; i++) {
		j = 0;
		while (pos[i][j]) {
			c = pos[i][j] - 1;
			j++;
			printf("%c%c ", Rank[c / 4], Suit[c % 4]);
		}
		putchar('\n');
	}
	/* leftover cards to temp */
	c = -1;
	for (i = 0; i < 4; i++) {
		if (wLeft) {
			j = --wLeft;
			c = deck[j] - 1;
			printf("%c%c ", Rank[c / 4], Suit[c % 4]);
		}
	}
	if (c >= 0) putchar('\n');
	return 0;
}
