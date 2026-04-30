/*
   pl_gjk_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <float.h>
#include "pl.h"
#include "pl_gjk_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

typedef struct _plGjkVertex
{
    plVec3 tMinkowski;
    plVec3 tShape1;
    plVec3 tShape2;
} plGjkVertex;

typedef struct _plGjkFace
{
    plVec3 tNormal;
    float  fDist;
    int    aiIndices[3];
} plGjkFace;

static plGjkVertex
pl__support_minkowski_diff(plGjkSupportFunc tFn1, const void* pShape1, plGjkSupportFunc tFn2, const void* pShape2, plVec3 tDir)
{
    plVec3 tNegDir = pl_mul_vec3_scalarf(tDir, -1.0f);
    plVec3 tVert1  = tFn1(pShape1, tDir);
    plVec3 tVert2  = tFn2(pShape2, tNegDir);
    plGjkVertex tResult = {
        .tMinkowski = pl_sub_vec3(tVert1, tVert2),
        .tShape1    = tVert1,
        .tShape2    = tVert2
    };
    return tResult;
}

static bool
pl__update_simplex(plGjkVertex atSimplex[4], int *piSimplexCount, plVec3 *ptDir)
{
    switch (*piSimplexCount) {
        case 2: { // line case
            plVec3 tA  = atSimplex[1].tMinkowski;
            plVec3 tB  = atSimplex[0].tMinkowski;
            plVec3 tAB = pl_sub_vec3(tB, tA);
            plVec3 tAO = pl_mul_vec3_scalarf(tA, -1.0f);

            if (pl_dot_vec3(tAB, tAO) > 0.0f) {
                *ptDir = pl_cross_vec3(pl_cross_vec3(tAB, tAO), tAB);
            } else {
                atSimplex[0] = atSimplex[1];
                *piSimplexCount = 1;
                *ptDir = tAO;
            }
            return false;
        }

        case 3: { // triangle case
            plGjkVertex tA = atSimplex[2];
            plGjkVertex tB = atSimplex[1];
            plGjkVertex tC = atSimplex[0];
            plVec3 tAB  = pl_sub_vec3(tB.tMinkowski, tA.tMinkowski);
            plVec3 tAC  = pl_sub_vec3(tC.tMinkowski, tA.tMinkowski);
            plVec3 tAO  = pl_mul_vec3_scalarf(tA.tMinkowski, -1.0f);
            plVec3 tABC = pl_cross_vec3(tAB, tAC);

            if (pl_dot_vec3(pl_cross_vec3(tABC, tAC), tAO) > 0.0f) {
                if (pl_dot_vec3(tAC, tAO) > 0.0f) {
                    atSimplex[0] = tC;
                    atSimplex[1] = tA;
                    *piSimplexCount = 2;
                    *ptDir = pl_cross_vec3(pl_cross_vec3(tAC, tAO), tAC);
                } else {
                    atSimplex[0] = tA;
                    *piSimplexCount = 1;
                    if (pl_dot_vec3(tAB, tAO) > 0.0f) {
                        atSimplex[1] = tB;
                        *piSimplexCount = 2;
                        *ptDir = pl_cross_vec3(pl_cross_vec3(tAB, tAO), tAB);
                    } else {
                        *ptDir = tAO;
                    }
                }
            } else {
                if (pl_dot_vec3(pl_cross_vec3(tAB, tABC), tAO) > 0.0f) {
                    atSimplex[0] = tA;
                    *piSimplexCount = 1;
                    if (pl_dot_vec3(tAB, tAO) > 0.0f) {
                        atSimplex[1] = tB;
                        *piSimplexCount = 2;
                        *ptDir = pl_cross_vec3(pl_cross_vec3(tAB, tAO), tAB);
                    } else {
                        *ptDir = tAO;
                    }
                } else {
                    if (pl_dot_vec3(tABC, tAO) > 0.0f) {
                        *ptDir = tABC;
                    } else {
                        // flip winding
                        atSimplex[0] = tB;
                        atSimplex[1] = tC;
                        *ptDir = pl_mul_vec3_scalarf(tABC, -1.0f);
                    }
                }
            }
            return false;
        }

        case 4: { // tetrahedron case
            plGjkVertex tA = atSimplex[3];
            plGjkVertex tB = atSimplex[2];
            plGjkVertex tC = atSimplex[1];
            plGjkVertex tD = atSimplex[0];
            plVec3 tAB  = pl_sub_vec3(tB.tMinkowski, tA.tMinkowski);
            plVec3 tAC  = pl_sub_vec3(tC.tMinkowski, tA.tMinkowski);
            plVec3 tAD  = pl_sub_vec3(tD.tMinkowski, tA.tMinkowski);
            plVec3 tAO  = pl_mul_vec3_scalarf(tA.tMinkowski, -1.0f);

            plVec3 tABC = pl_cross_vec3(tAB, tAC);
            plVec3 tACD = pl_cross_vec3(tAC, tAD);
            plVec3 tADB = pl_cross_vec3(tAD, tAB);

            if (pl_dot_vec3(tABC, tAO) > 0.0f) {
                atSimplex[0] = tC;
                atSimplex[1] = tB;
                atSimplex[2] = tA;
                *piSimplexCount = 3;
                *ptDir = tABC;
                return false;
            }

            if (pl_dot_vec3(tACD, tAO) > 0.0f) {
                atSimplex[0] = tD;
                atSimplex[1] = tC;
                atSimplex[2] = tA;
                *piSimplexCount = 3;
                *ptDir = tACD;
                return false;
            }

            if (pl_dot_vec3(tADB, tAO) > 0.0f) {
                atSimplex[0] = tB;
                atSimplex[1] = tD;
                atSimplex[2] = tA;
                *piSimplexCount = 3;
                *ptDir = tADB;
                return false;
            }

            // origin is inside the tetrahedron
            return true;
        }

        default:
            break;
    }
    return false;
}

static plGjkFace
pl__epa_make_face(const plGjkVertex* atVerts, int iA, int iB, int iC)
{
    plVec3 tAB = pl_sub_vec3(atVerts[iB].tMinkowski, atVerts[iA].tMinkowski);
    plVec3 tAC = pl_sub_vec3(atVerts[iC].tMinkowski, atVerts[iA].tMinkowski);
    plVec3 tN  = pl_norm_vec3(pl_cross_vec3(tAB, tAC));
    float  fD  = pl_dot_vec3(tN, atVerts[iA].tMinkowski);

    if (fD < 0.0f)
    {
        tN = pl_mul_vec3_scalarf(tN, -1.0f);
        fD = -fD;
        int iTemp = iB;
        iB = iC;
        iC = iTemp;
    }

    plGjkFace tFace = {
        .tNormal   = tN,
        .fDist     = fD,
        .aiIndices = {iA, iB, iC}
    };
    return tFace;
}

static void
pl__epa_compute_contact(const plGjkVertex* atVerts, const plGjkFace* ptFace, plGjkCollisionInfo* ptInfoOut)
{
    // project origin onto the closest face and compute barycentric coordinates
    plVec3 tA = atVerts[ptFace->aiIndices[0]].tMinkowski;
    plVec3 tB = atVerts[ptFace->aiIndices[1]].tMinkowski;
    plVec3 tC = atVerts[ptFace->aiIndices[2]].tMinkowski;

    plVec3 tAB = pl_sub_vec3(tB, tA);
    plVec3 tAC = pl_sub_vec3(tC, tA);

    // origin projected onto face plane
    plVec3 tP = pl_mul_vec3_scalarf(ptFace->tNormal, ptFace->fDist);
    plVec3 tAP = pl_sub_vec3(tP, tA);

    float fD00 = pl_dot_vec3(tAB, tAB);
    float fD01 = pl_dot_vec3(tAB, tAC);
    float fD11 = pl_dot_vec3(tAC, tAC);
    float fD20 = pl_dot_vec3(tAP, tAB);
    float fD21 = pl_dot_vec3(tAP, tAC);

    float fDenom = fD00 * fD11 - fD01 * fD01;
    if (fabsf(fDenom) < 1e-10f)
    {
        ptInfoOut->tPoint = atVerts[ptFace->aiIndices[0]].tShape1;
        return;
    }

    float fV = (fD11 * fD20 - fD01 * fD21) / fDenom;
    float fW = (fD00 * fD21 - fD01 * fD20) / fDenom;
    float fU = 1.0f - fV - fW;

    // interpolate shape1 support points using barycentric coordinates
    plVec3 tS1A = atVerts[ptFace->aiIndices[0]].tShape1;
    plVec3 tS1B = atVerts[ptFace->aiIndices[1]].tShape1;
    plVec3 tS1C = atVerts[ptFace->aiIndices[2]].tShape1;

    ptInfoOut->tPoint = pl_add_vec3(
        pl_add_vec3(
            pl_mul_vec3_scalarf(tS1A, fU),
            pl_mul_vec3_scalarf(tS1B, fV)),
        pl_mul_vec3_scalarf(tS1C, fW));
}

static void
pl__epa(plGjkSupportFunc tFn1, const void* pShape1,
        plGjkSupportFunc tFn2, const void* pShape2,
        plGjkVertex atSimplexEpa[4], plGjkCollisionInfo* ptInfoOut)
{
    plGjkVertex atVerts[64];
    atVerts[0] = atSimplexEpa[0];
    atVerts[1] = atSimplexEpa[1];
    atVerts[2] = atSimplexEpa[2];
    atVerts[3] = atSimplexEpa[3];
    int iVertCount = 4;

    plGjkFace atFaces[128];
    atFaces[0] = pl__epa_make_face(atVerts, 0, 1, 2);
    atFaces[1] = pl__epa_make_face(atVerts, 0, 3, 1);
    atFaces[2] = pl__epa_make_face(atVerts, 0, 2, 3);
    atFaces[3] = pl__epa_make_face(atVerts, 1, 3, 2);
    int iFaceCount = 4;

    for (int ii = 0; ii < 64; ii++)
    {
        // find closest face to origin
        int iClosest = 0;
        float fMinDist = atFaces[0].fDist;
        for (int jj = 1; jj < iFaceCount; jj++)
        {
            if (atFaces[jj].fDist < fMinDist)
            {
                fMinDist = atFaces[jj].fDist;
                iClosest = jj;
            }
        }

        plVec3 tNormal = atFaces[iClosest].tNormal;

        // get new support point along closest face normal
        plGjkVertex tSupport = pl__support_minkowski_diff(tFn1, pShape1, tFn2, pShape2, tNormal);
        float fSupportDist = pl_dot_vec3(tSupport.tMinkowski, tNormal);

        // converged
        if (fSupportDist - fMinDist < 1e-4f)
        {
            ptInfoOut->fPenetration = fMinDist;
            ptInfoOut->tNormal      = tNormal;
            pl__epa_compute_contact(atVerts, &atFaces[iClosest], ptInfoOut);

            // enforce convention: normal points toward shape1
            plVec3 tS2ToS1 = pl_sub_vec3(atVerts[atFaces[iClosest].aiIndices[0]].tShape1,
                                          atVerts[atFaces[iClosest].aiIndices[0]].tShape2);
            if (pl_dot_vec3(ptInfoOut->tNormal, tS2ToS1) < 0.0f)
                ptInfoOut->tNormal = pl_mul_vec3_scalarf(ptInfoOut->tNormal, -1.0f);
            return;
        }

        // add new vertex
        if (iVertCount >= 64) break;
        int iNewVert = iVertCount++;
        atVerts[iNewVert] = tSupport;

        // find and remove faces visible from the new point
        // collect unique edges from removed faces
        int aiEdges[256][2];
        int iEdgeCount = 0;

        for (int jj = iFaceCount - 1; jj >= 0; jj--)
        {
            plVec3 tToPoint = pl_sub_vec3(tSupport.tMinkowski, atVerts[atFaces[jj].aiIndices[0]].tMinkowski);
            if (pl_dot_vec3(atFaces[jj].tNormal, tToPoint) <= 0.0f)
                continue;

            // face is visible, collect edges
            for (int kk = 0; kk < 3; kk++)
            {
                int iA = atFaces[jj].aiIndices[kk];
                int iB = atFaces[jj].aiIndices[(kk + 1) % 3];

                // check if reverse edge already exists (shared edge)
                bool bFound = false;
                for (int ee = 0; ee < iEdgeCount; ee++)
                {
                    if (aiEdges[ee][0] == iB && aiEdges[ee][1] == iA)
                    {
                        aiEdges[ee][0] = aiEdges[iEdgeCount - 1][0];
                        aiEdges[ee][1] = aiEdges[iEdgeCount - 1][1];
                        iEdgeCount--;
                        bFound = true;
                        break;
                    }
                }
                if (!bFound && iEdgeCount < 256)
                {
                    aiEdges[iEdgeCount][0] = iA;
                    aiEdges[iEdgeCount][1] = iB;
                    iEdgeCount++;
                }
            }

            // remove face by swapping with last
            atFaces[jj] = atFaces[iFaceCount - 1];
            iFaceCount--;
        }

        // create new faces from horizon edges to new point
        for (int jj = 0; jj < iEdgeCount; jj++)
        {
            if (iFaceCount >= 128) break;
            atFaces[iFaceCount] = pl__epa_make_face(atVerts, aiEdges[jj][0], aiEdges[jj][1], iNewVert);
            iFaceCount++;
        }
    }

    // didn't converge, return best guess
    int iClosest = 0;
    float fMinDist = atFaces[0].fDist;
    for (int jj = 1; jj < iFaceCount; jj++)
    {
        if (atFaces[jj].fDist < fMinDist)
        {
            fMinDist = atFaces[jj].fDist;
            iClosest = jj;
        }
    }
    ptInfoOut->fPenetration = fMinDist;
    ptInfoOut->tNormal      = atFaces[iClosest].tNormal;
    pl__epa_compute_contact(atVerts, &atFaces[iClosest], ptInfoOut);

    // enforce convention: normal points toward shape1
    plVec3 tS2ToS1 = pl_sub_vec3(atVerts[atFaces[iClosest].aiIndices[0]].tShape1,
                                  atVerts[atFaces[iClosest].aiIndices[0]].tShape2);
    if (pl_dot_vec3(ptInfoOut->tNormal, tS2ToS1) < 0.0f)
        ptInfoOut->tNormal = pl_mul_vec3_scalarf(ptInfoOut->tNormal, -1.0f);
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_gjk_pen(plGjkSupportFunc tFn1, const void* pShape1, plGjkSupportFunc tFn2, const void* pShape2, plGjkCollisionInfo* ptInfoOut)
{
    // off-axis initial direction to avoid degenerate collinear simplexes
    plVec3 tDir          = (plVec3){1.0f, 0.57735f, 0.57735f};
    plGjkVertex atSimplex[4] = {0};

    atSimplex[0]  = pl__support_minkowski_diff(tFn1, pShape1, tFn2, pShape2, tDir);
    int iSimplexCount = 1;
    tDir = pl_mul_vec3_scalarf(atSimplex[0].tMinkowski, -1.0f);

    const int iMaxIterations = 64;
    for (int ii = 0; ii < iMaxIterations; ii++) {
        plGjkVertex tNewVert = pl__support_minkowski_diff(tFn1, pShape1, tFn2, pShape2, tDir);

        if (pl_dot_vec3(tNewVert.tMinkowski, tDir) < 0.0f) {
            return false;
        }

        atSimplex[iSimplexCount++] = tNewVert;

        if (pl__update_simplex(atSimplex, &iSimplexCount, &tDir)) {
            if (ptInfoOut) {
                pl__epa(tFn1, pShape1, tFn2, pShape2, atSimplex, ptInfoOut);
            }
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_gjk_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGjkI tApi = {
        .pen = pl_gjk_pen,
    };
    pl_set_api(ptApiRegistry, plGjkI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

void
pl_unload_gjk_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if (bReload)
        return;

    const plGjkI* ptApi = pl_get_api_latest(ptApiRegistry, plGjkI);
    ptApiRegistry->remove_api(ptApi);
}