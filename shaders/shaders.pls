{
    "bind group layouts": [
        {
            "pcName": "global",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 3, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 4, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atTextureBindings": [
                { "uSlot":    5, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]},
                { "uSlot": 4101, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "bNonUniformIndexing": true, "uDescriptorCount": 4096, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]}
            ]
        },
        {
            "pcName": "scene",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 4, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ],
            "atSamplerBindings": [
                { "uSlot": 5, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
            ]
        },
        {
            "pcName": "shadow",
            "atBufferBindings": [
                { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] },
                { "uSlot": 1, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
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
            "pcName": "filter_environment",
            "tShader": { "file": "filter_environment.comp"},
            "atBindGroupLayouts": [
                {
                    "atTextureBindings": [
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atSamplerBindings": [
                        { "uSlot": 0, "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ],
                    "atBufferBindings": [
                        { "uSlot": 2, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 3, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 4, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 5, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 6, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 7, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] },
                        { "uSlot": 8, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_COMPUTE"] }
                    ]
                }
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
            "tVertexShader":    { "file": "full_quad.vert"},
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
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" }
                    ]
                }
            ],
            "uSubpassIndex": 0,
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atBindGroupLayouts": [
                {
                    "atSamplerBindings": [
                        { "uSlot": 0, "tStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ],
                    "atTextureBindings": [
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 2, "tType": "PL_TEXTURE_BINDING_TYPE_SAMPLED", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                }
            ]
        },
        {
            "pcName": "skybox",
            "tVertexShader":    { "file": "skybox.vert"},
            "tFragmentShader":  { "file": "skybox.frag"},
            "tGraphicsState": {
                "ulDepthWriteEnabled": false,
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
                {
                    "atSamplerBindings": [
                        { "uSlot": 1, "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"]}
                    ],
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_STORAGE", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                },
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
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "global" },
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
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [
                { "pcName": "global" },
                { "pcName": "scene"  }
            ]
        },
        {
            "pcName": "uvmap",
            "tVertexShader":    { "file": "uvmap.vert"},
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
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" }
                    ]
                }
            ],
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
                {
                    "atBufferBindings": [
                        { "uSlot": 0, "tType": "PL_BUFFER_BINDING_TYPE_UNIFORM", "tStages": ["PL_SHADER_STAGE_FRAGMENT", "PL_SHADER_STAGE_VERTEX"] }
                    ]
                },
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
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "global" },
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
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "global" },
                { "pcName": "shadow" }
            ]
        },
        {
            "pcName": "deferred_lighting",
            "tVertexShader":    { "file": "deferred_lighting.vert"},
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
            "atVertexBufferLayouts": [
                {
                    "atAttributes": [
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" },
                        { "tFormat": "PL_VERTEX_FORMAT_FLOAT2" }
                    ]
                }
            ],
            "atBlendStates": [
                { "bBlendEnabled": false }
            ],
            "atConstants": [
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" },
                { "tType": "PL_DATA_TYPE_INT" }
            ],
            "atBindGroupLayouts": [ 
                { "pcName": "global" },
                {
                    "pcName": "deferred lighting 1",
                    "atTextureBindings": [
                        { "uSlot": 0, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 1, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 2, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] },
                        { "uSlot": 3, "tType": "PL_TEXTURE_BINDING_TYPE_INPUT_ATTACHMENT", "tStages": ["PL_SHADER_STAGE_FRAGMENT"] }
                    ]
                },
                { "pcName": "scene" }
            ]
        }
    ]
}