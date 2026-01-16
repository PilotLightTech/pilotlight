/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D & 3D)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// standard
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

// pilot light
#include "pl.h"
#include "pl_memory.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_icons.h"
#include "pl_json.h"

// stable extensions
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_log_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_tools_ext.h"
#include "pl_job_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_shader_ext.h"
#include "pl_string_intern_ext.h"
#include "pl_platform_ext.h"
#include "pl_console_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_starter_ext.h"
#include "pl_pak_ext.h"
#include "pl_datetime_ext.h"

// unstable extensions
#include "pl_ecs_ext.h"
#include "pl_material_ext.h"
#include "pl_mesh_ext.h"
#include "pl_camera_ext.h"
#include "pl_animation_ext.h"
#include "pl_config_ext.h"
#include "pl_resource_ext.h"
#include "pl_model_loader_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ecs_tools_ext.h"
#include "pl_gizmo_ext.h"
#include "pl_physics_ext.h"
#include "pl_collision_ext.h"
#include "pl_bvh_ext.h"
#include "pl_shader_variant_ext.h"
#include "pl_vfs_ext.h"
#include "pl_compress_ext.h"
#include "pl_script_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_starter_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plHeightMapElement
{
    uint32_t uX;
    uint32_t uZ;
    float  fX;
    float  fY;
    float  fZ;
    float  fError;
    int8_t iActivationLevel;
} plHeightMapElement;

typedef struct _plHeightMapInfo
{
    plHeightMapElement tElement;
    int i;
    int j;
} plHeightMapInfo;

typedef struct _plHeightMap
{
    int                 iSize;
    int                 iLogSize;
    float               fSampleSpacing;
    plHeightMapElement* atElements;


    plHeightMapInfo* sbtActivatedElements;
} plHeightMap;



void
activate_height_map_element(plHeightMapElement* ptElement, int iLevel)
{
    if(iLevel > ptElement->iActivationLevel)
        ptElement->iActivationLevel = (int8_t)iLevel;
}

plHeightMapElement*
get_elem(plHeightMap* ptHeightMap, int x, int z)
{
    return &ptHeightMap->atElements[x + z * ptHeightMap->iSize];
}

static int	lowest_one(int x)
// Returns the bit position of the lowest 1 bit in the given value.
// If x == 0, returns the number of bits in an integer.
//
// E.g. lowest_one(1) == 0; lowest_one(16) == 4; lowest_one(5) == 0;
{
	int	intbits = sizeof(x) * 8;
	int	i;
	for (i = 0; i < intbits; i++, x = x >> 1) {
		if (x & 1) break;
	}
	return i;
}

// Given the coordinates of the center of a quadtree node, this
// function returns its node index.  The node index is essentially
// the node's rank in a breadth-first quadtree traversal.  Assumes
// a [nw, ne, sw, se] traversal order.
//
// If the coordinates don't specify a valid node (e.g. if the coords
// are outside the heightfield) then returns -1.

int
node_index(plHeightMap* ptHeightMap, int x, int z)
{
    if (x < 0 || x >= ptHeightMap->iSize || z < 0 || z >= ptHeightMap->iSize) {
        return -1;
    }

    int	l1 = lowest_one(x | z);
    int	depth = ptHeightMap->iLogSize - l1 - 1;

    int	base = 0x55555555 & ((1 << depth*2) - 1);	// total node count in all levels above ours.
    int	shift = l1 + 1;

    // Effective coords within this node's level.
    int	col = x >> shift;
    int	row = z >> shift;

    return base + (row << depth) + col;
}

int
vertex_index(plHeightMap* ptHeightMap, int x, int z)
{
    if (x < 0 || x >= ptHeightMap->iSize || z < 0 || z >= ptHeightMap->iSize) {
        return -1;
    }

    return ptHeightMap->iSize * z + x;
}

// Given the triangle, computes an error value and activation level
// for its base vertex, and recurses to child triangles.
void update(plHeightMap* ptHeightMap, float base_max_error, int ax, int az, int rx, int rz, int lx, int lz);
void propagate_activation_level(plHeightMap* ptHeightMap, int cx, int cz, int level, int target_level);

void
update(plHeightMap* ptHeightMap, float base_max_error, int ax, int az, int rx, int rz, int lx, int lz)
{
	// Compute the coordinates of this triangle's base vertex.
	int	dx = lx - rx;
	int	dz = lz - rz;
	if (abs(dx) <= 1 && abs(dz) <= 1) {
		// We've reached the base level.  There's no base
		// vertex to update, and no child triangles to
		// recurse to.
		return;
	}

	// base vert is midway between left and right verts.
	int	bx = rx + (dx >> 1);
	int	bz = rz + (dz >> 1);

	float error = get_elem(ptHeightMap, bx, bz)->fY - (get_elem(ptHeightMap, lx, lz)->fY + get_elem(ptHeightMap, rx, rz)->fY) / 2.f;
	get_elem(ptHeightMap, bx, bz)->fError = error;	// Set this vert's error value.
	if (fabs(error) >= base_max_error) {
		// Compute the mesh level above which this vertex
		// needs to be included in LOD meshes.
		int	activation_level = (int) floor(log2(fabs(error) / base_max_error) + 0.5);

		// Force the base vert to at least this activation level.
		plHeightMapElement* ptElem = get_elem(ptHeightMap, bx, bz);
        activate_height_map_element(ptElem, activation_level);
	}

	// Recurse to child triangles.
	update(ptHeightMap, base_max_error, bx, bz, ax, az, rx, rz);	// base, apex, right
	update(ptHeightMap, base_max_error, bx, bz, lx, lz, ax, az);	// base, left, apex
}

// Does a quadtree descent through the heightfield, in the square with
// center at (cx, cz) and size of (2 ^ (level + 1) + 1).  Descends
// until level == target_level, and then propagates this square's
// child center verts to the corresponding edge vert, and the edge
// verts to the center.  Essentially the quadtree meshing update
// dependency graph as in my Gamasutra article.  Must call this with
// successively increasing target_level to get correct propagation.
void
propagate_activation_level(plHeightMap* ptHeightMap, int cx, int cz, int level, int target_level)
{
	int	half_size = 1 << level;
	int	quarter_size = half_size >> 1;

	if (level > target_level) {
		// Recurse to children.
		for (int j = 0; j < 2; j++) {
			for (int i = 0; i < 2; i++) {
				propagate_activation_level(ptHeightMap,
							   cx - quarter_size + half_size * i,
							   cz - quarter_size + half_size * j,
							   level - 1, target_level);
			}
		}
		return;
	}

	// We're at the target level.  Do the propagation on this
	// square.

	// ee == east edge, en = north edge, etc
	plHeightMapElement*	ee = get_elem(ptHeightMap, cx + half_size, cz);
	plHeightMapElement*	en = get_elem(ptHeightMap, cx, cz - half_size);
	plHeightMapElement*	ew = get_elem(ptHeightMap, cx - half_size, cz);
	plHeightMapElement*	es = get_elem(ptHeightMap, cx, cz + half_size);
	
	if (level > 0) {
		// Propagate child verts to edge verts.
		int	elev = get_elem(ptHeightMap, cx + quarter_size, cz - quarter_size)->iActivationLevel;	// ne
        activate_height_map_element(ee, elev);
        activate_height_map_element(en, elev);

		elev = get_elem(ptHeightMap, cx - quarter_size, cz - quarter_size)->iActivationLevel;	// nw
		activate_height_map_element(en, elev);
		activate_height_map_element(ew, elev);

		elev = get_elem(ptHeightMap, cx - quarter_size, cz + quarter_size)->iActivationLevel;	// sw
		activate_height_map_element(ew, elev);
		activate_height_map_element(es, elev);

		elev = get_elem(ptHeightMap, cx + quarter_size, cz + quarter_size)->iActivationLevel;	// se
		activate_height_map_element(es, elev);
		activate_height_map_element(ee, elev);
	}

	// Propagate edge verts to center.
	plHeightMapElement*	c = get_elem(ptHeightMap, cx, cz);
	activate_height_map_element(c, ee->iActivationLevel);
	activate_height_map_element(c, en->iActivationLevel);
	activate_height_map_element(c, es->iActivationLevel);
	activate_height_map_element(c, ew->iActivationLevel);
}

typedef struct _plTrianglePrimitive
{
    int      iLevel;
    uint32_t uApex;
    uint32_t uLeft;
    uint32_t uRight;
} plTrianglePrimitive;

typedef struct _plAppData
{
    plMeshBuilder* ptMeshBuilder;
    plTrianglePrimitive* sbtPrimitives;

    // window
    plWindow* ptWindow;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // shaders
    plShaderHandle tShader;
    plShaderHandle tWireframeShader;

    // buffers
    uint32_t uIndexCount;
    plBufferHandle tStagingBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tVertexBuffer;

    // heightmap
    plHeightMap tHeightMap;

    uint16_t* auHeightMapData;

    // visual options
    bool bShowDebugPoints;
    bool bShowOrigin;
    bool bWireframe;
    bool bShowHeightmap;
    int iActivationLevel;

    // meshing


} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plWindowI*            gptWindows       = NULL;
const plStatsI*             gptStats         = NULL;
const plGraphicsI*          gptGfx           = NULL;
const plToolsI*             gptTools         = NULL;
const plEcsI*               gptEcs           = NULL;
const plCameraI*            gptCamera        = NULL;
const plRendererI*          gptRenderer      = NULL;
const plModelLoaderI*       gptModelLoader   = NULL;
const plJobI*               gptJobs          = NULL;
const plDrawI*              gptDraw          = NULL;
const plUiI*                gptUI            = NULL;
const plIOI*                gptIO            = NULL;
const plShaderI*            gptShader        = NULL;
const plMemoryI*            gptMemory        = NULL;
const plNetworkI*           gptNetwork       = NULL;
const plStringInternI*      gptString        = NULL;
const plProfileI*           gptProfile       = NULL;
const plFileI*              gptFile          = NULL;
const plEcsToolsI*          gptEcsTools      = NULL;
const plGizmoI*             gptGizmo         = NULL;
const plConsoleI*           gptConsole       = NULL;
const plScreenLogI*         gptScreenLog     = NULL;
const plPhysicsI *          gptPhysics       = NULL;
const plCollisionI*         gptCollision     = NULL;
const plBVHI*               gptBvh           = NULL;
const plConfigI*            gptConfig        = NULL;
const plResourceI*          gptResource      = NULL;
const plStarterI*           gptStarter       = NULL;
const plAnimationI*         gptAnimation     = NULL;
const plMeshI*              gptMesh          = NULL;
const plShaderVariantI*     gptShaderVariant = NULL;
const plVfsI*               gptVfs           = NULL;
const plPakI*               gptPak           = NULL;
const plDateTimeI*          gptDateTime      = NULL;
const plCompressI*          gptCompress      = NULL;
const plMaterialI*          gptMaterial      = NULL;
const plScriptI*            gptScript        = NULL;
const plMeshBuilderI*       gptMeshBuilder   = NULL;
const plImageI*             gptImage         = NULL;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
#define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void pl__load_apis(plApiRegistryI*);

uint16_t pl__sample_height(plAppData* ptAppData, uint32_t x, uint32_t y)
{
    if(x > (uint32_t)ptAppData->tHeightMap.iSize - 1 || y > (uint32_t)ptAppData->tHeightMap.iSize - 1)
    {
        return 0;
    }
    return ptAppData->auHeightMapData[y * ptAppData->tHeightMap.iSize + x];
}


//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        pl__load_apis(ptApiRegistry);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->bShowHeightmap = true;
    ptAppData->iActivationLevel = -1;

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    pl__load_apis(ptApiRegistry);

    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_REVERSE_Z;
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/",
            "../examples/shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE | PL_SHADER_FLAGS_INCLUDE_DEBUG
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    ptAppData->tCamera = (plCamera){
        .tPos         = {5.0f, 10.0f, 10.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 5000.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = 1.0f,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
        .tType        = PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z
    };
    gptCamera->update(&ptAppData->tCamera);

    // for convience
    plDevice* ptDevice = gptStarter->get_device();

    ptAppData->tHeightMap.fSampleSpacing = 1.0f;
    ptAppData->tHeightMap.iSize = 513;
    ptAppData->tHeightMap.iLogSize = (int) (log2(ptAppData->tHeightMap.iSize - 1) + 0.5f);

    size_t szHeightMapSize = ptAppData->tHeightMap.iSize * ptAppData->tHeightMap.iSize * sizeof(uint16_t);
    ptAppData->auHeightMapData = PL_ALLOC(szHeightMapSize);

    size_t szFileSize = 0;
    gptFile->binary_read("../data/mountains.png", &szFileSize, NULL);
    uint8_t* puFileData = PL_ALLOC(szFileSize + 1);
    memset(puFileData, 0, szFileSize + 1);
    gptFile->binary_read("../data/mountains.png", &szFileSize, puFileData);
    int iWidth = 0;
    int iHeight = 0;
    int iChannels = 0;
    uint8_t* puData = gptImage->load(puFileData, (int)szFileSize, &iWidth, &iHeight, &iChannels, 1);

    plVec2 tCenter = {(float)ptAppData->tHeightMap.iSize / 2.0f, (float)ptAppData->tHeightMap.iSize / 2.0f};
    float fMaxHeight = 20.0f;
    float fMinHeight = 0.0f;
    for(uint32_t i = 0; i < (uint32_t)ptAppData->tHeightMap.iSize - 1; i++)
    {
        for(uint32_t j = 0; j < (uint32_t)ptAppData->tHeightMap.iSize - 1; j++)
        {
            // plVec2 tPoint = {(float)i, (float)j};
            // float fHeight = pl_length_vec2(pl_sub_vec2(tPoint, tCenter));
            // ptAppData->auHeightMapData[j * ptAppData->tHeightMap.iSize + i] = (uint16_t)(((fHeight - fMinHeight) / (fMaxHeight - fMinHeight)) * 65535.0f);

            ptAppData->auHeightMapData[j * ptAppData->tHeightMap.iSize + i] = (uint16_t)(((puData[i + j * iWidth] - fMinHeight) / (fMaxHeight - fMinHeight)) * 255.0f);
        }
    }



    float base_max_error = 0.1f;
    // int	tree_depth = 6;

    // Expand the heightfield dimension to contain the bitmap.
    while (((1 << ptAppData->tHeightMap.iLogSize) + 1) < ptAppData->tHeightMap.iSize)
        ptAppData->tHeightMap.iLogSize++;
    ptAppData->tHeightMap.iSize = (1 << ptAppData->tHeightMap.iLogSize) + 1;

    // Allocate storage.
    int	iSampleCount = ptAppData->tHeightMap.iSize * ptAppData->tHeightMap.iSize;
    ptAppData->tHeightMap.atElements = PL_ALLOC(iSampleCount * sizeof(plHeightMapElement));
    memset(ptAppData->tHeightMap.atElements, 0, iSampleCount * sizeof(plHeightMapElement));
    
    // Initialize the data.
    for (int i = 0; i < ptAppData->tHeightMap.iSize; i++)
    {
        for (int j = 0; j < ptAppData->tHeightMap.iSize; j++)
        {
            float y = 0;

            // Extract a height value from the pixel data.
            uint16_t r = pl__sample_height(ptAppData, pl_min(i, ptAppData->tHeightMap.iSize - 1), pl_min(j, ptAppData->tHeightMap.iSize - 1));
            // y = r * 1.0f;	// just using red component for now.
            // y = (float)r / 65355.0f;	// just using red component for now.
            y = (float)r / 255.0f;	// just using red component for now.
            y *= (fMaxHeight - fMinHeight);
            y += fMinHeight;

            // int iElementIndex = i + (ptAppData->tHeightMap.iSize - 1 - j) * ptAppData->tHeightMap.iSize;
            int iElementIndex = i + ptAppData->tHeightMap.iSize * j;

            ptAppData->tHeightMap.atElements[iElementIndex].iActivationLevel = -1;
            ptAppData->tHeightMap.atElements[iElementIndex].fX = (float)i * 10.0f;
            ptAppData->tHeightMap.atElements[iElementIndex].fY = y;
            ptAppData->tHeightMap.atElements[iElementIndex].fZ = (float)j * 10.0f;
            ptAppData->tHeightMap.atElements[iElementIndex].uX = i;
            ptAppData->tHeightMap.atElements[iElementIndex].uZ = j;
        }
    }

    printf("updating...\n");

	// Run a view-independent L-K style BTT update on the heightfield, to generate
	// error and activation_level values for each element.

	update(&ptAppData->tHeightMap, base_max_error, 0, ptAppData->tHeightMap.iSize-1, ptAppData->tHeightMap.iSize-1, ptAppData->tHeightMap.iSize-1, 0, 0);	// sw half of the square
	update(&ptAppData->tHeightMap, base_max_error, ptAppData->tHeightMap.iSize-1, 0, 0, 0, ptAppData->tHeightMap.iSize-1, ptAppData->tHeightMap.iSize-1);	// ne half of the square

    printf("propagating...\n");

	// Propagate the activation_level values of verts to their
	// parent verts, quadtree LOD style.  Gives same result as
	// L-K.
	for (int i = 0; i < ptAppData->tHeightMap.iLogSize; i++)
    {
		propagate_activation_level(&ptAppData->tHeightMap, ptAppData->tHeightMap.iSize >> 1, ptAppData->tHeightMap.iSize >> 1, ptAppData->tHeightMap.iLogSize - 1, i);
		propagate_activation_level(&ptAppData->tHeightMap, ptAppData->tHeightMap.iSize >> 1, ptAppData->tHeightMap.iSize >> 1, ptAppData->tHeightMap.iLogSize - 1, i);
	}

    printf("meshing...\n");

    pl_sb_reserve(ptAppData->tHeightMap.sbtActivatedElements, ptAppData->tHeightMap.iSize * ptAppData->tHeightMap.iSize);
    for(uint32_t i = 0; i < (uint32_t)ptAppData->tHeightMap.iSize; i++)
    {
        for(uint32_t j = 0; j < (uint32_t)ptAppData->tHeightMap.iSize; j++)
        {
            plHeightMapElement* ptElement = get_elem(&ptAppData->tHeightMap, i, j);

            // if(ptElement->iActivationLevel > -1)
            {
                plHeightMapInfo tInfo2 = {
                    .tElement = *ptElement,
                    .i = i,
                    .j = j
                };
                pl_sb_push(ptAppData->tHeightMap.sbtActivatedElements, tInfo2);
            }


        }
    }

    int iLevel = 0;

    // plTrianglePrimitive tPrimitiveRoot0 = {
    //     .iLevel = 0,
    //     .uApex = (uint32_t)vertex_index(&ptAppData->tHeightMap, ptAppData->tHeightMap.iSize - 1, 0),
    //     .uLeft = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, 0),
    //     .uRight = (uint32_t)vertex_index(&ptAppData->tHeightMap, ptAppData->tHeightMap.iSize - 1, ptAppData->tHeightMap.iSize - 1)
    // };

    // plTrianglePrimitive tPrimitiveRoot1 = {
    //     .iLevel = 0,
    //     .uApex = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, ptAppData->tHeightMap.iSize - 1),
    //     .uLeft = (uint32_t)vertex_index(&ptAppData->tHeightMap, ptAppData->tHeightMap.iSize - 1, ptAppData->tHeightMap.iSize - 1),
    //     .uRight = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, 0)
    // };

    // int iStartIndexBlah = (ptAppData->tHeightMap.iSize - 1) / 2;
    int iStartIndexBlah = (ptAppData->tHeightMap.iSize - 1);

    activate_height_map_element(get_elem(&ptAppData->tHeightMap, 0, 0), iLevel);
    activate_height_map_element(get_elem(&ptAppData->tHeightMap, 0, iStartIndexBlah), iLevel);
    activate_height_map_element(get_elem(&ptAppData->tHeightMap, iStartIndexBlah, iStartIndexBlah), iLevel);
    activate_height_map_element(get_elem(&ptAppData->tHeightMap, iStartIndexBlah, 0), iLevel);

    plTrianglePrimitive tPrimitiveRoot0 = {
        .iLevel = iLevel,
        .uApex = (uint32_t)vertex_index(&ptAppData->tHeightMap, iStartIndexBlah, 0),
        .uLeft = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, 0),
        .uRight = (uint32_t)vertex_index(&ptAppData->tHeightMap, iStartIndexBlah, iStartIndexBlah)
    };

    plTrianglePrimitive tPrimitiveRoot1 = {
        .iLevel = iLevel,
        .uApex = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, iStartIndexBlah),
        .uLeft = (uint32_t)vertex_index(&ptAppData->tHeightMap, iStartIndexBlah, iStartIndexBlah),
        .uRight = (uint32_t)vertex_index(&ptAppData->tHeightMap, 0, 0)
    };

    pl_sb_push(ptAppData->sbtPrimitives, tPrimitiveRoot0);
    pl_sb_push(ptAppData->sbtPrimitives, tPrimitiveRoot1);

    ptAppData->ptMeshBuilder = gptMeshBuilder->create((plMeshBuilderOptions){.fWeldRadius = 0.001f});

    
    // int iMaxLevel = ptAppData->tHeightMap.iLogSize * 2 - iLevel;
    int iMaxLevel = 15;
    // int iMaxLevel = 2;

    while(pl_sb_size(ptAppData->sbtPrimitives) > 0)
    {

        plTrianglePrimitive tPrimitive = pl_sb_pop(ptAppData->sbtPrimitives);



        plHeightMapElement* ptElement0 = &ptAppData->tHeightMap.atElements[tPrimitive.uApex];
        plHeightMapElement* ptElement1 = &ptAppData->tHeightMap.atElements[tPrimitive.uRight];
        plHeightMapElement* ptElement2 = &ptAppData->tHeightMap.atElements[tPrimitive.uLeft];

        bool bSubmit = false;
        bool bSplit = false;

        if(tPrimitive.iLevel >= iMaxLevel)
            bSubmit = true;

        int iMidPointX = ((int)ptElement1->uX + (int)ptElement2->uX) / 2;
        int iMidPointZ = ((int)ptElement1->uZ + (int)ptElement2->uZ) / 2;
        uint32_t uMidPoint = vertex_index(&ptAppData->tHeightMap, iMidPointX, iMidPointZ);

        plHeightMapElement* ptMidElement = get_elem(&ptAppData->tHeightMap, iMidPointX, iMidPointZ);
        
        if(!bSubmit && ptMidElement->iActivationLevel >= iLevel)
            bSplit = true;
        else
            bSubmit = true;


        if(bSplit)
        {
            
            plTrianglePrimitive tPrimitive0 = {
                .iLevel = tPrimitive.iLevel + 1,
                .uApex = uMidPoint,
                .uLeft = tPrimitive.uApex,
                .uRight = tPrimitive.uLeft
            };

            plTrianglePrimitive tPrimitive1 = {
                .iLevel = tPrimitive.iLevel + 1,
                .uApex = uMidPoint,
                .uLeft = tPrimitive.uRight,
                .uRight = tPrimitive.uApex
            };
            pl_sb_push(ptAppData->sbtPrimitives, tPrimitive0);
            pl_sb_push(ptAppData->sbtPrimitives, tPrimitive1);

            bSubmit = false;
        }

        if(bSubmit)
        {

            gptMeshBuilder->add_triangle(ptAppData->ptMeshBuilder, 
                (plVec3){
                    ptElement0->fX,
                    ptElement0->fY,
                    ptElement0->fZ
                },
                (plVec3){
                    ptElement1->fX,
                    ptElement1->fY,
                    ptElement1->fZ
                },
                (plVec3){
                    ptElement2->fX,
                    ptElement2->fY,
                    ptElement2->fZ
                }
            );
        }
    }

    // int blah = node_index(&ptAppData->tHeightMap, 0, 0);

    // generate_node_data(out, hf, 0, 0, hf.log_size, tree_depth-1);

    printf("done\n");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~vertex buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // vertex buffer data
    // const float atVertexData[] = { // x, y, z, r, g, b, a
    //     -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    //      0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
    //      0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    //     -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f
    // };

    uint32_t uVertexCount = 0;
    gptMeshBuilder->commit(ptAppData->ptMeshBuilder, NULL, NULL, &ptAppData->uIndexCount, &uVertexCount);

    plVec3* atVertexData = PL_ALLOC(uVertexCount * sizeof(plVec3));
    uint32_t* atIndexData = PL_ALLOC(ptAppData->uIndexCount * sizeof(uint32_t));
    gptMeshBuilder->commit(ptAppData->ptMeshBuilder, atIndexData, atVertexData, &ptAppData->uIndexCount, &uVertexCount);

    // plVec3* atVertexData = PL_ALLOC(ptAppData->tHeightMap.iSize * ptAppData->tHeightMap.iSize * sizeof(float) * 3);
    // ptAppData->uIndexCount = (ptAppData->tHeightMap.iSize - 1) * (ptAppData->tHeightMap.iSize - 1) * 2 * 3;
    // uint32_t* atIndexData = PL_ALLOC(ptAppData->uIndexCount * sizeof(uint32_t));

    // for(uint32_t i = 0; i < (uint32_t)ptAppData->tHeightMap.iSize; i++)
    // {
    //     for(uint32_t j = 0; j < (uint32_t)ptAppData->tHeightMap.iSize; j++)
    //     {
    //         atVertexData[i * ptAppData->tHeightMap.iSize + j].x = (float)j;
    //         atVertexData[i * ptAppData->tHeightMap.iSize + j].z = (float)i;

    //         atVertexData[i * ptAppData->tHeightMap.iSize + j].y = (float)pl__sample_height(ptAppData, j, i) / 65355.0f;	// just using red component for now.
    //         atVertexData[i * ptAppData->tHeightMap.iSize + j].y *= (fMaxHeight + fMinHeight);
    //         atVertexData[i * ptAppData->tHeightMap.iSize + j].y += fMinHeight;
            
    //     }
    // }

    // uint32_t uQuadCount = (uint32_t)ptAppData->tHeightMap.iSize - 1;
    // uint32_t uIndexCounter = 0;
    // for(uint32_t i = 0; i < uQuadCount; i++)
    // {
    //     for(uint32_t j = 0; j < uQuadCount; j++)
    //     {
    //         atIndexData[uIndexCounter + 0] = j + i * (uint32_t)ptAppData->tHeightMap.iSize;
    //         atIndexData[uIndexCounter + 1] = atIndexData[uIndexCounter + 0] + (uint32_t)ptAppData->tHeightMap.iSize;
    //         atIndexData[uIndexCounter + 2] = atIndexData[uIndexCounter + 1] + 1;

    //         atIndexData[uIndexCounter + 3] = atIndexData[uIndexCounter + 0];
    //         atIndexData[uIndexCounter + 4] = atIndexData[uIndexCounter + 2];
    //         atIndexData[uIndexCounter + 5] = atIndexData[uIndexCounter + 0] + 1;

    //         uIndexCounter += 6;
    //     }
    // }


    // create vertex buffer
    const plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = uVertexCount * sizeof(plVec3),
        .pcDebugName = "vertex buffer"
    };
    ptAppData->tVertexBuffer = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tVertexBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tVertexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "vertex buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tVertexBuffer, &tVertexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~index buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // index buffer data

    // create index buffer
    const plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX | PL_BUFFER_USAGE_TRANSFER_DESTINATION,
        .szByteSize  = ptAppData->uIndexCount * sizeof(uint32_t),
        .pcDebugName = "index buffer"
    };
    ptAppData->tIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tIndexBuffer);

    // allocate memory for the index buffer
    const plDeviceMemoryAllocation tIndexBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "index buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tIndexBuffer, &tIndexBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~staging buffer~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create vertex buffer
    const plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_TRANSFER_SOURCE,
        .szByteSize  = tIndexBufferDesc.szByteSize + tVertexBufferDesc.szByteSize,
        .pcDebugName = "staging buffer"
    };
    ptAppData->tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, NULL);

    // retrieve buffer to get memory allocation requirements (do not store buffer pointer)
    plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_HOST_VISIBLE | PL_MEMORY_FLAGS_HOST_COHERENT,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tStagingBuffer, &tStagingBufferAllocation);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~transfers~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // copy memory to mapped staging buffer
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, atVertexData, tVertexBufferDesc.szByteSize);
    memcpy(&ptStagingBuffer->tMemoryAllocation.pHostMapped[tVertexBufferDesc.szByteSize], atIndexData, tIndexBufferDesc.szByteSize);

    // begin blit pass, copy buffer, end pass
    // NOTE: we are using the starter extension to get a blit encoder, later examples we will
    //       handle this ourselves
    plBlitEncoder* ptEncoder = gptStarter->get_blit_encoder();
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tVertexBuffer, 0, 0, tVertexBufferDesc.szByteSize);
    gptGfx->copy_buffer(ptEncoder, ptAppData->tStagingBuffer, ptAppData->tIndexBuffer, (uint32_t)tVertexBufferDesc.szByteSize, 0, tIndexBufferDesc.szByteSize);
    gptStarter->return_blit_encoder(ptEncoder);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    plShaderDesc tShaderDesc = {
        .tVertexShader    = gptShader->load_glsl("example_gfx_0.vert", "main", NULL, NULL),
        .tFragmentShader  = gptShader->load_glsl("example_gfx_0.frag", "main", NULL, NULL),
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_GREATER_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .atVertexBufferLayouts = {
            {
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3}
                }
            }
        },
        .atBlendStates = {
            {
                .bBlendEnabled   = false,
                .uColorWriteMask = PL_COLOR_WRITE_MASK_ALL
            }
        },
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
    };
    ptAppData->tShader = gptGfx->create_shader(ptDevice, &tShaderDesc);

    tShaderDesc.tGraphicsState.ulWireframe = 1;
    tShaderDesc.tGraphicsState.ulDepthWriteEnabled = 0;
    tShaderDesc.tGraphicsState.ulDepthMode = PL_COMPARE_MODE_ALWAYS;
    ptAppData->tWireframeShader = gptGfx->create_shader(ptDevice, &tShaderDesc);

    // return app memory
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();

    // ensure the GPU is done with our resources
    gptGfx->flush_device(ptDevice);

    // cleanup our resources
    gptGfx->destroy_buffer(ptDevice, ptAppData->tVertexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tStagingBuffer);

    gptShader->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // for convience
    plIO* ptIO = gptIO->get_io();

    static const float fCameraTravelSpeed = 50.0f;
    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    if(gptIO->is_key_down(PL_KEY_R)) { gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        gptCamera->rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    gptCamera->update(ptCamera);

    // 3d drawing API usage
    if(ptAppData->bShowOrigin)
    {
        const plMat4 tOrigin = pl_identity_mat4();
        gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, (plDrawLineOptions){.fThickness = 0.2f});
    }

    if(ptAppData->bShowDebugPoints)
    {
        for(uint32_t i = 0; i < pl_sb_size(ptAppData->tHeightMap.sbtActivatedElements); i++)
        {
            if(ptAppData->tHeightMap.sbtActivatedElements[i].tElement.iActivationLevel >= ptAppData->iActivationLevel)
            {
                gptDraw->add_3d_cross(ptAppData->pt3dDrawlist,
                    (plVec3){
                        (float)ptAppData->tHeightMap.sbtActivatedElements[i].i / 1.0f,
                        // (float)ptAppData->tHeightMap.sbtActivatedElements[i].tElement.iActivationLevel,
                        (float)ptAppData->tHeightMap.sbtActivatedElements[i].tElement.fY,
                        // 0.0f,
                        (float)ptAppData->tHeightMap.sbtActivatedElements[i].j / 1.0f},
                        0.1f,
                        (plDrawLineOptions){
                            .fThickness = 0.1f,
                            .uColor = PL_COLOR_32_GREEN
                            // .uColor = PL_COLOR_32_RGBA(ptAppData->tHeightMap.sbtActivatedElements[i].tElement.iActivationLevel / 20.0f, 0.0f, 0.0f, 1.0f)
                        }
                        );
                }
        }
    }

    if(gptUI->begin_window("Debug", NULL, 0))
    {
        gptUI->checkbox("Show Debug", &ptAppData->bShowDebugPoints);
        gptUI->checkbox("Show Origin", &ptAppData->bShowOrigin);
        gptUI->checkbox("Wireframe", &ptAppData->bWireframe);
        gptUI->checkbox("Show Heightmap", &ptAppData->bShowHeightmap);
        gptUI->input_int("Activation", &ptAppData->iActivationLevel, 0);
        if(gptUI->button("+")) ptAppData->iActivationLevel++;
        if(gptUI->button("-")) ptAppData->iActivationLevel--;
        gptUI->end_window();
    }

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    plDevice* ptDevice = gptStarter->get_device();
    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    plDynamicDataBlock tCurrentDynamicBufferBlock = gptGfx->allocate_dynamic_data_block(ptDevice);

    if(ptAppData->bShowHeightmap)
    {
        plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &tCurrentDynamicBufferBlock);
        plMat4* mvp = (plMat4*)tDynamicBinding.pcData;
        *mvp = tMVP;

        // submit nonindexed draw using basic API
        plShaderHandle tShader = ptAppData->bWireframe ? ptAppData->tWireframeShader : ptAppData->tShader;
        gptGfx->bind_shader(ptEncoder, tShader);
        gptGfx->bind_vertex_buffer(ptEncoder, ptAppData->tVertexBuffer);
        gptGfx->bind_graphics_bind_groups(ptEncoder, tShader, 0, 0, NULL, 1, &tDynamicBinding);

        const plDrawIndex tDraw = {
            .uInstanceCount = 1,
            .uIndexCount    = ptAppData->uIndexCount,
            // .uIndexCount    = 9,
            .tIndexBuffer   = ptAppData->tIndexBuffer
        };
        gptGfx->draw_indexed(ptEncoder, 1, &tDraw);
    }

    

    // submit 3d drawlist
    gptDraw->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE | PL_DRAW_FLAG_REVERSE_Z_DEPTH,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

void
pl__load_apis(plApiRegistryI* ptApiRegistry)
{
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools      = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo         = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision     = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptMaterial      = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript        = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptMeshBuilder   = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    gptImage         = pl_get_api_latest(ptApiRegistry, plImageI);
}