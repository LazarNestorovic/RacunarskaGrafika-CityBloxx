#pragma once
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Block.h"

enum GameState {
    PLAYING,
    GAME_OVER
};

class Game {
private:
    // OpenGL objekti
    unsigned int VAO, VBO;
    unsigned int ropeVAO, ropeVBO;  // Poseban VAO za konopac
    unsigned int blockVAO, blockVBO;  // Poseban VAO za blokove
    unsigned int backgroundVAO, backgroundVBO;  // ✅ VAO za pozadinu
    unsigned int shaderProgram;
    
    // Stanje igre
    GameState state;
    std::vector<Block> placedBlocks;  // Postavljeni blokovi (zgrada)
    Block* currentBlock;              // Trenutni blok koji se ljulja
    
    // Parametri trenutnog bloka
    bool blockFalling;                // Da li blok pada
    float swingAngle;                 // Ugao ljuljanja
    float swingSpeed;                 // Brzina ljuljanja
    float swingRadius;                // Radijus ljuljanja
    float fallSpeed;                  // Brzina padanja
    
    // Parametri zgrade
    float buildingSwayAngle;          // Ugao njihanja zgrade
    float buildingSwaySpeed;          // Brzina njihanja zgrade
    float buildingSwayAmplitude;      // Amplituda njihanja
    
    // Kamera - prati rast zgrade
    float cameraY;                    // Trenutna Y pozicija kamere (world space)
    float targetCameraY;              // Željena Y pozicija kamere (smooth interpolacija)
    
    // Aspect Ratio i Projection
    float aspectRatio;                // Odnos širine i visine ekrana
    float projectionMatrix[16];       // Projection matrica za korekciju aspect ratio-a
    
    // Teksture
    unsigned int groundTexture;       // Tekstura zemlje
    unsigned int ropeTexture;         // Tekstura konopca
    unsigned int blockTexture;        // Tekstura blokova (block2.png)
    unsigned int backgroundTexture;   // ✅ Tekstura pozadine (background.jpg)

    // Score
    int score;
    
    // Konstante - bazne vrednosti (za kvadratni ekran 1:1)
    const float BLOCK_WIDTH = 0.25f;   // Bazna širina bloka
    const float BLOCK_HEIGHT = 0.25f;  // Bazna visina bloka
    const float SWING_RADIUS = 0.4f;   // Radijus horizontalnog ljuljanja
    const float SWING_SPEED = 2.0f;
    const float FALL_SPEED = 1.2f;
    const float HOOK_Y = 0.9f;         // Pozicija kuke NA EKRANU (screen space)
    const float GROUND_Y = -0.95f;     // Pozicija zemlje (world space)
    const float OVERHANG_LIMIT = 0.33f; // 1/3 bloka sme da viri
    const float ROPE_LENGTH = 0.75f;    // Dužina užeta (radijus kružne putanje)
    const float MAX_SWING_ANGLE = 1.0f; // Maksimalni ugao ljuljanja u radijanima (~57 stepeni)
    const float GRAVITY = 9.81f;       // Gravitaciona konstanta
    const float CAMERA_SPEED = 3.0f;   // Brzina praćenja kamere (smooth interpolacija)
    
    void initOpenGL();
    void preprocessTexture(unsigned int& texture, const char* filepath);
    void spawnNewBlock();
    void updateCamera(float deltaTime);
    void drawBlock(const Block& block, float offsetX = 0.0f, float rotation = 0.0f);
    void drawRope(float x1, float y1, float x2, float y2);
    void drawHook(float x, float y);
    void drawText(const char* text, float x, float y, float scale);
    float getRandomColor();
    
public:
    Game();
    ~Game();
    
    void setAspectRatio(float width, float height);
    
    void update(float deltaTime);
    void render();
    void dropBlock();
    
    bool isGameOver() const { return state == GAME_OVER; }
    int getScore() const { return score; }
};
