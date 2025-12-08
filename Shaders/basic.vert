#version 330 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;

uniform mat4 uModel;
uniform mat4 uProjection;

out vec2 texCoord;

void main()
{
    gl_Position = uProjection * uModel * vec4(inPos, 0.0, 1.0);
    texCoord = inTexCoord;
}
