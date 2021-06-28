#!/usr/bin/perl

use strict;
use warnings;

use Test::PerlTidy (qw( run_tests ));

run_tests(
    exclude    => [qr#rinutils#ms],
    path       => $ENV{FCS_SRC_PATH},
    perltidyrc => "$ENV{FCS_SRC_PATH}/.perltidyrc",
);
