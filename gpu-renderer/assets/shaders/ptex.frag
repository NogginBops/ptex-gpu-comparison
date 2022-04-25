#version 330 core

in vec2 UV;
flat in int faceID;

out vec4 FragColor;

uniform sampler2D tex;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
	FragColor = vec4(hsv2rgb(vec3(((faceID * 143) % 256) / 256.0, UV)), 1);
    FragColor = vec4(UV, 0, 1);

    vec3 color = texture(tex, UV).rgb;
    FragColor = vec4(color, 1);
}
