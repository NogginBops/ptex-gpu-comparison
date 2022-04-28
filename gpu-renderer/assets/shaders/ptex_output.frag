#version 330 core

in vec2 UV;
flat in int faceID;

layout (location = 0) out uint outFaceID;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outUVDeriv;

void main()
{
	outFaceID = uint(faceID) + 1u;
	outUV = UV;
	outUVDeriv.xy = dFdx(UV);
	outUVDeriv.zw = dFdy(UV);
}
