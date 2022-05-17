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

#define NUM_TEX 24

uniform sampler2DArray aTexBorder[NUM_TEX];
uniform sampler2DArray aTexClamp[NUM_TEX];

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 ptexture_single(sampler2DArray tex[NUM_TEX], vec2 uv, uint texIDsliceID)
{
    uint texID = texIDsliceID & 0xFFFFu;
    uint sliceID = texIDsliceID >> 16;

    vec4 color = texture(tex[texID], vec3(uv, sliceID));

    vec2 wUV = fwidth(UV);
    vec3 size = textureSize(tex[texID], 0);

    vec2 m = step(vec2(1, 1), wUV * size.xy);
    //color += mix(vec4(1, 0, 0, 0), vec4(0, 0, 1, 0), max(m.x, m.y));

    return color;
}

vec3 ptexture_intel(sampler2DArray texBorder[NUM_TEX], sampler2DArray texClamp[NUM_TEX], vec2 uv, uint texIDsliceID)
{
    uint texID = texIDsliceID & 0xFFFFu;
    uint sliceID = texIDsliceID >> 16;

    vec4 border_color = texture(texBorder[texID], vec3(uv, sliceID));
    vec4 clamp_color = texture(texClamp[texID], vec3(uv, sliceID));

    vec3 color = border_color.a == 0 ? clamp_color.rgb : border_color.rgb / border_color.a;

    return color;
}

vec3 ptexture_hybrid(sampler2DArray texBorder[NUM_TEX], sampler2DArray texClamp[NUM_TEX], vec2 uv, int faceID)
{
    FaceData data = face_data[faceID];

    // Figure out if we will be applying a min or mag filter
    uint texID = data.texIDsliceID & 0xFFFFu;
    vec2 filterSize = fwidth(UV) * textureSize(texBorder[texID], 0).xy;
    float a = smoothstep(0, 1, filterSize.x) + smoothstep(0, 1, filterSize.y);
    if (a > 1.5)
    {
        // This means that we are applying a min filter in at least one axis
        // With MSAA this means we can get away with only one texture sample.

        return ptexture_intel(texBorder, texClamp, uv, data.texIDsliceID);
    }
    else
    {
        // Here we are applying a mag filter in both axis, do the nvidia method

        vec4 color = vec4(0);
        color += ptexture_single(texBorder, uv, data.texIDsliceID);

        uint neighbor0_id = data.neighbor01Indices & 0xFFFFu;
        uint neighbor1_id = data.neighbor01Indices >> 16;

        uint neighbor2_id = data.neighbor23Indices & 0xFFFFu;
        uint neighbor3_id = data.neighbor23Indices >> 16;

        uint n0_transform = (face_data[neighbor0_id].neighbor0123Transform >> 0 ) & 0xFFu;
        uint n1_transform = (face_data[neighbor0_id].neighbor0123Transform >> 8 ) & 0xFFu;
        uint n2_transform = (face_data[neighbor0_id].neighbor0123Transform >> 16) & 0xFFu;
        uint n3_transform = (face_data[neighbor0_id].neighbor0123Transform >> 24) & 0xFFu;
    
        vec2 n0_uv = neighborTransforms[n0_transform] * vec3(uv, 1);
        vec2 n1_uv = neighborTransforms[n1_transform] * vec3(uv, 1);
        vec2 n2_uv = neighborTransforms[n2_transform] * vec3(uv, 1);
        vec2 n3_uv = neighborTransforms[n3_transform] * vec3(uv, 1);

        color += ptexture_single(texBorder, n0_uv, face_data[neighbor0_id].texIDsliceID);
        color += ptexture_single(texBorder, n1_uv, face_data[neighbor1_id].texIDsliceID);
        color += ptexture_single(texBorder, n2_uv, face_data[neighbor2_id].texIDsliceID);
        color += ptexture_single(texBorder, n3_uv, face_data[neighbor3_id].texIDsliceID);

        color += vec4(1, 0, 0, 0);

        return color.rgb / color.a;
    }
}

void main()
{
    vec3 color = ptexture_hybrid(aTexBorder, aTexClamp, UV, faceID).rgb;

    FragColor = vec4(color, 1);
}
