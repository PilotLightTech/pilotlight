#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aInfo;
layout(location = 2) in vec3 aPosOther;
layout(location = 3) in vec4 aColor;
layout(push_constant) uniform uPushConstant { mat4 tMVP; float fAspect; } pc;
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; } Out;

void main()
{
    Out.Color = aColor;

    // clip space
    vec4 tCurrentProj = pc.tMVP * vec4(aPos.xyz, 1.0);
    vec4 tOtherProj   = pc.tMVP * vec4(aPosOther.xyz, 1.0);

    // NDC space
    vec2 tCurrentNDC = tCurrentProj.xy / tCurrentProj.w;
    vec2 tOtherNDC = tOtherProj.xy / tOtherProj.w;

    // correct for aspect
    tCurrentNDC.x *= pc.fAspect;
    tOtherNDC.x *= pc.fAspect;

    // normal of line (B - A)
    vec2 dir = aInfo.z * normalize(tOtherNDC - tCurrentNDC);
    vec2 normal = vec2(-dir.y, dir.x);

    // extrude from center & correct aspect ratio
    normal *= aInfo.y / 2.0;
    normal.x /= pc.fAspect;

    // offset by the direction of this point in the pair (-1 or 1)
    vec4 offset = vec4(normal* aInfo.x, 0.0, 0.0);
    gl_Position = tCurrentProj + offset;
}