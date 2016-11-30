/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */

/* Position storage.  A forest of binary trees labeled by cluster. */

#include <stdio.h>
#include <stdlib.h>

#include "inline.h"

#include "instance.h"
#include "pat.h"
#include "tree.h"

/* Given a cluster number, return a tree.  There are 14^4 possible
clusters, but we'll only use a few hundred of them at most.  Hash on
the cluster number, then locate its tree, creating it if necessary. */

static GCC_INLINE fcs_pats__treelist_t *cluster_tree(
    fcs_pats_thread_t *const soft_thread, const int cluster)
{

    /* Pick a bucket, any bucket. */

    const int bucket = cluster % FCS_PATS__TREE_LIST_NUM_BUCKETS;

    /* Find the tree in this bucket with that cluster number. */

    fcs_pats__treelist_t *last = NULL;
    fcs_pats__treelist_t *tl;
    for (tl = soft_thread->tree_list[bucket]; tl; tl = tl->next)
    {
        if (tl->cluster == cluster)
        {
            break;
        }
        last = tl;
    }

    /* If we didn't find it, make a new one and add it to the list. */

    if (tl == NULL)
    {
        tl = fc_solve_pats__new(soft_thread, fcs_pats__treelist_t);
        if (tl == NULL)
        {
            return NULL;
        }
        tl->tree = NULL;
        tl->cluster = cluster;
        tl->next = NULL;
        if (last == NULL)
        {
            soft_thread->tree_list[bucket] = tl;
        }
        else
        {
            last->next = tl;
        }
    }

    return tl;
}

static GCC_INLINE int compare_piles(
    const size_t bytes_per_pile, const u_char *const a, const u_char *const b)
{
    return memcmp(a, b, bytes_per_pile);
}

/* Return the previous result of fc_solve_pats__new_from_block() to the block.
This
can ONLY be called once, immediately after the call to
fc_solve_pats__new_from_block().
That is, no other calls to give_back_block() are allowed. */

static GCC_INLINE void give_back_block(
    fcs_pats_thread_t *const soft_thread, u_char *const p)
{
    typeof(soft_thread->my_block) b = soft_thread->my_block;
    const size_t s = b->ptr - p;
    b->ptr -= s;
    b->remaining += s;
}

/* Add it to the binary tree for this cluster.  The piles are stored
following the fcs_pats__tree_t structure. */

static GCC_INLINE fcs_pats__insert_code_t insert_node(
    fcs_pats_thread_t *const soft_thread, fcs_pats__tree_t *const n,
    const int d, fcs_pats__tree_t **const tree, fcs_pats__tree_t **const node)
{
    const u_char *const key = (u_char *)n + sizeof(fcs_pats__tree_t);
    n->depth = d;
    n->left = n->right = NULL;
    *node = n;
    fcs_pats__tree_t *t = *tree;
    if (t == NULL)
    {
        *tree = n;
        return FCS_PATS__INSERT_CODE_NEW;
    }
    const_SLOT(bytes_per_pile, soft_thread);
    while (1)
    {
        const int c = compare_piles(
            bytes_per_pile, key, ((u_char *)t + sizeof(fcs_pats__tree_t)));
        if (c == 0)
        {
            break;
        }
        if (c < 0)
        {
            if (t->left == NULL)
            {
                t->left = n;
                return FCS_PATS__INSERT_CODE_NEW;
            }
            t = t->left;
        }
        else
        {
            if (t->right == NULL)
            {
                t->right = n;
                return FCS_PATS__INSERT_CODE_NEW;
            }
            t = t->right;
        }
    }

    /* We get here if it's already in the tree.  Don't add it again.
    If the new path to this position was shorter, record the new depth
    so we can prune the original path. */

    if (d < t->depth && !soft_thread->to_stack)
    {
        t->depth = d;
        *node = t;
        return FCS_PATS__INSERT_CODE_FOUND_BETTER;
    }
    else
    {
        return FCS_PATS__INSERT_CODE_FOUND;
    }
}

/* Compact position representation.  The position is stored as an
array with the following format:
    pile0# pile1# ... pileN# (N = LOCAL_STACKS_NUM)
where each pile number is packed into 12 bits (so 2 piles take 3 bytes).
Positions in this format are unique can be compared with memcmp().  The
soft_thread->current_pos.foundations
cells are encoded as a cluster number: no two positions with different
cluster numbers can ever be the same, so we store different clusters in
different trees.  */

static GCC_INLINE fcs_pats__tree_t *pack_position(
    fcs_pats_thread_t *const soft_thread)
{
    DECLARE_STACKS();

    /* Allocate space and store the pile numbers.  The tree node
    will get filled in later, by insert_node(). */

    u_char *p = fc_solve_pats__new_from_block(
        soft_thread, soft_thread->bytes_per_tree_node);
    if (p == NULL)
    {
        return NULL;
    }
    fcs_pats__tree_t *const node = (fcs_pats__tree_t *)p;
    p += sizeof(fcs_pats__tree_t);

    /* Pack the pile numers j into bytes p.
               j             j
        +--------+----:----+--------+
        |76543210|7654:3210|76543210|
        +--------+----:----+--------+
            p         p         p
    */

    fcs_bool_t k = FALSE;
    for (int w = 0; w < LOCAL_STACKS_NUM; w++)
    {
        int j = soft_thread->current_pos
                    .stack_ids[soft_thread->current_pos.column_idxs[w]];
        if (k)
        {
            *p++ |= j >> 8; /* j is positive */
            *p++ = j & 0xFF;
            k = FALSE;
        }
        else
        {
            *p++ = j >> 4;
            *p = (j & 0xF) << 4;
            k = TRUE;
        }
    }

    return node;
}

/* Insert key into the tree unless it's already there.  Return true if
it was new. */

fcs_pats__insert_code_t fc_solve_pats__insert(
    fcs_pats_thread_t *const soft_thread, int *const cluster, const int d,
    fcs_pats__tree_t **const node)
{

    /* Get the cluster number from the Out cell contents. */

    /* Get the tree for this cluster. */

    fcs_pats__treelist_t *const tl = cluster_tree(soft_thread,
        (*cluster = ((fcs_foundation_value(soft_thread->current_pos.s, 0) +
                         (fcs_foundation_value(soft_thread->current_pos.s, 1)
                             << 4)) |
                     ((fcs_foundation_value(soft_thread->current_pos.s, 2) +
                          (fcs_foundation_value(soft_thread->current_pos.s, 3)
                              << 4))
                         << 8))));

    if (tl == NULL)
    {
        return FCS_PATS__INSERT_CODE_ERR;
    }

    /* Create a compact position representation. */

    fcs_pats__tree_t *const new_pos = pack_position(soft_thread);
    if (!new_pos)
    {
        return FCS_PATS__INSERT_CODE_ERR;
    }
    soft_thread->num_states_in_collection++;

    const fcs_pats__insert_code_t verdict =
        insert_node(soft_thread, new_pos, d, &tl->tree, node);

    if (verdict != FCS_PATS__INSERT_CODE_NEW)
    {
        give_back_block(soft_thread, (u_char *)new_pos);
    }

    return verdict;
}

/* my_block storage.  Reduces overhead, and can be freed quickly. */

fcs_pats__block_t *fc_solve_pats__new_block(
    fcs_pats_thread_t *const soft_thread)
{
    fcs_pats__block_t *const b =
        fc_solve_pats__new(soft_thread, fcs_pats__block_t);
    if (b == NULL)
    {
        return NULL;
    }
    const typeof(b->block) block = fc_solve_pats__new_array(
        soft_thread, u_char, FC_SOLVE__PATS__BLOCKSIZE);
    if ((b->block = block) == NULL)
    {
        fc_solve_pats__free_ptr(soft_thread, b, fcs_pats__block_t);
        return NULL;
    }
    b->ptr = block;
    b->remaining = FC_SOLVE__PATS__BLOCKSIZE;
    b->next = NULL;

    return b;
}

/* Like new(), only from the current block.  Make a new block if necessary. */

u_char *fc_solve_pats__new_from_block(
    fcs_pats_thread_t *const soft_thread, const size_t s)
{
    fcs_pats__block_t *b = soft_thread->my_block;
    if (s > b->remaining)
    {
        b = fc_solve_pats__new_block(soft_thread);
        if (b == NULL)
        {
            return NULL;
        }
        b->next = soft_thread->my_block;
        soft_thread->my_block = b;
    }

    u_char *const p = b->ptr;
    b->remaining -= s;
    b->ptr += s;

    return p;
}
