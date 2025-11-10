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
    vec3 tActualPosition;
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

vec3
pl_get_pos(float longitude, float latitude)
{
    vec3 N = vec3(0);
    N.x = cos(latitude) * cos(longitude);
    N.y = cos(latitude) * sin(longitude);
    N.z = sin(latitude);
    float R = 1737400.0;
    float R2 = R * R;
    vec3 P = vec3(0);
    P.x = R2 * N.x / longitude;
    P.y = R2 * N.y / longitude;
    P.z = R2 * N.z / longitude;
    return P;
}

void main() 
{


    // // degrees -> radians
    float rad = 3.14159265358979323846 / 180.0;
    float deg = 1.0 / rad;

    // // Inputs: lon, lat in degrees (planetocentric, east-positive)
    // // CRS params (use from your WKT): lon0 = 0 for IAU_2015:30130, k0 = 1
    float R   = 1737400.0;
    float R2   = R * R;

    // float lat = asin(tShaderIn.tActualPosition.y / R);
    // float lon = atan(tShaderIn.tActualPosition.z / tShaderIn.tActualPosition.x);

    vec3 Ns = vec3(tShaderIn.tActualPosition.x / R2, tShaderIn.tActualPosition.y / R2, tShaderIn.tActualPosition.z / R2);
    vec3 N = normalize(Ns);

    // float lon = atan(N.y / N.x);
    // float lat = asin(N.z / length(Ns));

    float lon = atan(N.x / N.z);
    float l = length(Ns);
    float lat = asin(N.y);


    outColor.r = 0.2;
    outColor.g = 0.2;
    outColor.b = 0.2;
    outColor.a = 1.0;

    // vec2 tGeographic = vec2(lon, lat);

    // longitude, latitude
    // vec2 tUpperLeft = vec2(-45.0 * rad, -29.0 * rad);
    // vec2 tLowerLeft = vec2(-135.0 * rad, -29.0 * rad);
    // vec2 tUpperRight = vec2(45.0 * rad, -29.0 * rad);
    // vec2 tLowerRight = vec2(135.0 * rad, -29.0 * rad);
    
    // // convert to cartesian
    // vec3 tUpperLeftP = pl_get_pos(tUpperLeft.x, tUpperLeft.y);
    // vec3 tLowerLeftP = pl_get_pos(tLowerLeft.x, tLowerLeft.y);
    // vec3 tUpperRightP = pl_get_pos(tUpperRight.x, tUpperRight.y);
    // vec3 tLowerRightP = pl_get_pos(tLowerRight.x, tLowerRight.y);

    // float rr = length(tUpperLeftP);


    if(lat * deg < -29.0)
    {
        outColor.r += 0.5;
    }

    if(lat * deg < -29.0)
    {
        outColor.r += 0.5;
    }

    if(
        lat * deg < 10.0 && lat * deg > -10.0 && lon * deg < 15.0 && lon * deg > -15.0
        )
    {
        outColor.g += 0.2;
    }

}