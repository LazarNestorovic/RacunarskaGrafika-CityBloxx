#include "../Header/Block.h"
#include <algorithm>
#include <cmath>

bool Block::overlaps(const Block& other) const {
    float left1 = x - width / 2.0f;
    float right1 = x + width / 2.0f;
    float left2 = other.x - other.width / 2.0f;
    float right2 = other.x + other.width / 2.0f;
    
    return !(right1 <= left2 || right2 <= left1);
}

float Block::getLeftOverhang(const Block& base) const {
    float myLeft = x - width / 2.0f;
    float baseLeft = base.x - base.width / 2.0f;
    
    if (myLeft < baseLeft) {
        return baseLeft - myLeft;
    }
    return 0.0f;
}

float Block::getRightOverhang(const Block& base) const {
    float myRight = x + width / 2.0f;
    float baseRight = base.x + base.width / 2.0f;
    
    if (myRight > baseRight) {
        return myRight - baseRight;
    }
    return 0.0f;
}

float Block::getTotalOverhang(const Block& base) const {
    return getLeftOverhang(base) + getRightOverhang(base);
}
