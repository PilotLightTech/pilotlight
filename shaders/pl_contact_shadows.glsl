#ifndef PL_CONTACT_SHADOWS_GLSL
#define PL_CONTACT_SHADOWS_GLSL

// The snapshot contains deferred and forward opaque/masked depth from this view.
// View space is +Z forward; depth is reversed, with zero denoting background.
float
pl_contact_shadow(vec3 viewPosition, vec3 worldNormal, vec3 pointToLight, float lightDistance)
{
    vec4 settings = tGpuScene.tData.tContactShadow;
    if(settings.x <= 0.0 || lightDistance <= 0.0 || bool(tObjectInfo.tData.iObjectShaderFlags & PL_OBJECT_SHADER_FLAG_PROBE))
        return 1.0;

    vec3 lightDirection = normalize(pointToLight);
    if(dot(worldNormal, lightDirection) <= 0.0)
        return 1.0;

    uint camera = tObjectInfo.tData.uGlobalIndex;
    mat3 viewRotation = mat3(tViewInfo.tData.tCameraView[camera]);
    vec3 origin = viewPosition + viewRotation * worldNormal * settings.z;
    vec3 direction = viewRotation * lightDirection;
    float rayLength = min(settings.x, lightDistance);
    int steps = clamp(int(settings.w), 1, 64);
    float stepLength = rayLength / float(steps);
    
    // Stable spatial jitter avoids moving noise when no temporal resolve is used.
    float jitter = fract(52.9829189 * fract(dot(floor(gl_FragCoord.xy), vec2(0.06711056, 0.00583715))));
    ivec2 receiverPixel = ivec2(gl_FragCoord.xy);
    vec2 viewport = tViewInfo.tData.tViewportSize.xy;
    int depthIndex = tViewInfo.tData.iContactShadowDepthIndex;

    for(int i = 0; i < steps; i++)
    {
        float distanceAlongRay = (float(i) + 0.5 + 0.5 * jitter) * stepLength;
        vec3 position = origin + direction * distanceAlongRay;
        if(position.z <= tViewInfo.tData.fCameraNearZ)
            break;
        vec4 clip = tViewInfo.tData.tCameraProjection[camera] * vec4(position, 1.0);
        if(clip.w <= 0.0)
            break;
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        if(any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0))) || ndc.z <= 0.0 || ndc.z > 1.0)
            break;

        // Use active viewport pixels, not allocation dimensions (views have padding).
        ivec2 pixel = ivec2(uv * viewport);
        if(all(equal(pixel, receiverPixel)))
            continue;
        float depth = texelFetch(sampler2D(at2DTextures[nonuniformEXT(depthIndex)], tSamplerNearestClamp), pixel, 0).r;
        if(depth <= 0.0)
            continue;
        vec2 sampleUV = (vec2(pixel) + 0.5) / viewport;
        vec4 surface = tViewInfo.tData.tCameraProjectionInv[camera] * vec4(sampleUV * 2.0 - 1.0, depth, 1.0);
        float delta = position.z - surface.z / surface.w;
        if(delta > 0.0 && delta < settings.y)
        {
            vec2 edge = min(uv, vec2(1.0) - uv);
            float edgeFade = smoothstep(0.0, 0.05, min(edge.x, edge.y));
            float lengthFade = 1.0 - smoothstep(0.8, 1.0, distanceAlongRay / rayLength);
            float thicknessFade = 1.0 - smoothstep(0.5 * settings.y, settings.y, delta);
            return 1.0 - edgeFade * lengthFade * thicknessFade;
        }
    }
    return 1.0;
}

#endif
