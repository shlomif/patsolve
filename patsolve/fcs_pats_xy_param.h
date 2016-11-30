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
 * fcs_pats_xy_param.h - the purpose of this file is to define the
 * fcs_pats_xy_param_t type.
 *
 */

#pragma once

#define FC_SOLVE_PATS__NUM_X_PARAM 11
#define FC_SOLVE_PATS__NUM_Y_PARAM 3

typedef struct
{
    int x[FC_SOLVE_PATS__NUM_X_PARAM];
    double y[FC_SOLVE_PATS__NUM_Y_PARAM];
} fcs_pats_xy_param_t;
