#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "../Header/Util.h"
#include "../Header/Game.h"

// CityBloxx 2D igra

Game* game = nullptr;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && action == GLFW_PRESS) {
        if (game) {
            game->dropBlock();
        }
    }
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, mode->width, mode->height);

    glfwSetKeyCallback(window, keyCallback);

    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);  // Svetlo plava pozadina (nebo)
    
    // Inicijalizuj igru
    game = new Game();
    
    // ========================================
    // NORMALIZUJ DIMENZIJE ZA DAT EKRAN
    // ========================================
    // Ovo postavlja projection matricu koja ispravlja aspect ratio
    // tako da objekti izgledaju isto na svim rezolucijama
    game->setAspectRatio((float)mode->width, (float)mode->height);
    
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        
        // Ograniči deltaTime da ne bude prevelik (u slučaju pauze ili sporog frame-a)
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Update i render igre
        game->update(deltaTime);
        game->render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    delete game;

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}