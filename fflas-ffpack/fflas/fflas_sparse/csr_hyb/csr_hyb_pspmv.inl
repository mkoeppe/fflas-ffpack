/* -*- mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
// vim:sts=8:sw=8:ts=8:noet:sr:cino=>s,f0,{0,g0,(0,\:0,t0,+0,=s
/*
 * Copyright (C) 2014 the FFLAS-FFPACK group
 *
 * Written by   Bastien Vialla <bastien.vialla@lirmm.fr>
 *
 *
 * ========LICENCE========
 * This file is part of the library FFLAS-FFPACK.
 *
 * FFLAS-FFPACK is free software: you can redistribute it and/or modify
 * it under the terms of the  GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 * ========LICENCE========
 *.
 */

#ifndef __FFLASFFPACK_fflas_sparse_CSR_HYB_pspmv_INL
#define __FFLASFFPACK_fflas_sparse_CSR_HYB_pspmv_INL

#ifdef __FFLASFFPACK_USE_TBB
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#endif

namespace FFLAS {
namespace sparse_details_impl {
template <class Field>
inline void pfspmv(const Field &F,
                   const Sparse<Field, SparseMatrix_t::CSR_HYB> &A,
                   typename Field::ConstElement_ptr x,
                   typename Field::Element_ptr y, FieldCategories::GenericTag) {
#ifdef __FFLASFFPACK_USE_TBB
    int step = __FFLASFFPACK_CACHE_LINE_SIZE / sizeof(typename Field::Element);
    tbb::parallel_for(tbb::blocked_range<index_t>(0, A.m, step),
                      [&F, &A, &x, &y](const tbb::blocked_range<index_t> &r) {
        for (index_t i = r.begin(), end = r.end(); i < end; ++i) {
            index_t start = A.st[4 * i], stop = A.st[4 * i + 1];
            for (uint64_t j = start; j < stop; ++j) {
                F.subin(y[i], x[A.col[j]]);
            }
            start = A.st[4 * i + 1], stop = A.st[4 * i + 2];
            for (uint64_t j = start; j < stop; ++j) {
                F.addin(y[i], x[A.col[j]]);
            }
            start = A.st[4 * i + 2], stop = A.st[4 * (i + 1)];
            index_t startDat = A.st[4 * i + 3];
            for (uint64_t j = start, k = 0; j < stop; ++j, ++k) {
                F.axpyin(y[i], A.dat[startDat + k], x[A.col[j]]);
            }
        }
    });
#else
#pragma omp parallel for
    for (uint64_t i = 0; i < A.m; ++i) {
        index_t start = A.st[4 * i], stop = A.st[4 * i + 1];
        for (uint64_t j = start; j < stop; ++j) {
            F.subin(y[i], x[A.col[j]]);
        }
        start = A.st[4 * i + 1], stop = A.st[4 * i + 2];
        for (uint64_t j = start; j < stop; ++j) {
            F.addin(y[i], x[A.col[j]]);
        }
        start = A.st[4 * i + 2], stop = A.st[4 * (i + 1)];
        index_t startDat = A.st[4 * i + 3];
        for (uint64_t j = start, k = 0; j < stop; ++j, ++k) {
            F.axpyin(y[i], A.dat[startDat + k], x[A.col[j]]);
        }
    }
#endif
}

template <class Field>
inline void
pfspmv(const Field &F, const Sparse<Field, SparseMatrix_t::CSR_HYB> &A,
       typename Field::ConstElement_ptr x, typename Field::Element_ptr y,
       FieldCategories::UnparametricTag) {
#ifdef __FFLASFFPACK_USE_TBB
    int step = __FFLASFFPACK_CACHE_LINE_SIZE / sizeof(typename Field::Element);
    tbb::parallel_for(tbb::blocked_range<index_t>(0, A.m, step),
                      [&F, &A, &x, &y](const tbb::blocked_range<index_t> &r) {
        for (index_t i = r.begin(), end = r.end(); i < end; ++i) {
            index_t start = A.st[4 * i], stop = A.st[4 * i + 1];
            index_t diff = stop - start;
            typename Field::Element y1 = 0, y2 = 0, y3 = 0, y4 = 0;
            uint64_t j = 0;
            for (; j < ROUND_DOWN(diff, 4); j += 4) {
                y1 += x[A.col[start + j]];
                y2 += x[A.col[start + j + 1]];
                y3 += x[A.col[start + j + 2]];
                y4 += x[A.col[start + j + 3]];
            }
            for (; j < diff; ++j) {
                y1 += x[A.col[start + j]];
            }
            y[i] -= y1 + y2 + y3 + y4;
            y1 = 0;
            y2 = 0;
            y3 = 0;
            y4 = 0;
            start = A.st[4 * i + 1], stop = A.st[4 * i + 2];
            diff = stop - start;
            j = 0;
            for (; j < ROUND_DOWN(diff, 4); j += 4) {
                y1 += x[A.col[start + j]];
                y2 += x[A.col[start + j + 1]];
                y3 += x[A.col[start + j + 2]];
                y4 += x[A.col[start + j + 3]];
            }
            for (; j < diff; ++j) {
                y1 += x[A.col[start + j]];
            }
            y[i] += y1 + y2 + y3 + y4;
            y1 = 0;
            y2 = 0;
            y3 = 0;
            y4 = 0;
            start = A.st[4 * i + 2], stop = A.st[4 * (i + 1)];
            diff = stop - start;
            index_t startDat = A.st[4 * i + 3];
            j = 0;
            for (; j < ROUND_DOWN(diff, 4); j += 4) {
                y1 += A.dat[startDat + j] * x[A.col[start + j]];
                y2 += A.dat[startDat + j + 1] * x[A.col[start + j + 1]];
                y3 += A.dat[startDat + j + 2] * x[A.col[start + j + 2]];
                y4 += A.dat[startDat + j + 3] * x[A.col[start + j + 3]];
            }
            for (; j < diff; ++j) {
                y1 += A.dat[startDat + j] * x[A.col[start + j]];
            }
            y[i] += y1 + y2 + y3 + y4;
        }
    });
#else
#pragma omp parallel for
    for (uint64_t i = 0; i < A.m; ++i) {
        index_t start = A.st[4 * i], stop = A.st[4 * i + 1];
        index_t diff = stop - start;
        typename Field::Element y1 = 0, y2 = 0, y3 = 0, y4 = 0;
        uint64_t j = 0;
        for (; j < ROUND_DOWN(diff, 4); j += 4) {
            y1 += x[A.col[start + j]];
            y2 += x[A.col[start + j + 1]];
            y3 += x[A.col[start + j + 2]];
            y4 += x[A.col[start + j + 3]];
        }
        for (; j < diff; ++j) {
            y1 += x[A.col[start + j]];
        }
        y[i] -= y1 + y2 + y3 + y4;
        y1 = 0;
        y2 = 0;
        y3 = 0;
        y4 = 0;
        start = A.st[4 * i + 1], stop = A.st[4 * i + 2];
        diff = stop - start;
        j = 0;
        for (; j < ROUND_DOWN(diff, 4); j += 4) {
            y1 += x[A.col[start + j]];
            y2 += x[A.col[start + j + 1]];
            y3 += x[A.col[start + j + 2]];
            y4 += x[A.col[start + j + 3]];
        }
        for (; j < diff; ++j) {
            y1 += x[A.col[start + j]];
        }
        y[i] += y1 + y2 + y3 + y4;
        y1 = 0;
        y2 = 0;
        y3 = 0;
        y4 = 0;
        start = A.st[4 * i + 2], stop = A.st[4 * (i + 1)];
        diff = stop - start;
        index_t startDat = A.st[4 * i + 3];
        j = 0;
        for (; j < ROUND_DOWN(diff, 4); j += 4) {
            y1 += A.dat[startDat + j] * x[A.col[start + j]];
            y2 += A.dat[startDat + j + 1] * x[A.col[start + j + 1]];
            y3 += A.dat[startDat + j + 2] * x[A.col[start + j + 2]];
            y4 += A.dat[startDat + j + 3] * x[A.col[start + j + 3]];
        }
        for (; j < diff; ++j) {
            y1 += A.dat[startDat + j] * x[A.col[start + j]];
        }
        y[i] += y1 + y2 + y3 + y4;
    }
#endif
}

template <class Field>
inline void pfspmv(const Field &F,
                   const Sparse<Field, SparseMatrix_t::CSR_HYB> &A,
                   typename Field::ConstElement_ptr x,
                   typename Field::Element_ptr y, const int64_t kmax) {
    // TODO
}

} // CSR_HYB_details

} // FFLAS

#endif //  __FFLASFFPACK_fflas_CSR_HYB_pspmv_INL