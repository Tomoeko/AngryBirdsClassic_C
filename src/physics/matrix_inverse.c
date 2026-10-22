#include "ab/physics_matrix.h"

#include <math.h>

static bool finite_matrix(const AbSymmetric3 *matrix) {
    return isfinite(matrix->xx) && isfinite(matrix->xy) && isfinite(matrix->xz) &&
           isfinite(matrix->yy) && isfinite(matrix->yz) && isfinite(matrix->zz);
}

bool ab_symmetric3_inverse22(const AbSymmetric3 *input, AbSymmetric3 *output,
                             AbError *error) {
    if (!input || !output || !isfinite(input->xx) || !isfinite(input->xy) ||
        !isfinite(input->yy)) {
        return ab_fail(error, "Invalid symmetric 2D inverse inputs/storage");
    }
    float determinant = input->xy * input->xy;
    determinant = -determinant;
    determinant += input->xx * input->yy;
    if (determinant != 0.0f) {
        determinant = 1.0f / determinant;
    }
    float negative = -determinant;
    AbSymmetric3 result = {.xx = determinant * input->yy,
                           .xy = negative * input->xy,
                           .yy = determinant * input->xx};
    if (!finite_matrix(&result)) {
        return ab_fail(error, "Symmetric 2D inverse arithmetic overflow");
    }
    *output = result;
    return true;
}

bool ab_symmetric3_inverse(const AbSymmetric3 *input, AbSymmetric3 *output,
                           AbError *error) {
    if (!input || !output || !finite_matrix(input)) {
        return ab_fail(error, "Invalid symmetric 3D inverse inputs/storage");
    }
    float x = input->yy * input->zz;
    x -= input->yz * input->yz;
    float y = input->zz * input->xy;
    y = -y;
    y += input->yz * input->xz;
    float z = input->yy * input->xz;
    z = -z;
    z += input->yz * input->xy;
    float determinant = y * input->xy;
    determinant += x * input->xx;
    determinant += z * input->xz;
    if (determinant != 0.0f) {
        determinant = 1.0f / determinant;
    }
    float yy = input->xz * input->xz;
    yy = -yy;
    yy += input->zz * input->xx;
    float yz = input->yz * input->xx;
    yz = -yz;
    yz += input->xz * input->xy;
    float zz = input->xy * input->xy;
    zz = -zz;
    zz += input->yy * input->xx;
    AbSymmetric3 result = {.xx = x * determinant,
                           .xy = y * determinant,
                           .xz = determinant * z,
                           .yy = yy * determinant,
                           .yz = yz * determinant,
                           .zz = zz * determinant};
    if (!finite_matrix(&result)) {
        return ab_fail(error, "Symmetric 3D inverse arithmetic overflow");
    }
    *output = result;
    return true;
}
