#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
#include "Astro.h"
//
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
//
static const float ASTRO_PI = 3.14159265358979f;

int astroGravityinit(int windowWidth, int windowHeight, std::vector<int>& grid) {
	grid.resize(windowWidth * windowHeight, 0);
	return 0;
}
int astroWait(int milliseconds) {
	glfwWaitEventsTimeout(milliseconds / 1000.0);
	return 0;
}
int astroInitWindow(int width, int height, const char* title, AstroWindow** window) {
	if (!glfwInit())
		return -1;
	*window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!(*window))
	{
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(*window);
	return 0;
}

int astroGetKey(int key) {
	int get = glfwGetKey(glfwGetCurrentContext(), key);
	return get == GLFW_PRESS ? 1 : 0;
}

void astroSetupProjection(int windowWidth, int windowHeight) {
	glViewport(0, 0, windowWidth, windowHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowWidth, windowHeight, 0, -1, 1); 
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

// ===Objects
int AstroObjects::astroStartObject(int ntype, unsigned int nwidth, unsigned int nheight, int nx_location, int ny_location, int nred, int ngreen, int nblue) {
	type = ntype;
	width = nwidth;
	height = nheight;
	x_location = (float)nx_location;
	y_location = (float)ny_location;
	red = nred;
	green = ngreen;
	blue = nblue;
	isCollidable = (ntype != ASTRO_OBJECT_BACKROUND);

	pixels.resize(width * height * 3);
	for (size_t i = 0; i + 2 < pixels.size(); i += 3)
	{
		pixels[i + 0] = (unsigned char)red;
		pixels[i + 1] = (unsigned char)green;
		pixels[i + 2] = (unsigned char)blue;
	}
	astroUploadTexture();
	return 0;
}

void AstroObjects::astroUploadTexture() {
	if (!texId) glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, 0,
		GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

int AstroObjects::astroSetTexture(const std::string& filename, std::vector<unsigned char>& texturePixels) {
	std::ifstream file(filename);
	if (!file) {
		std::cerr << "Error: Could not open file " << filename << std::endl;
		return -1;
	}
	texturePixels.resize(width * height * 3);
	pixels.resize(width * height * 3);

	std::string line;
	size_t i = 0;
	while (std::getline(file, line) && i + 2 < pixels.size()) {
		std::istringstream iss(line);
		int nred, ngreen, nblue;
		if (!(iss >> nred >> ngreen >> nblue)) continue;
		pixels[i + 0] = (unsigned char)nred;
		pixels[i + 1] = (unsigned char)ngreen;
		pixels[i + 2] = (unsigned char)nblue;
		texturePixels[i + 0] = pixels[i + 0];
		texturePixels[i + 1] = pixels[i + 1];
		texturePixels[i + 2] = pixels[i + 2];
		i += 3;
	}
	astroUploadTexture();
	return 0;
}

int AstroObjects::astroUpdateTexture(const std::vector<unsigned char>& texturePixels) {
	if (texturePixels.size() != (size_t)width * height * 3) {
		std::cerr << "Error: texture size mismatch" << std::endl;
		return -1;
	}
	pixels = texturePixels;
	astroUploadTexture();
	return 0;
}

int AstroObjects::astroUpdateObject(int /*windowWidth*/, int /*windowHeight*/) {
	if (!isVisible) return 0;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glColor3f(1.0f, 1.0f, 1.0f);

	float hw = width / 2.0f;
	float hh = height / 2.0f;

	glPushMatrix();
	glTranslatef(x_location + hw, y_location + hh, 0.0f);
	glRotatef(angle, 0.0f, 0.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(-hw, -hh);
	glTexCoord2f(1, 0); glVertex2f(hw, -hh);
	glTexCoord2f(1, 1); glVertex2f(hw, hh);
	glTexCoord2f(0, 1); glVertex2f(-hw, hh);
	glEnd();
	glPopMatrix();

	glDisable(GL_TEXTURE_2D);
	return 0;
}
// ===Collision detection
namespace {
	struct Vec2 { float x, y; };

	void getCorners(const AstroObjects& o, Vec2 out[4]) {
		float cx = o.x_location + o.width / 2.0f;
		float cy = o.y_location + o.height / 2.0f;
		float rad = o.angle * ASTRO_PI / 180.0f;
		float c = std::cos(rad), s = std::sin(rad);
		float hw = o.width / 2.0f, hh = o.height / 2.0f;
		const float lx[4] = { -hw,  hw, hw, -hw };
		const float ly[4] = { -hh, -hh, hh,  hh };
		for (int i = 0; i < 4; i++) {
			out[i].x = cx + lx[i] * c - ly[i] * s;
			out[i].y = cy + lx[i] * s + ly[i] * c;
		}
	}

	bool overlapOnAxis(const Vec2 a[4], const Vec2 b[4], Vec2 axis) {
		float minA = 1e30f, maxA = -1e30f, minB = 1e30f, maxB = -1e30f;
		for (int i = 0; i < 4; i++) {
			float pa = a[i].x * axis.x + a[i].y * axis.y;
			float pb = b[i].x * axis.x + b[i].y * axis.y;
			minA = std::min(minA, pa); maxA = std::max(maxA, pa);
			minB = std::min(minB, pb); maxB = std::max(maxB, pb);
		}
		return !(maxA < minB || maxB < minA);
	}
}

bool AstroObjects::collides(const AstroObjects& o) const {
	if (!isCollidable || !o.isCollidable) return false;

	if (angle == 0.0f && o.angle == 0.0f) {
		return x_location < o.x_location + (float)o.width &&
			x_location + (float)width > o.x_location &&
			y_location < o.y_location + (float)o.height &&
			y_location + (float)height > o.y_location;
	}

	Vec2 a[4], b[4];
	getCorners(*this, a);
	getCorners(o, b);
	for (int shape = 0; shape < 2; shape++) {
		const Vec2* p = (shape == 0) ? a : b;
		for (int i = 0; i < 2; i++) {
			Vec2 e = { p[i + 1].x - p[i].x, p[i + 1].y - p[i].y };
			Vec2 axis = { -e.y, e.x };
			if (!overlapOnAxis(a, b, axis)) return false;
		}
	}
	return true;
}
// ===Fhysics
namespace {
	bool pushX(std::vector<AstroObjects*>& objects, AstroObjects* p, float dx, int depth) {
		if (depth > 8) return false;
		float oldX = p->x_location;
		p->x_location += dx;
		for (AstroObjects* b : objects) {
			if (b == p || !p->collides(*b)) continue;
			bool bothFlat = (p->angle == 0.0f && b->angle == 0.0f);
			bool movable = b->usePhysics && !b->isAnchor;
			if (bothFlat && movable) {
				float pen = (dx > 0.0f)
					? (p->x_location + (float)p->width) - b->x_location
					: (b->x_location + (float)b->width) - p->x_location;
				if (pen > 0.0f && pushX(objects, b, dx > 0.0f ? pen : -pen, depth + 1))
					continue;
			}
			p->x_location = oldX;
			return false;
		}
		return true;
	}
}

void astroPhysicsStep(std::vector<AstroObjects*>& objects, float dt) {
	for (AstroObjects* a : objects) {
		if (!a->usePhysics || a->isAnchor) continue;

		a->vy += ASTRO_GRAVITY * dt;
		a->onGround = false;

		float oldX = a->x_location;
		a->x_location += a->vx * dt;
		for (AstroObjects* b : objects) {
			if (b == a || !a->collides(*b)) continue;
			bool bothFlat = (a->angle == 0.0f && b->angle == 0.0f);

			if (bothFlat && b->usePhysics && !b->isAnchor && a->vx != 0.0f) {
				float pen = (a->vx > 0.0f)
					? (a->x_location + (float)a->width) - b->x_location
					: (b->x_location + (float)b->width) - a->x_location;
				if (pen > 0.0f && pushX(objects, b, a->vx > 0.0f ? pen : -pen, 1))
					continue;
			}

			if (bothFlat && a->vx > 0.0f)
				a->x_location = b->x_location - (float)a->width;     
			else if (bothFlat&& a->vx < 0.0f)
				a->x_location = b->x_location + (float)b->width;     
			else
				a->x_location = oldX;                                
			a->vx = 0.0f;
		}

		float oldY = a->y_location;
		a->y_location += a->vy * dt;
		for (AstroObjects* b : objects) {
			if (b == a || !a->collides(*b)) continue;
			bool bothFlat = (a->angle == 0.0f && b->angle == 0.0f);
			bool falling = a->vy > 0.0f;
			if (bothFlat && falling)
				a->y_location = b->y_location - (float)a->height;    
			else if (bothFlat && a->vy < 0.0f)
				a->y_location = b->y_location + (float)b->height;    
			else
				a->y_location = oldY;                                
			if (falling) a->onGround = true;
			a->vy = 0.0f;
		}
	}
}