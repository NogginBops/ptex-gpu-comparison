#version 330 core
in vec3 fColor;
layout (location = 0) out uint faceID;
layout (location = 1) out vec3 uv;
layout (location = 2) out vec4 uv_deriv;

void main()
{
   faceID = uint(gl_PrimitiveID + 1);
   uv = fColor.xyz;
   uv_deriv.xy = dFdx(fColor.xy);
   uv_deriv.zw = dFdy(fColor.xy);
}
