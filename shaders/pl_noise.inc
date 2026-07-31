#ifndef NOISE_INC
#define NOISE_INC

//from: "Next Generation Post Processing in Call Of Duty Advanced Warfare" slide page 123
float
interleavedGradientNoise(vec2 uv)
{
    vec3 magic = vec3(0.06711056, 0.00583715, 62.9829189);
    return fract(magic.z * fract(dot(uv, magic.xy)));
}

//from: https://www.shadertoy.com/view/XdGfRR, by David Hoskins
//recommended in https://www.shadertoy.com/view/Xt3cDn, as a fast but low quality hash
//used here because many high quality hashes have int in our output which is less convenient to work with
//TODO: research higher quality options and impact on used techniques
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))
vec3
hash32(vec2 q)
{
    uvec3 n = uvec3(ivec3(q.xyx)) * UI3;
    n = (n.x ^ n.y ^ n.z) * UI3;
    return vec3(n) * UIF;
}

//from: http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
//see: https://en.wikipedia.org/wiki/Xorshift#Example_implementation
uint
xorshift32(inout uint state)
{
    // Xorshift algorithm from George Marsaglia's paper
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

//from: http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint
wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

//from Criver on the Graphics Programming Discord
float
rand(inout uint state)
{
    uint x = xorshift32(state);
    state = x;
    return clamp(float(x)*uintBitsToFloat(0x2f800004u), 0, 1);	//clamped because rarely returns values otside of [0, 1]
}

#endif // #ifndef NOISE_INC