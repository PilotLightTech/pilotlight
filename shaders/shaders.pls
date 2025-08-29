{
    "bind group layouts": [
        {
            "pcName": "scene",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX", "PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 4, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 5, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 6, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 7, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atTextureBindings": [
                { "uSlot":    8, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]},
                { "uSlot": 4104, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]}
            ]
        },
        {
            "pcName": "view",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX", "PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 4, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 5, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ]
        },
        {
            "pcName": "shadow",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ]
        },
        {
            "pcName": "cube_filter_set_0",
            "atTextureBindings": [
                { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 0, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ]
        },
        {
            "pcName": "cube_filter_set_1",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 4, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                { "uSlot": 5, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
            ]
        }
    ],

    "compute shaders": [
        {
            "pcName": "panorama_to_cubemap",
            "tShader": { "file": "panorama_to_cubemap.comp"},
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 4, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 5, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 6, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "jumpfloodalgo",
            "tShader": { "file": "jumpfloodalgo.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "gaussian_blur",
            "tShader": { "file": "gaussian_blur.comp"},
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_apply",
            "tShader": { "file": "bloom_apply.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 2, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_downsample",
            "tShader": { "file": "bloom_downsample.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 2, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "bloom_upsample",
            "tShader": { "file": "bloom_upsample.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 2, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 3, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "brdf_lut",
            "tShader": { "file": "brdf_lut.comp"},
            "atBindGroupLayouts": [
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        },
        {
            "pcName": "cube_filter_specular",
            "tShader": { "file": "cube_filter_specular.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "cube_filter_diffuse",
            "tShader": { "file": "cube_filter_diffuse.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "cube_filter_sheen",
            "tShader": { "file": "cube_filter_sheen.comp"},
            "atBindGroupLayouts": [
                { "pcName": "cube_filter_set_0" },
                { "pcName": "cube_filter_set_1" }
            ]
        },
        {
            "pcName": "skinning",
            "tShader": { "file": "skinning.comp"},
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                {
                    "atSamplerBindings": [
                        { "uSlot": 3, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"]}
                    ]
                }
            ]
        },
        {
            "pcName": "tonemap",
            "tShader": { "file": "tonemap.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
            ]
        }
    ],

    "graphics shaders": [
        {
            "pcName": "jumpfloodalgo2",
            "tVertexShader":    { "file": "full_screen.vert"},
            "tFragmentShader":  { "file": "jumpfloodalgo.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled": false,
                "ulDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "uSubpassIndex": 0,
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                }
            ]
        },
        {
            "pcName": "skybox",
            "tVertexShader":    { "file": "skybox.vert"},
            "tFragmentShader":  { "file": "skybox.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_EQUAL",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "uSubpassIndex": 2,
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  },
                {
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "gbuffer_fill",
            "tVertexShader":    { "file": "gbuffer_fill.vert"},
            "tFragmentShader":  { "file": "gbuffer_fill.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "uSubpassIndex": 0,
            "atBlendStates": [
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false },
                { "bBlendEnabled": false }
            ],
            "atVertexConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "forward",
            "tVertexShader":    { "file": "forward.vert"},
            "tFragmentShader":  { "file": "forward.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "uSubpassIndex": 2,
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  }
            ]
        },
        {
            "pcName": "transmission",
            "tVertexShader":    { "file": "forward.vert"},
            "tFragmentShader":  { "file": "forward.frag"},
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atVertexConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "scene" },
                { "pcName": "view"  }
            ]
        },
        {
            "pcName": "uvmap",
            "tVertexShader":    { "file": "full_screen.vert"},
            "tFragmentShader":  { "file": "uvmap.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": true,
                "ulStencilMode":        "PL_COMPARE_MODE_LESS",
                "ulStencilRef":         128,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atBlendStates": [
                { "bBlendEnabled": false }
            ]
        },
        {
            "pcName": "picking",
            "tVertexShader":    { "file": "picking.vert"},
            "tFragmentShader":  { "file": "picking.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_EQUAL",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
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
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                }
            ]
        },
        {
            "pcName": "shadow",
            "tVertexShader": { "file": "shadow.vert"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  true,
                "ulDepthMode":          "PL_COMPARE_MODE_GREATER_OR_EQUAL",
                "ulCullMode":           "PL_CULL_MODE_CULL_BACK",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  true,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "alphashadow",
            "tVertexShader":    { "file": "shadow.vert"},
            "tFragmentShader":  { "file": "shadow.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  true,
                "ulDepthMode":          "PL_COMPARE_MODE_GREATER_OR_EQUAL",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  true,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atVertexConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "deferred_lighting",
            "tVertexShader":    { "file": "full_screen.vert"},
            "tFragmentShader":  { "file": "deferred_lighting.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "uSubpassIndex": 1,
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                {
                    "pcName": "deferred lighting 1",
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 2, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 3, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                }
                
            ]
        },
        {
            "pcName": "deferred_lighting_volume",
            "tVertexShader":    { "file": "deferred_lighting.vert"},
            "tFragmentShader":  { "file": "deferred_lighting.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_ALWAYS",
                "ulCullMode":           "PL_CULL_MODE_CULL_FRONT",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  true,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "uSubpassIndex": 1,
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT3" }
                    ]
                }
            ],
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ],
            "atFragmentConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "scene" },
                { "pcName": "view" },
                { "pcName": "deferred lighting 1" }
                
            ]
        },
        {
            "pcName": "grid",
            "tVertexShader":    { "file": "grid.vert"},
            "tFragmentShader":  { "file": "grid.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled":  false,
                "ulDepthMode":          "PL_COMPARE_MODE_GREATER",
                "ulCullMode":           "PL_CULL_MODE_NONE",
                "ulWireframe":          false,
                "ulDepthClampEnabled":  false,
                "ulStencilTestEnabled": false,
                "ulStencilMode":        "PL_COMPARE_MODE_ALWAYS",
                "ulStencilRef":         255,
                "ulStencilMask":        255,
                "ulStencilOpFail":      "PL_STENCIL_OP_KEEP",
                "ulStencilOpDepthFail": "PL_STENCIL_OP_KEEP",
                "ulStencilOpPass":      "PL_STENCIL_OP_KEEP"
            },
            "uSubpassIndex": 0,
            "atBlendStates": [
                {
                    "bBlendEnabled":   true,
                    "tSrcColorFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstColorFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tColorOp":        "PL_BLEND_OP_ADD",
                    "tSrcAlphaFactor": "PL_BLEND_FACTOR_SRC_ALPHA",
                    "tDstAlphaFactor": "PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA",
                    "tAlphaOp":        "PL_BLEND_OP_ADD"
                }
            ]
        }
    ]
}