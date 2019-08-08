// This file is part of patsolve. It is subject to the license terms in
// the LICENSE file found in the top-level directory of this distribution
// and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
// part of patsolve, including this file, may be copied, modified, propagated,
// or distributed except according to the terms contained in the COPYING file.
//
// Copyright (c) 2002 Tom Holroyd
// tree.h : header of the patsolve's splay tree.
#pragma once

#include "freecell-solver/fcs_conf.h"

// A splay tree.
typedef struct fcs_pats__tree_node_struct fcs_pats__tree;

struct fcs_pats__tree_node_struct
{
    fcs_pats__tree *left;
    fcs_pats__tree *right;
    short depth;
};
