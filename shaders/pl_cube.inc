#ifndef CUBE_INC
#define CUBE_INC

vec3
pl_uv_to_xyz(int iFace, vec2 tUv)
{
    if (iFace == 0) // right
        return vec3(1.0, -tUv.y, tUv.x);

    else if (iFace == 1) // left
        return vec3(-1.0, -tUv.y, -tUv.x);

    else if (iFace == 2) // top
        return vec3(tUv.x, 1.0, -tUv.y);

    else if (iFace == 3) // bottom
        return vec3(tUv.x, -1.0, tUv.y);

    else if (iFace == 4) // front
        return vec3(tUv.x, -tUv.y, -1.0);

    else //if(iFace == 5)
        return vec3(-tUv.x, -tUv.y, 1.0);
}

#endif // CUBE_INC