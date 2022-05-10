#version 330 core

in vec4 WorldPosition;
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
	//outUVDeriv.xz = outUVDeriv.zx;

	//outUVDeriv.yw = outUVDeriv.wy;
	//mat2 inv = inverse(mat2(outUVDeriv.xz, outUVDeriv.yw));
	//outUVDeriv.xz = inv[0];
	//outUVDeriv.yw = inv[1];
	//outUVDeriv.x = length(dFdx(UV));
	//outUVDeriv.z = length(dFdy(UV));
	//outUVDeriv.yw = vec2(0);
	//outUVDeriv.xz = outUVDeriv.zx;
}
