#pragma once

struct Block {
    float x, y;        // Pozicija (centar bloka)
    float width, height;
    float r, g, b;     // Boja bloka
    
    Block(float xPos, float yPos, float w, float h, float red, float green, float blue)
        : x(xPos), y(yPos), width(w), height(h), r(red), g(green), b(blue) {}
    
    // Proverava da li se blokovi preklapaju
    bool overlaps(const Block& other) const;
    
    // Vra?a koliko blok viri sa leve strane
    float getLeftOverhang(const Block& base) const;
    
    // Vra?a koliko blok viri sa desne strane
    float getRightOverhang(const Block& base) const;
    
    // Vra?a ukupan overhang (koliko blok viri van osnove)
    float getTotalOverhang(const Block& base) const;
};
