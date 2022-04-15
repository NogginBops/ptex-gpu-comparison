#version 330 core
in vec3 fColor;
out vec4 FragColor;
void main()
{
//   const vec3 colors[3] = vec3[]( vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );
//   FragColor = vec4(round(fColor * 255.0) / 255.0, 1.0);
   FragColor = vec4(fColor, 1.0);
}
