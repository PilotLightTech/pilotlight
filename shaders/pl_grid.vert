#version 460 core

#include "pl_shader_interop_renderer.h"

layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    plGpuDynGrid tData;
} tObjectInfo;

layout (location=0) out vec2 uv;
layout (location=1) out vec2 out_camPos;

const vec3 pos[4] = vec3[4](
  vec3(-1.0, 0.0, -1.0),
  vec3( 1.0, 0.0, -1.0),
  vec3( 1.0, 0.0,  1.0),
  vec3(-1.0, 0.0,  1.0)
);

const int indices[6] = int[6](
  0, 1, 2, 2, 3, 0
);

void main()
{
  int idx = indices[gl_VertexIndex];
  vec3 position = pos[idx] * tObjectInfo.tData.fGridSize;

  position.x += tObjectInfo.tData.fCameraXPos;
  position.z += tObjectInfo.tData.fCameraZPos;

  out_camPos.x = tObjectInfo.tData.fCameraXPos;
  out_camPos.y = tObjectInfo.tData.fCameraZPos;

  gl_Position = tObjectInfo.tData.tCameraViewProjection * vec4(position, 1.0);
  uv = position.xz;
}