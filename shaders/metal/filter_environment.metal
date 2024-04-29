#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct MicrofacetDistributionSample
{
    float pdf;
    float cosTheta;
    float sinTheta;
    float phi;
};

struct _tBufferOut0
{
    float4 atPixelData[1];
};

struct _tBufferOut1
{
    float4 atPixelData[1];
};

struct _tBufferOut2
{
    float4 atPixelData[1];
};

struct _tBufferOut3
{
    float4 atPixelData[1];
};

struct _tBufferOut4
{
    float4 atPixelData[1];
};

struct _tBufferOut5
{
    float4 atPixelData[1];
};

constant int u_sampleCount_tmp [[function_constant(2)]];
constant int u_sampleCount = is_function_constant_defined(u_sampleCount_tmp) ? u_sampleCount_tmp : 0;
constant int u_distribution_tmp [[function_constant(5)]];
constant int u_distribution = is_function_constant_defined(u_distribution_tmp) ? u_distribution_tmp : 0;
constant bool _541 = (u_distribution == 0);
constant bool _551 = (u_distribution == 1);
constant bool _560 = (u_distribution == 2);
constant int u_width_tmp [[function_constant(3)]];
constant int u_width = is_function_constant_defined(u_width_tmp) ? u_width_tmp : 0;
constant float u_roughness_tmp [[function_constant(1)]];
constant float u_roughness = is_function_constant_defined(u_roughness_tmp) ? u_roughness_tmp : 0.0;
constant float u_lodBias_tmp [[function_constant(4)]];
constant float u_lodBias = is_function_constant_defined(u_lodBias_tmp) ? u_lodBias_tmp : 0.0;
constant bool _656 = (u_distribution == 0);
constant bool _680 = (u_distribution == 1);
constant bool _681 = (u_distribution == 2);
constant bool _682 = (_680 || _681);
constant bool _840 = (u_distribution == 1);
constant bool _875 = (u_distribution == 2);
constant int u_isGeneratingLUT_tmp [[function_constant(6)]];
constant int u_isGeneratingLUT = is_function_constant_defined(u_isGeneratingLUT_tmp) ? u_isGeneratingLUT_tmp : 0;
constant bool _941 = (u_isGeneratingLUT == 0);
constant int resolution_tmp [[function_constant(0)]];
constant int resolution = is_function_constant_defined(resolution_tmp) ? resolution_tmp : 0;
constant int currentMipLevel_tmp [[function_constant(7)]];
constant int currentMipLevel = is_function_constant_defined(currentMipLevel_tmp) ? currentMipLevel_tmp : 0;
constant int _977 = (1 << currentMipLevel);

struct _tBufferOut6
{
    float4 atPixelData[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(16u, 16u, 3u);

struct spvDescriptorSetBuffer0
{
    sampler tDefaultSampler [[id(0)]];
    texturecube<float> uCubeMap [[id(1)]];
    device _tBufferOut0* FaceOut_0 [[id(2)]];
    device _tBufferOut1* FaceOut_1 [[id(3)]];
    device _tBufferOut2* FaceOut_2 [[id(4)]];
    device _tBufferOut3* FaceOut_3 [[id(5)]];
    device _tBufferOut4* FaceOut_4 [[id(6)]];
    device _tBufferOut5* FaceOut_5 [[id(7)]];
    device _tBufferOut6* LUTOut [[id(8)]];
};

static inline __attribute__((always_inline))
float3 uvToXYZ(thread const int& face, thread const float2& uv)
{
    if (face == 0)
    {
        return float3(1.0, -uv.y, -uv.x);
    }
    else
    {
        if (face == 1)
        {
            return float3(-1.0, -uv.y, uv.x);
        }
        else
        {
            if (face == 2)
            {
                return float3(uv.x, 1.0, uv.y);
            }
            else
            {
                if (face == 3)
                {
                    return float3(uv.x, -1.0, -uv.y);
                }
                else
                {
                    if (face == 4)
                    {
                        return float3(uv.x, -uv.y, 1.0);
                    }
                    else
                    {
                        return float3(-uv.x, -uv.y, -1.0);
                    }
                }
            }
        }
    }
}

static inline __attribute__((always_inline))
float radicalInverse_VdC(thread uint& bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 1431655765u) << 1u) | ((bits & 2863311530u) >> 1u);
    bits = ((bits & 858993459u) << 2u) | ((bits & 3435973836u) >> 2u);
    bits = ((bits & 252645135u) << 4u) | ((bits & 4042322160u) >> 4u);
    bits = ((bits & 16711935u) << 8u) | ((bits & 4278255360u) >> 8u);
    return float(bits) * 2.3283064365386962890625e-10;
}

static inline __attribute__((always_inline))
float2 hammersley2d(thread const int& i, thread const int& N)
{
    uint param = uint(i);
    float _324 = radicalInverse_VdC(param);
    return float2(float(i) / float(N), _324);
}

static inline __attribute__((always_inline))
MicrofacetDistributionSample Lambertian(thread const float2& xi, thread const float& roughness)
{
    MicrofacetDistributionSample lambertian;
    lambertian.cosTheta = sqrt(1.0 - xi.y);
    lambertian.sinTheta = sqrt(xi.y);
    lambertian.phi = 6.283185482025146484375 * xi.x;
    lambertian.pdf = lambertian.cosTheta / 3.1415927410125732421875;
    return lambertian;
}

static inline __attribute__((always_inline))
float saturate0(thread const float& v)
{
    return fast::clamp(v, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float D_GGX(thread const float& NdotH, thread const float& roughness)
{
    float a = NdotH * roughness;
    float k = roughness / ((1.0 - (NdotH * NdotH)) + (a * a));
    return (k * k) * 0.3183098733425140380859375;
}

static inline __attribute__((always_inline))
MicrofacetDistributionSample GGX(thread const float2& xi, thread const float& roughness)
{
    float alpha = roughness * roughness;
    float param = sqrt((1.0 - xi.y) / (1.0 + (((alpha * alpha) - 1.0) * xi.y)));
    MicrofacetDistributionSample ggx;
    ggx.cosTheta = saturate0(param);
    ggx.sinTheta = sqrt(1.0 - (ggx.cosTheta * ggx.cosTheta));
    ggx.phi = 6.283185482025146484375 * xi.x;
    float param_1 = ggx.cosTheta;
    float param_2 = alpha;
    ggx.pdf = D_GGX(param_1, param_2);
    ggx.pdf /= 4.0;
    return ggx;
}

static inline __attribute__((always_inline))
float D_Charlie(thread float& sheenRoughness, thread const float& NdotH)
{
    sheenRoughness = fast::max(sheenRoughness, 9.9999999747524270787835121154785e-07);
    float invR = 1.0 / sheenRoughness;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return ((2.0 + invR) * pow(sin2h, invR * 0.5)) / 6.283185482025146484375;
}

static inline __attribute__((always_inline))
MicrofacetDistributionSample Charlie(thread const float2& xi, thread const float& roughness)
{
    float alpha = roughness * roughness;
    MicrofacetDistributionSample charlie;
    charlie.sinTheta = pow(xi.y, alpha / ((2.0 * alpha) + 1.0));
    charlie.cosTheta = sqrt(1.0 - (charlie.sinTheta * charlie.sinTheta));
    charlie.phi = 6.283185482025146484375 * xi.x;
    float param = alpha;
    float param_1 = charlie.cosTheta;
    float _503 = D_Charlie(param, param_1);
    charlie.pdf = _503;
    charlie.pdf /= 4.0;
    return charlie;
}

static inline __attribute__((always_inline))
float3x3 generateTBN(thread const float3& normal)
{
    float3 bitangent = float3(0.0, 1.0, 0.0);
    float NdotUp = dot(normal, float3(0.0, 1.0, 0.0));
    float epsilon = 1.0000000116860974230803549289703e-07;
    if ((1.0 - abs(NdotUp)) <= epsilon)
    {
        if (NdotUp > 0.0)
        {
            bitangent = float3(0.0, 0.0, 1.0);
        }
        else
        {
            bitangent = float3(0.0, 0.0, -1.0);
        }
    }
    float3 tangent = fast::normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);
    return float3x3(float3(tangent), float3(bitangent), float3(normal));
}

static inline __attribute__((always_inline))
float4 getImportanceSample(thread const int& sampleIndex, thread const float3& N, thread const float& roughness)
{
    int param = sampleIndex;
    int param_1 = u_sampleCount;
    float2 xi = hammersley2d(param, param_1);
    MicrofacetDistributionSample importanceSample;
    if (_541)
    {
        float2 param_2 = xi;
        float param_3 = roughness;
        importanceSample = Lambertian(param_2, param_3);
    }
    else
    {
        if (_551)
        {
            float2 param_4 = xi;
            float param_5 = roughness;
            importanceSample = GGX(param_4, param_5);
        }
        else
        {
            if (_560)
            {
                float2 param_6 = xi;
                float param_7 = roughness;
                importanceSample = Charlie(param_6, param_7);
            }
        }
    }
    float3 localSpaceDirection = fast::normalize(float3(importanceSample.sinTheta * cos(importanceSample.phi), importanceSample.sinTheta * sin(importanceSample.phi), importanceSample.cosTheta));
    float3 param_8 = N;
    float3x3 TBN = generateTBN(param_8);
    float3 direction = TBN * localSpaceDirection;
    return float4(direction, importanceSample.pdf);
}

static inline __attribute__((always_inline))
float computeLod(thread const float& pdf)
{
    float lod = 0.5 * log2(((6.0 * float(u_width)) * float(u_width)) / (float(u_sampleCount) * pdf));
    return lod;
}

static inline __attribute__((always_inline))
float3 linearTosRGB(thread const float3& color)
{
    return pow(color, float3(0.4545454680919647216796875));
}

static inline __attribute__((always_inline))
float3 filterColor(thread const float3& N, texturecube<float> uCubeMap, sampler tDefaultSampler)
{
    float3 color = float3(0.0);
    float weight = 0.0;
    for (int i = 0; i < u_sampleCount; i++)
    {
        int param = i;
        float3 param_1 = N;
        float param_2 = u_roughness;
        float4 importanceSample = getImportanceSample(param, param_1, param_2);
        float3 H = float3(importanceSample.xyz);
        float pdf = importanceSample.w;
        float param_3 = pdf;
        float lod = computeLod(param_3);
        lod += u_lodBias;
        if (_656)
        {
            float3 param_4 = uCubeMap.sample(tDefaultSampler, H, level(lod)).xyz;
            float3 lambertian = linearTosRGB(param_4);
            color += lambertian;
        }
        else
        {
            if (_682)
            {
                float3 V = N;
                float3 L = fast::normalize(reflect(-V, H));
                float NdotL = dot(N, L);
                if (NdotL > 0.0)
                {
                    if (u_roughness == 0.0)
                    {
                        lod = u_lodBias;
                    }
                    float3 sampleColor = uCubeMap.sample(tDefaultSampler, L, level(lod)).xyz;
                    color += (sampleColor * NdotL);
                    weight += NdotL;
                }
            }
        }
    }
    if (weight != 0.0)
    {
        color /= float3(weight);
    }
    else
    {
        color /= float3(float(u_sampleCount));
    }
    return color;
}

static inline __attribute__((always_inline))
void writeFace(thread const int& pixel, thread const int& face, thread const float3& colorIn, device _tBufferOut0& FaceOut_0, device _tBufferOut1& FaceOut_1, device _tBufferOut2& FaceOut_2, device _tBufferOut3& FaceOut_3, device _tBufferOut4& FaceOut_4, device _tBufferOut5& FaceOut_5)
{
    float4 color = float4(colorIn, 1.0);
    if (face == 0)
    {
        FaceOut_0.atPixelData[pixel] = color;
    }
    else
    {
        if (face == 1)
        {
            FaceOut_1.atPixelData[pixel] = color;
        }
        else
        {
            if (face == 2)
            {
                FaceOut_2.atPixelData[pixel] = color;
            }
            else
            {
                if (face == 3)
                {
                    FaceOut_3.atPixelData[pixel] = color;
                }
                else
                {
                    if (face == 4)
                    {
                        FaceOut_4.atPixelData[pixel] = color;
                    }
                    else
                    {
                        FaceOut_5.atPixelData[pixel] = color;
                    }
                }
            }
        }
    }
}

static inline __attribute__((always_inline))
float V_SmithGGXCorrelated(thread const float& NoV, thread const float& NoL, thread const float& roughness)
{
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(((NoV * NoV) * (1.0 - a2)) + a2);
    float GGXL = NoV * sqrt(((NoL * NoL) * (1.0 - a2)) + a2);
    return 0.5 / (GGXV + GGXL);
}

static inline __attribute__((always_inline))
float V_Ashikhmin(thread const float& NdotL, thread const float& NdotV)
{
    return fast::clamp(1.0 / (4.0 * ((NdotL + NdotV) - (NdotL * NdotV))), 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LUT(thread const float& NdotV, thread const float& roughness)
{
    float3 V = float3(sqrt(1.0 - (NdotV * NdotV)), 0.0, NdotV);
    float3 N = float3(0.0, 0.0, 1.0);
    float A = 0.0;
    float B = 0.0;
    float C = 0.0;
    for (int i = 0; i < u_sampleCount; i++)
    {
        int param = i;
        float3 param_1 = N;
        float param_2 = roughness;
        float4 importanceSample = getImportanceSample(param, param_1, param_2);
        float3 H = importanceSample.xyz;
        float3 L = fast::normalize(reflect(-V, H));
        float param_3 = L.z;
        float NdotL = saturate0(param_3);
        float param_4 = H.z;
        float NdotH = saturate0(param_4);
        float param_5 = dot(V, H);
        float VdotH = saturate0(param_5);
        if (NdotL > 0.0)
        {
            if (_840)
            {
                float param_6 = NdotV;
                float param_7 = NdotL;
                float param_8 = roughness;
                float V_pdf = ((V_SmithGGXCorrelated(param_6, param_7, param_8) * VdotH) * NdotL) / NdotH;
                float Fc = pow(1.0 - VdotH, 5.0);
                A += ((1.0 - Fc) * V_pdf);
                B += (Fc * V_pdf);
                C += 0.0;
            }
            if (_875)
            {
                float param_9 = roughness;
                float param_10 = NdotH;
                float _883 = D_Charlie(param_9, param_10);
                float sheenDistribution = _883;
                float param_11 = NdotL;
                float param_12 = NdotV;
                float sheenVisibility = V_Ashikhmin(param_11, param_12);
                A += 0.0;
                B += 0.0;
                C += (((sheenVisibility * sheenDistribution) * NdotL) * VdotH);
            }
        }
    }
    return float3(4.0 * A, 4.0 * B, 25.1327419281005859375 * C) / float3(float(u_sampleCount));
}

kernel void kernel_main(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]])
{
    float xcoord = float((gl_WorkGroupID.x * 16u) + gl_LocalInvocationID.x);
    float ycoord = float((gl_WorkGroupID.y * 16u) + gl_LocalInvocationID.y);
    float3 color = float3(0.0, 1.0, 0.0);
    if (_941)
    {
        int face = int((gl_WorkGroupID.z * 3u) + gl_LocalInvocationID.z);
        float xinc = 1.0 / float(resolution);
        float yinc = 1.0 / float(resolution);
        float2 inUV = float2(xcoord * xinc, ycoord * yinc);
        int currentPixel = int(xcoord + (ycoord * float(u_width)));
        float2 newUV = inUV * float(_977);
        newUV = (newUV * 2.0) - float2(1.0);
        int param = face;
        float2 param_1 = newUV;
        float3 scan = uvToXYZ(param, param_1);
        float3 direction = fast::normalize(scan);
        direction.z = -direction.z;
        float3 param_2 = direction;
        color = filterColor(param_2, spvDescriptorSet0.uCubeMap, spvDescriptorSet0.tDefaultSampler);
        int param_3 = currentPixel;
        int param_4 = face;
        float3 param_5 = color;
        writeFace(param_3, param_4, param_5, (*spvDescriptorSet0.FaceOut_0), (*spvDescriptorSet0.FaceOut_1), (*spvDescriptorSet0.FaceOut_2), (*spvDescriptorSet0.FaceOut_3), (*spvDescriptorSet0.FaceOut_4), (*spvDescriptorSet0.FaceOut_5));
    }
    else
    {
        float lutxinc = 1.0 / float(resolution);
        float lutyinc = 1.0 / float(resolution);
        float2 lutUV = float2(xcoord * lutxinc, ycoord * lutyinc);
        int currentLUTPixel = int(xcoord + (ycoord * float(resolution)));
        float param_6 = lutUV.x;
        float param_7 = lutUV.y;
        color = LUT(param_6, param_7);
        (*spvDescriptorSet0.LUTOut).atPixelData[currentLUTPixel] = float4(color, 1.0);
    }
}

