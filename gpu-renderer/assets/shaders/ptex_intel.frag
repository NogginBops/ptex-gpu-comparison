#version 330 core

in vec2 UV;
flat in int faceID;

out vec4 FragColor;

struct FaceData 
{
	uint texIDsliceID;
};

#define MAX_FACES 1024
layout(std140) uniform FaceDataUniform {
	FaceData face_data[MAX_FACES];
};

uniform sampler2DArray aTexBorder[24];
uniform sampler2DArray aTexClamp[24];

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

    return texture(tex[texID], vec3(uv, sliceID));
}

vec3 ptexture(sampler2DArray texBorder[24], sampler2DArray texClamp[24], vec2 uv, int faceID)
{
    uint texIDsliceID = face_data[faceID].texIDsliceID;

    uint texID = texIDsliceID & 0xFFFFu;
    uint sliceID = texIDsliceID >> 16;

    vec4 border_color = texture(texBorder[texID], vec3(uv, sliceID));
    vec4 clamp_color = texture(texClamp[texID], vec3(uv, sliceID));

    vec3 color = border_color.a == 0 ? clamp_color.rgb : border_color.rgb / border_color.a;

    return color;
}

void main()
{
    vec3 color = ptexture(aTexBorder, aTexClamp, UV, faceID).rgb;

    FragColor = vec4(color, 1);
}
