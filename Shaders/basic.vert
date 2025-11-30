#version 330 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;  // NOVO: UV koordinate

uniform mat4 uModel;
uniform mat4 uProjection;

out vec2 texCoord;  // NOVO: Prosle?ivanje UV koordinata fragment shader-u

void main()
{
    gl_Position = uProjection * uModel * vec4(inPos, 0.0, 1.0);
    texCoord = inTexCoord;  // Prosle?ivanje UV koordinata
}
