#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in int aFaceID;

out vec2 UV;
flat out int faceID;

uniform mat4 mvp;

void main()
{
	UV = aUV;
	faceID = aFaceID;
	gl_Position = mvp * vec4(aPos.x, aPos.y, aPos.z, 1.0);
}

