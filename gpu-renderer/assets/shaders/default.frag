#version 330 core

in vec3 fColor;

out vec4 FragColor;

uniform sampler2D tex;

void main()
{
//	const vec3 colors[3] = vec3[]( vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );
//	FragColor = vec4(round(fColor * 255.0) / 255.0, 1.0);
	vec3 color = texture(tex, fColor.xy).rgb;
	FragColor = vec4(color, 1.0);
}
