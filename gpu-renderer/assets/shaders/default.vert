#version 330 core

layout (location = 0) in vec3 aPos;
//layout (location = 1) in vec2 aUV;

out vec3 fColor;

uniform mat4 vp;

void main()
{
	const vec3 colors[3] = vec3[]( vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );
	fColor = colors[gl_VertexID%3];
	gl_Position = vp * vec4(aPos.x, aPos.y, aPos.z, 1.0);
}