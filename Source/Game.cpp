#include "../Header/Game.h"
#include "../Header/Util.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Game::Game() 
    : state(PLAYING), currentBlock(nullptr), blockFalling(false),
      swingAngle(0.0f), swingSpeed(2.0f), swingRadius(0.4f),
      fallSpeed(1.2f), buildingSwayAngle(0.0f), buildingSwaySpeed(0.0f),
      buildingSwayAmplitude(0.0f), score(0), cameraY(0.0f), targetCameraY(0.0f),
      aspectRatio(1.0f), groundTexture(0), ropeTexture(0), blockTexture(0)  // ✅ Dodaj blockTexture
{
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Inicijalizuj projection matricu kao identity matricu
    for (int i = 0; i < 16; i++) {
        projectionMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    
    initOpenGL();
    spawnNewBlock();
}

Game::~Game() {
    if (currentBlock) delete currentBlock;
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &ropeVAO);  // ✅ Oslobodi rope VAO
    glDeleteBuffers(1, &ropeVBO);        // ✅ Oslobodi rope VBO
    glDeleteVertexArrays(1, &blockVAO);  // ✅ Oslobodi block VAO
    glDeleteBuffers(1, &blockVBO);        // ✅ Oslobodi block VBO
    glDeleteProgram(shaderProgram);
}

// ================================================================
// setAspectRatio - Normalizuje dimenzije za razli?ite ekrane
// ================================================================
void Game::setAspectRatio(float width, float height) {
    aspectRatio = width / height;
    
    // Kreiramo orthographic projection matricu
    if (aspectRatio > 1.0f) {
        // ŠIROKI EKRAN (npr. 16:9)
        // Proširujemo horizontalno vidno polje
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
    } else {
        // VISOKI EKRAN (portret mode)
        // Proširujemo vertikalno vidno polje
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

// ================================================================
// preprocessTexture - Priprema teksturu za korišćenje (kao na vežbama)
// ================================================================
void Game::preprocessTexture(unsigned int& texture, const char* filepath) {
    texture = loadImageToTexture(filepath);  // Funkcija iz Util.h
    
    if (texture == 0) {  // Provera greške
        std::cout << "GREŠKA: Tekstura nije učitana: " << filepath << std::endl;
        return;  // Izađi samo ako je greška
    }
    
    std::cout << "Tekstura učitana: " << filepath << ", ID: " << texture << std::endl;
    
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Generisanje mipmapa
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // ✅ RAZLIČITE WRAP STRATEGIJE ZA RAZLIČITE TEKSTURE
    std::string path(filepath);
    if (path.find("rope") != std::string::npos) {
        // KONOPAC - clamp horizontalno, repeat VERTIKALNO
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else if (path.find("block") != std::string::npos) {
        // BLOKOVI - clamp obe ose (1x tekstura po bloku)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
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
    // ✅ GLAVNI VAO - za blokove i zemlju
    float vertices[] = {
        // Pozicija      UV koordinate
        // X      Y       U       V
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
    
    // Atribut 0 (pozicija) - 2 float-a, stride = 4 * sizeof(float)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Atribut 1 (UV koordinate) - 2 float-a, offset = 2 * sizeof(float)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // ✅ ROPE VAO - za konopac sa vertikalnim ponavljanjem teksture
    float ropeVertices[] = {
        // Pozicija      UV koordinate za VERTIKALNO ponavljanje
        // X      Y       U       V
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
    
    // Atribut 0 (pozicija)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Atribut 1 (UV koordinate)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // ✅ BLOCK VAO - za blokove sa 1x1 teksturom
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
    
    // Atribut 0 (pozicija)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Atribut 1 (UV koordinate)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Ucitaj shader
    shaderProgram = createShader("Shaders/basic.vert", "Shaders/basic.frag");
    
    // Učitaj teksture
    preprocessTexture(groundTexture, "Textures/pixel-ground.png");
    preprocessTexture(ropeTexture, "Textures/rope.png");
    preprocessTexture(blockTexture, "Textures/block2.png");  // ✅ Učitaj block2.png
}

float Game::getRandomColor() {
    return 0.3f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.7f));
}

void Game::spawnNewBlock() {
    if (currentBlock) delete currentBlock;
    
    float r = getRandomColor();
    float g = getRandomColor();
    float b = getRandomColor();
    
    // ========================================
    // POČETNA POZICIJA - Blok visi direktno ispod kuke
    // ========================================
    
    float initialX = 0.0f;                  // Centralno ispod kuke
    float initialY = HOOK_Y - ROPE_LENGTH;  // Na dužini užeta ispod kuke
    
    currentBlock = new Block(
        initialX,       // x - centar (direktno ispod kuke)
        initialY,       // y - visi na dužini užeta
        BLOCK_WIDTH, 
        BLOCK_HEIGHT, 
        r, g, b
    );
    
    blockFalling = false;
    swingAngle = 0.0f;    // Ugao = 0 (blok u srednjoj poziciji)
    swingSpeed = 2.0f;    // Početna brzina - daj bloku malo energije da krene
}

void Game::updateCamera(float deltaTime) {
    // Izra?unaj target poziciju kamere na osnovu vrha zgrade
    if (!placedBlocks.empty()) {
        Block& topBlock = placedBlocks.back();
        float buildingTop = topBlock.y + topBlock.height / 2.0f;
        
        // Konstantna distanca = dužina užeta + visina bloka + 0.25 (dodatni prostor)
        float desiredDistance = ROPE_LENGTH + BLOCK_HEIGHT + 0.25f;
        
        // Trenutna pozicija ljuljaju?eg bloka (u world space)
        float currentBlockWorldY = HOOK_Y + cameraY;  // Pozicija kuke u world space
        
        // Trenutna distanca izme?u vrha zgrade i pozicije kuke (gde visi blok)
        float currentDistance = currentBlockWorldY - buildingTop;
        
        // ? USLOV: Pomeri kameru SAMO ako je distanca manja od željene
        // Ovo spre?ava kretanje kamere dok zgrada još nije dovoljno visoka
        if (currentDistance < desiredDistance) {
            // Target kamera Y = vrh zgrade + želena distanca - HOOK_Y
            // (HOOK_Y je pozicija kuke na ekranu, mora ostati na 0.9)
            targetCameraY = buildingTop + desiredDistance - HOOK_Y;
        } else {
            // Distanca je dovoljna, kamera ostaje gde jeste
            targetCameraY = cameraY;
        }
    } else {
        // Ako nema blokova, kamera je na po?etnoj poziciji
        targetCameraY = 0.0f;
    }
    
    // Smooth interpolacija kamere (lerp)
    cameraY += (targetCameraY - cameraY) * CAMERA_SPEED * deltaTime;
}

void Game::update(float deltaTime) {
    if (state == GAME_OVER) return;
    
    // Ažuriraj kameru
    updateCamera(deltaTime);
    
    if (!blockFalling) {
        // ========================================
        // KONTINUALNO LJULJANJE SA REALISTIČ NIM KRETANJEM
        // ========================================
        
        // === 1. GRAVITACIONA AKCELERACIJA (prirodno ubrzanje) ===
        // Blok se ubrzava dok ide ka centru (prirodna gravitacija)
        // Usporava dok ide ka krajevima (protiv gravitacije)
        float angularAcceleration = -(GRAVITY / ROPE_LENGTH) * sin(swingAngle);
        swingSpeed += angularAcceleration * deltaTime;
        
        // === 2. AŽURIRAJ UGAO (bez prigušenja - nema gubitka energije) ===
        swingAngle += swingSpeed * deltaTime;
        
        // === 3. OGRANIČENJE SA BLAGIM USPORAVANJEM ===
        // Kada dosegne granicu, odbij se sa 95% energije (blago usporavanje)
        if (swingAngle > MAX_SWING_ANGLE) {
            swingAngle = MAX_SWING_ANGLE;     // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        } else if (swingAngle < -MAX_SWING_ANGLE) {
            swingAngle = -MAX_SWING_ANGLE;    // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        }
        
        // === 4. IZRAČUNAJ POZICIJU BLOKA ===
        float pivotX = 0.0f;
        float pivotY = HOOK_Y + cameraY;  // Pivot se pomera sa kamerom u world space
        
        currentBlock->x = pivotX + ROPE_LENGTH * sin(swingAngle);
        currentBlock->y = pivotY - ROPE_LENGTH * cos(swingAngle);
        
        // Rezultat:
        // - Blok se stalno ljulja između granica (ne zaustavlja se)
        // - Uspori se blago na krajevima (realistično)
        // - Ubrzava se ka centru zbog gravitacije (prirodno)
        // - Energia boost održava konstantnu amplitudu ljuljanja
        
    } else {
        // Padanje bloka
        currentBlock->y -= fallSpeed * deltaTime;
        
        // Provera da li je blok stigao do zemlje
        if (placedBlocks.empty()) {
            if (currentBlock->y - currentBlock->height / 2.0f <= GROUND_Y) {
                // Prvi blok uvek uspešno pada na zemlju
                currentBlock->y = GROUND_Y + currentBlock->height / 2.0f;
                placedBlocks.push_back(*currentBlock);
                score++;
                spawnNewBlock();
            }
        } else {
            // Provera kolizije sa vrhom zgrade
            Block& topBlock = placedBlocks.back();
            float targetY = topBlock.y + topBlock.height / 2.0f + currentBlock->height / 2.0f;
            
            if (currentBlock->y <= targetY) {
                currentBlock->y = targetY;
                
                // Provera preklapanja
                if (currentBlock->overlaps(topBlock)) {
                    float overhang = currentBlock->getTotalOverhang(topBlock);
                    float maxOverhang = currentBlock->width * OVERHANG_LIMIT;
                    
                    if (overhang > maxOverhang) {
                        // Previše viri - Game Over
                        state = GAME_OVER;
                    } else {
                        // Uspešno postavljanje
                        placedBlocks.push_back(*currentBlock);
                        score++;
                        
                        // Povecaj nijansanje zgrade proporcionalno grešci
                        if (overhang > 0.01f) {
                            float errorRatio = overhang / maxOverhang;
                            buildingSwayAmplitude += errorRatio * 0.05f;
                            buildingSwaySpeed = 2.0f + buildingSwayAmplitude * 3.0f;
                        }
                        
                        spawnNewBlock();
                    }
                } else {
                    // Potpuno promašaj - Game Over
                    state = GAME_OVER;
                }
            }
        }
    }
    
    // Njihanje zgrade
    if (buildingSwayAmplitude > 0.0f) {
        buildingSwayAngle += buildingSwaySpeed * deltaTime;
    }
}

void Game::dropBlock() {
    if (state == GAME_OVER || blockFalling) return;
    blockFalling = true;
}
void Game::drawBlock(const Block& block, float offsetX, float rotation) {
    glUseProgram(shaderProgram);
    
    // Postavi projection matricu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    // Model matrica - koristi se za skaliranje i translaciju
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
    
    // ✅ Proveri da li je block tekstura učitana
    if (blockTexture != 0) {
        // KORISTI TEKSTURU block2.png
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1);  // Uključi teksturu
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blockTexture);
        
        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);
        
        // BEZ BOJENJA - prikaži teksturu kakva jeste
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela - bez tinta
        
        // Koristi blockVAO sa ispravnim UV koordinatama
        glBindVertexArray(blockVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    } else {
        // Fallback na boju ako tekstura nije učitana
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
    
    // Postavi projection matricu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    // === 1. GEOMETRIJA - Računanje dužine i ugla ===
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);
    
    // === 2. UGAO ROTACIJE ===
    float angle = atan2(dx, -dy);
    
    // === 3. DIMENZIJE ===
    float ropeWidth = 0.03f;  // Debljina konopca
    
    // === 4. CENTAR UŽETA ===
    float centerX = (x1 + x2) / 2.0f;
    float centerY = (y1 + y2) / 2.0f;
    
    // === 5. TRIGONOMETRIJA ZA ROTACIJU ===
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    // === 6. MODEL MATRICA (4x4) ===
    float model[16] = {
        ropeWidth * cosA,  ropeWidth * sinA,  0.0f, 0.0f,
        -length * sinA,    length * cosA,     0.0f, 0.0f,
        0.0f,              0.0f,              1.0f, 0.0f,
        centerX,           centerY,           0.0f, 1.0f
    };
    
    // === 7. POSTAVI UNIFORME I RENDERUJ ===
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    
    // ✅ Proveri da li je tekstura učitana
    if (ropeTexture != 0) {
        // KORISTI TEKSTURU
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1);  // Uključi teksturu
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ropeTexture);
        
        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);
        
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela (bez tintovanja)
        
        // ✅ KORISTI ROPE VAO sa ispravnim UV koordinatama
        glBindVertexArray(ropeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    } else {
        // Fallback na braon boju ako tekstura nije učitana
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);
        
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.5f, 0.35f, 0.2f, 1.0f);  // Braon boja
        
        // Koristi obični VAO za boju
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

void Game::drawHook(float x, float y) {
    glUseProgram(shaderProgram);
    
    // Postavi projection matricu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    // Isključi teksturu - koristimo BOJU
    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    glUniform1i(useTexLoc, 0);
    
    // Kuka kao mali pravougaonik
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
    
    // Postavi projection matricu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    // Isklju?i teksturu - koristimo BOJU
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

void Game::render() {
    glUseProgram(shaderProgram);
    
    // Postavi projection matricu - VAŽNO: Ovo se postavlja jednom za celu scenu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    
    // ========================================
    // CRTAJ ZEMLJU SA TEKSTUROM - KAO NA VEžBAMA
    // ========================================
    float groundHeight = GROUND_Y - (-1.0f);
    float groundCenterY = (GROUND_Y + (-1.0f)) / 2.0f;
    
    float groundModel[16] = {
        100.0f, 0.0f, 0.0f, 0.0f,  // Širina 100.0 (od -50.0 do +50.0)
        0.0f, groundHeight, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, groundCenterY - cameraY, 0.0f, 1.0f
    };
    
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, groundModel);
    
    // ? AKTIVIRAJ TEKSTURU - KAO NA VEžBAMA
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, groundTexture);
    
    GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
    glUniform1i(texLoc, 0);  // Texture unit 0
    
    glUniform1i(useTexLoc, 1);  // Koristi TEKSTURU
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela (bez tinting-a)
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // ========================================
    // CRTAJ BLOKOVE SA BOJOM
    // ========================================
    glUniform1i(useTexLoc, 0);  // Isključi teksturu za blokove
    
    float swayOffset = 0.0f;
    if (buildingSwayAmplitude > 0.0f) {
        swayOffset = sin(buildingSwayAngle) * buildingSwayAmplitude;
    }
    
    for (const auto& block : placedBlocks) {
        drawBlock(block, swayOffset);
    }
    
    // Crtaj trenutni blok sa kukom i užetom
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
    
    // Crtaj score (jednostavan prikaz)
    if (state == GAME_OVER) {
        drawText("GAME OVER", -0.3f, 0.0f, 1.5f);
    }
    
    // Prikaži score kao niz blokova (svaki blok = 1 poen)
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
