#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 4;

use Test::Trap qw( trap $trap :flow:stderr(systemsafe):stdout(systemsafe):warn );

sub _slurp
{
    my $filename = shift;

    open my $in, '<', $filename
        or die "Cannot open '$filename' for slurping - $!";

    local $/;
    my $contents = <$in>;

    close($in);

    return $contents;
}

# Cleanup.
unlink("win");

{
    trap
    {
        system("./patsolve", "./t/data/24.board");
    };

    # TEST
    is ($trap->stdout(), <<'EOF', '24 stdout');
Freecell; any card may start a pile.
8 work piles, 4 temp cells.
A winner.
91 moves.
EOF

    # TEST
    ok (!defined($trap->exit()), '0 exit status.');

    # TEST
    is ($trap->stderr(), <<'EOF', '24 stderr');
4C 2C 9C 8C QS 4S 2H 
5H QH 3C AC 3H 4H QD 
QC 9S 6H 9H 3S KS 3D 
5D 2S JC 5C JH 6D AS 
2D KD TH TC TD 8D 
7H JS KH TS KC 7C 
AH 5S 6S AD 8H JD 
7S 6C 7D 4D 8S 9D 
            
            
---
EOF

    # TEST
    is (_slurp('win'), <<'EOF', '24 win contents');
AS out
7C to 8D 
QD to KC 
JD to temp
8H to temp
AD out
6S to temp
5S to 6D 
AH out
2H out
4S to temp
QS to empty pile
8C to 9D 
4H to 5S 
3H out
AC out
JD to QS 
9C to temp
2C out
3C out
4C out
9C to empty pile
8H to 9C 
4H out
5S to temp
7C to 8H 
6D to 7C 
JH to temp
5C out
JC to QD 
2S out
4S to 5D 
QH to temp
5H out
3D to 4S 
KS to empty pile
3S out
QH to KS 
3D to temp
4S out
5S out
6S out
8D to temp
JC to QH 
TD to JC 
TC to JD 
9H to TC 
6H out
TH to temp
8D to 9S 
KD to temp
2D out
3D out
8C to 9H 
9D to empty pile
8S to 9D 
4D out
5D out
6D out
7D out
6C out
8D out
7C out
8C out
7S out
8S out
9D out
TD out
9S out
QD to empty pile
KC to empty pile
TS out
KH to empty pile
JS out
7H out
8H out
9H out
9C out
TC out
JD out
JC out
QD out
TH out
QS out
QC out
KD out
JH out
QH out
KH out
KS out
KC out
EOF

}

