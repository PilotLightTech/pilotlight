
float
clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

vec3
linearTosRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}