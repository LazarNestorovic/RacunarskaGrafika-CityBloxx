#include "../Header/Game.h"
#include "../Header/Util.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream> 
#include <fstream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Game::Game()
    : state(PLAYING), currentBlock(nullptr), blockFalling(false),
    swingAngle(0.0f), swingSpeed(2.0f), swingRadius(0.4f),
    fallSpeed(1.2f), buildingSwayAngle(0.0f), buildingSwaySpeed(0.0f),
    buildingSwayAmplitude(0.0f), score(0), cameraY(0.0f), targetCameraY(0.0f),
    aspectRatio(1.0f), groundTexture(0), ropeTexture(0), blockTexture(0), backgroundTexture(0),
    textRenderer(nullptr), windowWidth(1920), windowHeight(1080), textShaderProgram(0)
{
    srand(static_cast<unsigned int>(time(nullptr)));

    // Inicijalizuj projection matricu kao identity matricu
    for (int i = 0; i < 16; i++) {
        projectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    initOpenGL();
    initTextRenderer();
    spawnNewBlock();
}

Game::~Game() {
    if (currentBlock) delete currentBlock;
    if (textRenderer) delete textRenderer;
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &ropeVAO);
    glDeleteBuffers(1, &ropeVBO);
    glDeleteVertexArrays(1, &blockVAO);
    glDeleteBuffers(1, &blockVBO);
    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteBuffers(1, &backgroundVBO);
    glDeleteProgram(shaderProgram);
    if (textShaderProgram) glDeleteProgram(textShaderProgram);
}

GameState Game::getGameState() const {
    return state;
}

// setAspectRatio - Normalizuje dimenzije za razli?ite ekrane
void Game::setAspectRatio(float width, float height) {
    aspectRatio = width / height;

    if (aspectRatio > 1.0f) {
        // ŠIROKI EKRAN (npr. 16:9)
        float left = -aspectRatio;
        float right = aspectRatio;
        float bottom = -1.0f;
        float top = 1.0f;

        // Orthographic projection formula
        projectionMatrix[0] = 2.0f / (right - left);   // Xskaliranje
        projectionMatrix[1] = 0.0f;
        projectionMatrix[2] = 0.0f;
        projectionMatrix[3] = 0.0f;

        projectionMatrix[4] = 0.0f;
        projectionMatrix[5] = 2.0f / (top - bottom);   // Yskaliranje
        projectionMatrix[6] = 0.0f;
        projectionMatrix[7] = 0.0f;

        projectionMatrix[8] = 0.0f;
        projectionMatrix[9] = 0.0f;
        projectionMatrix[10] = 1.0f;
        projectionMatrix[11] = 0.0f;

        projectionMatrix[12] = -(right + left) / (right - left);   // Xtranslacija
        projectionMatrix[13] = -(top + bottom) / (top - bottom);   // Ytranslacija
        projectionMatrix[14] = 0.0f;
        projectionMatrix[15] = 1.0f;
    }
    else {
        // VISOKI EKRAN (portret mode)
        float left = -1.0f;
        float right = 1.0f;
        float bottom = -1.0f / aspectRatio;
        float top = 1.0f / aspectRatio;

        projectionMatrix[0] = 2.0f / (right - left);
        projectionMatrix[1] = 0.0f;
        projectionMatrix[2] = 0.0f;
        projectionMatrix[3] = 0.0f;

        projectionMatrix[4] = 0.0f;
        projectionMatrix[5] = 2.0f / (top - bottom);
        projectionMatrix[6] = 0.0f;
        projectionMatrix[7] = 0.0f;

        projectionMatrix[8] = 0.0f;
        projectionMatrix[9] = 0.0f;
        projectionMatrix[10] = 1.0f;
        projectionMatrix[11] = 0.0f;

        projectionMatrix[12] = -(right + left) / (right - left);
        projectionMatrix[13] = -(top + bottom) / (top - bottom);
        projectionMatrix[14] = 0.0f;
        projectionMatrix[15] = 1.0f;
    }
}

void Game::preprocessTexture(unsigned int& texture, const char* filepath) {
    texture = loadImageToTexture(filepath);

    if (texture == 0) {
        std::cout << "GREŠKA: Tekstura nije ucitana: " << filepath << std::endl;
        return;
    }

    std::cout << "Tekstura ucitana: " << filepath << ", ID: " << texture << std::endl;

    glBindTexture(GL_TEXTURE_2D, texture);

    glGenerateMipmap(GL_TEXTURE_2D);

    std::string path(filepath);
    if (path.find("rope") != std::string::npos) {
        // KONOPAC - clamp horizontalno, repeat VERTIKALNO
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else if (path.find("block") != std::string::npos) {
        // BLOKOVI - clamp obe ose (1x tekstura po bloku)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else if (path.find("background") != std::string::npos) {
        // POZADINA - clamp obe ose (1x po ekranu)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else {
        // ZEMLJA - repeat horizontalno, clamp vertikalno
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Filter strategije (linear za glatko skaliranje)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);  // Unbind
}

void Game::initOpenGL() {
    // GLAVNI VAO - za blokove i zemlju
    float vertices[] = {
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f, -0.5f,   50.0f,  0.0f,   // Donji desni (50x ponavljanje horizontalno)
         0.5f,  0.5f,   50.0f,  1.0f,   // Gornji desni (50x horizontalno, 1x vertikalno - razvu?eno)
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f,  0.5f,   50.0f,  1.0f,   // Gornji desni
        -0.5f,  0.5f,   0.0f,   1.0f    // Gornji levi (1x vertikalno - razvu?eno)
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //  ROPE VAO - za konopac sa vertikalnim ponavljanjem teksture
    float ropeVertices[] = {
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f, -0.5f,   1.0f,   0.0f,   // Donji desni (1x horizontalno - clamp)
         0.5f,  0.5f,   1.0f,   10.0f,  // Gornji desni (10x VERTIKALNO ponavljanje)
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f,  0.5f,   1.0f,   10.0f,  // Gornji desni
        -0.5f,  0.5f,   0.0f,   10.0f   // Gornji levi (10x VERTIKALNO ponavljanje)
    };

    glGenVertexArrays(1, &ropeVAO);
    glGenBuffers(1, &ropeVBO);

    glBindVertexArray(ropeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ropeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ropeVertices), ropeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // BLOCK VAO - za blokove sa 1x1 teksturom
    float blockVertices[] = {
        // Pozicija      UV koordinate
        // X      Y       U       V
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f, -0.5f,   1.0f,   0.0f,   // Donji desni
         0.5f,  0.5f,   1.0f,   1.0f,   // Gornji desni
        -0.5f, -0.5f,   0.0f,   0.0f,   // Donji levi
         0.5f,  0.5f,   1.0f,   1.0f,   // Gornji desni
        -0.5f,  0.5f,   0.0f,   1.0f    // Gornji levi
    };

    glGenVertexArrays(1, &blockVAO);
    glGenBuffers(1, &blockVBO);

    glBindVertexArray(blockVAO);
    glBindBuffer(GL_ARRAY_BUFFER, blockVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blockVertices), blockVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // BACKGROUND VAO - puna pokrivka ekrana (fiksna pozadina)
    float backgroundVertices[] = {
        // Pozicija (pokriva ceo ekran)  UV koordinate
        // X      Y       U       V
        -10.0f, -10.0f,  0.0f,   0.0f,   // Donji levi
         10.0f, -10.0f,  1.0f,   0.0f,   // Donji desni
         10.0f,  10.0f,  1.0f,   1.0f,   // Gornji desni
        -10.0f, -10.0f,  0.0f,   0.0f,   // Donji levi
         10.0f,  10.0f,  1.0f,   1.0f,   // Gornji desni
        -10.0f,  10.0f,  0.0f,   1.0f    // Gornji levi
    };

    glGenVertexArrays(1, &backgroundVAO);
    glGenBuffers(1, &backgroundVBO);

    glBindVertexArray(backgroundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVertices), backgroundVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    shaderProgram = createShader("Shaders/basic.vert", "Shaders/basic.frag");

    preprocessTexture(backgroundTexture, "Textures/background4.jpg");
    preprocessTexture(groundTexture, "Textures/pixel-ground.png");
    preprocessTexture(ropeTexture, "Textures/rope.png");
    preprocessTexture(blockTexture, "Textures/block2.png");
}

void Game::initTextRenderer() {
    std::cout << "\n=== INICIJALIZACIJA TEXT RENDERER-A ===" << std::endl;

    textShaderProgram = createShader("Shaders/text.vert", "Shaders/text.frag");

    textRenderer = new TextRenderer(textShaderProgram, windowWidth, windowHeight);

    if (!textRenderer->loadFont("C:/Windows/Fonts/arial.ttf", 48)) {
        std::cout << "ERROR: Failed to load font!" << std::endl;
    }

    std::cout << "=== TEXT RENDERER INICIJALIZOVAN ===" << std::endl;
}

void Game::setWindowSize(int width, int height) {
    windowWidth = width;
    windowHeight = height;

    if (textRenderer) {
        delete textRenderer;
        textRenderer = new TextRenderer(textShaderProgram, width, height);
        textRenderer->loadFont("C:/Windows/Fonts/arial.ttf", 48);
    }
}

float Game::getRandomColor() {
    return 0.3f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.7f));
}

void Game::spawnNewBlock() {
    if (currentBlock) delete currentBlock;

    float r = getRandomColor();
    float g = getRandomColor();
    float b = getRandomColor();

    // POCETNA POZICIJA - Blok visi direktno ispod kuke

    float initialX = 0.0f;                              // Centralno ispod kuke
    float initialY = (HOOK_Y + cameraY) - ROPE_LENGTH;  // Na dužini užeta ispod kuke u world space

    currentBlock = new Block(
        initialX, 
        initialY,
        BLOCK_WIDTH,
        BLOCK_HEIGHT,
        r, g, b
    );

    blockFalling = false;
    swingAngle = 0.0f;
    swingSpeed = 2.0f;
}

void Game::updateCamera(float deltaTime) {
    // Izracunaj target poziciju kamere na osnovu vrha zgrade
    if (!placedBlocks.empty()) {
        Block& topBlock = placedBlocks.back();
        float buildingTop = topBlock.y + topBlock.height / 2.0f;

        // Konstantna distanca = dužina užeta + visina bloka + 0.25 (dodatni prostor)
        float desiredDistance = ROPE_LENGTH + BLOCK_HEIGHT + 0.25f;

        // Trenutna pozicija ljuljajuceg bloka
        float currentBlockWorldY = HOOK_Y + cameraY;  // Pozicija kuke u world space

        // Trenutna distanca izmedju vrha zgrade i pozicije kuke (gde visi blok)
        float currentDistance = currentBlockWorldY - buildingTop;

        if (currentDistance < desiredDistance) {
            targetCameraY = buildingTop + desiredDistance - HOOK_Y;
        }
        else {
            targetCameraY = cameraY;
        }
    }
    else {
        targetCameraY = 0.0f;
    }

    cameraY += (targetCameraY - cameraY) * CAMERA_SPEED * deltaTime;
}

void Game::update(float deltaTime) {
    static GameState lastState = PLAYING;
    if (state != lastState) {
        std::cout << "\n?? STATUS PROMENJEN: "
            << (lastState == PLAYING ? "PLAYING" : "GAME_OVER")
            << " -> "
            << (state == PLAYING ? "PLAYING" : "GAME_OVER") << std::endl;
        lastState = state;
    }

    if (state == GAME_OVER) {
        enteringName = true;
        double now = glfwGetTime();
        if (now - lastCursorBlink > 0.5) {
            cursorVisible = !cursorVisible;
            lastCursorBlink = now;
        }
        return;
    }

    updateCamera(deltaTime);

    if (!blockFalling) {
        float angularAcceleration = -(GRAVITY / ROPE_LENGTH) * sin(swingAngle);
        swingSpeed += angularAcceleration * deltaTime;

        //AŽURIRAJ UGAO
        swingAngle += swingSpeed * deltaTime;

        //OGRANICENIE SA BLAGIM USPORAVANJEM
        if (swingAngle > MAX_SWING_ANGLE) {
            swingAngle = MAX_SWING_ANGLE;     // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        }
        else if (swingAngle < -MAX_SWING_ANGLE) {
            swingAngle = -MAX_SWING_ANGLE;    // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        }

        // IZRACUNAJ POZICIJU BLOKA 
        float pivotX = 0.0f;
        float pivotY = HOOK_Y + cameraY;  // Pivot se pomera sa kamerom u world space

        currentBlock->x = pivotX + ROPE_LENGTH * sin(swingAngle);
        currentBlock->y = pivotY - ROPE_LENGTH * cos(swingAngle);

    }
    else {
        // Padanje bloka
        currentBlock->y -= fallSpeed * deltaTime;

        // Provera da li je blok stigao do zemlje
        if (placedBlocks.empty()) {
            if (currentBlock->y - currentBlock->height / 2.0f <= GROUND_Y) {
                currentBlock->y = GROUND_Y + currentBlock->height / 2.0f;
                placedBlocks.push_back(*currentBlock);
                score++;
                spawnNewBlock();
            }
        }
        else {
            // Provera kolizije sa vrhom zgrade
            Block& topBlock = placedBlocks.back();
            float targetY = topBlock.y + topBlock.height / 2.0f + currentBlock->height / 2.0f;

            if (currentBlock->y <= targetY) {
                currentBlock->y = targetY;

                if (currentBlock->overlaps(topBlock)) {
                    float overhang = currentBlock->getTotalOverhang(topBlock);
                    float maxOverhang = currentBlock->width * OVERHANG_LIMIT;

                    if (overhang > maxOverhang) {
                        std::cout << "\n========================================" << std::endl;
                        std::cout << "?? GAME OVER - Blok previše viri!" << std::endl;
                        std::cout << "   Overhang: " << overhang << " (Max: " << maxOverhang << ")" << std::endl;
                        std::cout << "   Score: " << score << std::endl;
                        std::cout << "========================================\n" << std::endl;
                        state = GAME_OVER;
                    }
                    else {
                        placedBlocks.push_back(*currentBlock);
                        score++;
                        std::cout << "? Blok uspešno postavljen! Score: " << score << std::endl;

                        if (overhang > 0.01f) {
                            float errorRatio = overhang / maxOverhang;
                            buildingSwayAmplitude += errorRatio * 0.05f;
                            buildingSwaySpeed = 2.0f + buildingSwayAmplitude * 3.0f;
                        }

                        spawnNewBlock();
                    }
                }
                else {
                    std::cout << "\n========================================" << std::endl;
                    std::cout << "?? GAME OVER - Potpuni promašaj!" << std::endl;
                    std::cout << "   Blokovi se ne preklapaju" << std::endl;
                    std::cout << "   Current block X: " << currentBlock->x << std::endl;
                    std::cout << "   Top block X: " << topBlock.x << std::endl;
                    std::cout << "   Score: " << score << std::endl;
                    std::cout << "========================================\n" << std::endl;
                    state = GAME_OVER;
                }
            }
        }
    }

    if (buildingSwayAmplitude > 0.0f) {
        buildingSwayAngle += buildingSwaySpeed * deltaTime;
    }
}

void Game::dropBlock() {
    if (state == GAME_OVER) {
        std::cout << "?? dropBlock(): Ne mogu dropovati blok - GAME_OVER!" << std::endl;
        return;
    }
    if (blockFalling) {
        std::cout << "?? dropBlock(): Blok ve? pada!" << std::endl;
        return;
    }

    std::cout << "?? dropBlock(): Pustem blok!" << std::endl;
    blockFalling = true;
}

// restart - Restartuje igru
void Game::restart() {
    std::cout << "\n?? RESTARTOVANJE IGRE..." << std::endl;

    state = PLAYING;
    score = 0;

    placedBlocks.clear();

    cameraY = 0.0f;
    targetCameraY = 0.0f;

    buildingSwayAngle = 0.0f;
    buildingSwaySpeed = 0.0f;
    buildingSwayAmplitude = 0.0f;

    if (currentBlock) {
        delete currentBlock;
        currentBlock = nullptr;
    }

    blockFalling = false;
    swingAngle = 0.0f;
    swingSpeed = 2.0f;

    spawnNewBlock();

    std::cout << "? Igra restartovana! Score: 0" << std::endl;
}

void Game::drawBlock(const Block& block, float offsetX, float rotation) {
    glUseProgram(shaderProgram);

    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    float cosR = cos(rotation);
    float sinR = sin(rotation);

    float model[16] = {
        block.width * cosR, block.width * sinR, 0.0f, 0.0f,
        -block.height * sinR, block.height * cosR, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        block.x + offsetX, block.y - cameraY, 0.0f, 1.0f
    };

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    if (blockTexture != 0) {
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1); 

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blockTexture);

        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela - bez tinta

        glBindVertexArray(blockVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    else {
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, block.r, block.g, block.b, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

void Game::drawRope(float x1, float y1, float x2, float y2) {
    glUseProgram(shaderProgram);

    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);

    float angle = atan2(dx, -dy);

    float ropeWidth = 0.03f;

    float centerX = (x1 + x2) / 2.0f;
    float centerY = (y1 + y2) / 2.0f;

    float cosA = cos(angle);
    float sinA = sin(angle);

    float model[16] = {
        ropeWidth * cosA,  ropeWidth * sinA,  0.0f, 0.0f,
        -length * sinA,    length * cosA,     0.0f, 0.0f,
        0.0f,              0.0f,              1.0f, 0.0f,
        centerX,           centerY,           0.0f, 1.0f
    };

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    if (ropeTexture != 0) {
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ropeTexture);

        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(ropeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    else {
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.5f, 0.35f, 0.2f, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

void Game::drawHook(float x, float y) {
    glUseProgram(shaderProgram);

    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    glUniform1i(useTexLoc, 0);

    float hookWidth = 0.06f;
    float hookHeight = 0.04f;

    float model[16] = {
        hookWidth, 0.0f, 0.0f, 0.0f,
        0.0f, hookHeight, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, 0.0f, 1.0f
    };

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform4f(colorLoc, 0.7f, 0.7f, 0.7f, 1.0f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Game::drawText(const char* text, float x, float y, float scale) {
    glUseProgram(shaderProgram);

    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    glUniform1i(useTexLoc, 0);

    float charWidth = 0.05f * scale;
    float charHeight = 0.08f * scale;
    float spacing = charWidth * 1.2f;

    int i = 0;
    while (text[i] != '\0') {
        float model[16] = {
            charWidth, 0.0f, 0.0f, 0.0f,
            0.0f, charHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x + i * spacing, y, 0.0f, 1.0f
        };

        GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        i++;
    }
}

void Game::onKeyPressed(int key) {

    if (state == GAME_OVER) {

        if (!enteringName)
            enteringName = true;

        if (key == GLFW_KEY_BACKSPACE) {
            if (!playerName.empty()) {
                playerName.pop_back();
            }
            return;
        }

        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            enteringName = false;

            if (!playerName.empty()) {
                std::cout << "Ime sacuvano: " << playerName << std::endl;
                std::string filename = "PlayersScore.csv";
                std::ifstream infile(filename);
                std::vector<std::pair<std::string, int>> scores;
                bool found = false;
                if (infile.is_open()) {
                    std::string line;
                    while (std::getline(infile, line)) {
                        std::stringstream ss(line);
                        std::string name;
                        int scoreValue;
                        if (std::getline(ss, name, ',') && ss >> scoreValue) {
                            if (name == playerName) {
                                scoreValue = score;
                                found = true;
                            }
                            scores.push_back({ name, scoreValue });
                        }
                    }
                    infile.close();
                }

                if (!found) {
                    scores.push_back({ playerName, score });
                }

                std::ofstream outfile(filename, std::ios::trunc);
                for (const auto& entry : scores) {
                    outfile << entry.first << "," << entry.second << "\n";
                }
                outfile.close();
            }
            return;
        }
    }
}

void Game::onCharEntered(unsigned int codepoint) {
    if (state == GAME_OVER && enteringName) {

        if (playerName.length() < MAX_NAME_LENGTH) {
            playerName.push_back((char)codepoint);
        }
    }
}

void Game::render() {
    glUseProgram(shaderProgram);

    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");

    if (backgroundTexture != 0) {
        float bgScaleX = aspectRatio * 0.1f;
        float bgScaleY = 0.1f;

        float backgroundModel[16] = {
            bgScaleX, 0.0f, 0.0f, 0.0f,
            0.0f, bgScaleY, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, backgroundModel);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);

        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);

        glUniform1i(useTexLoc, 1);
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(backgroundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    float groundHeight = GROUND_Y - (-1.0f);
    float groundCenterY = (GROUND_Y + (-1.0f)) / 2.0f;

    float groundModel[16] = {
        100.0f, 0.0f, 0.0f, 0.0f,
        0.0f, groundHeight, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, groundCenterY - cameraY, 0.0f, 1.0f
    };

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, groundModel);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, groundTexture);

    GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
    glUniform1i(texLoc, 0);

    glUniform1i(useTexLoc, 1);
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUniform1i(useTexLoc, 0);

    float swayOffset = 0.0f;
    if (buildingSwayAmplitude > 0.0f) {
        swayOffset = sin(buildingSwayAngle) * buildingSwayAmplitude;
    }

    for (const auto& block : placedBlocks) {
        drawBlock(block, swayOffset);
    }

    if (currentBlock) {
        if (!blockFalling) {
            float hookX = 0.0f;
            float hookY = HOOK_Y;
            drawHook(hookX, hookY);

            float blockTopX = currentBlock->x;
            float blockTopY = currentBlock->y + currentBlock->height / 2.0f;

            drawRope(hookX, hookY, blockTopX, blockTopY - cameraY);
        }

        drawBlock(*currentBlock);
    }

    if (textRenderer) {
        std::string controlsText = "ESC - Exit  |  CTRL + R - Restart  |  ENTER or LEFT MOUSE CLICK - Drop Block";
        float controlsWidth = textRenderer->getTextWidth(controlsText, 0.5f);
        float controlsX = windowWidth - controlsWidth - 20.0f; 
        textRenderer->renderText(controlsText, controlsX, 50.0f, 0.5f, 0.9f, 0.9f, 0.9f);

        std::ostringstream scoreStream;
        scoreStream << "Score: " << score;
        std::string scoreText = scoreStream.str();
        float scoreWidth = textRenderer->getTextWidth(scoreText, 1.0f);
        float scoreX = windowWidth - scoreWidth - 20.0f;
        textRenderer->renderText(scoreText, scoreX, 100.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (textRenderer) {
        float panelWidth = 800.0f;
        float panelHeight = 300.0f;
        float panelX = 0.0f;  
        float panelY = 0.0f;  

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(shaderProgram);

        float screenProjection[16] = {
            2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / windowHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };

        GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);

        float rectModel[16] = {
            panelWidth, 0.0f, 0.0f, 0.0f,
            0.0f, panelHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            panelX, panelY, 0.0f, 1.0f
        };

        GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rectModel);

        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.0f, 0.2f, 0.5f, 0.8f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDisable(GL_BLEND);

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

        std::string line1 = "RA 145/2022";
        std::string line2 = "Lazar Nestorovic";

        float fontSize = 0.5f;

        float text1Width = textRenderer->getTextWidth(line1, fontSize);
        float text2Width = textRenderer->getTextWidth(line2, fontSize);

        float textX1 = (panelWidth - text1Width) / 2.0f - 200.0f;
        float textX2 = (panelWidth - text2Width) / 2.0f - 200.0f;

        float textY1 = 80.0f;
        float textY2 = 110.0f;

        textRenderer->renderText(line1, textX1, textY1, fontSize, 1.0f, 1.0f, 1.0f);
        textRenderer->renderText(line2, textX2, textY2, fontSize, 1.0f, 1.0f, 1.0f);
    }

    if (textRenderer) {
        float panelWidth = 500.0f; 
        float panelHeight = 300.0f; 
        float panelX = 0.0f;  
        float panelY = 0.0f;   


        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(shaderProgram);

        float screenProjection[16] = {
            2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / windowHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f
        };
        GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);

        float rectModel[16] = {
            panelWidth, 0.0f,       0.0f, 0.0f,
            0.0f,       panelHeight,0.0f, 0.0f,
            0.0f,       0.0f,       1.0f, 0.0f,
            panelX,     panelY,     0.0f, 1.0f
        };
        GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rectModel);

        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);

        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 0.8f); 

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDisable(GL_BLEND);

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

        std::vector<std::pair<std::string, int>> topScores;
        std::ifstream file("PlayersScore.csv");
        if (!file.is_open()) {
            std::cerr << "Ne mogu da otvorim fajl: " << "PlayersScore.csv" << std::endl;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string playerName;
            std::string scoreStr;

            if (std::getline(ss, playerName, ',') && std::getline(ss, scoreStr)) {
                int score = std::stoi(scoreStr);
                topScores.emplace_back(playerName, score);
            }
        }
        file.close();

        std::sort(topScores.begin(), topScores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        if (topScores.size() > 3) {
            topScores.resize(3);
        }

        std::pair<std::string, int> temp = topScores[0];
        topScores[0] = topScores[2];
        topScores[2] = temp;

        float fontSize = 0.7f;
        float startX = 20.0f; 
        float startY = 20.0f;   
        float lineSpacing = 40.0f; 

        for (int i = 0; i < topScores.size(); ++i) {
            std::string text = topScores[i].first + " " + std::to_string(topScores[i].second);
            float y = startY + i * lineSpacing;

            float r, g, b;
            if (i == 2) {
                r = 1.0f; g = 0.84f; b = 0.0f;       // zlato
            }
            else if (i == 1) {
                r = 0.75f; g = 0.75f; b = 0.75f;    // srebro
            }
            else {
                r = 0.8f; g = 0.5f; b = 0.2f;       // bronza
            }

            textRenderer->renderText(text, startX, windowHeight - y, fontSize, r, g, b);
        }
    }


    if (state == GAME_OVER) {

        if (textRenderer) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(shaderProgram);

            float screenProjection[16] = {
                2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f / windowHeight, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                -1.0f, 1.0f, 0.0f, 1.0f
            };

            GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);

            float panelWidth = 800.0f;
            float panelHeight = 600.0f;
            float panelX = windowWidth / 2.0f;
            float panelY = windowHeight / 2.0f; 

            float rectModel[16] = {
                panelWidth, 0.0f, 0.0f, 0.0f,
                0.0f, panelHeight, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                panelX, panelY, 0.0f, 1.0f
            };

            GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rectModel);

            GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
            glUniform1i(useTexLoc, 0);

            GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
            glUniform4f(colorLoc, 0.2f, 0.2f, 0.2f, 0.85f);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glDisable(GL_BLEND);

            glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

            std::string gameOverText = "GAME OVER";
            float textWidth = textRenderer->getTextWidth(gameOverText, 2.0f);
            float centerX = (windowWidth - textWidth) / 2.0f;
            float centerY = windowHeight / 2.0f;

            textRenderer->renderText(gameOverText, centerX, centerY, 2.0f, 1.0f, 0.0f, 0.0f);

            std::string nameText = "Enter your name to save your score (optional)";
            float nameTextWidth = textRenderer->getTextWidth(nameText, 0.6f);
            float nameX = (windowWidth - nameTextWidth) / 2.0f;
            float nameY = centerY - 200.0f;

            textRenderer->renderText(nameText, nameX, nameY, 0.6f, 1.0f, 0.0f, 0.0f);

            if (state == GAME_OVER && enteringName) {

                std::string display = playerName;
                float w = textRenderer->getTextWidth(display, 1.2f);
                float x = (windowWidth - w) / 2.0f;
                float y = nameY + 70.0f;

                textRenderer->renderText(display, x, y, 1.2f, 1.0f, 1.0f, 1.0f);

                if (cursorVisible) {
                    float width = textRenderer->getTextWidth(playerName, 1.2f);
                    textRenderer->renderText("|", x + width, y, 1.2f, 1.0f, 1.0f, 1.0f);
                }
            }

            std::ostringstream scoreSt;
            scoreSt << "Your score: " << score;
            std::string scoreText = scoreSt.str();
            float scoreWidth = textRenderer->getTextWidth(scoreText, 1.0f);
            float scoreX = (windowWidth - scoreWidth) / 2.0f;
            float scoreY = centerY + 80.0f;

            textRenderer->renderText(scoreText, scoreX, scoreY, 1.0f, 1.0f, 1.0f, 1.0f);

            std::string restartText = "Press CTRL + R to Restart";
            float restartWidth = textRenderer->getTextWidth(restartText, 1.0f);
            float restartX = (windowWidth - restartWidth) / 2.0f;
            float restartY = scoreY + 80.0f;

            textRenderer->renderText(restartText, restartX, restartY, 1.0f, 1.0f, 1.0f, 1.0f);

        }
        else {
            std::cout << "   ? ERROR: textRenderer je nullptr!" << std::endl;
        }
    }

    for (int i = 0; i < score && i < 20; i++) {
        float model[16] = {
            0.02f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.02f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -0.95f + i * 0.03f, 0.95f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniform4f(colorLoc, 1.0f, 1.0f, 0.0f, 1.0f);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}
