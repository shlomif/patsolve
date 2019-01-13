#!/usr/bin/env perl

use 5.014;
use strict;
use warnings;
use autodie;

use Path::Tiny qw/ path /;
use Getopt::Long qw/ GetOptions /;

sub do_system
{
    my ($args) = @_;

    my $cmd = $args->{cmd};
    print "Running [@$cmd]";
    if ( system(@$cmd) )
    {
        die "Running [@$cmd] failed!";
    }
}

my $IS_WIN = ( $^O eq "MSWin32" );

# Cmake does not like backslashes.
my $CMAKE_SAFE_SEPARATOR_ON_WINDOWS = '/';
my $SEP  = $IS_WIN ? $CMAKE_SAFE_SEPARATOR_ON_WINDOWS : '/';
my $MAKE = $IS_WIN ? 'gmake' : 'make';

my $cmake_gen;
GetOptions( 'gen=s' => \$cmake_gen, )
    or die 'Wrong options';

local $ENV{RUN_TESTS_VERBOSE} = 1;

my $CWD = Path::Tiny->cwd;
path("B")->mkpath;
chdir("B");
do_system(
    {
        cmd => [
                  "cmake "
                . ( defined($cmake_gen) ? qq#-G "$cmake_gen"# : "" ) . ' '
                . (
                defined( $ENV{CMAKE_MAKE_PROGRAM} )
                ? "-DCMAKE_MAKE_PROGRAM=$ENV{CMAKE_MAKE_PROGRAM}"
                : ()
                )
                . qq# -DFC_SOLVE_SRC_PATH="$CWD${SEP}fc-solve${SEP}fc-solve${SEP}source" ../patsolve#
        ]
    }
);
do_system( { cmd => [$MAKE] } );
do_system( { cmd => [ $MAKE, 'check', ] } );
chdir($CWD);
