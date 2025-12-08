#version 330 core

in vec2 texCoord;

out vec4 outCol;

uniform vec4 uColor;
uniform sampler2D uTexture; 
uniform int uUseTexture;

void main()
{
    if (uUseTexture == 1) {
        vec4 texColor = texture(uTexture, texCoord);
        outCol = texColor * uColor;
    } else {
        outCol = uColor;
    }
}
