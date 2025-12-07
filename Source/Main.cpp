#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>
#include <chrono>

#include "../Header/Util.h"
#include "../Header/Game.h"

// CityBloxx 2D igra

Game* game = nullptr;

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    if (game) {
        game->onCharEntered(codepoint);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && action == GLFW_PRESS) {
        if (game) {
            game->dropBlock();
        }
    }
    
    // ✅ RESTART NA TASTER R + CTRL
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (game) {
            game->restart();
        }
    }
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (game && game->getGameState() == GAME_OVER) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            game->onKeyPressed(key);
        }
        return;
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (game) {
            game->dropBlock();
        }
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Dobij rezoluciju primarnog monitora
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    
    // Kreiraj prozor u fullscreen režimu
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "CityBloxx 2D", primaryMonitor, NULL);
    if (window == NULL) return endProgram("Prozor nije uspeo da se kreira.");
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) return endProgram("GLEW nije uspeo da se inicijalizuje.");

    GLFWcursor* myCursor = loadImageToCursor("Resources/cursor_mid.png");
    if (myCursor) {
		std::cout << "? Kursor ucitan uspešno." << std::endl;
        glfwSetCursor(window, myCursor);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, mode->width, mode->height);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);  // Svetlo plava pozadina (nebo)
    
    // Inicijalizuj igru
    game = new Game();
    
    // ========================================
    // NORMALIZUJ DIMENZIJE ZA DAT EKRAN
    // ========================================
    // Ovo postavlja projection matricu koja ispravlja aspect ratio
    // tako da objekti izgledaju isto na svim rezolucijama
    game->setAspectRatio((float)mode->width, (float)mode->height);
    game->setWindowSize(mode->width, mode->height);  // ✅ Postavi dimenzije za TextRenderer
    
    const double TARGET_FPS = 75.0;
    const double FRAME_TIME = 1.0 / TARGET_FPS;
    double lastTime = glfwGetTime();
    double frameStartTime = lastTime;

    while (!glfwWindowShouldClose(window))
    {
        frameStartTime = glfwGetTime();

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        
        // Ograniči deltaTime da ne bude prevelik (u slučaju pauze ili sporog frame-a)
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 1e-4f) deltaTime = 1e-4f;
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Update i render igre
        game->update(deltaTime);
        game->render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        double frameEndTime = glfwGetTime();
        double frameDuration = frameEndTime - frameStartTime;

        if (frameDuration < FRAME_TIME) {
            double sleepTime = FRAME_TIME - frameDuration;
            std::this_thread::sleep_for(
                std::chrono::duration<double>(sleepTime)
            );
        }
    }
    
    delete game;

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}