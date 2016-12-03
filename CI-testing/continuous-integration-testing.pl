#!/usr/bin/env perl

use strict;
use warnings;
use autodie;

use Cwd qw/getcwd/;
use Getopt::Long qw/GetOptions/;

sub do_system
{
    my ($args) = @_;

    my $cmd = $args->{cmd};
    print "Running [@$cmd]";
    if (system(@$cmd))
    {
        die "Running [@$cmd] failed!";
    }
}

my $IS_WIN = ($^O eq "MSWin32");
my $SEP = $IS_WIN ? "\\" : '/';
my $MAKE = $IS_WIN ? 'gmake' : 'make';

my $cmake_gen;
GetOptions(
    'gen=s' => \$cmake_gen,
) or die 'Wrong options';

local $ENV{RUN_TESTS_VERBOSE} = 1;

do_system({cmd => ["cd patsolve && mkdir B && cd B && cmake " . (defined($cmake_gen) ? qq#-G "$cmake_gen"# : "") . " -DFC_SOLVE_SRC_PATH=" . getcwd() . "${SEP}fc-solve${SEP}fc-solve${SEP}source ..${SEP}patsolve && $MAKE && $MAKE check"]});

do_system({cmd => ["cd black-hole-solitaire${SEP}Games-Solitaire-BlackHole-Solver && dzil test --all"]});
