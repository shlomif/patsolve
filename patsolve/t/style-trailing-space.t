#!/usr/bin/perl

use strict;
use warnings;

use Test::TrailingSpace ();
use Test::More tests => 2;

foreach my $path ( @ENV{qw/FCS_SRC_PATH FCS_BIN_PATH/} )
{
    my $finder = Test::TrailingSpace->new(
        {
            root              => $path,
            filename_regex    => qr/./,
            abs_path_prune_re => qr#CMakeFiles|_Inline|
            (?:\.(?:a|patch|xcf)\z) |
            (?:(?:\A|/|\\)(?:pats-msdeal|patsolve|threaded-pats)(?:\.exe)?\z)
            #msx,
        }
    );

    # TEST*2
    $finder->no_trailing_space("No trailing whitespace was found.");
}
