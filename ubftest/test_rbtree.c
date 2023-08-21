/*--------------------------------------------------------------------------
 *
 * test_rbtree.c
 *		Test correctness of red-black tree operations.
 *
 * Copyright (c) 2009-2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/test/modules/test_rbtree/test_rbtree.c
 *
 * -------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <cgreen/cgreen.h>
// #include <ubf.h>
#include <ndrstandard.h>
#include <string.h>
#include <unistd.h>
// #include "test.fd.h"
// #include "ubfunit1.h"
#include "ndebug.h"
// #include "atmi_int.h"

#include <rbtree.h>

#define Max(x, y)		((x) > (y) ? (x) : (y))

int M_size = 0;
int M_delSize = 0;

/*
 * Our test trees store an integer key, and nothing else.
 */
typedef struct ndrx_int_RBTree_Node
{
    ndrx_rbt_node_t     rbtnode;
    int                 key;
} ndrx_int_RBTree_Node_t;


/*
 * Node comparator.  We don't worry about overflow in the subtraction,
 * since none of our test keys are negative.
 */
static int ndrx_irbt_cmp(const ndrx_rbt_node_t *a, const ndrx_rbt_node_t *b, void *arg)
{
    const ndrx_int_RBTree_Node_t *ea = (const ndrx_int_RBTree_Node_t *) a;
    const ndrx_int_RBTree_Node_t *eb = (const ndrx_int_RBTree_Node_t *) b;

    return ea->key - eb->key;
}

/*
 * Node combiner.  For testing purposes, just check that library doesn't
 * try to combine unequal keys.
 */
static void ndrx_irbt_combine(ndrx_rbt_node_t *existing, const ndrx_rbt_node_t *newdata, void *arg)
{
    const ndrx_int_RBTree_Node_t *eexist = (const ndrx_int_RBTree_Node_t *) existing;
    const ndrx_int_RBTree_Node_t *enew = (const ndrx_int_RBTree_Node_t *) newdata;

    if (eexist->key != enew->key)
        NDRX_LOG(log_error, "red-black tree combines %d into %d",
             enew->key, eexist->key);
}

/* Node allocator */
static ndrx_rbt_node_t *ndrx_irbt_alloc(void *arg)
{
    return (ndrx_rbt_node_t *) NDRX_FPMALLOC(sizeof(ndrx_int_RBTree_Node_t), EXFALSE);
}

/* Node freer */
static void ndrx_irbt_free(ndrx_rbt_node_t *node, void *arg)
{
    NDRX_FREE(node);
}

/*
 * Create a red-black tree using our support functions
 */
static ndrx_rbt_tree_t *ndrx_create_int_rbtree(void)
{
    return ndrx_rbt_create(sizeof(ndrx_int_RBTree_Node_t),
                      ndrx_irbt_cmp,
                      ndrx_irbt_combine,
                      ndrx_irbt_alloc,
                      ndrx_irbt_free,
                      NULL);
}

/*
 * Generate a random permutation of the integers 0..size-1
 */
static int *ndrx_GetPermutation(int size)
{
    int		   *permutation;
    int			i;

    permutation = (int *) NDRX_FPMALLOC(size * sizeof(int), EXFALSE);

    permutation[0] = 0;

    /*
     * This is the "inside-out" variant of the Fisher-Yates shuffle algorithm.
     * Notionally, we append each new value to the array and then swap it with
     * a randomly-chosen array element (possibly including itself, else we
     * fail to generate permutations with the last integer last).  The swap
     * step can be optimized by combining it with the insertion.
     */
    for (i = 1; i < size; i++)
    {
        int j = ndrx_rand() % (i + 1);

        if (j < i)				/* avoid fetching undefined data if j=i */
            permutation[i] = permutation[j];

        permutation[j] = i;
    }

    return permutation;
}

/*
 * Populate an empty RBTree with "size" integers having the values
 * 0, step, 2*step, 3*step, ..., inserting them in random order
 */
static void ndrx_rbt_populate(ndrx_rbt_tree_t *tree, int size, int step)
{
    int                    *permutation = ndrx_GetPermutation(size);
    ndrx_int_RBTree_Node_t  node;
    int                     isNew;
    int                     i;

    /* Insert values.  We don't expect any collisions. */
    for (i = 0; i < size; i++)
    {
        node.key = step * permutation[i];
        ndrx_rbt_insert(tree, (ndrx_rbt_node_t *) &node, &isNew);
        if (!isNew)
            NDRX_LOG(log_error, "unexpected !isNew result from ndrx_rbt_insert");
    }

    /*
     * Re-insert the first value to make sure collisions work right.  It's
     * probably not useful to test that case over again for all the values.
     */
    if (size > 0)
    {
        node.key = step * permutation[0];
        ndrx_rbt_insert(tree, (ndrx_rbt_node_t *) &node, &isNew);
        if (isNew)
            NDRX_LOG(log_error, "unexpected isNew result from ndrx_rbt_insert");
    }

    NDRX_FREE(permutation);
}

/*  
 * Check the correctness of left-right traversal.
 * Left-right traversal is correct if all elements are
 * visited in increasing order.
 */
Ensure(testleftright)
{
    ndrx_rbt_tree_t	        *tree = ndrx_create_int_rbtree();
    ndrx_int_RBTree_Node_t  *node;
    ndrx_rbt_ree_iterator_t iter;
    int lastKey = -1;
    int count = 0;
    int size = M_size;

    /* check iteration over empty tree */
    ndrx_rbt_begin_iterate(tree, LeftRightWalk, &iter);
    if (ndrx_rbt_iterate(&iter) != NULL)
        NDRX_LOG(log_error, "left-right walk over empty tree produced an element");

    /* fill tree with consecutive natural numbers */
    ndrx_rbt_populate(tree, size, 1);

    /* iterate over the tree */
    ndrx_rbt_begin_iterate(tree, LeftRightWalk, &iter);

    while ((node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_iterate(&iter)) != NULL)
    {
        /* check that order is increasing */
        if (node->key <= lastKey)
            NDRX_LOG(log_error, "left-right walk gives elements not in sorted order");
        lastKey = node->key;
        count++;
    }

    if (lastKey != size - 1)
        NDRX_LOG(log_error, "left-right walk did not reach end");
    if (count != size)
        NDRX_LOG(log_error, "left-right walk missed some elements");
}

/*
 * Check the correctness of right-left traversal.
 * Right-left traversal is correct if all elements are
 * visited in decreasing order.
 */
Ensure (testrightleft)
{
    ndrx_rbt_tree_t         *tree = ndrx_create_int_rbtree();
    ndrx_int_RBTree_Node_t  *node;
    ndrx_rbt_ree_iterator_t iter;
    int lastKey = M_size;
    int count = 0;
    int size = M_size;

    /* check iteration over empty tree */
    ndrx_rbt_begin_iterate(tree, RightLeftWalk, &iter);
    if (ndrx_rbt_iterate(&iter) != NULL)
        NDRX_LOG(log_error, "right-left walk over empty tree produced an element");

    /* fill tree with consecutive natural numbers */
    ndrx_rbt_populate(tree, size, 1);

    /* iterate over the tree */
    ndrx_rbt_begin_iterate(tree, RightLeftWalk, &iter);

    while ((node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_iterate(&iter)) != NULL)
    {
        /* check that order is decreasing */
        if (node->key >= lastKey)
            NDRX_LOG(log_error, "right-left walk gives elements not in sorted order");
        lastKey = node->key;
        count++;
    }

    if (lastKey != 0)
        NDRX_LOG(log_error, "right-left walk did not reach end");
    if (count != size)
        NDRX_LOG(log_error, "right-left walk missed some elements");
}

/*
 * Check the correctness of the rbt_find operation by searching for
 * both elements we inserted and elements we didn't.
 */
Ensure(testfind)
{
    ndrx_rbt_tree_t	   *tree = ndrx_create_int_rbtree();
    int i;
    int size = M_size;

    /* Insert even integers from 0 to 2 * (size-1) */
    ndrx_rbt_populate(tree, size, 2);

    /* Check that all inserted elements can be found */
    for (i = 0; i < size; i++)
    {
        ndrx_int_RBTree_Node_t node;
        ndrx_int_RBTree_Node_t *resultNode;

        node.key = 2 * i;
        resultNode = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find(tree, (ndrx_rbt_node_t *) &node);
        if (resultNode == NULL)
            NDRX_LOG(log_error, "inserted element was not found");
        if (node.key != resultNode->key)
            NDRX_LOG(log_error, "find operation in rbtree gave wrong result");
    }

    /*
     * Check that not-inserted elements can not be found, being sure to try
     * values before the first and after the last element.
     */
    for (i = -1; i <= 2 * size; i += 2)
    {
        ndrx_int_RBTree_Node_t node;
        ndrx_int_RBTree_Node_t *resultNode;

        node.key = i;
        resultNode = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find(tree, (ndrx_rbt_node_t *) &node);
        if (resultNode != NULL)
            NDRX_LOG(log_error, "not-inserted element was found");
    }
}

/*
 * Check the correctness of the rbt_find_less() and rbt_find_great() functions
 * by searching for an equal key and iterating the lesser keys then the greater
 * keys.
 */
Ensure(testfindltgt)
{
    ndrx_rbt_tree_t *tree = ndrx_create_int_rbtree();
    int             size = M_size;

    /*
     * Using the size as the random key to search wouldn't allow us to get at
     * least one greater match, so we do size - 1
     */
    int     randomKey = ndrx_rand() % (size - 1);
    int     keyDeleted;
    ndrx_int_RBTree_Node_t searchNode = {.key = randomKey};
    ndrx_int_RBTree_Node_t *lteNode;
    ndrx_int_RBTree_Node_t *gteNode;
    ndrx_int_RBTree_Node_t *node;

    /* Insert natural numbers */
    ndrx_rbt_populate(tree, size, 1);

    /*
     * Since the search key is included in the naturals of the tree, we're
     * sure to find an equal match
     */
    lteNode = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_less(tree, (ndrx_rbt_node_t *) &searchNode, EXTRUE);
    gteNode = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_great(tree, (ndrx_rbt_node_t *) &searchNode, EXTRUE);

    if (lteNode == NULL || lteNode->key != searchNode.key)
        NDRX_LOG(log_error, "rbt_find_less() didn't find the equal key");

    if (gteNode == NULL || gteNode->key != searchNode.key)
        NDRX_LOG(log_error, "rbt_find_great() didn't find the equal key");

    if (lteNode != gteNode)
        NDRX_LOG(log_error, "rbt_find_less() and rbt_find_great() found different equal keys");

    /* Find the rest of the naturals lesser than the search key */
    keyDeleted = EXFALSE;
    for (; searchNode.key > 0; searchNode.key--)
    {
        /*
         * Find the next key.  If the current key is deleted, we can pass
         * equal_match == true and still find the next one.
         */
        node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_less(tree, (ndrx_rbt_node_t *) &searchNode,
                                               keyDeleted);

        /* ensure we find a lesser match */
        if (!node || !(node->key < searchNode.key))
            NDRX_LOG(log_error, "rbt_find_less() didn't find a lesser key");

        /* randomly delete the found key or leave it */
        keyDeleted = ndrx_rand() % 2;
        if (keyDeleted)
            ndrx_rbt_delete(tree, (ndrx_rbt_node_t *) node);
    }

    /* Find the rest of the naturals greater than the search key */
    keyDeleted = EXFALSE;
    for (searchNode.key = randomKey; searchNode.key < size - 1; searchNode.key++)
    {
        /*
         * Find the next key.  If the current key is deleted, we can pass
         * equal_match == true and still find the next one.
         */
        node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_great(tree, (ndrx_rbt_node_t *) &searchNode,
                                                keyDeleted);

        /* ensure we find a greater match */
        if (!node || !(node->key > searchNode.key))
            NDRX_LOG(log_error, "rbt_find_great() didn't find a greater key");

        /* randomly delete the found key or leave it */
        keyDeleted = ndrx_rand() % 2;
        if (keyDeleted)
            ndrx_rbt_delete(tree, (ndrx_rbt_node_t *) node);
    }

    /* Check out of bounds searches find nothing */
    searchNode.key = -1;
    node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_less(tree, (ndrx_rbt_node_t *) &searchNode, EXTRUE);
    if (node != NULL)
        NDRX_LOG(log_error, "rbt_find_less() found non-inserted element");
    searchNode.key = 0;
    node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_less(tree, (ndrx_rbt_node_t *) &searchNode, EXFALSE);
    if (node != NULL)
        NDRX_LOG(log_error, "rbt_find_less() found non-inserted element");
    searchNode.key = size;
    node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_great(tree, (ndrx_rbt_node_t *) &searchNode, EXTRUE);
    if (node != NULL)
        NDRX_LOG(log_error, "rbt_find_great() found non-inserted element");
    searchNode.key = size - 1;
    node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find_great(tree, (ndrx_rbt_node_t *) &searchNode, EXFALSE);
    if (node != NULL)
        NDRX_LOG(log_error, "rbt_find_great() found non-inserted element");
}

/*
 * Check the correctness of the rbt_leftmost operation.
 * This operation should always return the smallest element of the tree.
 */
Ensure(testleftmost)
{
    ndrx_rbt_tree_t         *tree = ndrx_create_int_rbtree();
    ndrx_int_RBTree_Node_t  *result;
    int                     size = M_size;

    /* Check that empty tree has no leftmost element */
    if (ndrx_rbt_leftmost(tree) != NULL)
        NDRX_LOG(log_error, "leftmost node of empty tree is not NULL");

    /* fill tree with consecutive natural numbers */
    ndrx_rbt_populate(tree, size, 1);

    /* Check that leftmost element is the smallest one */
    result = (ndrx_int_RBTree_Node_t *) ndrx_rbt_leftmost(tree);
    if (result == NULL || result->key != 0)
        NDRX_LOG(log_error, "rbt_leftmost gave wrong result");
}

/*
 * Check the correctness of the rbt_delete operation.
 */
Ensure(testdelete)
{
    ndrx_rbt_tree_t *tree = ndrx_create_int_rbtree();
    int             *deleteIds;
    int             *chosen;
    int             i;
    int             size = M_size;
    int             delsize = M_delSize;

    /* fill tree with consecutive natural numbers */
    ndrx_rbt_populate(tree, size, 1);

    /* Choose unique ids to delete */
    deleteIds = (int *) NDRX_FPMALLOC(delsize * sizeof(int), EXFALSE);
    chosen = (int *) NDRX_FPMALLOC(size * sizeof(int), EXFALSE);

    for (i = 0; i < delsize; i++)
    {
        int k = ndrx_rand() % size;

        while (chosen[k])
            k = (k + 1) % size;
        deleteIds[i] = k;
        chosen[k] = EXTRUE;
    }

    /* Delete elements */
    for (i = 0; i < delsize; i++)
    {
        ndrx_int_RBTree_Node_t find;
        ndrx_int_RBTree_Node_t *node;

        find.key = deleteIds[i];
        /* Locate the node to be deleted */
        node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find(tree, (ndrx_rbt_node_t *) &find);
        if (node == NULL || node->key != deleteIds[i])
            NDRX_LOG(log_error, "expected element was not found during deleting");
        /* Delete it */
        ndrx_rbt_delete(tree, (ndrx_rbt_node_t *) node);
    }

    /* Check that deleted elements are deleted */
    for (i = 0; i < size; i++)
    {
        ndrx_int_RBTree_Node_t node;
        ndrx_int_RBTree_Node_t *result;

        node.key = i;
        result = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find(tree, (ndrx_rbt_node_t *) &node);
        if (chosen[i])
        {
            /* Deleted element should be absent */
            if (result != NULL)
                NDRX_LOG(log_error, "deleted element still present in the rbtree");
        }
        else
        {
            /* Else it should be present */
            if (result == NULL || result->key != i)
                NDRX_LOG(log_error, "delete operation removed wrong rbtree value");
        }
    }

    /* Delete remaining elements, so as to exercise reducing tree to empty */
    for (i = 0; i < size; i++)
    {
        ndrx_int_RBTree_Node_t find;
        ndrx_int_RBTree_Node_t *node;

        if (chosen[i])
            continue;
        find.key = i;
        /* Locate the node to be deleted */
        node = (ndrx_int_RBTree_Node_t *) ndrx_rbt_find(tree, (ndrx_rbt_node_t *) &find);
        if (node == NULL || node->key != i)
            NDRX_LOG(log_error, "expected element was not found during deleting");
        /* Delete it */
        ndrx_rbt_delete(tree, (ndrx_rbt_node_t *) node);
    }

    /* Tree should now be empty */
    if (ndrx_rbt_leftmost(tree) != NULL)
        NDRX_LOG(log_error, "deleting all elements failed");

    NDRX_FREE(deleteIds);
    NDRX_FREE(chosen);
}

TestSuite *test_rb_tree(void)
{
    TestSuite *suite = create_test_suite();

    M_size = ndrx_rand() % (NDRX_MSGSIZEMAX / sizeof(int));
    M_delSize = Max(M_size / 10, 1);

    if (M_size <= 0 || M_size > NDRX_MSGSIZEMAX / sizeof(int))
        NDRX_LOG(log_error, "invalid size for test_rb_tree: %d", M_size);

    add_test(suite, testleftright);
    add_test(suite, testrightleft);
    add_test(suite, testfind);
    add_test(suite, testfindltgt);
    add_test(suite, testleftmost);
    add_test(suite, testdelete);
    return suite;
}
/* vim: set ts=4 sw=4 et smartindent: */
