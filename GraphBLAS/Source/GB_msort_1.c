//------------------------------------------------------------------------------
// GB_msort_1: sort a list of integers
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2020, All Rights Reserved.
// http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

//------------------------------------------------------------------------------

// A parallel mergesort of an array of n integers.

#include "GB_msort_1.h"

//------------------------------------------------------------------------------
// GB_merge_sequential_1: merge two sorted lists via a single thread
//------------------------------------------------------------------------------

// merge Left [0..nleft-1] and Right [0..nright-1] into S [0..nleft+nright-1] */

static void GB_merge_sequential_1
(
    int64_t *GB_RESTRICT S_0,              // output of length nleft + nright
    const int64_t *GB_RESTRICT Left_0,     // left input of length nleft
    const int64_t nleft,
    const int64_t *GB_RESTRICT Right_0,    // right input of length nright
    const int64_t nright
)
{
    int64_t p, pleft, pright ;

    // merge the two inputs, Left and Right, while both inputs exist
    for (p = 0, pleft = 0, pright = 0 ; pleft < nleft && pright < nright ; p++)
    {
        if (GB_lt_1 (Left_0, pleft, Right_0, pright))
        { 
            // S [p] = Left [pleft++]
            S_0 [p] = Left_0 [pleft] ;
            pleft++ ;
        }
        else
        { 
            // S [p] = Right [pright++]
            S_0 [p] = Right_0 [pright] ;
            pright++ ;
        }
    }

    // either input is exhausted; copy the remaining list into S
    if (pleft < nleft)
    { 
        int64_t nremaining = (nleft - pleft) ;
        memcpy (S_0 + p, Left_0 + pleft, nremaining * sizeof (int64_t)) ;
    }
    else if (pright < nright)
    { 
        int64_t nremaining = (nright - pright) ;
        memcpy (S_0 + p, Right_0 + pright, nremaining * sizeof (int64_t)) ;
    }
}

//------------------------------------------------------------------------------
// GB_merge_parallel_1: parallel merge
//------------------------------------------------------------------------------

// The two input arrays, Bigger [0..nbigger-1] and Smaller [0..nsmaller-1], are
// sorted.  They are merged into the output array S [0..nleft+nright-1], using
// a parallel merge.  nbigger >= nsmaller always holds.

void GB_merge_parallel_1                // parallel merge
(
    int64_t *GB_RESTRICT S_0,           // output of length nbigger + nsmaller
    const int64_t *GB_RESTRICT Bigger_0,   // Bigger [0..nbigger-1]
    const int64_t nbigger,
    const int64_t *GB_RESTRICT Smaller_0,  // Smaller [0..nsmaller-1]
    const int64_t nsmaller
)
{

    //--------------------------------------------------------------------------
    // split the bigger input in half
    //--------------------------------------------------------------------------

    // The first task will handle Bigger [0..nhalf-1], and the second task
    // will handle Bigger [nhalf..n-1].

    int64_t nhalf = nbigger/2 ;
    int64_t Pivot_0 [1] ; Pivot_0 [0] = Bigger_0 [nhalf] ;

    //--------------------------------------------------------------------------
    // find where the Pivot appears in the smaller list
    //--------------------------------------------------------------------------

    // binary search of Smaller [0..nsmaller-1] for the Pivot

    long pleft = 0, pright = nsmaller-1 ;
    while (pleft < pright)
    {
        long pmiddle = (pleft + pright) / 2 ;
        if (GB_lt_1 (Smaller_0, pmiddle, Pivot_0, 0))
        { 
            // if in the list, Pivot appears in [pmiddle+1..pright]
            pleft = pmiddle + 1 ;
        }
        else
        { 
            // if in the list, Pivot appears in [pleft..pmiddle]
            pright = pmiddle ;
        }
    }

    // binary search is narrowed down to a single item
    // or it has found the list is empty:
    ASSERT (pleft == pright || pleft == pright + 1) ;

    // If found is true then Smaller [pleft == pright] == Pivot.  If duplicates
    // appear then Smaller [pleft] is any one of the entries equal to the Pivot
    // in the list.  If found is false then
    //    Smaller [original_pleft ... pleft-1] < Pivot and
    //    Smaller [pleft+1 ... original_pright] > Pivot holds.
    //    The value Smaller [pleft] may be either < or > Pivot.
    bool found = (pleft == pright &&
        Smaller_0 [pleft] == Pivot_0 [0]) ;

    // Modify pleft and pright:
    if (!found && (pleft == pright))
    { 
        if (GB_lt_1 (Smaller_0, pleft, Pivot_0, 0))
        {
            pleft++ ;
        }
        else
        {
            pright++ ;
        }
    }

    // Now the following conditions hold:

    // If found is false then
    //    Smaller [original_pleft ... pleft-1] < Pivot and
    //    Smaller [pleft ... original_pright] > Pivot holds,
    //    and pleft-1 == pright

    // If Smaller has no duplicates, then whether or not Pivot is found,
    //    Smaller [original_pleft ... pleft-1] < Pivot and
    //    Smaller [pleft ... original_pright] >= Pivot holds.

    //--------------------------------------------------------------------------
    // merge each part in parallel
    //--------------------------------------------------------------------------

    // The first task merges Bigger [0..nhalf-1] and Smaller [0..pleft-1] into
    // the output S [0..nhalf+pleft-1].  The entries in Bigger [0..nhalf-1] are
    // all < Pivot (if no duplicates appear in Bigger) or <= Pivot otherwise.

    int64_t *GB_RESTRICT S_task0_0 = S_0 ;

    const int64_t *GB_RESTRICT Left_task0_0 = Bigger_0 ;
    const int64_t nleft_task0 = nhalf ;

    const int64_t *GB_RESTRICT Right_task0_0 = Smaller_0 ;
    const int64_t nright_task0 = pleft ;

    // The second task merges Bigger [nhalf..nbigger-1] and
    // Smaller [pleft..nsmaller-1] into the output S [nhalf+pleft..n-1].
    // The entries in Bigger [nhalf..nbigger-1] and Smaller [pleft..nsmaller-1]
    // are all >= Pivot.

    int64_t *GB_RESTRICT S_task1_0 = S_0 + nhalf + pleft ;

    const int64_t *GB_RESTRICT Left_task1_0 = Bigger_0 + nhalf ;
    const int64_t nleft_task1 = (nbigger - nhalf) ;

    const int64_t *GB_RESTRICT Right_task1_0 = Smaller_0 + pleft ;
    const int64_t nright_task1 = (nsmaller - pleft) ;

    GB_TASK (GB_merge_select_1, S_task0_0,
        Left_task0_0,  nleft_task0,
        Right_task0_0, nright_task0) ;

    GB_TASK (GB_merge_select_1, S_task1_0,
        Left_task1_0,  nleft_task1,
        Right_task1_0, nright_task1) ;

    GB_TASK_WAIT
}

//------------------------------------------------------------------------------
// GB_merge_select_1: parallel or sequential merge
//------------------------------------------------------------------------------

// The two input arrays, Left [0..nleft-1] and Right [0..nright-1], are sorted.
// They are merged into the output array S [0..nleft+nright-1], using either
// the sequential merge (for small lists) or the parallel merge (for big
// lists).

void GB_merge_select_1      // parallel or sequential merge of 2-by-n arrays
(
    int64_t *GB_RESTRICT S_0,              // output of length nleft+nright
    const int64_t *GB_RESTRICT Left_0,     // Left [0..nleft-1]
    const int64_t nleft,
    const int64_t *GB_RESTRICT Right_0,    // Right [0..nright-1]
    const int64_t nright
)
{

    if (nleft + nright < GB_BASECASE)
    { 
        // sequential merge
        GB_merge_sequential_1 (S_0, Left_0, nleft, Right_0, nright) ;
    }
    else if (nleft >= nright)
    { 
        // parallel merge, where Left [0..nleft-1] is the bigger of the two.
        GB_merge_parallel_1 (S_0, Left_0, nleft, Right_0, nright) ;
    }
    else
    { 
        // parallel merge, where Right [0..nright-1] is the bigger of the two.
        GB_merge_parallel_1 (S_0, Right_0, nright, Left_0, nleft) ;
    }
}

//------------------------------------------------------------------------------
// GB_mergesort_1:  parallel merge sort of a length-n array
//------------------------------------------------------------------------------

// GB_mergesort_1 sorts an int64_t array A of size n in ascending
// order, using a parallel mergesort.  W is a workspace array of size n.
// Small arrays are sorted with a quicksort method.

void GB_mergesort_1 // sort array A of size n
(
    int64_t *GB_RESTRICT A_0,      // size n array
    int64_t *GB_RESTRICT W_0,      // size n array, workspace
    const int64_t n
)
{

    if (n <= GB_BASECASE)
    { 

        // ---------------------------------------------------------------------
        // sequential quicksort; no workspace needed
        // ---------------------------------------------------------------------

        GB_qsort_1a (A_0, n) ;

    }
    else
    { 

        // ---------------------------------------------------------------------
        // recursive merge sort if A has length greater than GB_BASECASE
        // ---------------------------------------------------------------------

        // ---------------------------------------------------------------------
        // split A into four quarters
        // ---------------------------------------------------------------------

        int64_t n12 = n / 2 ;           // split n into n12 and n34
        int64_t n34 = n - n12 ;

        int64_t n1 = n12 / 2 ;          // split n12 into n1 and n2
        int64_t n2 = n12 - n1 ;

        int64_t n3 = n34 / 2 ;          // split n34 into n3 and n4
        int64_t n4 = n34 - n3 ;

        int64_t n123 = n12 + n3 ;       // start of 4th quarter = n1 + n2 + n3

        // 1st quarter of A and W
        int64_t *GB_RESTRICT A_1st0 = A_0 ;

        int64_t *GB_RESTRICT W_1st0 = W_0 ;

        // 2nd quarter of A and W
        int64_t *GB_RESTRICT A_2nd0 = A_0 + n1 ;

        int64_t *GB_RESTRICT W_2nd0 = W_0 + n1 ;

        // 3rd quarter of A and W
        int64_t *GB_RESTRICT A_3rd0 = A_0 + n12 ;

        int64_t *GB_RESTRICT W_3rd0 = W_0 + n12 ;

        // 4th quarter of A and W
        int64_t *GB_RESTRICT A_4th0 = A_0 + n123 ;

        int64_t *GB_RESTRICT W_4th0 = W_0 + n123 ;

        // ---------------------------------------------------------------------
        // sort each quarter of A in parallel, using W as workspace
        // ---------------------------------------------------------------------

        GB_TASK (GB_mergesort_1, A_1st0, W_1st0, n1) ;
        GB_TASK (GB_mergesort_1, A_2nd0, W_2nd0, n2) ;
        GB_TASK (GB_mergesort_1, A_3rd0, W_3rd0, n3) ;
        GB_TASK (GB_mergesort_1, A_4th0, W_4th0, n4) ;

        GB_TASK_WAIT

        // ---------------------------------------------------------------------
        // merge pairs of quarters of A into two halves of W, in parallel
        // ---------------------------------------------------------------------

        GB_TASK (GB_merge_select_1, W_1st0, A_1st0, n1, A_2nd0, n2) ;
        GB_TASK (GB_merge_select_1, W_3rd0, A_3rd0, n3, A_4th0, n4) ;

        GB_TASK_WAIT

        // ---------------------------------------------------------------------
        // merge the two halves of W into A
        // ---------------------------------------------------------------------

        GB_merge_select_1 (A_0, W_1st0, n12, W_3rd0, n34) ;
    }
}

//------------------------------------------------------------------------------
// GB_msort_1: gateway for parallel merge sort
//------------------------------------------------------------------------------

GB_PUBLIC   // accessed by the MATLAB tests in GraphBLAS/Test only
void GB_msort_1     // sort array A of size n.
(
    int64_t *GB_RESTRICT A_0,   // size n array
    int64_t *GB_RESTRICT W_0,   // size n array, workspace
    const int64_t n,
    int nthreads                // # of threads to use
)
{

    nthreads = GB_MSORT_NTHREADS (nthreads) ;

    if (nthreads > 1)
    {

        // ---------------------------------------------------------------------
        // parallel mergesort
        // ---------------------------------------------------------------------

        if (GB_OPENMP_GET_NUM_THREADS > 1)
        {

            // -----------------------------------------------------------------
            // parallel mergesort: already in parallel region
            // -----------------------------------------------------------------

            // GB_msort_1 is already in a parallel region in the caller.  This
            // does not occur inside GraphBLAS, but the user application might
            // be calling GraphBLAS inside its own parallel region.

            GB_mergesort_1 (A_0, W_0, n) ;

        }
        else
        { 

            // -----------------------------------------------------------------
            // parallel mergesort: start a parallel region
            // -----------------------------------------------------------------

            GB_TASK_MASTER (nthreads)
            GB_mergesort_1 (A_0, W_0, n) ;

        }

    }
    else
    {

        // ---------------------------------------------------------------------
        // sequential quicksort
        // ---------------------------------------------------------------------

        // The method is in-place, and the workspace is not used.
        GB_qsort_1a (A_0, n) ;
    }
}

