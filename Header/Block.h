#pragma once

struct Block {
    float x, y;  
    float width, height;
    float r, g, b;    
    
    Block(float xPos, float yPos, float w, float h, float red, float green, float blue)
        : x(xPos), y(yPos), width(w), height(h), r(red), g(green), b(blue) {}
    
    bool overlaps(const Block& other) const;
    
    float getLeftOverhang(const Block& base) const;
    
    float getRightOverhang(const Block& base) const;
    
    float getTotalOverhang(const Block& base) const;
};
