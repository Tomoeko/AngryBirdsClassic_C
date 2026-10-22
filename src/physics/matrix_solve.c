#include "ab/physics_matrix.h"

#include <math.h>

bool ab_symmetric3_solve22(const AbSymmetric3 *matrix, AbVec2 right, AbVec2 *output,
                           AbError *error) {
    if (!matrix || !output || !isfinite(matrix->xx) || !isfinite(matrix->xy) ||
        !isfinite(matrix->yy) || !isfinite(right.x) || !isfinite(right.y)) {
        return ab_fail(error, "Invalid symmetric 2D solve inputs/storage");
    }
    float determinant = matrix->xy * matrix->xy;
    determinant = -determinant;
    determinant += matrix->xx * matrix->yy;
    if (determinant != 0.0f) {
        determinant = 1.0f / determinant;
    }
    float x = right.y * matrix->xy;
    x = -x;
    x += right.x * matrix->yy;
    float y = matrix->xy * right.x;
    y = -y;
    y += matrix->xx * right.y;
    AbVec2 result = {x * determinant, y * determinant};
    if (!isfinite(result.x) || !isfinite(result.y)) {
        return ab_fail(error, "Symmetric 2D solve arithmetic overflow");
    }
    *output = result;
    return true;
}

bool ab_symmetric3_solve(const AbSymmetric3 *matrix, AbVec3 right, AbVec3 *output,
                         AbError *error) {
    if (!matrix || !output || !isfinite(matrix->xx) || !isfinite(matrix->xy) ||
        !isfinite(matrix->xz) || !isfinite(matrix->yy) || !isfinite(matrix->yz) ||
        !isfinite(matrix->zz) || !isfinite(right.x) || !isfinite(right.y) ||
        !isfinite(right.z)) {
        return ab_fail(error, "Invalid symmetric 3D solve inputs/storage");
    }
    float cofactor_y = matrix->zz * matrix->xy;
    cofactor_y = -cofactor_y;
    cofactor_y += matrix->yz * matrix->xz;
    float cofactor_x = matrix->yz * matrix->yz;
    cofactor_x = -cofactor_x;
    cofactor_x += matrix->yy * matrix->zz;
    float cofactor_z = matrix->yy * matrix->xz;
    cofactor_z = -cofactor_z;
    cofactor_z += matrix->yz * matrix->xy;
    float determinant = cofactor_y * matrix->xy;
    determinant += cofactor_x * matrix->xx;
    determinant += cofactor_z * matrix->xz;
    if (determinant != 0.0f) {
        determinant = 1.0f / determinant;
    }
    float x = cofactor_y * right.y;
    x += cofactor_x * right.x;
    x += cofactor_z * right.z;
    float y = matrix->zz * right.x;
    y = -y;
    y += matrix->xz * right.z;
    y *= matrix->xy;
    float y_middle = matrix->yz * right.z;
    y_middle = -y_middle;
    y_middle += matrix->zz * right.y;
    y += matrix->xx * y_middle;
    float y_last = matrix->xz * right.y;
    y_last = -y_last;
    y_last += matrix->yz * right.x;
    y += matrix->xz * y_last;
    float z = matrix->xy * right.z;
    z = -z;
    z += matrix->yz * right.x;
    z *= matrix->xy;
    float z_middle = matrix->yz * right.y;
    z_middle = -z_middle;
    z_middle += matrix->yy * right.z;
    z += matrix->xx * z_middle;
    float z_last = matrix->yy * right.x;
    z_last = -z_last;
    z_last += matrix->xy * right.y;
    z += matrix->xz * z_last;
    AbVec3 result = {determinant * x, determinant * y, determinant * z};
    if (!isfinite(result.x) || !isfinite(result.y) || !isfinite(result.z)) {
        return ab_fail(error, "Symmetric 3D solve arithmetic overflow");
    }
    *output = result;
    return true;
}
