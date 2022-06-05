#version 330 core

in vec2 UV;
flat in int faceID;

out vec4 FragColor;

const mat3x2 neighborTransforms[16] = mat3x2[16](
    mat3x2(
        -1, 0,
        0, -1,
        1, 0
    ),
    mat3x2(
        0, -1,
        1, 0,
        1, 1
    ),
    mat3x2(
        1, 0,
        0, 1,
        0, 1
    ),
    mat3x2(
        0, 1,
        -1, 0,
        0, 0
    ),
    mat3x2(
        0, 1,
        -1, 0,
        1, -1
    ),
    mat3x2(
        -1, 0,
        0, -1,
        2, -1
    ),
    mat3x2(
        0, -1,
        1, 0,
        0, 2
    ),
    mat3x2(
        1, 0,
        0, 1,
        -1, 0
    ),
    mat3x2(
        1, 0,
        0, 1,
        0, -1
    ),
    mat3x2(
        0, 1,
        -1, 0,
        2, 0
    ),
    mat3x2(
        -1, 0,
        0, -1,
        1, 2
    ),
    mat3x2(
        0, -1,
        1, 0,
        -1, 1
    ),
    mat3x2(
        0, -1,
        1, 0,
        0, 0
    ),
    mat3x2(
        1, 0,
        0, 1,
        1, 0
    ),
    mat3x2(
        0, 1,
        -1, 0,
        1, 1
    ),
    mat3x2(
        -1, 0,
        0, -1,
        0, 1
    )
);

struct FaceData 
{
	uint texIDsliceID;
	uint neighbor01Indices;
	uint neighbor23Indices;
	uint neighbor0123Transform;
};

#define MAX_FACES 1024
layout(std140) uniform FaceDataUniform {
	FaceData face_data[MAX_FACES];
};

#define NUM_TEX 32

uniform sampler2DArray aTex[NUM_TEX];

uniform bool visualize;

vec4 ptexture_single(sampler2DArray tex[NUM_TEX], vec2 uv, uint texIDsliceID)
{
    uint texID = texIDsliceID & 0xFFFFu;
    uint sliceID = texIDsliceID >> 16;

    vec4 color = texture(tex[texID], vec3(uv, sliceID));

    return color;
}

vec3 ptexture_nvidia(vec4 sample0, sampler2DArray tex[NUM_TEX], vec2 uv, FaceData data)
{
        vec4 color = vec4(0);
        color += sample0;

        uint neighbor0_id = data.neighbor01Indices & 0xFFFFu;
        uint neighbor1_id = data.neighbor01Indices >> 16;

        uint neighbor2_id = data.neighbor23Indices & 0xFFFFu;
        uint neighbor3_id = data.neighbor23Indices >> 16;

        uint n0_transform = (data.neighbor0123Transform >> 0 ) & 0xFFu;
        uint n1_transform = (data.neighbor0123Transform >> 8 ) & 0xFFu;
        uint n2_transform = (data.neighbor0123Transform >> 16) & 0xFFu;
        uint n3_transform = (data.neighbor0123Transform >> 24) & 0xFFu;
    
        vec2 n0_uv = neighborTransforms[n0_transform] * vec3(uv, 1);
        vec2 n1_uv = neighborTransforms[n1_transform] * vec3(uv, 1);
        vec2 n2_uv = neighborTransforms[n2_transform] * vec3(uv, 1);
        vec2 n3_uv = neighborTransforms[n3_transform] * vec3(uv, 1);

        if (neighbor0_id != 0xFFFFu) color += ptexture_single(tex, n0_uv, face_data[neighbor0_id].texIDsliceID);
        if (neighbor1_id != 0xFFFFu) color += ptexture_single(tex, n1_uv, face_data[neighbor1_id].texIDsliceID);
        if (neighbor2_id != 0xFFFFu) color += ptexture_single(tex, n2_uv, face_data[neighbor2_id].texIDsliceID);
        if (neighbor3_id != 0xFFFFu) color += ptexture_single(tex, n3_uv, face_data[neighbor3_id].texIDsliceID);

        color.rgb = color.rgb / color.a;

        return color.rgb;
}

vec3 ptexture_reduced_traverse(sampler2DArray tex[NUM_TEX], vec2 uv, int faceID)
{
    FaceData data = face_data[faceID];

    vec4 sample0 = ptexture_single(tex, uv, data.texIDsliceID);
    if (sample0.a >= 1.0)
    {
        return sample0.rgb;
    }
    else
    {
        vec3 color = ptexture_nvidia(sample0, tex, uv, data);

        if (visualize) color.rgb += vec3(0, 0.5, 0);

        return color.rgb;
    }
}

void main()
{
    vec3 color = ptexture_reduced_traverse(aTex, UV, faceID).rgb;

    FragColor = vec4(color, 1);
}
