#version 330 core

in vec2 texCoord;  // NOVO: Primljene UV koordinate

out vec4 outCol;

uniform vec4 uColor;           // Boja (za kompatibilnost sa starim kodom)
uniform sampler2D uTexture;    // NOVO: Tekstura
uniform int uUseTexture;       // NOVO: 0 = koristi boju, 1 = koristi teksturu

void main()
{
    if (uUseTexture == 1) {
        // REŽIM TEKSTURE
        vec4 texColor = texture(uTexture, texCoord);
        outCol = texColor * uColor;  // Mogu?e mešanje sa bojom (tinting)
    } else {
        // REŽIM BOJE (stari na?in - kompatibilno sa postoje?im kodom)
        outCol = uColor;
    }
}
