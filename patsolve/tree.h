/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
/*
 * tree.h : header of the patsolve's splay tree.
 */
#pragma once

#include "config.h"

/* A splay tree. */

typedef struct fcs_pats__tree_node_struct fcs_pats__tree_t;

struct fcs_pats__tree_node_struct
{
    fcs_pats__tree_t *left;
    fcs_pats__tree_t *right;
    short depth;
};
