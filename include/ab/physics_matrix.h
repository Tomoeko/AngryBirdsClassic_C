#ifndef AB_PHYSICS_MATRIX_H
#define AB_PHYSICS_MATRIX_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    float xx, xy, xz;
    float yy, yz;
    float zz;
} AbSymmetric3;

/* Target binary32 symmetric-matrix inversions for joint constraints. The 2D
 * route reads xx/xy/yy and clears the remaining fields to positive zero.
 * Singular determinants retain their zero sign through cofactor multiplication.
 * Required inputs and final results must be finite. No normalization, epsilon
 * threshold, allocation or body mutation. Exact in-place operation is allowed;
 * otherwise input/output are disjoint. Failure preserves output. */
bool ab_symmetric3_inverse22(const AbSymmetric3 *input, AbSymmetric3 *output,
                             AbError *error);
bool ab_symmetric3_inverse(const AbSymmetric3 *input, AbSymmetric3 *output,
                           AbError *error);

/* Direct target determinant solves. Their product/sum order differs from
 * inverse-matrix multiplication. The 2D route reads only xx/xy/yy. Singular
 * determinants follow native signed-zero arithmetic. Required inputs/results
 * must be finite. No allocation; disjoint storage, failure preserves output. */
bool ab_symmetric3_solve22(const AbSymmetric3 *matrix, AbVec2 right, AbVec2 *output,
                           AbError *error);
bool ab_symmetric3_solve(const AbSymmetric3 *matrix, AbVec3 right, AbVec3 *output,
                         AbError *error);

#endif
