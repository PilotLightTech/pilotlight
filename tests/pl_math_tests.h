#include "pl_test.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

void
math_scalar_matrix_tests(void* pData)
{
    plMat3 tMat3 = PL_ZERO_INIT;
    plMat4 tMat4 = PL_ZERO_INIT;
    plMat3d tMat3d = PL_ZERO_INIT;
    plMat4d tMat4d = PL_ZERO_INIT;
    for(int i = 0; i < 16; i++)
    {
        tMat4.d[i] = (float)(i - 7);
        tMat4d.d[i] = (double)(i - 7);
        if(i < 9)
        {
            tMat3.d[i] = (float)(i - 4);
            tMat3d.d[i] = (double)(i - 4);
        }
    }

    // Nonsymmetric inputs expose a transpose; exact integers avoid tolerance noise.
    const int aiScalars[] = {2, -3, 0};
    for(int i = 0; i < 3; i++)
    {
        const float fScalar = (float)aiScalars[i];
        const double dScalar = (double)aiScalars[i];
        plMat3 tResult3 = pl_mul_scalarf_mat3(fScalar, &tMat3);
        plMat4 tResult4 = pl_mul_scalarf_mat4(fScalar, &tMat4);
        plMat3d tResult3d = pl_mul_scalar_mat3(dScalar, &tMat3d);
        plMat4d tResult4d = pl_mul_scalar_mat4(dScalar, &tMat4d);
        for(int j = 0; j < 16; j++)
        {
            pl_test_expect_true(tResult4.d[j] == fScalar * tMat4.d[j], "float 4x4 scalar preserves element position");
            pl_test_expect_true(tResult4d.d[j] == dScalar * tMat4d.d[j], "double 4x4 scalar preserves element position");
            if(j < 9)
            {
                pl_test_expect_true(tResult3.d[j] == fScalar * tMat3.d[j], "float 3x3 scalar preserves element position");
                pl_test_expect_true(tResult3d.d[j] == dScalar * tMat3d.d[j], "double 3x3 scalar preserves element position");
            }
        }
    }
}

void
pl_math_tests(void* pData)
{
    pl_test_register_test(math_scalar_matrix_tests, pData);
}
