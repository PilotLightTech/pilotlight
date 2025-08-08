#ifndef MATH_GLSL
#define MATH_GLSL

// math
#define PL_PI 3.1415926535897932384626433832795

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

float
clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

vec3
pl_linear_to_srgb(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

vec3
pl_srgb_to_linear(vec3 color)
{
    return pow(color, vec3(GAMMA));
}

vec4
pl_srgb_to_linear(vec4 color)
{
    return vec4(pl_srgb_to_linear(color.rgb), color.a);
}

float
pl_saturate(float fV)
{
    return clamp(fV, 0.0, 1.0);
}

float
pl_random(vec3 seed, int i)
{
    vec4 seed4 = vec4(seed, float(i));
    float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

float
pl_random(vec2 co)
{
    float dt = dot(co.xy, vec2(12.9898, 78.233));
    float sn = mod(dt, 3.14);
    return fract(sin(sn) * 43758.5453);
}

vec2
pl_hammersley_2d(int i, int iN)
{
    // Hammersley Points on the Hemisphere
    // CC BY 3.0 (Holger Dammertz)
    // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    // with adapted interface
    uint uBits =  uint(i);
    uBits = (uBits << 16u) | (uBits >> 16u);
    uBits = ((uBits & 0x55555555u) << 1u) | ((uBits & 0xAAAAAAAAu) >> 1u);
    uBits = ((uBits & 0x33333333u) << 2u) | ((uBits & 0xCCCCCCCCu) >> 2u);
    uBits = ((uBits & 0x0F0F0F0Fu) << 4u) | ((uBits & 0xF0F0F0F0u) >> 4u);
    uBits = ((uBits & 0x00FF00FFu) << 8u) | ((uBits & 0xFF00FF00u) >> 8u);
    float rdi = float(uBits) * 2.3283064365386963e-10; // / 0x100000000

    // hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
    // that can be used for quasi Monte Carlo integration
    return vec2(float(i)/float(iN), rdi);
}

vec2
OctWrap( vec2 v )
{
    vec2 w = 1.0 - abs( v.yx );
    if (v.x < 0.0) w.x = -w.x;
    if (v.y < 0.0) w.y = -w.y;
    return w;
}
 
vec2
Encode( vec3 n )
{
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = n.z > 0.0 ? n.xy : OctWrap( n.xy );
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

vec3
Decode( vec2 f )
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = max( -n.z, 0.0 );
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize( n );
}

vec3 
sampleCube(vec3 v)
{
	vec3 vAbs = abs(v);
	float ma;
	vec2 uv;
    float faceIndex = 0.0;
	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 1.0 : 0.0;
		ma = 0.5 / vAbs.z;
		uv = vec2(v.z < 0.0 ? v.x : -v.x, -v.y);
	}
	else if(vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 5.0 : 4.0;
		ma = 0.5 / vAbs.y;
		uv = vec2(-v.x, v.y < 0.0 ? -v.z : v.z);
	}
	else
	{
		faceIndex = v.x < 0.0 ? 3.0 : 2.0;
		ma = 0.5 / vAbs.x;
		uv = vec2(v.x < 0.0 ? -v.z : v.z, -v.y);
	}
	vec2 result = uv * ma + vec2(0.5, 0.5);
    return vec3(result, faceIndex);
}

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

#endif // MATH_GLSL