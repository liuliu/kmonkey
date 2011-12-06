#ifndef GUARD_kmonkey_h
#define GUARD_kmonkey_h

#include <stdlib.h>
#include "llvm/Module.h"
#include "llvm/Instruction.h"

typedef struct _km_module_t km_module_t;

typedef struct _km_inst_t {
	int uid;
	double weight;
	km_module_t* module;
	llvm::Instruction* inst;
} km_inst_t;

typedef struct _km_module_t {
	llvm::Module* module;
	int num_insts;
	char* file;
	int skip;
	km_inst_t insts[0];
} km_module_t;

typedef struct _km_program_t {
	int num_insts;
	int num_modules;
	km_inst_t** insts;
	km_module_t* modules[0];
} km_program_t;

km_program_t* km_program_new(const char** files, int* skips, int num_files);
void km_emit_coverage_reporter(km_program_t* program);
void km_program_genetic_optimize(km_program_t* program, const char* compile_command, const char* test_command, int size, int generation);
void km_emit_program(km_program_t* program);
void km_program_free(km_program_t* program);

/****************************************************************************************\

  Generic implementation of QuickSort algorithm.
  ----------------------------------------------
  Using this macro user can declare customized sort function that can be much faster
  than built-in qsort function because of lower overhead on elements
  comparison and exchange. The macro takes less_than (or LT) argument - a macro or function
  that takes 2 arguments returns non-zero if the first argument should be before the second
  one in the sorted sequence and zero otherwise.

  Example:

    Suppose that the task is to sort points by ascending of y coordinates and if
    y's are equal x's should ascend.

    The code is:
    ------------------------------------------------------------------------------
           #define cmp_pts( pt1, pt2 ) \
               ((pt1).y < (pt2).y || ((pt1).y < (pt2).y && (pt1).x < (pt2).x))

           [static] CV_IMPLEMENT_QSORT( icvSortPoints, CvPoint, cmp_pts )
    ------------------------------------------------------------------------------

    After that the function "void icvSortPoints( CvPoint* array, size_t total, int aux );"
    is available to user.

  aux is an additional parameter, which can be used when comparing elements.
  The current implementation was derived from *BSD system qsort():

    * Copyright (c) 1992, 1993
    *  The Regents of the University of California.  All rights reserved.
    *
    * Redistribution and use in source and binary forms, with or without
    * modification, are permitted provided that the following conditions
    * are met:
    * 1. Redistributions of source code must retain the above copyright
    *    notice, this list of conditions and the following disclaimer.
    * 2. Redistributions in binary form must reproduce the above copyright
    *    notice, this list of conditions and the following disclaimer in the
    *    documentation and/or other materials provided with the distribution.
    * 3. All advertising materials mentioning features or use of this software
    *    must display the following acknowledgement:
    *  This product includes software developed by the University of
    *  California, Berkeley and its contributors.
    * 4. Neither the name of the University nor the names of its contributors
    *    may be used to endorse or promote products derived from this software
    *    without specific prior written permission.
    *
    * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
    * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
    * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    * SUCH DAMAGE.

\****************************************************************************************/

#define KM_SWAP(a,b,t) ((t) = (a), (a) = (b), (b) = (t))

#define km_min(a, b) (((a) < (b)) ? (a) : (b))

#define KM_IMPLEMENT_QSORT_EX(func_name, T, LT, swap_func, user_data_type)                      \
void func_name(T *array, size_t total, user_data_type aux)                                      \
{                                                                                               \
    int isort_thresh = 7;                                                                       \
    T t;                                                                                        \
    int sp = 0;                                                                                 \
                                                                                                \
    struct                                                                                      \
    {                                                                                           \
        T *lb;                                                                                  \
        T *ub;                                                                                  \
    }                                                                                           \
    stack[48];                                                                                  \
                                                                                                \
    if( total <= 1 )                                                                            \
        return;                                                                                 \
                                                                                                \
    stack[0].lb = array;                                                                        \
    stack[0].ub = array + (total - 1);                                                          \
                                                                                                \
    while( sp >= 0 )                                                                            \
    {                                                                                           \
        T* left = stack[sp].lb;                                                                 \
        T* right = stack[sp--].ub;                                                              \
                                                                                                \
        for(;;)                                                                                 \
        {                                                                                       \
            int i, n = (int)(right - left) + 1, m;                                              \
            T* ptr;                                                                             \
            T* ptr2;                                                                            \
                                                                                                \
            if( n <= isort_thresh )                                                             \
            {                                                                                   \
            insert_sort:                                                                        \
                for( ptr = left + 1; ptr <= right; ptr++ )                                      \
                {                                                                               \
                    for( ptr2 = ptr; ptr2 > left && LT(ptr2[0],ptr2[-1], aux); ptr2--)          \
                        swap_func( ptr2[0], ptr2[-1], array, aux, t );                          \
                }                                                                               \
                break;                                                                          \
            }                                                                                   \
            else                                                                                \
            {                                                                                   \
                T* left0;                                                                       \
                T* left1;                                                                       \
                T* right0;                                                                      \
                T* right1;                                                                      \
                T* pivot;                                                                       \
                T* a;                                                                           \
                T* b;                                                                           \
                T* c;                                                                           \
                int swap_cnt = 0;                                                               \
                                                                                                \
                left0 = left;                                                                   \
                right0 = right;                                                                 \
                pivot = left + (n/2);                                                           \
                                                                                                \
                if( n > 40 )                                                                    \
                {                                                                               \
                    int d = n / 8;                                                              \
                    a = left, b = left + d, c = left + 2*d;                                     \
                    left = LT(*a, *b, aux) ? (LT(*b, *c, aux) ? b : (LT(*a, *c, aux) ? c : a))  \
                                      : (LT(*c, *b, aux) ? b : (LT(*a, *c, aux) ? a : c));      \
                                                                                                \
                    a = pivot - d, b = pivot, c = pivot + d;                                    \
                    pivot = LT(*a, *b, aux) ? (LT(*b, *c, aux) ? b : (LT(*a, *c, aux) ? c : a)) \
                                      : (LT(*c, *b, aux) ? b : (LT(*a, *c, aux) ? a : c));      \
                                                                                                \
                    a = right - 2*d, b = right - d, c = right;                                  \
                    right = LT(*a, *b, aux) ? (LT(*b, *c, aux) ? b : (LT(*a, *c, aux) ? c : a)) \
                                      : (LT(*c, *b, aux) ? b : (LT(*a, *c, aux) ? a : c));      \
                }                                                                               \
                                                                                                \
                a = left, b = pivot, c = right;                                                 \
                pivot = LT(*a, *b, aux) ? (LT(*b, *c, aux) ? b : (LT(*a, *c, aux) ? c : a))     \
                                   : (LT(*c, *b, aux) ? b : (LT(*a, *c, aux) ? a : c));         \
                if( pivot != left0 )                                                            \
                {                                                                               \
                    swap_func( *pivot, *left0, array, aux, t );                                 \
                    pivot = left0;                                                              \
                }                                                                               \
                left = left1 = left0 + 1;                                                       \
                right = right1 = right0;                                                        \
                                                                                                \
                for(;;)                                                                         \
                {                                                                               \
                    while( left <= right && !LT(*pivot, *left, aux) )                           \
                    {                                                                           \
                        if( !LT(*left, *pivot, aux) )                                           \
                        {                                                                       \
                            if( left > left1 )                                                  \
                                swap_func( *left1, *left, array, aux, t );                      \
                            swap_cnt = 1;                                                       \
                            left1++;                                                            \
                        }                                                                       \
                        left++;                                                                 \
                    }                                                                           \
                                                                                                \
                    while( left <= right && !LT(*right, *pivot, aux) )                          \
                    {                                                                           \
                        if( !LT(*pivot, *right, aux) )                                          \
                        {                                                                       \
                            if( right < right1 )                                                \
                                swap_func( *right1, *right, array, aux, t );                    \
                            swap_cnt = 1;                                                       \
                            right1--;                                                           \
                        }                                                                       \
                        right--;                                                                \
                    }                                                                           \
                                                                                                \
                    if( left > right )                                                          \
                        break;                                                                  \
                    swap_func( *left, *right, array, aux, t );                                  \
                    swap_cnt = 1;                                                               \
                    left++;                                                                     \
                    right--;                                                                    \
                }                                                                               \
                                                                                                \
                if( swap_cnt == 0 )                                                             \
                {                                                                               \
                    left = left0, right = right0;                                               \
                    goto insert_sort;                                                           \
                }                                                                               \
                                                                                                \
                n = km_min( (int)(left1 - left0), (int)(left - left1) );                        \
                for( i = 0; i < n; i++ )                                                        \
                    swap_func( left0[i], left[i-n], array, aux, t );                            \
                                                                                                \
                n = km_min( (int)(right0 - right1), (int)(right1 - right) );                    \
                for( i = 0; i < n; i++ )                                                        \
                    swap_func( left[i], right0[i-n+1], array, aux, t );                         \
                n = (int)(left - left1);                                                        \
                m = (int)(right1 - right);                                                      \
                if( n > 1 )                                                                     \
                {                                                                               \
                    if( m > 1 )                                                                 \
                    {                                                                           \
                        if( n > m )                                                             \
                        {                                                                       \
                            stack[++sp].lb = left0;                                             \
                            stack[sp].ub = left0 + n - 1;                                       \
                            left = right0 - m + 1, right = right0;                              \
                        }                                                                       \
                        else                                                                    \
                        {                                                                       \
                            stack[++sp].lb = right0 - m + 1;                                    \
                            stack[sp].ub = right0;                                              \
                            left = left0, right = left0 + n - 1;                                \
                        }                                                                       \
                    }                                                                           \
                    else                                                                        \
                        left = left0, right = left0 + n - 1;                                    \
                }                                                                               \
                else if( m > 1 )                                                                \
                    left = right0 - m + 1, right = right0;                                      \
                else                                                                            \
                    break;                                                                      \
            }                                                                                   \
        }                                                                                       \
    }                                                                                           \
}

#define _km_qsort_default_swap(a, b, array, aux, t) KM_SWAP((a), (b), (t))

#define KM_IMPLEMENT_QSORT(func_name, T, cmp)  \
    KM_IMPLEMENT_QSORT_EX(func_name, T, cmp, _km_qsort_default_swap, int)
#endif
