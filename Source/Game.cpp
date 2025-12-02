#include "../Header/Game.h"
#include "../Header/Util.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream> 
#include <fstream>
#include <algorithm>// ? Za std::ostringstream (konverzija int -> string)

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
    initTextRenderer();  // ? Inicijalizuj text renderer
    spawnNewBlock();
}

Game::~Game() {
    if (currentBlock) delete currentBlock;
    if (textRenderer) delete textRenderer;  // ? Oslobodi text renderer
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &ropeVAO);
    glDeleteBuffers(1, &ropeVBO);
    glDeleteVertexArrays(1, &blockVAO);
    glDeleteBuffers(1, &blockVBO);
    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteBuffers(1, &backgroundVBO);
    glDeleteProgram(shaderProgram);
    if (textShaderProgram) glDeleteProgram(textShaderProgram);  // ? Oslobodi text shader
}

GameState Game::getGameState() const {
    return state;
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
// preprocessTexture - Priprema teksturu za korišcenje (kao na vežbama)
// ================================================================
void Game::preprocessTexture(unsigned int& texture, const char* filepath) {
    texture = loadImageToTexture(filepath);  // Funkcija iz Util.h
    
    if (texture == 0) {  // Provera greške
        std::cout << "GREŠKA: Tekstura nije ucitana: " << filepath << std::endl;
        return;  // Izadi samo ako je greška
    }
    
    std::cout << "Tekstura ucitana: " << filepath << ", ID: " << texture << std::endl;
    
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Generisanje mipmapa
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // ? RAZLICITE WRAP STRATEGIJE ZA RAZLICITE TEKSTURE
    std::string path(filepath);
    if (path.find("rope") != std::string::npos) {
        // KONOPAC - clamp horizontalno, repeat VERTIKALNO
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else if (path.find("block") != std::string::npos) {
        // BLOKOVI - clamp obe ose (1x tekstura po bloku)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else if (path.find("background") != std::string::npos) {
        // POZADINA - clamp obe ose (1x po ekranu)
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
    // ? GLAVNI VAO - za blokove i zemlju
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
    
    // ? ROPE VAO - za konopac sa vertikalnim ponavljanjem teksture
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
    
    // ? BLOCK VAO - za blokove sa 1x1 teksturom
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
    
    // ? BACKGROUND VAO - puna pokrivka ekrana (fiksna pozadina)
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
    
    // Ucitaj teksture
    preprocessTexture(backgroundTexture, "Textures/background4.jpg");
    preprocessTexture(groundTexture, "Textures/pixel-ground.png");
    preprocessTexture(ropeTexture, "Textures/rope.png");
    preprocessTexture(blockTexture, "Textures/block2.png");
}

// ================================================================
// initTextRenderer - Inicijalizacija TextRenderer-a
// ================================================================
void Game::initTextRenderer() {
    std::cout << "\n=== INICIJALIZACIJA TEXT RENDERER-A ===" << std::endl;
    
    // Kreiraj text shader
    textShaderProgram = createShader("Shaders/text.vert", "Shaders/text.frag");
    
    // Kreiraj TextRenderer
    textRenderer = new TextRenderer(textShaderProgram, windowWidth, windowHeight);
    
    // Ucitaj font
    if (!textRenderer->loadFont("C:/Windows/Fonts/arial.ttf", 48)) {
        std::cout << "ERROR: Failed to load font!" << std::endl;
    }
    
    std::cout << "=== TEXT RENDERER INICIJALIZOVAN ===" << std::endl;
}

void Game::setWindowSize(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    
    // Kreiraj novi TextRenderer sa novim dimenzijama
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
    
    // ========================================
    // POCETNA POZICIJA - Blok visi direktno ispod kuke
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
    swingSpeed = 2.0f;    // Pocetna brzina - daj bloku malo energije da krene
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
    // ? DEBUG: Proveri status na po?etku update-a
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
    
    // Ažuriraj kameru
    updateCamera(deltaTime);

    /*if (state == GAME_OVER) {
        double now = glfwGetTime();
        if (now - lastCursorBlink > 0.5) {
            cursorVisible = !cursorVisible;
            lastCursorBlink = now;
        }
    }*/
    
    if (!blockFalling) {
        // ========================================
        // KONTINUALNO LJULJANJE SA REALISTIC NIM KRETANJEM
        // ========================================
        
        // === 1. GRAVITACIONA AKCELERACIJA (prirodno ubrzanje) ===
        // Blok se ubrzava dok ide ka centru (prirodna gravitacija)
        // Usporava dok ide ka krajevima (protiv gravitacije)
        float angularAcceleration = -(GRAVITY / ROPE_LENGTH) * sin(swingAngle);
        swingSpeed += angularAcceleration * deltaTime;
        
        // === 2. AŽURIRAJ UGAO (bez prigušenja - nema gubitka energije) ===
        swingAngle += swingSpeed * deltaTime;
        
        // === 3. OGRANICENIE SA BLAGIM USPORAVANJEM ===
        // Kada dosegne granicu, odbij se sa 95% energije (blago usporavanje)
        if (swingAngle > MAX_SWING_ANGLE) {
            swingAngle = MAX_SWING_ANGLE;     // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        } else if (swingAngle < -MAX_SWING_ANGLE) {
            swingAngle = -MAX_SWING_ANGLE;    // Postavi na granicu
            swingSpeed = -swingSpeed;         // Okreni smer (ide nazad)
        }
        
        // === 4. IZRACUNAJ POZICIJU BLOKA ===
        float pivotX = 0.0f;
        float pivotY = HOOK_Y + cameraY;  // Pivot se pomera sa kamerom u world space
        
        currentBlock->x = pivotX + ROPE_LENGTH * sin(swingAngle);
        currentBlock->y = pivotY - ROPE_LENGTH * cos(swingAngle);
        
        // Rezultat:
        // - Blok se stalno ljulja izmedu granica (ne zaustavlja se)
        // - Uspori se blago na krajevima (realisticno)
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
                        std::cout << "\n========================================" << std::endl;
                        std::cout << "?? GAME OVER - Blok previše viri!" << std::endl;
                        std::cout << "   Overhang: " << overhang << " (Max: " << maxOverhang << ")" << std::endl;
                        std::cout << "   Score: " << score << std::endl;
                        std::cout << "========================================\n" << std::endl;
                        state = GAME_OVER;
                    } else {
                        // Uspešno postavljanje
                        placedBlocks.push_back(*currentBlock);
                        score++;
                        std::cout << "? Blok uspešno postavljen! Score: " << score << std::endl;
                        
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
    
    // Njihanje zgrade
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

// ================================================================
// restart - Restartuje igru na po?etno stanje
// ================================================================
void Game::restart() {
    std::cout << "\n?? RESTARTOVANJE IGRE..." << std::endl;
    
    // Resetuj stanje igre
    state = PLAYING;
    score = 0;
    
    // O?isti sve postavljene blokove
    placedBlocks.clear();
    
    // Resetuj kameru
    cameraY = 0.0f;
    targetCameraY = 0.0f;
    
    // Resetuj building sway parametre
    buildingSwayAngle = 0.0f;
    buildingSwaySpeed = 0.0f;
    buildingSwayAmplitude = 0.0f;
    
    // Obriši trenutni blok i spawn novi
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
    
    // ? Proveri da li je block tekstura ucitana
    if (blockTexture != 0) {
        // KORISTI TEKSTURU block2.png
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1);  // Ukljuci teksturu
        
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
        // Fallback na boju ako tekstura nije ucitana
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
    
    // === 1. GEOMETRIJA - Racunanje dužine i ugla ===
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
    
    // ? Proveri da li je tekstura ucitana
    if (ropeTexture != 0) {
        // KORISTI TEKSTURU
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 1);  // Ukljuci teksturu
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ropeTexture);
        
        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);
        
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela (bez tintovanja)
        
        // ? KORISTI ROPE VAO sa ispravnim UV koordinatama
        glBindVertexArray(ropeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    } else {
        // Fallback na braon boju ako tekstura nije ucitana
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);
        
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.5f, 0.35f, 0.2f, 1.0f);  // Braon boja
        
        // Koristi obicni VAO za boju
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
    
    // Iskljuci teksturu - koristimo BOJU
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

void Game::onKeyPressed(int key) {

    if (state == GAME_OVER) {

        // Ako prvi put kucamo, aktiviraj unos
        if (!enteringName)
            enteringName = true;

        // BACKSPACE
        if (key == GLFW_KEY_BACKSPACE) {
            if (!playerName.empty()) {
                playerName.pop_back();
            }
            return;
        }

        // ENTER potvrđuje ime
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            enteringName = false;

            // OVDJE SNIMI SCORE
            // saveScore(playerName);
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
                            scores.push_back({name, scoreValue});
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
    
    // Postavi projection matricu - VAŽNO: Ovo se postavlja jednom za celu scenu
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
    
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    
    // ========================================
    // ? CRTAJ POZADINU PRVO (iza svega)
    // ========================================
    if (backgroundTexture != 0) {
        // ? Model matrica za pozadinu - VEOMA MALA VREDNOST
        // Za 16:9 ekran (aspectRatio = 1.778), koristi 0.1x
        float bgScaleX = aspectRatio * 0.1f;  // ? Horizontalno - 0.1x
        float bgScaleY = 0.1f;                 // ? Vertikalno - 0.1x
        
        float backgroundModel[16] = {
            bgScaleX, 0.0f, 0.0f, 0.0f,  // ? Skaliranje horizontalno
            0.0f, bgScaleY, 0.0f, 0.0f,  // ? Skaliranje vertikalno
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f       // Bez translacije - fiksna pozadina
        };
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, backgroundModel);
        
        // Aktiviraj pozadinsku teksturu
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        
        GLuint texLoc = glGetUniformLocation(shaderProgram, "uTexture");
        glUniform1i(texLoc, 0);
        
        glUniform1i(useTexLoc, 1);  // Koristi teksturu
        glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // Bela (bez tinta)
        
        glBindVertexArray(backgroundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    
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
    
    // ? AKTIVIRAJ TEKSTURU - KAO NA VEžBAMAN
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
    glUniform1i(useTexLoc, 0);  // Iskljuci teksturu za blokove
    
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
    
    // ========================================
    // ? UI ELEMENTI U GORNJEM DESNOM UGLU
    // ========================================
    if (textRenderer) {
        // ? KONTROLE - desni gornji ugao
        std::string controlsText = "ESC - Exit  |  R - Restart  |  ENTER - Drop Block";
        float controlsWidth = textRenderer->getTextWidth(controlsText, 0.5f);
        float controlsX = windowWidth - controlsWidth - 20.0f;  // 20px margina sa desne strane
        textRenderer->renderText(controlsText, controlsX, 50.0f, 0.5f, 0.9f, 0.9f, 0.9f);  // 50px od vrha
        
        // ? SCORE - ispod kontrola, poravnat desno
        std::ostringstream scoreStream;
        scoreStream << "Score: " << score;
        std::string scoreText = scoreStream.str();
        float scoreWidth = textRenderer->getTextWidth(scoreText, 1.0f);
        float scoreX = windowWidth - scoreWidth - 20.0f;  // 20px margina sa desne strane
        textRenderer->renderText(scoreText, scoreX, 100.0f, 1.0f, 1.0f, 1.0f, 1.0f);  // 100px od vrha
    }

    // ========================================
    // ? INFO PANEL I TEKST UNUTAR NJEGA
    // ========================================
    if (textRenderer) {
        // ? PRVO CRTAJ PLAVI BLOK
        // Dimenzije pravougaonika (screen space pixels)
        float panelWidth = 800.0f;
        float panelHeight = 300.0f;
        float panelX = 0.0f;  // Po?inje od leve ivice (panelX = 0)
        float panelY = 0.0f;  // Gornja ivica ekrana
        
        // ? CRTAJ TAMNO PLAVI PRAVOUGAONIK SA 80% OPACITY
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glUseProgram(shaderProgram);
        
        // Postavi projection za screen space
        float screenProjection[16] = {
            2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / windowHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        
        GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);
        
        // Model matrica za pravougaonik
        float rectModel[16] = {
            panelWidth, 0.0f, 0.0f, 0.0f,
            0.0f, panelHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            panelX, panelY, 0.0f, 1.0f
        };
        
        GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rectModel);
        
        // Isklju?i teksturu
        GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
        glUniform1i(useTexLoc, 0);
        
        // Tamno plava boja sa 80% opacity (0.0, 0.2, 0.5, 0.8)
        GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform4f(colorLoc, 0.0f, 0.2f, 0.5f, 0.8f);  // Tamno plava, 80% opacity
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        glDisable(GL_BLEND);
        
        // Resetuj projection matricu za game world
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
        
        // ? SADA CRTAJ TEKST UNUTAR PLAVOG BLOKA
        std::string line1 = "RA 145/2022";
        std::string line2 = "Lazar Nestorovic";
        
        // ? SMANJEN FONT - 0.5f umesto 0.6f
        float fontSize = 0.5f;
        
        // Izra?unaj širinu teksta
        float text1Width = textRenderer->getTextWidth(line1, fontSize);
        float text2Width = textRenderer->getTextWidth(line2, fontSize);
        
        // ? POMERI 200px ULEVO OD CENTRA
        float textX1 = (panelWidth - text1Width) / 2.0f - 200.0f;  // Centriraj pa pomeri 200px ulevo
        float textX2 = (panelWidth - text2Width) / 2.0f - 200.0f;  // Centriraj pa pomeri 200px ulevo
        
        // ? MALO GORE - pomeri tekst više ka vrhu (80px i 110px umesto 120px i 160px)
        float textY1 = 80.0f;   // Prvi red - 80px od vrha (više ka vrhu)
        float textY2 = 110.0f;  // Drugi red - 110px od vrha (30px ispod prvog)
        
        textRenderer->renderText(line1, textX1, textY1, fontSize, 1.0f, 1.0f, 1.0f);  // Beli tekst
        textRenderer->renderText(line2, textX2, textY2, fontSize, 1.0f, 1.0f, 1.0f);  // Beli tekst
    }

    if (textRenderer) {
        // ============================================
        // 1. CRTAJ SIVU POZADINU U DONJEM LEVOM UGLU
        // ============================================
        float panelWidth = 500.0f;   // širina panela
        float panelHeight = 300.0f;   // visina panela
        float panelX = 0.0f;     // levi ugao
        float panelY = 0.0f;     // donji ugao

        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(shaderProgram);

        // Projection matrica za screen space
        float screenProjection[16] = {
            2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / windowHeight, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f
        };
        GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);

        // Model matrica za pravougaonik
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
        glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 0.8f);  // siva sa 80% opacity

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDisable(GL_BLEND);

        // Reset projection za game world
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

        // ============================================
        // 2. CRTAJ TOP 3 SCORE-A
        // ============================================
        /*std::vector<std::pair<std::string, int>> topScores = {
            {"Player3", 700},
            {"Player2", 850},
            {"Player1", 1000}
        };*/

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

        // Sortiraj po visini score-a (najveći prvi)
        std::sort(topScores.begin(), topScores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Uzmi samo top 3
        if (topScores.size() > 3) {
            topScores.resize(3);
        }

        std::pair<std::string, int> temp = topScores[0];
		topScores[0] = topScores[2];
		topScores[2] = temp;

        float fontSize = 0.7f;
        float startX = 20.0f;   // mali offset od leve ivice panela
        float startY = 20.0f;   // mali offset od donje ivice panela
        float lineSpacing = 40.0f;   // razmak između redova

        for (int i = 0; i < topScores.size(); ++i) {
            std::string text = topScores[i].first + " " + std::to_string(topScores[i].second);
            float y = startY + i * lineSpacing;

            // Boja teksta: zlatna, srebrna, bronzana
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


    // Crtaj score (jednostavan prikaz)
    if (state == GAME_OVER) {
        //std::cout << "?? render(): state == GAME_OVER detektovan!" << std::endl;
        
        // ? GAME OVER sa TextRenderer
        if (textRenderer) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(shaderProgram);

            // Screen space projection
            float screenProjection[16] = {
                2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f / windowHeight, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                -1.0f, 1.0f, 0.0f, 1.0f
            };

            GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, screenProjection);

            // Dimenzije pravougaonika (prilagodi po potrebi)
            float panelWidth = 800.0f;
            float panelHeight = 600.0f;
            float panelX = windowWidth / 2.0f;  // Centriraj horizontalno
            float panelY = windowHeight / 2.0f;  // Centriraj vertikalno

            // Model matrica za pravougaonik
            float rectModel[16] = {
                panelWidth, 0.0f, 0.0f, 0.0f,
                0.0f, panelHeight, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                panelX, panelY, 0.0f, 1.0f
            };

            GLuint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, rectModel);

            // Isključi teksturu
            GLuint useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
            glUniform1i(useTexLoc, 0);

            // Siva boja sa 85% opacity (0.2, 0.2, 0.2, 0.85)
            GLuint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
            glUniform4f(colorLoc, 0.2f, 0.2f, 0.2f, 0.85f);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glDisable(GL_BLEND);

            // Resetuj projection matricu za tekst
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);
            //std::cout << "   textRenderer postoji - renderam tekst..." << std::endl;

            // Unesi svoje ime

            std::string gameOverText = "GAME OVER";
            float textWidth = textRenderer->getTextWidth(gameOverText, 2.0f);
            float centerX = (windowWidth - textWidth) / 2.0f;
            float centerY = windowHeight / 2.0f;

            textRenderer->renderText(gameOverText, centerX, centerY, 2.0f, 1.0f, 0.0f, 0.0f);

            // Enter your name tekst
            std::string nameText = "Enter your name to save your score (optional)";
            float nameTextWidth = textRenderer->getTextWidth(nameText, 0.6f);
            float nameX = (windowWidth - nameTextWidth) / 2.0f;
            float nameY = centerY - 200.0f;

            textRenderer->renderText(nameText, nameX, nameY, 0.6f, 1.0f, 0.0f, 0.0f);  // Crveni tekst

            // Prikaz unetog imena
            if (state == GAME_OVER && enteringName){

                std::string display = playerName;
                float w = textRenderer->getTextWidth(display, 1.2f);
                float x = (windowWidth - w) / 2.0f;
                float y = nameY + 70.0f;

                textRenderer->renderText(display, x, y, 1.2f, 1.0f, 1.0f, 1.0f);

                if (cursorVisible) {
                    float width = textRenderer->getTextWidth(playerName, 1.2f);  // funkcija koja računa širinu teksta
                    textRenderer->renderText("|", x + width, y, 1.2f, 1.0f, 1.0f, 1.0f);
                }
            }

            // ? SCORE
            std::ostringstream scoreSt;
            scoreSt << "Your score: " << score;
            std::string scoreText = scoreSt.str();
            float scoreWidth = textRenderer->getTextWidth(scoreText, 1.0f);
            float scoreX = (windowWidth - scoreWidth) / 2.0f;
            float scoreY = centerY + 80.0f;  // 80px ispod GAME OVER teksta

            textRenderer->renderText(scoreText, scoreX, scoreY, 1.0f, 1.0f, 1.0f, 1.0f);
            
            // ? RESTART INSTRUKCIJE
            std::string restartText = "Press CTRL + R to Restart";
            float restartWidth = textRenderer->getTextWidth(restartText, 1.0f);
            float restartX = (windowWidth - restartWidth) / 2.0f;
            float restartY = scoreY + 80.0f;  // 80px ispod SCORE teksta
            
            textRenderer->renderText(restartText, restartX, restartY, 1.0f, 1.0f, 1.0f, 1.0f);  // Beli tekst
            
            //std::cout << "   Tekst renderovan!" << std::endl;
        } else {
            std::cout << "   ? ERROR: textRenderer je nullptr!" << std::endl;
        }
    }
    
    // Prikaži score kao niz blokova (svaki blok = 1 poen) - OPCIONALNO, može se ukloniti
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
