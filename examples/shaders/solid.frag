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

    float lon = atan(abs(N.x) / abs(N.z));
    float l = length(Ns);
    float lat = asin(N.y);

    if(N.z < 0.0 && N.x > 0.0)
    {
        lon = rad * 180.0 - lon;
    }
    else if(N.z < 0.0 && N.x < 0.0)
    {
        lon = -180.0 * rad + lon;
    }
    else if(N.z > 0.0 && N.x < 0.0)
    {
        lon = -lon;
    }


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

    // UV stuff
    {
        float xresolution = 28800.0;
        float yresolution = 28800.0;
        float xmax = 1440000.0;
        float xmin = -1440000.0;
        float ymax = 1440000.0;
        float ymin = -1440000.0;
        float x0 = 0.0;
        float y0 = 0.0;

        // float r = 2 * R * tan(0.25 * 3.14159265358979323846 - 0.5 * lat); // north hem
        float r = 2 * R * tan(0.25 * 3.14159265358979323846 + 0.5 * lat); // south hem
        float x = r * sin(lon) + x0;
        float y = r * cos(lon) + y0;

        float xp = xresolution * (x - xmin) / (xmax - xmin);
        float yp = yresolution * (1.0 - ((y - ymin) / (ymax - ymin)));

        if(xp >= 0.0 && xp < xresolution && yp >= 0.0 && yp < yresolution)
        {
            // outColor.b += 0.5;

            outColor.r = xp / xresolution;
            outColor.g = yp / yresolution;
            outColor.b = 0.0;
        }
    }

    // outColor.a = 0.0;
    // if(lon * deg > -180.0 && lon * deg < 0.0)
    // {
    //     outColor.a = 1.0;
    // }

    // outColor.r = (lon - - 180.0 * rad) / (360.0 * rad);
    // outColor.g = outColor.r;
    // outColor.b = outColor.r;
    // outColor.a = outColor.r;

#if 0
    if(lat * deg < -29.0)
    {
        outColor.r += 0.5;
    }

    if(lat * deg < -29.0)
    {
        outColor.r += 0.5;
    }

    if(lat * deg < 10.0 && lat * deg > -10.0 && lon * deg < 15.0 && lon * deg > -15.0)
    {
        outColor.g += 0.2;
    }
#endif

}