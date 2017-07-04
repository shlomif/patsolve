#!/usr/bin/perl

use strict;
use warnings;

use autodie;

use Cwd;
use File::Spec;
use File::Copy;
use File::Path;
use Getopt::Long;
use Env::Path;
use File::Basename qw( basename dirname );

my $bindir     = dirname(__FILE__);
my $abs_bindir = File::Spec->rel2abs($bindir);

# Whether to use prove instead of runprove.
my $use_prove = $ENV{FCS_USE_TEST_RUN} ? 0 : 1;
my $num_jobs = $ENV{TEST_JOBS};

sub _is_parallized
{
    return ( $use_prove && $num_jobs );
}

sub _calc_prove
{
    return [ 'prove',
        ( defined($num_jobs) ? sprintf( "-j%d", $num_jobs ) : () ) ];
}

sub run_tests
{
    my $tests = shift;

    my @cmd = ( ( $use_prove ? @{ _calc_prove() } : 'runprove' ), @$tests );
    if ( $ENV{RUN_TESTS_VERBOSE} )
    {
        print "Running [@cmd]\n";
    }

    # Workaround for Windows spawning-SNAFU.
    my $exit_code = system(@cmd);
    exit( $exit_code ? (-1) : 0 );
}

my $tests_glob = "*.{exe,py,t}";

GetOptions(
    '--glob=s'   => \$tests_glob,
    '--prove!'   => \$use_prove,
    '--jobs|j=n' => \$num_jobs,
) or die "--glob='tests_glob'";

{
    my $fcs_path = Cwd::getcwd();
    local $ENV{FCS_PATH}     = $fcs_path;
    local $ENV{FCS_SRC_PATH} = $abs_bindir;

    local $ENV{FREECELL_SOLVER_QUIET} = 1;
    Env::Path->PATH->Prepend(
        File::Spec->catdir( Cwd::getcwd(), "board_gen" ),
        File::Spec->catdir( $abs_bindir, "t", "scripts" ),
    );

    Env::Path->CPATH->Prepend( $abs_bindir, );

    Env::Path->LD_LIBRARY_PATH->Prepend($fcs_path);

    foreach my $add_lib ( Env::Path->PERL5LIB(), Env::Path->PYTHONPATH() )
    {
        $add_lib->Append( File::Spec->catdir( $abs_bindir, "t", "t", "lib" ), );
    }

    my $get_config_fn = sub {
        my $basename = shift;

        return File::Spec->rel2abs(
            File::Spec->catdir( $bindir, "t", "config", $basename ),
        );
    };

    local $ENV{HARNESS_ALT_INTRP_FILE} =
        $get_config_fn->("alternate-interpreters.yml");

    local $ENV{HARNESS_TRIM_FNS} = 'keep:1';

    local $ENV{HARNESS_PLUGINS} = join(
        ' ', qw(
            BreakOnFailure ColorSummary ColorFileVerdicts AlternateInterpreters
            TrimDisplayedFilenames
            )
    );

    my $is_ninja = ( -e "build.ninja" );

    if ( !$is_ninja )
    {
        my $IS_WIN = ( $^O eq "MSWin32" );
        my $MAKE = $IS_WIN ? 'gmake' : 'make';
        if ( system( $MAKE, "-s" ) )
        {
            die "$MAKE failed";
        }
    }

    # Put the valgrind test last because it takes a long time.
    my @tests =
        sort {
        ( ( ( $a =~ /valgrind/ ) <=> ( $b =~ /valgrind/ ) ) *
                ( _is_parallized() ? -1 : 1 ) )
            || ( basename($a) cmp basename($b) )
            || ( $a cmp $b )
        } (
        glob("t/$tests_glob"),
        (
              ( $fcs_path ne $abs_bindir )
            ? ( glob("$abs_bindir/t/$tests_glob") )
            : ()
        ),
        );

    if ( !$ENV{FCS_TEST_BUILD} )
    {
        @tests = grep { !/build-process/ } @tests;
    }

    if ( $ENV{FCS_TEST_WITHOUT_VALGRIND} )
    {
        @tests = grep { !/valgrind/ } @tests;
    }

    print STDERR "FCS_PATH = $ENV{FCS_PATH}\n";
    print STDERR "FCS_SRC_PATH = $ENV{FCS_SRC_PATH}\n";
    if ( $ENV{FCS_TEST_SHELL} )
    {
        system("bash");
    }
    else
    {
        run_tests( \@tests );
    }
}

__END__

=head1 COPYRIGHT AND LICENSE

This file is part of Freecell Solver. It is subject to the license terms in
the COPYING.txt file found in the top-level directory of this distribution
and at http://fc-solve.shlomifish.org/docs/distro/COPYING.html . No part of
Freecell Solver, including this file, may be copied, modified, propagated,
or distributed except according to the terms contained in the COPYING file.

Copyright (c) 2000 Shlomi Fish

=cut
