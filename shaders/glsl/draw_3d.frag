#version 450 core
layout(location = 0) out vec4 fColor;
layout(location = 0) in struct { vec4 Color; } In;
void main()
{
    fColor = In.Color;
}