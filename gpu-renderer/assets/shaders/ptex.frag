#version 330 core

in vec2 UV;
flat in int faceID;
flat in uint texIDsliceID;
flat in uint neighbor01Indices;
flat in uint neighbor23Indices;

out vec4 FragColor;

const mat2x3 neighborTransforms[16] = mat2x3[16](
    mat2x3(
        -1, 0, 1,
        0, -1, 0
    ),
    mat2x3(
        0, 1, 1,
        -1, 0, 1
    ),
    mat2x3(
        1, 0, 0,
        0, 1, 1
    ),
    mat2x3(
        0, -1, 0,
        1, 0, 0
    ),
    mat2x3(
        0, -1, 1,
        1, 0, -1
    ),
    mat2x3(
        -1, 0, 2,
        0, -1, -1
    ),
    mat2x3(
        0, 1, 0,
        -1, 0, 2
    ),
    mat2x3(
        1, 0, -1,
        0, 1, 0
    ),
    mat2x3(
        1, 0, 0,
        0, 1, -1
    ),
    mat2x3(
        0, -1, 2,
        1, 0, 0
    ),
    mat2x3(
        -1, 0, 1,
        0, -1, 2
    ),
    mat2x3(
        0, 1, -1,
        -1, 0, 1
    ),
    mat2x3(
        0, 1, 0,
        -1, 0, 0
    ),
    mat2x3(
        1, 0, 1,
        0, 1, 0
    ),
    mat2x3(
        0, -1, 1,
        1, 0, 1
    ),
    mat2x3(
        -1, 0, 0,
        0, -1, 1
    )
);

struct FaceData 
{
	uint texIDsliceID;
	uint neighbor01Indices;
	uint neighbor23Indices;
	uint neighbor01Transform;
	uint neighbor23Transform;
};

#define MAX_FACES 1024
uniform FaceDataUniform {
	FaceData face_data[MAX_FACES];
};

uniform sampler2DArray aTex[32];

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 ptexture_single(sampler2DArray tex[32], vec2 uv, uint texIDsliceID)
{
    uint texID = texIDsliceID & 0xFFFFu;
    uint sliceID = texIDsliceID >> 16;

    return texture(tex[texID], vec3(UV, sliceID));
}

vec4 ptexture(sampler2DArray tex[32], vec2 uv, int faceID)
{
    FaceData data = face_data[faceID];

    vec4 sample0 = ptexture_single(tex, uv, data.texIDsliceID);

    uint neighbor0_id = data.neighbor01Indices & 0xFFFFu;
    uint neighbor1_id = data.neighbor01Indices >> 16;

    uint neighbor2_id = data.neighbor23Indices & 0xFFFFu;
    uint neighbor3_id = data.neighbor23Indices >> 16;

    vec4 sample1 = ptexture_single(tex, 1-uv, face_data[neighbor0_id].texIDsliceID);
    vec4 sample2 = ptexture_single(tex, 1-uv, face_data[neighbor1_id].texIDsliceID);
    vec4 sample3 = ptexture_single(tex, 1-uv, face_data[neighbor2_id].texIDsliceID);
    vec4 sample4 = ptexture_single(tex, 1-uv, face_data[neighbor3_id].texIDsliceID);

    return (sample0) / 1.0;
}

void main()
{
    vec3 color = ptexture(aTex, UV, faceID).rgb;

    FragColor = vec4(color, 1);
}
