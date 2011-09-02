#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 12;

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

{
    trap
    {
        system("./patsolve", "-S", "./t/data/24.board");
    };

    # TEST
    is ($trap->stdout(), <<'EOF', '24 -S stdout');
Freecell; any card may start a pile.
8 work piles, 4 temp cells.
A winner.
171 moves.
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
2H to temp
4S to temp
7C to 8D 
QD to KC 
JD to QS 
8H to temp
AD out
6S to temp
5S to 6D 
AH out
2H out
5S to temp
6D to 7C 
4H to empty pile
3H out
AC out
3C to 4H 
5S to 6D 
QH to temp
4S to 5H 
3D to 4S 
QH to KS 
QD to temp
3C to temp
4H out
6S to empty pile
QD to KC 
JH to temp
5S to temp
5C to 6D 
JC to QD 
2S out
5D to 6S 
5S to empty pile
3D to temp
4S to 5D 
5H out
8H to empty pile
3D to 4S 
5S to temp
9D to empty pile
JC to QH 
3D to temp
8S to 9D 
3C to 4D 
JD to temp
JH to QS 
JC to temp
3D to 4S 
8S to temp
JC to QD 
8H to temp
8S to 9D 
QH to temp
KS to empty pile
3S out
QH to KS 
JC to temp
8S to 9H 
JC to QH 
3D to temp
4S out
5S out
8S to 9D 
9H to temp
6H out
8H to 9S 
3C to temp
4D to 5C 
7D to 8S 
6C to 7D 
5D to 6C 
JC to QD 
6S out
3C to 4D 
QH to empty pile
JC to QH 
QD to KS 
KC to temp
7S out
TS to JH 
3D to empty pile
KH to temp
JS to QD 
7H out
8H out
9H out
TS to empty pile
9S to temp
JD to QC 
TS to JD 
5D to temp
6C to empty pile
5D to 6C 
3D to temp
KH to empty pile
JH to temp
QS to KH 
7D to 8C 
8S out
9S out
TS out
JS out
QS out
QD to temp
KS out
JH to empty pile
5D to temp
6C to 7D 
JD to empty pile
QC to KH 
JC to empty pile
JD to QC 
5D to 6C 
JH to temp
QD to empty pile
5D to empty pile
JD to temp
JH to QC 
JC to QH 
6C to temp
7D to empty pile
8C to 9D 
6C to 7D 
JH to temp
JD to QC 
9C to temp
2C out
3C out
4C out
JC to QD 
JH to empty pile
8C to temp
5D to 6C 
4D to empty pile
5C out
8C to 9D 
JH to temp
9C to empty pile
5D to temp
6C out
7D to 8C 
6D to empty pile
7C out
8D to 9C 
TD to JC 
TC to JD 
TH out
JH out
QH out
KD to temp
2D out
3D out
4D out
5D out
6D out
7D out
8C out
8D out
9D out
9C out
TD out
TC out
JC out
JD out
QD out
QC out
KH out
KC out
KD out
EOF

}

{
    trap
    {
        system("./patsolve", "-s", "./t/data/1.seahaven.board");
    };

    # TEST
    is ($trap->stdout(), <<'EOF', 'seahaven 1 STDOUT');
Seahaven; any card may start a pile.
10 work piles, 4 temp cells.
A winner.
76 moves.
EOF

    # TEST
    ok (!defined($trap->exit()), '0 exit status.');

    # TEST
    is ($trap->stderr(), <<'EOF', 'sea1 stderr');
JD 9S JS 4D 6D 
2D 5S AS 7S 8S 
9H AD AH 3S 8D 
JC QC 3C TD QS 
5D KH 4C 4S 6C 
7H 3H 5C TH 3D 
7C 2S TS 8H 8C 
5H KS QH 2C TC 
KD 9D 4H JH 6S 
KC QD AC 7D 9C 
2H 6H       
            
---
EOF

    # TEST
    is (_slurp('win'), <<'EOF', 'seahaven 1 win contents');
8D to temp
3S to temp
AH out
AD out
2H out
9H to temp
8S to empty pile
7S to 8S 
AS out
5S to 6S 
2D out
3D out
9C to empty pile
6D to 7D 
4D out
8C to 9C 
9H to TH 
8H to temp
TS to JS 
2S out
3S out
6C to 7C 
4S out
5S out
6S out
7S out
8S out
4C to empty pile
KH to temp
5D out
6D out
7D out
AC out
8D out
TS to empty pile
JS to temp
9S out
TS out
JS out
QS out
TC to empty pile
2C out
TD to JD 
3C out
4C out
9H to empty pile
TH to temp
5C out
6C out
7C out
8C out
9C out
TC out
3H out
QH to empty pile
KS out
JH to empty pile
4H out
5H out
9D out
TD out
JD out
QD out
KD out
6H out
7H out
8H out
9H out
TH out
JH out
QH out
KH out
QC to empty pile
JC out
QC out
KC out
EOF

}
