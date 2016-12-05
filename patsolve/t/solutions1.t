#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 20;

use Test::Trap
    qw( trap $trap :flow:stderr(systemsafe):stdout(systemsafe):warn );

use File::Basename qw(dirname);
use File::Spec;
use Path::Tiny;
use Socket qw(:crlf);

sub _normalize_lf
{
    my ($s) = @_;
    $s =~ s#$CRLF#$LF#g;
    return $s;
}

my $base_dir = dirname(__FILE__);
my $data_dir = File::Spec->catdir( $base_dir, 'data' );

sub _slurp_win { return _normalize_lf( path("win")->slurp_utf8 ); }

sub remove_trailing_whitespace
{
    my ($s) = @_;

    my $ret = _normalize_lf($s);
    $ret =~ s#[ \t]+$##gms;

    return $ret;
}

# Cleanup.
unlink("win");

# TEST:$pat_test=0;
sub pat_test
{
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my ($args) = @_;
    my $blurb = $args->{blurb};
    trap
    {
        system( "./patsolve", "-f", @{ $args->{cmd_line} } );
    };

    # TEST:$pat_test++;
    is(
        _normalize_lf( $trap->stdout() ),
        _normalize_lf( $args->{stdout} ),
        "$blurb stdout"
    );

    # TEST:$pat_test++;
    ok( !defined( $trap->exit() ), "$blurb : 0 exit status." );

    # TEST:$pat_test++;
    is(
        remove_trailing_whitespace( $trap->stderr() ),
        _normalize_lf( $args->{stderr} ),
        "$blurb : stderr"
    );

    # TEST:$pat_test++;
    is( _slurp_win(), _normalize_lf( $args->{win} ), "$blurb : win contents" );

    return;
}

{
    # TEST*$pat_test
    pat_test(
        {
            blurb    => '24',
            cmd_line => [ '-f', File::Spec->catfile( $data_dir, '24.board' ) ],
            stdout   => <<'EOF',
Freecell; any card may start a pile.
8 work piles, 4 temp cells.
A winner.
91 moves.
EOF
            stderr => <<'EOF',
Foundations: H-0 C-0 D-0 S-0
Freecells:
: 4C 2C 9C 8C QS 4S 2H
: 5H QH 3C AC 3H 4H QD
: QC 9S 6H 9H 3S KS 3D
: 5D 2S JC 5C JH 6D AS
: 2D KD TH TC TD 8D
: 7H JS KH TS KC 7C
: AH 5S 6S AD 8H JD
: 7S 6C 7D 4D 8S 9D

---
EOF
            win => <<'EOF',
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
    );
}

{
    # TEST*$pat_test
    pat_test(
        {
            blurb => '24 -S',
            cmd_line =>
                [ '-f', '-S', File::Spec->catfile( $data_dir, '24.board' ) ],
            stdout => <<'EOF',
Freecell; any card may start a pile.
8 work piles, 4 temp cells.
A winner.
171 moves.
EOF
            stderr => <<'EOF',
Foundations: H-0 C-0 D-0 S-0
Freecells:
: 4C 2C 9C 8C QS 4S 2H
: 5H QH 3C AC 3H 4H QD
: QC 9S 6H 9H 3S KS 3D
: 5D 2S JC 5C JH 6D AS
: 2D KD TH TC TD 8D
: 7H JS KH TS KC 7C
: AH 5S 6S AD 8H JD
: 7S 6C 7D 4D 8S 9D

---
EOF
            win => <<'EOF',
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
    );
}

{
    # TEST*$pat_test
    pat_test(
        {
            blurb => "seahaven 1",
            cmd_line =>
                [ "-s", File::Spec->catfile( $data_dir, "1.seahaven.board" ), ],
            stdout => <<'EOF',
Seahaven; any card may start a pile.
10 work piles, 4 temp cells.
A winner.
76 moves.
EOF
            stderr => <<'EOF',
Foundations: H-0 C-0 D-0 S-0
Freecells:  2H  6H
: JD 9S JS 4D 6D
: 2D 5S AS 7S 8S
: 9H AD AH 3S 8D
: JC QC 3C TD QS
: 5D KH 4C 4S 6C
: 7H 3H 5C TH 3D
: 7C 2S TS 8H 8C
: 5H KS QH 2C TC
: KD 9D 4H JH 6S
: KC QD AC 7D 9C

---
EOF

            win => <<'EOF',
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
    );
}

{
    trap
    {
        system( "./patsolve", "-s", "-S",
            File::Spec->catfile( $data_dir, "1.seahaven.board" ),
        );
    };

    # TEST
    is( _normalize_lf( $trap->stdout() ),
        _normalize_lf(<<'EOF'), 'seahaven 1 -S STDOUT' );
Seahaven; any card may start a pile.
10 work piles, 4 temp cells.
A winner.
94 moves.
EOF

    # TEST
    ok( !defined( $trap->exit() ), '0 exit status.' );

    # TEST
    is( remove_trailing_whitespace( $trap->stderr() ),
        <<'EOF', 'sea1 -S stderr' );
Foundations: H-0 C-0 D-0 S-0
Freecells:  2H  6H
: JD 9S JS 4D 6D
: 2D 5S AS 7S 8S
: 9H AD AH 3S 8D
: JC QC 3C TD QS
: 5D KH 4C 4S 6C
: 7H 3H 5C TH 3D
: 7C 2S TS 8H 8C
: 5H KS QH 2C TC
: KD 9D 4H JH 6S
: KC QD AC 7D 9C

---
EOF

    # TEST
    is( _slurp_win(), _normalize_lf(<<'EOF'), 'seahaven 1 -S win contents' );
8D to temp
3S to temp
AH out
AD out
2H out
3D to temp
8C to 9C
9H to TH
8S to empty pile
7S to 8S
AS out
8H to 9H
6S to 7S
5S to 6S
2D out
3D out
6D to empty pile
4D out
6C to temp
TS to JS
2S out
3S out
4S out
5S out
6S out
7S out
8S out
TS to empty pile
JS to temp
9S out
TS out
JS out
QS out
8C to temp
9C to TC
TD to JD
7D to empty pile
AC out
6C to 7C
6D to 7D
9C to temp
8C to empty pile
6C to temp
9C to TC
7C to 8C
9C to empty pile
TC to temp
2C out
3C out
4C out
6C to 7C
9C to temp
KH to empty pile
5D out
6D out
7D out
8D out
JH to temp
QH to KH
KS out
8H to empty pile
TD to empty pile
JH to QH
4H to 5H
9D out
TD out
JD out
QD out
KD out
9H to empty pile
8H to 9H
KC to temp
TH to JH
5C out
6C out
7C out
8C out
3H out
4H out
5H out
6H out
7H out
8H out
9H out
TH out
JH out
QH out
KH out
9C out
TC out
QC to temp
JC out
QC out
KC out
EOF

}

{
    # TEST*$pat_test
    pat_test(
        {
            blurb => '3 -S',
            cmd_line =>
                [ '-f', "-S", File::Spec->catfile( $data_dir, '3.board' ) ],
            stdout => <<'EOF',
Freecell; any card may start a pile.
8 work piles, 4 temp cells.
A winner.
91 moves.
EOF
            stderr => <<'EOF',
Foundations: H-0 C-0 D-0 S-0
Freecells:
: KC 7D TC 4H 6C 9S 8C
: 2D JH QH AS TD 2C 4S
: QC 9D TS JD 2S 3H 5S
: 7H JS 5D 8D 3C 4C 5C
: 6S QS 6H AC 9H AH
: 8H 8S KS 6D KD 2H
: TH 9C 7C 3D 7S JC
: 4D QD AD KH 3S 5H

---
EOF
            win => <<'EOF',
AH out
2H out
4S to temp
2C to temp
TD to JC
AS out
5H to temp
3S to temp
8C to 9H
9S to TD
5H to 6C
5S to temp
3H out
2S out
3S out
4S out
5S out
5H to temp
6C to temp
4H out
5H out
9S to temp
8C to temp
9H to TC
AC out
2C out
5C to temp
8C to 9H
KH to temp
AD out
6H out
QS to KD
6S out
KH to empty pile
4C to temp
3C out
4C out
5C out
QS to KH
TD to temp
KD to temp
JC to QD
7S out
6C out
JC to QH
3D to temp
7C out
8C out
9C out
9S to TH
JC to QD
QH to temp
JH to QS
2D out
3D out
JC to temp
8D to 9S
QD to empty pile
4D out
5D out
6D out
KD to empty pile
JD to temp
TD to JS
9H to TS
TC out
7D out
8D out
KS to temp
8S out
9S out
JC out
9H to temp
TS out
9D out
TD out
JD out
QC out
QD out
KD out
KC out
JS out
7H out
8H out
9H out
TH out
JH out
QS out
QH out
KH out
KS out
EOF
        }
    );
}
