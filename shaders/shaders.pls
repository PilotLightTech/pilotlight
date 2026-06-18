{
    "bind group layouts": [
        {
            "pcName": "scene",
            "atBufferBindings": [
                { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX", "PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 3, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 4, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 5, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 6, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 7, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atTextureBindings": [
                { "uSlot":    8, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]},
                { "uSlot": 4104, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]}
            ]
        },
        {
            "pcName": "view",
            "atBufferBindings": [
                { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX", "PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "eType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 3, "eType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 4, "eType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 5, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 6, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 7, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 8, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ]
        },
        {
            "pcName": "shadow",
            "atBufferBindings": [
                { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ]
        },
        {
            "pcName": "cube_filter_set_0",
            "atTextureBindings": [
                { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 0, "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ]
        },
        {
            "pcName": "cube_filter_set_1",
            "atBufferBindings": [
                { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 2, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 3, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 4, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 5, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ]
        }
    ],

    "compute shaders": [
        {
            "pcName": "panorama_to_cubemap",
            "tShader": { "file": "pl_panorama_to_cubemap.comp"},
            "atConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 2, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 3, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 4, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 5, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 6, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "jumpfloodalgo",
            "tShader": { "file": "pl_jumpfloodalgo.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "gaussian_blur",
            "tShader": { "file": "pl_gaussian_blur.comp"},
            "atConstants": [
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_apply",
            "tShader": { "file": "pl_bloom_apply.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 2, "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_downsample",
            "tShader": { "file": "pl_bloom_downsample.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 2, "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_upsample",
            "tShader": { "file": "pl_bloom_upsample.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 2, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 3, "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "brdf_lut",
            "tShader": { "file": "pl_brdf_lut.comp"},
            "atBindGroupLayouts": [
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "cube_filter_specular",
            "tShader": { "file": "pl_cube_filter_specular.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "cube_filter_diffuse",
            "tShader": { "file": "pl_cube_filter_diffuse.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "cube_filter_sheen",
            "tShader": { "file": "pl_cube_filter_sheen.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "skinning",
            "tShader": { "file": "pl_skinning.comp"},
            "atConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atSamplerBindings": [
                        { "uSlot": 2, "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"]}
                    ]
                }
            ]
        },
        {
            "pcName": "tonemap",
            "tShader": { "file": "pl_tonemap.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        }
    ],

    "graphics shaders": [
        {
            "pcName": "jumpfloodalgo2",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_jumpfloodalgo.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled": false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                }
            ]
        },
        {
            "pcName": "skybox",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_skybox.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_EQUAL",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  },
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "gbuffer_fill",
            "tVertexShader":    { "file": "pl_gbuffer_fill.vert"},
            "tFragmentShader":  { "file": "pl_gbuffer_fill.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "gbuffer_fill_debug",
            "tVertexShader":    { "file": "pl_gbuffer_fill.vert"},
            "tFragmentShader":  { "file": "pl_gbuffer_fill_debug.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "forward",
            "tVertexShader":    { "file": "pl_forward.vert"},
            "tFragmentShader":  { "file": "pl_forward.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  }
            ]
        },
        {
            "pcName": "forward_debug",
            "tVertexShader":    { "file": "pl_forward.vert"},
            "tFragmentShader":  { "file": "pl_forward_debug.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  }
            ]
        },
        {
            "pcName": "transmission",
            "tVertexShader":    { "file": "pl_forward.vert"},
            "tFragmentShader":  { "file": "pl_forward.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  }
            ]
        },
        {
            "pcName": "uvmap",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_uvmap.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": true,
                "eStencilMode":        "PL_COMPARE_MODE_LESS",
                "uStencilRef":         128,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                { "bBlendEnabled": false }
            ]
        },
        {
            "pcName": "picking",
            "tVertexShader":    { "file": "pl_picking.vert"},
            "tFragmentShader":  { "file": "pl_picking.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_EQUAL",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "view" },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "shadow",
            "tVertexShader": { "file": "pl_shadow.vert"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  true,
                "eDepthMode":          "PL_COMPARE_MODE_GREATER_OR_EQUAL",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  true,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled":   false }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "alphashadow",
            "tVertexShader":    { "file": "pl_shadow.vert"},
            "tFragmentShader":  { "file": "pl_shadow.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  true,
                "eDepthMode":          "PL_COMPARE_MODE_GREATER_OR_EQUAL",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  true,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "deferred_lighting",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_deferred_lighting_probe.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "eAlphaOp":        "PL_BLEND_OP_ADD",
                    "uColorWriteMask": 7
                },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                {
                    "pcName": "deferred lighting 1",
                    "atTextureBindings": [
                        { "uSlot": 0, "eType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 1, "eType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 2, "eType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 3, "eType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "eStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                }
                
            ]
        },
        {
            "pcName": "deferred_lighting_debug",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_deferred_lighting_debug.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                { "pcName": "deferred lighting 1" }
                
            ]
        },
        {
            "pcName": "deferred_lighting_directional",
            "tVertexShader":    { "file": "pl_full_screen.vert"},
            "tFragmentShader":  { "file": "pl_deferred_lighting_directional.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" },
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                { "pcName": "deferred lighting 1" }
                
            ]
        },
        {
            "pcName": "deferred_lighting_spot",
            "tVertexShader":    { "file": "pl_deferred_lighting_spot.vert"},
            "tFragmentShader":  { "file": "pl_deferred_lighting_spot.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_CULL_FRONT",
                "bWireframe":          false,
                "eDepthClampEnabled":  true,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 },
                { "bBlendEnabled": false, "uColorWriteMask": 0 }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                { "pcName": "deferred lighting 1" }
                
            ]
        },
        {
            "pcName": "deferred_lighting_point",
            "tVertexShader":    { "file": "pl_deferred_lighting_point.vert"},
            "tFragmentShader":  { "file": "pl_deferred_lighting_point.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "eCullMode":           "PL_CULL_MODE_CULL_FRONT",
                "bWireframe":          false,
                "eDepthClampEnabled":  true,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atFragmentConstants": [
                { "eType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                { "pcName": "deferred lighting 1" }
                
            ]
        },
        {
            "pcName": "terrain",
            "tVertexShader":    { "file": "pl_terrain.vert"},
            "tFragmentShader":  { "file": "pl_terrain.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  true,
                "eDepthMode":          "PL_COMPARE_MODE_GREATER",
                "eCullMode":           "PL_CULL_MODE_CULL_BACK",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" },
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "eType": "PL_BUFFER_BINDING_TYPE_STORAGE", "eStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "terrain_shadow",
            "tVertexShader": { "file": "pl_terrain_shadow.vert"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  true,
                "eDepthMode":          "PL_COMPARE_MODE_GREATER_OR_EQUAL",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  true,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT3" },
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                        { "eFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "grid",
            "tVertexShader":    { "file": "pl_grid.vert"},
            "tFragmentShader":  { "file": "pl_grid.frag"},
            "tGraphicsState": {
                "bDepthWriteEnabled":  false,
                "eDepthMode":          "PL_COMPARE_MODE_GREATER",
                "eCullMode":           "PL_CULL_MODE_NONE",
                "bWireframe":          false,
                "eDepthClampEnabled":  false,
                "bStencilTestEnabled": false,
                "eStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "uStencilRef":         255,
                "eStencilMask":        255,
                "eStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "eStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "eStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "eSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eColorOp":        "PL_BLEND_OP_ADD",
                    "eSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "eDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "eAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ]
        }
    ]
}