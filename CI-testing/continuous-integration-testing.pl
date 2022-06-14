#!/usr/bin/env perl

use 5.014;
use strict;
use warnings;
use autodie;

use Path::Tiny   qw/ path cwd /;
use Getopt::Long qw/ GetOptions /;

sub do_system
{
    my ($args) = @_;

    my $cmd = $args->{cmd};
    print "Running [@$cmd]\n";
    if ( system(@$cmd) )
    {
        die "Running [@$cmd] failed!";
    }
}
my $cwd = cwd();

my $IS_WIN = ( $^O eq "MSWin32" );
my $MAKE   = $IS_WIN ? 'gmake' : 'make';

# Cmake does not like backslashes.
my $CMAKE_SAFE_SEPARATOR_ON_WINDOWS = '/';
my $SEP  = $IS_WIN ? $CMAKE_SAFE_SEPARATOR_ON_WINDOWS : '/';
my $SUDO = $IS_WIN ? ''                               : 'sudo';

my $cmake_gen;
GetOptions( 'gen=s' => \$cmake_gen, )
    or die 'Wrong options';

local $ENV{RUN_TESTS_VERBOSE} = 1;
if ( !$ENV{SKIP_RINUTILS_INSTALL} )
{
    do_system(
        {
            cmd => [ qw#git clone https://github.com/shlomif/rinutils#, ]
        }
    );
    do_system(
        {
            cmd => [
                      qq#cd rinutils && mkdir B && cd B && cmake #
                    . " -DWITH_TEST_SUITE=OFF "
                    . ( defined($cmake_gen) ? qq# -G "$cmake_gen" # : "" )
                    . (
                    defined( $ENV{CMAKE_MAKE_PROGRAM} )
                    ? " -DCMAKE_MAKE_PROGRAM=$ENV{CMAKE_MAKE_PROGRAM} "
                    : ""
                    )
                    . ( $IS_WIN ? " -DCMAKE_INSTALL_PREFIX=C:/foo " : '' )
                    . qq# .. && $SUDO $MAKE install#
            ]
        }
    );
}
my $CMAKE_PREFIX_PATH;

if ($IS_WIN)
{
    $CMAKE_PREFIX_PATH = join ";", ( map { ; $IS_WIN ? "c:$_" : $_ } ("/foo") );
}

path("B")->mkpath;
chdir("B");
do_system(
    {
        cmd => [
            "cmake "
                . (
                defined($CMAKE_PREFIX_PATH)
                ? ( " -DCMAKE_PREFIX_PATH=" . $CMAKE_PREFIX_PATH . " " )
                : ''
                )
                . " "
                . ( defined($cmake_gen) ? qq#-G "$cmake_gen"# : "" ) . ' '
                . (
                defined( $ENV{CMAKE_MAKE_PROGRAM} )
                ? "-DCMAKE_MAKE_PROGRAM=$ENV{CMAKE_MAKE_PROGRAM}"
                : ()
                )
                . qq# -DFC_SOLVE_SRC_PATH="$cwd${SEP}fc-solve${SEP}fc-solve${SEP}source" ../patsolve#
        ]
    }
);
do_system( { cmd => [$MAKE] } );
do_system( { cmd => [ $MAKE, 'check', ] } );
chdir($cwd);
