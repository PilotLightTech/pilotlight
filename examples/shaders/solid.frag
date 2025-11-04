#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable


layout(set = 3, binding = 0) uniform PL_DYNAMIC_DATA
{
    mat4 tCameraViewProjection;
    vec4 tCameraPosHigh;
    vec4 tCameraPosLow;
} tObjectInfo;

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct plShaderOut {
    vec3 tPosition;
} tShaderIn;

//-----------------------------------------------------------------------------
// [SECTION] entry
//-----------------------------------------------------------------------------

const vec4 ambientColor = vec4(0.01, 0.0, 0.0, 1.0);
const vec4 diffuseColor = vec4(0.25, 0.0, 0.0, 1.0);
const vec4 specularColor = vec4(1.0, 1.0, 1.0, 1.0);
const float shininess = 20.0;
const vec4 lightColor = vec4(1.0, 1.0, 1.0, 1.0);
const float irradiPerp = 1.0;

vec3 phongBRDF(vec3 lightDir, vec3 viewDir, vec3 normal, vec3 phongDiffuseCol, vec3 phongSpecularCol, float phongShininess) {
  vec3 color = phongDiffuseCol;
  vec3 reflectDir = reflect(-lightDir, normal);
  float specDot = max(dot(reflectDir, viewDir), 0.0);
  color += pow(specDot, phongShininess) * phongSpecularCol;
  return color;
}

void main() 
{
    // vec3 sunlightDirection = vec3(-1.0, -1.0, -1.0);

    // vec3 ambient = vec3(0);

    // vec3 lightDir = normalize(sunlightDirection);
    // vec3 viewDir = normalize(tShaderIn.tPosition - tObjectInfo.tCameraPosHigh.xyz);
    // // vec3 viewDir = normalize(-tShaderIn.tPosition);
    // vec3 n = normalize(tShaderIn.tPosition);

    // vec3 radiance = ambient;
    
    // float ddot = dot(lightDir, n);
    // float irradiance = max(ddot, 0.0) * irradiPerp;
    // if(irradiance > 0.0)
    // {
    //     vec3 brdf = phongBRDF(lightDir, viewDir, n, diffuseColor.rgb, specularColor.rgb, shininess);
    //     radiance += brdf * irradiance * lightColor.rgb;
    // }

    // radiance = pow(radiance, vec3(1.0 / 2.2) ); // gamma correction
    outColor.rgb = vec3(1.0);
    outColor.a = 1.0;
}