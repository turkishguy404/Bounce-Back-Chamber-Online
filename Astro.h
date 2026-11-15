#pragma once
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
//
#define AstroWindow GLFWwindow
#define ASTRO_VERSION 0.4
#define ASTRO_PLAYER_OBJECT 1
#define ASTRO_REGULAR_OBJECT 2
#define ASTRO_OBJECT_AI 3
#define ASTRO_OBJECT_BACKROUND 4
#define astroUninitWindow(window) glfwDestroyWindow(window); glfwTerminate();
#define AstroProgramRunning(window) !glfwWindowShouldClose(window)
//
// Gravity
#define ASTRO_GRAVITY 1500.0f
//
int astroGravityinit(int windowWidth, int windowHeight, std::vector<int>& grid);
int astroInitWindow(int width, int height, const char* title, AstroWindow** window);
int astroGetKey(int key);
void astroSetupProjection(int windowWidth, int windowHeight);
int astroWait(int milliseconds);

class AstroObjects {
public:
	int type = 5;
	unsigned int width = 0;
	unsigned int height = 0;
	float x_location = 0;        
	float y_location = 0;
	int red = 0;
	int green = 0;
	int blue = 0;
	float angle = 0.0f;          
	bool isVisible = true;
	bool isCollidable = true;
	bool isAnchor = false;      

	// ===Fhysics
	bool usePhysics = false;     
	float vx = 0.0f;             
	float vy = 0.0f;             
	bool onGround = false;       

	std::vector<unsigned char> pixels;
	unsigned int texId = 0;

	int astroStartObject(int type, unsigned int width, unsigned int height, int x_location, int y_location, int red, int green, int blue);
	int astroSetTexture(const std::string& filename, std::vector<unsigned char>& texturePixels);
	int astroUpdateTexture(const std::vector<unsigned char>& texturePixels);
	void astroUploadTexture();
	int astroUpdateObject(int windowWidth, int windowHeight);

	bool collides(const AstroObjects& other) const;
};
void astroPhysicsStep(std::vector<AstroObjects*>& objects, float dt);