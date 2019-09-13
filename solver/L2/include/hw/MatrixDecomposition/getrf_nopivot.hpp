/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WANCUNCUANTIES ONCU CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @file potrf_nopivot.hpp
 * @brief  This files contains implementation of Cholesky decomposition
 */

#ifndef _XF_SOLVER_GETRF_NOPIVOT_
#define _XF_SOLVER_GETRF_NOPIVOT_

#include "hls_stream.h"

#include <iostream>

namespace xf {
namespace solver {

namespace internal {

// update submatrix
template <class T, int NRCU, int NCMAX>
void rowUpdate(T A[NRCU][NCMAX], T pivot[NCMAX], int rs, int re, int cs, int ce) {
    T a00 = pivot[cs];

    T Acs[NRCU];
    // pragma HLS resource variable=Acs core=XPM_MEMORY uram

    int nrows = re - rs + 1;
    int ncols = ce - cs;

LoopMulSub:
    for (unsigned int i = 0; i < nrows * ncols; i++) {
#pragma HLS pipeline
#pragma HLS dependence variable = A inter false
// clang-format off
#pragma HLS loop_tripcount min = 1 max = NCMAX*NRCU
        // clang-format on

        int r = i / ncols + rs;
        int c = i % ncols + cs + 1;

        A[r][c] = A[r][c] - A[r][cs] * pivot[c];
    };
}

// core part of getrf (no pivoting)
template <class T, int NRCU, int NCMAX, int NCU>
void getrf_nopivot_core(int m, int n, T A[NCU][NRCU][NCMAX], int lda) {
LoopSweeps:
    for (int s = 0; s < (m - 1); s++) {
        T pivot[NCU][NCMAX];
#pragma HLS array_partition variable = pivot dim = 1
#pragma HLS resource variable = pivot core = RAM_2P_BRAM
    // #pragma HLS resource variable=pivot core=XPM_MEMORY uram

    LoopPivot:
        for (int k = s; k < n; k++) {
#pragma HLS pipeline
#pragma HLS loop_tripcount min = 1 max = NCMAX

            for (int r = 0; r < NCU; r++) {
#pragma HLS unroll
                pivot[r][k] = A[s % NCU][s / NCU][k];
            };
        };

        T a00 = pivot[0][s];

    LoopDiv:
        for (int j = s + 1; j < m; j++) {
#pragma HLS pipeline
#pragma HLS dependence variable = A inter false
#pragma HLS loop_tripcount min = 1 max = NRCU
            int i = j % NCU;
            int r = j / NCU;
            A[i][r][s] = A[i][r][s] / a00;
        };

    LoopMat:
        for (int i = 0; i < NCU; i++) {
#pragma HLS unroll
            int rs, re, cs, ce;
            rs = (i <= (s % NCU)) ? (s / NCU + 1) : (s / NCU);
            re = NRCU - 1;
            cs = s;
            ce = NCMAX - 1;
            rowUpdate<T, NRCU, NCMAX>(A[i], pivot[i], rs, re, cs, ce);
        };
    };
};

}; // end of namespace internal

/**
 * @brief This function computes the LU decomposition (without pivoting) of matrix \f$A\f$ \n
          \f{equation*} {A = L U, }\f}
          where \f$A\f$ is a dense matrix of size \f$m \times n\f$, \f$L\f$ is a lower triangular matrix with unit
 diagonal, and \f$U\f$ is a upper triangular matrix. This function does not implement pivoting.\n
   The maximum matrix size supported in FPGA is templated by NRMAX and NCMAX.
 *
 * @tparam T data type (support float and double)
 * @tparam NRMAX maximum number of rows for input matrix
 * @tparam NCMAX maximum number of columns for input matrix
 * @tparam NCU number of computation unit
 * @param[in] m real row number of input matrix
 * @param[in] n real column number of input matrix
 * @param[in,out] A input matrix
 * @param[in] lda leading dimention of input matrix A
 * @param[out] info return value, if info=0, the LU factorization is successful
 */
template <class T, int NRMAX, int NCMAX, int NCU>
void getrf_nopivot(int m, int n, T* A, int lda, int& info) {
    const int NRCU = int((NRMAX + NCU - 1) / NCU);

    T matA[NCU][NRCU][NCMAX];
#pragma HLS array_partition variable = matA dim = 1 complete
#pragma HLS resource variable = matA core = XPM_MEMORY uram
// #pragma HLS resource variable=matA core=RAM_2P_BRAM

LoopRead:
    for (int r = 0; r < m; r++) {
        for (int c = 0; c < n; c++) {
#pragma HLS pipeline
            matA[r % NCU][r / NCU][c] = A[lda * r + c];
        };
    };

    internal::getrf_nopivot_core<T, NRCU, NCMAX, NCU>(m, n, matA, lda);

LoopWrite:
    for (int r = 0; r < m; r++) {
        for (int c = 0; c < n; c++) {
#pragma HLS pipeline
            A[r * lda + c] = matA[r % NCU][r / NCU][c];
        };
    };
};

}; // end of namcespace solver
}; // end of namespace xf

#endif
