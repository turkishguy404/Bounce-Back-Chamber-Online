#include "Net.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "Astro.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
//
// ===Sound system
ma_engine engine;
enum SfxId { SFX_JUMP, SFX_WIN, SFX_SPAWN, SFX_CLONE, SFX_PORTAL, SFX_DEATH, SFX_ROPE, SFX_POP, SFX_ALERT, SFX_COUNT };
const char* SFX_FILES[SFX_COUNT] = {
	"sound.mp3", "win.mp3", "spawn.mp3", "clone.mp3",
	"portal.mp3", "death.mp3", "rope.mp3", "pop.mp3",
	"alert.mp3"
};
ma_sound sfx[SFX_COUNT];
bool sfxLoaded[SFX_COUNT] = {};
ma_sound music;
bool musicLoaded = false;

void playSfx(int id) {
	if (!sfxLoaded[id]) return;
	ma_sound_seek_to_pcm_frame(&sfx[id], 0);
	ma_sound_start(&sfx[id]);
}
//
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
//
const struct WindowSettings {
	int windowWidth = 800;
	int windowHeight = 600;
} w;

const float MOVE_SPEED = 250.0f;
const float JUMP_SPEED = 600.0f;
const float ROTATE_SPEED = 180.0f;
const float SWING_PUSH = 500.0f;
const int   MAX_CLONES = NET_MAX_CLONES;

bool overlapsTrigger(const AstroObjects& p, AstroObjects& t) {
	bool old = t.isCollidable;
	t.isCollidable = true;
	bool hit = p.collides(t);
	t.isCollidable = old;
	return hit;
}

void updateRopeObject(AstroObjects& rope, float ax, float ay, float px, float py) {
	float dx = px - ax, dy = py - ay;
	float len = std::sqrt(dx * dx + dy * dy);
	rope.astroStartObject(ASTRO_REGULAR_OBJECT, (unsigned)(len < 2.0f ? 2.0f : len), 4, 0, 0, 139, 90, 43);
	rope.isCollidable = false;
	rope.angle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
	rope.x_location = (ax + px) / 2.0f - rope.width / 2.0f;
	rope.y_location = (ay + py) / 2.0f - rope.height / 2.0f;
	rope.isVisible = true;
}

// ===Tileset style textures
struct TileStyle { unsigned char fill[3], light[3], line[3]; };
const TileStyle TS_GREEN = { {60,160,100},  {100,205,100}, {30,60,90} };
const TileStyle TS_NAVY = { {45,50,120},   {75,85,165},   {25,28,70} };
const TileStyle TS_ORANGE = { {255,128,0},   {255,185,80},  {110,50,0} };
const TileStyle TS_YELLOW = { {255,200,60},  {255,235,130}, {120,90,10} };
const TileStyle TS_CYAN = { {0,200,200},   {120,240,240}, {0,80,90} };
const TileStyle TS_BLUE = { {60,120,230},  {140,175,255}, {20,40,110} };

void applyTile(AstroObjects& o, const TileStyle& t, int ts = 32) {
	for (unsigned y = 0; y < o.height; y++) {
		for (unsigned x = 0; x < o.width; x++) {
			int tx = x % ts, ty = y % ts;
			const unsigned char* c = t.fill;
			if (tx == 0 || ty == 0 || tx == ts - 1 || ty == ts - 1 ||
				x == o.width - 1 || y == o.height - 1) c = t.line;
			else if (tx == 1 || ty == 1) c = t.light;
			size_t i = ((size_t)y * o.width + x) * 3;
			o.pixels[i] = c[0]; o.pixels[i + 1] = c[1]; o.pixels[i + 2] = c[2];
		}
	}
	o.astroUploadTexture();
}

// ===Drone character sprite
const int DRONE_ART = 25;
const char* DRONE_HEAD[21] = {
	"..........A..............",
	"..........o..............",
	"..........o..............",
	"......oooooooooooo.......",
	".....oYHHYYYYYYYYYo......",
	"....oYHYYYYYYYYYYYYooo...",
	"....oYYYYYYYYYYYYYYoLLo..",
	"....oyyyyyyyyyyyyyyoLLo..",
	"...oooooooooooooooooooo..",
	"....oggggggggggggggo.....",
	"....ogggggggggeeeego.....",
	"....oggggggggeEEWEeo.....",
	"....oggggggggeEEEEeo.....",
	"....ogggggggggeeeego.....",
	"....oooooooooooooooo.....",
	".....oGGGGGGGGGGGGo......",
	"...ooGYYyyYYyyYYyyGoo....",
	"..oGoGyyYYyyYYyyYYGoGo...",
	"..oGoGYYYYYYYYYYYYGoGo...",
	"..oooGYYYYYYYYYYYYGooo...",
	".....oGGGGGGGGGGGGo......",
};
enum DroneFrame { DF_IDLE, DF_WALK, DF_JUMP, DF_COUNT };
const char* DRONE_LEGS[DF_COUNT][4] = {
	{
		"......ogggo..ogggo.......",
		"......ogggo..ogggo.......",
		"......oGGGoo.oGGGoo......",
		"......oooooo.oooooo......",
	},
	{
		".....ogggo....ogggo......",
		"....ogggo......ogggo.....",
		"....oGGGoo.....oGGGoo....",
		"....oooooo.....oooooo....",
	},
	{
		".....ogggo...ogggo.......",
		".....oGGGoo..oGGGoo......",
		".....oooooo..oooooo......",
		".........................",
	},
};

struct DronePalette {
	unsigned char paint[3], shade[3], shine[3];
	unsigned char eye[3];
	unsigned char light[3];
	bool powered;
};
const DronePalette DP_PLAYER = { {255,150,30}, {200,95,15},  {255,215,150}, {60,230,255},  {255,60,60},  true };
const DronePalette DP_REMOTE = { {70,130,235}, {35,75,165},  {165,200,255}, {120,255,140}, {80,255,120}, true };
const DronePalette DP_CLONE = { {205,175,70}, {150,120,40}, {235,215,140}, {0,0,0},       {0,0,0},      false };
const DronePalette DP_RCLONE = { {60,170,175}, {30,110,115}, {130,215,215}, {0,0,0},       {0,0,0},      false };

void paintDrone(AstroObjects& o, const DronePalette& p, int frame, bool flip, bool blink, bool lightOn) {
	size_t n = (size_t)o.width * o.height;
	o.pixels.assign(n * 3, 0);
	o.alpha.assign(n, 0);

	const unsigned char OUTLINE[3] = { 22, 20, 30 };
	const unsigned char METAL[3] = { 150, 155, 168 };
	const unsigned char DARK[3] = { 78, 82, 96 };
	const unsigned char LENS[3] = { 18, 40, 60 };
	const unsigned char GLINT[3] = { 235, 255, 255 };
	const unsigned char LAMP[3] = { 255, 245, 190 };
	const unsigned char LAMP_OFF[3] = { 120, 115, 95 };
	const unsigned char LIGHT_OFF[3] = { 90, 35, 35 };

	for (unsigned y = 0; y < o.height; y++) {
		for (unsigned x = 0; x < o.width; x++) {
			int ax = (int)(x * DRONE_ART / o.width);
			int ay = (int)(y * DRONE_ART / o.height);
			if (flip) ax = DRONE_ART - 1 - ax;
			char c = (ay < 21) ? DRONE_HEAD[ay][ax] : DRONE_LEGS[frame][ay - 21][ax];
			if (c == '.') continue;

			bool eyeLit = p.powered && !blink;
			const unsigned char* col = OUTLINE;
			switch (c) {
			case 'Y': col = p.paint; break;
			case 'y': col = p.shade; break;
			case 'H': col = p.shine; break;
			case 'G': col = METAL; break;
			case 'g': col = DARK; break;
			case 'e': col = LENS; break;
			case 'E': col = eyeLit ? p.eye : LENS; break;
			case 'W': col = eyeLit ? GLINT : LENS; break;
			case 'L': col = p.powered ? LAMP : LAMP_OFF; break;
			case 'A': col = (p.powered && lightOn) ? p.light : LIGHT_OFF; break;
			default: break;
			}
			size_t i = (size_t)y * o.width + x;
			o.pixels[i * 3 + 0] = col[0];
			o.pixels[i * 3 + 1] = col[1];
			o.pixels[i * 3 + 2] = col[2];
			o.alpha[i] = 255;
		}
	}
	o.astroUploadTexture();
}

struct DroneLook { int key = -1; };
void updateDrone(AstroObjects& o, DroneLook& look, const DronePalette& p,
	int frame, bool flip, bool blink, bool lightOn) {
	int key = frame * 8 + (flip ? 4 : 0) + (blink ? 2 : 0) + (lightOn ? 1 : 0);
	if (key == look.key) return;
	look.key = key;
	paintDrone(o, p, frame, flip, blink, lightOn);
}

bool droneBlink(double t, double offset) { return std::fmod(t + offset, 3.4) < 0.12; }
bool droneLight(double t) { return ((int)(t * 2.0)) % 2 == 0; }

// ===Enemy: hunter drone sprite
const int HUNTER_ART = 20;
const char* HUNTER_BODY[16] = {
	"..........R.........",
	"..........o.........",
	"......oooooooo......",
	".....oHHPPPPPPo.....",
	"....oHPPPPPPPPPo....",
	"...oPPPPPPPPPPPPo...",
	"..oPPkkkkkkkkkkPPo..",
	"oooPkkkkkkrrrrkkPooo",
	"oFFPkkkkkrRRRRrkPFFo",
	"oFFPkkkkkrRRWRrkPFFo",
	"oFFPkkkkkrRRRRrkPFFo",
	"oooPkkkkkkrrrrkkPooo",
	"..oPPkkkkkkkkkkPPo..",
	"...opppppppppppppo..",
	"....opppppppppppo...",
	".....ooooooooooo....",
};
const char* HUNTER_FLAME[2][4] = {
	{
		"......oTo...oTo.....",
		"......oTo...oTo.....",
		"......oto...oto.....",
		".......t.....t......",
	},
	{
		"......oTo...oTo.....",
		"......oto...oto.....",
		".......t.....t......",
		"....................",
	},
};

void paintHunter(AstroObjects& o, int flame, bool flip, bool alert) {
	size_t n = (size_t)o.width * o.height;
	o.pixels.assign(n * 3, 0);
	o.alpha.assign(n, 0);

	const unsigned char OUTLINE[3] = { 15, 10, 20 };
	const unsigned char BODY[3] = { 95, 70, 120 };
	const unsigned char SHADE[3] = { 60, 45, 80 };
	const unsigned char SHINE[3] = { 155, 125, 185 };
	const unsigned char VISOR[3] = { 25, 15, 30 };
	const unsigned char FIN[3] = { 110, 110, 125 };
	const unsigned char FLAME_A[3] = { 255, 170, 60 };
	const unsigned char FLAME_B[3] = { 255, 90, 30 };
	const unsigned char EYE_ALERT[3] = { 255, 40, 40 }, RING_ALERT[3] = { 130, 15, 25 }, GLINT_ALERT[3] = { 255, 220, 220 };
	const unsigned char EYE_CALM[3] = { 255, 200, 40 }, RING_CALM[3] = { 120, 80, 10 }, GLINT_CALM[3] = { 255, 245, 200 };

	for (unsigned y = 0; y < o.height; y++) {
		for (unsigned x = 0; x < o.width; x++) {
			int ax = (int)(x * HUNTER_ART / o.width);
			int ay = (int)(y * HUNTER_ART / o.height);
			if (flip) ax = HUNTER_ART - 1 - ax;
			char c = (ay < 16) ? HUNTER_BODY[ay][ax] : HUNTER_FLAME[flame][ay - 16][ax];
			if (c == '.') continue;

			const unsigned char* col = OUTLINE;
			switch (c) {
			case 'P': col = BODY; break;
			case 'p': col = SHADE; break;
			case 'H': col = SHINE; break;
			case 'k': col = VISOR; break;
			case 'F': col = FIN; break;
			case 'T': col = FLAME_A; break;
			case 't': col = FLAME_B; break;
			case 'R': col = alert ? EYE_ALERT : EYE_CALM; break;
			case 'r': col = alert ? RING_ALERT : RING_CALM; break;
			case 'W': col = alert ? GLINT_ALERT : GLINT_CALM; break;
			default: break;
			}
			size_t i = (size_t)y * o.width + x;
			o.pixels[i * 3 + 0] = col[0];
			o.pixels[i * 3 + 1] = col[1];
			o.pixels[i * 3 + 2] = col[2];
			o.alpha[i] = 255;
		}
	}
	o.astroUploadTexture();
}

// ===Enemy AI
const float HUNTER_PATROL_SPEED = 80.0f;
const float HUNTER_CHASE_SPEED = 140.0f;
const float HUNTER_SEE_RANGE = 240.0f;
const float HUNTER_LOSE_RANGE = 360.0f;

struct Hunter {
	AstroObjects body;
	int lookKey = -1;
	float homeX = 0, homeY = 0, minX = 0, maxX = 0;
	float dir = 1.0f, facing = -1.0f;
	bool alert = false;
	double calmUntil = 0.0;

	void reset(double now, double calmFor) {
		body.x_location = homeX;
		body.y_location = homeY;
		dir = 1.0f;
		alert = false;
		calmUntil = now + calmFor;
	}

	void updateLook(double now) {
		int flame = ((int)(now * 12.0)) % 2;
		int key = flame * 4 + (facing < 0.0f ? 2 : 0) + (alert ? 1 : 0);
		if (key == lookKey) return;
		lookKey = key;
		paintHunter(body, flame, facing < 0.0f, alert);
	}

	bool blocked(const std::vector<AstroObjects*>& walls, const std::deque<AstroObjects>& clones) const {
		for (const AstroObjects* o : walls)
			if (o->isCollidable && body.collides(*o)) return true;
		for (const AstroObjects& c : clones)
			if (body.collides(c)) return true;
		return false;
	}

	bool update(float dt, double now, float targetX, float targetY,
		const std::vector<AstroObjects*>& walls, const std::deque<AstroObjects>& clones) {
		float cx = body.x_location + body.width / 2.0f;
		float cy = body.y_location + body.height / 2.0f;
		float dx = targetX - cx, dy = targetY - cy;
		float dist = std::sqrt(dx * dx + dy * dy);

		bool spotted = false;
		if (now < calmUntil) alert = false;
		else if (!alert && dist < HUNTER_SEE_RANGE) { alert = true; spotted = true; }
		else if (alert && dist > HUNTER_LOSE_RANGE) alert = false;

		float vx, vy;
		if (alert && dist > 1.0f) {
			vx = dx / dist * HUNTER_CHASE_SPEED;
			vy = dy / dist * HUNTER_CHASE_SPEED;
		}
		else {
			if (body.x_location < minX) dir = 1.0f;
			if (body.x_location > maxX) dir = -1.0f;
			vx = dir * HUNTER_PATROL_SPEED;
			float ty = homeY + std::sin((float)now * 2.0f) * 12.0f;
			vy = std::max(-HUNTER_PATROL_SPEED, std::min(HUNTER_PATROL_SPEED, (ty - body.y_location) * 3.0f));
		}

		body.isCollidable = true;
		float oldX = body.x_location;
		body.x_location += vx * dt;
		if (blocked(walls, clones)) {
			body.x_location = oldX;
			if (!alert) dir = -dir;
		}
		float oldY = body.y_location;
		body.y_location += vy * dt;
		if (blocked(walls, clones)) body.y_location = oldY;

		if (vx > 1.0f) facing = 1.0f;
		else if (vx < -1.0f) facing = -1.0f;
		updateLook(now);
		return spotted;
	}
};

// ===Animated background
void drawAnimatedBackground(float t, int W, int H) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBegin(GL_QUADS);
	glColor4f(0.08f, 0.09f, 0.14f, 1.0f); glVertex2f(0, 0); glVertex2f((float)W, 0);
	glColor4f(0.22f, 0.25f, 0.33f, 1.0f); glVertex2f((float)W, (float)H); glVertex2f(0, (float)H);
	glEnd();

	float off = std::fmod(t * 20.0f, 64.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 0.05f);
	glBegin(GL_LINES);
	for (float x = -off; x < W; x += 64.0f) { glVertex2f(x, 0); glVertex2f(x, (float)H); }
	for (float y = off - 64.0f; y < H; y += 64.0f) { glVertex2f(0, y); glVertex2f((float)W, y); }
	glEnd();

	for (int i = 0; i < 24; i++) {
		float speed = 12.0f + (float)((i * 37) % 30);
		float size = 8.0f + (float)((i * 13) % 24);
		float x = std::fmod(i * 97.0f + t * 6.0f, (float)W + 60.0f) - 30.0f;
		float y = (float)H + 30.0f - std::fmod(i * 71.0f + t * speed, (float)H + 60.0f);
		float a = 0.10f + 0.07f * std::sin(t * 1.5f + i);
		glPushMatrix();
		glTranslatef(x, y, 0.0f);
		glRotatef(t * (20.0f + i * 3.0f), 0.0f, 0.0f, 1.0f);
		glColor4f(0.45f, 0.75f, 1.0f, a);
		glBegin(GL_QUADS);
		glVertex2f(-size / 2, -size / 2); glVertex2f(size / 2, -size / 2);
		glVertex2f(size / 2, size / 2);   glVertex2f(-size / 2, size / 2);
		glEnd();
		glPopMatrix();
	}

	float bx = std::fmod(t * 90.0f, (float)W + 300.0f) - 150.0f;
	glBegin(GL_QUADS);
	glColor4f(1, 1, 1, 0.0f);  glVertex2f(bx - 70, 0); glVertex2f(bx - 70, (float)H);
	glColor4f(1, 1, 1, 0.07f); glVertex2f(bx, (float)H); glVertex2f(bx, 0);
	glEnd();
	glBegin(GL_QUADS);
	glColor4f(1, 1, 1, 0.07f); glVertex2f(bx, 0); glVertex2f(bx, (float)H);
	glColor4f(1, 1, 1, 0.0f);  glVertex2f(bx + 70, (float)H); glVertex2f(bx + 70, 0);
	glEnd();

	glDisable(GL_BLEND);
}

// ===Effects
struct Particle { float x, y, vx, vy, life, maxLife, size, r, g, b; };
struct Ring { float x, y, t, dur, maxR, r, g, b; bool inward; };
std::vector<Particle> particles;
std::vector<Ring> rings;

float frand() { return (float)std::rand() / (float)RAND_MAX; }

void fxBurst(float cx, float cy, float r, float g, float b, int count, float speed) {
	for (int i = 0; i < count; i++) {
		float a = frand() * 6.2831853f;
		float s = speed * (0.3f + 0.7f * frand());
		Particle p;
		p.x = cx; p.y = cy;
		p.vx = std::cos(a) * s; p.vy = std::sin(a) * s;
		p.maxLife = p.life = 0.4f + 0.5f * frand();
		p.size = 3.0f + 4.0f * frand();
		p.r = r; p.g = g; p.b = b;
		particles.push_back(p);
	}
	Ring rg = { cx, cy, 0.0f, 0.35f, 60.0f, r, g, b, false };
	rings.push_back(rg);
}

void fxGather(float cx, float cy, float r, float g, float b, int count) {
	for (int i = 0; i < count; i++) {
		float a = frand() * 6.2831853f;
		float d = 50.0f + 50.0f * frand();
		Particle p;
		p.maxLife = p.life = 0.45f;
		p.x = cx + std::cos(a) * d; p.y = cy + std::sin(a) * d;
		p.vx = -std::cos(a) * d / p.maxLife; p.vy = -std::sin(a) * d / p.maxLife;
		p.size = 3.0f + 3.0f * frand();
		p.r = r; p.g = g; p.b = b;
		particles.push_back(p);
	}
	Ring rg = { cx, cy, 0.0f, 0.45f, 70.0f, r, g, b, true };
	rings.push_back(rg);
}

void fxConfetti(float cx, float cy) {
	for (int i = 0; i < 70; i++) {
		float a = -3.14159f * frand();
		float s = 150.0f + 250.0f * frand();
		Particle p;
		p.x = cx; p.y = cy;
		p.vx = std::cos(a) * s; p.vy = std::sin(a) * s;
		p.maxLife = p.life = 1.0f + 0.8f * frand();
		p.size = 4.0f + 4.0f * frand();
		p.r = 0.4f + 0.6f * frand(); p.g = 0.4f + 0.6f * frand(); p.b = 0.4f + 0.6f * frand();
		particles.push_back(p);
	}
	for (int i = 0; i < 3; i++) {
		Ring rg = { cx, cy, -0.12f * i, 0.5f, 90.0f + 40.0f * i, 1.0f, 1.0f, 0.6f, false };
		rings.push_back(rg);
	}
}

void updateFx(float dt) {
	for (size_t i = 0; i < particles.size(); ) {
		Particle& p = particles[i];
		p.life -= dt;
		if (p.life <= 0.0f) { particles[i] = particles.back(); particles.pop_back(); continue; }
		p.x += p.vx * dt; p.y += p.vy * dt;
		if (p.vy > -1000.0f && p.maxLife > 0.9f) p.vy += 400.0f * dt;
		i++;
	}
	for (size_t i = 0; i < rings.size(); ) {
		rings[i].t += dt;
		if (rings[i].t >= rings[i].dur) { rings[i] = rings.back(); rings.pop_back(); continue; }
		i++;
	}
}

void drawFx() {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	for (const Particle& p : particles) {
		float a = p.life / p.maxLife;
		float s = p.size * (0.5f + 0.5f * a);
		glColor4f(p.r, p.g, p.b, a);
		glBegin(GL_QUADS);
		glVertex2f(p.x - s, p.y - s); glVertex2f(p.x + s, p.y - s);
		glVertex2f(p.x + s, p.y + s); glVertex2f(p.x - s, p.y + s);
		glEnd();
	}

	glLineWidth(3.0f);
	for (const Ring& rg : rings) {
		if (rg.t < 0.0f) continue;
		float k = rg.t / rg.dur;
		float rad = rg.inward ? rg.maxR * (1.0f - k) : rg.maxR * k;
		float a = rg.inward ? k : (1.0f - k);
		glColor4f(rg.r, rg.g, rg.b, a);
		glBegin(GL_LINE_LOOP);
		for (int i = 0; i < 32; i++) {
			float ang = i * 6.2831853f / 32.0f;
			glVertex2f(rg.x + std::cos(ang) * rad, rg.y + std::sin(ang) * rad);
		}
		glEnd();
	}
	glLineWidth(1.0f);

	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
}

// ===Bitmap font (5x7)
struct Glyph { char c; unsigned char rows[7]; };
const Glyph FONT[] = {
	{'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
	{'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
	{'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
	{'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
	{'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
	{'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
	{'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
	{'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
	{'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
	{'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
	{'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
	{'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}}, {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
	{'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}}, {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
	{'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
	{'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
	{'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
	{'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
	{'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
	{'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}}, {':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}},
	{'-',{0x00,0x00,0x00,0x1F,0x00,0x00,0x00}}, {'_',{0x00,0x00,0x00,0x00,0x00,0x00,0x1F}},
	{'>',{0x08,0x04,0x02,0x01,0x02,0x04,0x08}}, {'<',{0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
	{'!',{0x04,0x04,0x04,0x04,0x04,0x00,0x04}}, {'/',{0x00,0x01,0x02,0x04,0x08,0x10,0x00}},
	{'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
};

const unsigned char* findGlyph(char c) {
	if (c >= 'a' && c <= 'z') c = (char)(c - 32);
	for (const Glyph& g : FONT)
		if (g.c == c) return g.rows;
	return nullptr;
}

float textWidth(const char* s, float scale) {
	size_t n = std::strlen(s);
	return n ? (n * 6.0f - 1.0f) * scale : 0.0f;
}

void drawText(const char* s, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(r, g, b, a);
	glBegin(GL_QUADS);
	for (const char* p = s; *p; ++p, x += 6.0f * scale) {
		const unsigned char* rows = findGlyph(*p);
		if (!rows) continue;
		for (int ry = 0; ry < 7; ry++) {
			for (int rx = 0; rx < 5; rx++) {
				if (!(rows[ry] & (0x10 >> rx))) continue;
				float px = x + rx * scale, py = y + ry * scale;
				glVertex2f(px, py);         glVertex2f(px + scale, py);
				glVertex2f(px + scale, py + scale); glVertex2f(px, py + scale);
			}
		}
	}
	glEnd();
	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
}

void drawTextShadow(const char* s, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
	float off = std::max(1.0f, scale * 0.5f);
	drawText(s, x + off, y + off, scale, 0.0f, 0.0f, 0.0f, 0.6f * a);
	drawText(s, x, y, scale, r, g, b, a);
}

void drawTextCentered(const char* s, float cx, float y, float scale, float r, float g, float b, float a = 1.0f) {
	drawTextShadow(s, cx - textWidth(s, scale) / 2.0f, y, scale, r, g, b, a);
}

void drawRect(float x, float y, float rw, float rh, float r, float g, float b, float a) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(r, g, b, a);
	glBegin(GL_QUADS);
	glVertex2f(x, y); glVertex2f(x + rw, y); glVertex2f(x + rw, y + rh); glVertex2f(x, y + rh);
	glEnd();
	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
}

void drawRectOutline(float x, float y, float rw, float rh, float r, float g, float b, float a, float lw = 2.0f) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(lw);
	glColor4f(r, g, b, a);
	glBegin(GL_LINE_LOOP);
	glVertex2f(x, y); glVertex2f(x + rw, y); glVertex2f(x + rw, y + rh); glVertex2f(x, y + rh);
	glEnd();
	glLineWidth(1.0f);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
}

// ===Menu keyboard input
const int TRACKED_KEYS[] = {
	GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_W, GLFW_KEY_S,
	GLFW_KEY_ENTER, GLFW_KEY_KP_ENTER, GLFW_KEY_SPACE,
	GLFW_KEY_ESCAPE, GLFW_KEY_BACKSPACE
};
bool keyNow[GLFW_KEY_LAST + 1] = {};
bool keyOld[GLFW_KEY_LAST + 1] = {};

void pollKeys() {
	for (int k : TRACKED_KEYS) {
		keyOld[k] = keyNow[k];
		keyNow[k] = astroGetKey(k) != 0;
	}
}
bool keyHit(int k) { return keyNow[k] && !keyOld[k]; }
void consumeKeys() { for (int k : TRACKED_KEYS) keyOld[k] = keyNow[k]; }

std::string ipInput;
bool ipTyping = false;
void onChar(GLFWwindow*, unsigned int cp) {
	if (!ipTyping) return;
	if (((cp >= '0' && cp <= '9') || cp == '.') && ipInput.size() < 15)
		ipInput.push_back((char)cp);
}

// ===Chambers
const int LEVEL_COUNT = 3;
const char* LEVEL_NAMES[LEVEL_COUNT] = { "THE CHAMBER", "THE SHAFT", "THE SWING" };

std::deque<AstroObjects> level;
std::vector<AstroObjects*> staticObjs;
std::vector<AstroObjects*> checkpoints;
std::vector<AstroObjects*> acids;
std::deque<Hunter> hunters;
AstroObjects* PortalA = nullptr, * PortalB = nullptr, * ExitDoor = nullptr;
int currentLevel = 0;

AstroObjects* add(int pw, int ph, int px, int py, int r, int g, int b,
	bool collidable = true, bool visible = true, const TileStyle* style = nullptr) {
	level.emplace_back();
	AstroObjects* o = &level.back();
	o->astroStartObject(ASTRO_REGULAR_OBJECT, pw, ph, px, py, r, g, b);
	o->isAnchor = true;
	o->isCollidable = collidable;
	o->isVisible = visible;
	if (style) applyTile(*o, *style);
	staticObjs.push_back(o);
	return o;
}

AstroObjects* addAcid(int pw, int ph, int px, int py) {
	AstroObjects* o = add(pw, ph, px, py, 120, 255, 60, false);
	acids.push_back(o);
	return o;
}

void addHunter(float x, float y, float minX, float maxX) {
	hunters.emplace_back();
	Hunter& h = hunters.back();
	h.body.astroStartObject(ASTRO_OBJECT_AI, 40, 40, (int)x, (int)y, 0, 0, 0);
	h.homeX = x; h.homeY = y;
	h.minX = minX; h.maxX = maxX;
}

void addFrame() {
	add(10, w.windowHeight, 0, 0, 0, 0, 0, true, true, &TS_GREEN);
	add(10, w.windowHeight, w.windowWidth - 10, 0, 0, 0, 0, true, true, &TS_GREEN);
	add(w.windowWidth, 10, 0, 0, 0, 0, 0, true, true, &TS_GREEN);
}

void buildChamber1() {
	const int G = 550;

	addAcid(200, 40, 430, 560);

	add(430, 50, 0, G, 0, 0, 0, true, true, &TS_GREEN);
	add(170, 50, 630, G, 0, 0, 0, true, true, &TS_GREEN);
	addFrame();

	add(100, 20, 110, 450, 0, 0, 0, true, true, &TS_NAVY);
	add(30, 350, 300, G - 350, 0, 0, 0, true, true, &TS_NAVY);
	PortalA = add(10, 70, 265, G - 70, 255, 60, 0, false);
	PortalB = add(10, 70, 365, G - 70, 0, 160, 255, false);

	add(50, 20, 505, 470, 0, 0, 0, true, true, &TS_NAVY);

	checkpoints.push_back(add(10, 80, 650, G - 80, 0, 200, 0, false));
	add(30, 200, 700, G - 200, 0, 0, 0, true, true, &TS_NAVY);

	ExitDoor = add(50, 80, 740, G - 80, 255, 255, 255, false);

	addHunter(470.0f, 300.0f, 360.0f, 610.0f);
}

void buildChamber2() {
	const int G = 550;
	const int LEDGE = 150;

	addAcid(280, 40, 250, 560);

	add(250, 50, 0, G, 0, 0, 0, true, true, &TS_GREEN);
	add(270, 50, 530, G, 0, 0, 0, true, true, &TS_GREEN);
	addFrame();

	add(60, 20, 370, 470, 0, 0, 0, true, true, &TS_NAVY);

	checkpoints.push_back(add(10, 80, 545, G - 80, 0, 200, 0, false));
	add(30, 150, 600, G - 150, 0, 0, 0, true, true, &TS_NAVY);

	PortalA = add(10, 70, 720, G - 70, 255, 60, 0, false);

	add(200, 20, 10, LEDGE, 0, 0, 0, true, true, &TS_NAVY);
	PortalB = add(10, 70, 160, LEDGE - 70, 0, 160, 255, false);
	ExitDoor = add(50, 80, 20, LEDGE - 80, 255, 255, 255, false);

	addHunter(440.0f, 260.0f, 270.0f, 470.0f);
	addHunter(690.0f, 250.0f, 640.0f, 740.0f);
}

void buildChamber3() {
	const int G = 550;

	addAcid(300, 40, 200, 560);

	add(200, 50, 0, G, 0, 0, 0, true, true, &TS_GREEN);
	add(290, 50, 500, G, 0, 0, 0, true, true, &TS_GREEN);
	addFrame();

	checkpoints.push_back(add(10, 80, 515, G - 80, 0, 200, 0, false));

	add(100, 20, 600, 450, 0, 0, 0, true, true, &TS_NAVY);
	add(100, 20, 460, 350, 0, 0, 0, true, true, &TS_NAVY);
	add(100, 20, 600, 250, 0, 0, 0, true, true, &TS_NAVY);
	add(190, 20, 330, 150, 0, 0, 0, true, true, &TS_NAVY);

	ExitDoor = add(50, 80, 340, 150 - 80, 255, 255, 255, false);

	addHunter(330.0f, 220.0f, 220.0f, 440.0f);
	addHunter(650.0f, 90.0f, 560.0f, 730.0f);
}

void clearLevel() {
	for (AstroObjects& o : level)
		if (o.texId) glDeleteTextures(1, &o.texId);
	for (Hunter& h : hunters)
		if (h.body.texId) glDeleteTextures(1, &h.body.texId);
	level.clear();
	staticObjs.clear();
	checkpoints.clear();
	acids.clear();
	hunters.clear();
	PortalA = PortalB = ExitDoor = nullptr;
}

void loadLevel(int index, double now) {
	clearLevel();
	currentLevel = index;
	if (index == 0) buildChamber1();
	else if (index == 1) buildChamber2();
	else buildChamber3();
	for (Hunter& h : hunters) {
		h.reset(now, 1.5);
		h.updateLook(now);
	}
}

bool inAnyAcid(const AstroObjects& o) {
	for (AstroObjects* a : acids)
		if (overlapsTrigger(o, *a)) return true;
	return false;
}

bool swinging = false, fWasDown = false, inPortal = false;
float anchorX = 0, anchorY = 0, ropeLen = 0, facing = 1.0f;

// ===App states
enum AppState { STATE_MENU, STATE_IP, STATE_GAME };
enum MenuItem { MENU_SINGLE, MENU_HOST, MENU_JOIN, MENU_QUIT, MENU_COUNT };
const char* MENU_LABELS[MENU_COUNT] = { "SINGLE PLAYER", "HOST A ROOM", "JOIN A ROOM", "QUIT" };
const float MENU_Y = 250.0f;
const float MENU_STEP = 48.0f;
const float FADE_TIME = 0.4f;
const double BANNER_TIME = 2.5;

int main(int argc, char** argv)
{
	AstroNet net;
	bool online = false, isClient = false;
	float spawnX = 50.0f;
	const float spawnY = 490.0f;

	AstroWindow* window;
	if (astroInitWindow(w.windowWidth, w.windowHeight, "Bounce Back", &window) != 0)
		return -1;
	glfwSwapInterval(1);
	glfwSetCharCallback(window, onChar);
	astroSetupProjection(w.windowWidth, w.windowHeight);

	// ===Audio
	bool engineOk = (ma_engine_init(NULL, &engine) == MA_SUCCESS);
	if (!engineOk) {
		std::cout << "Failed to start the audio engine!\n";
	}
	else {
		for (int i = 0; i < SFX_COUNT; i++) {
			if (ma_sound_init_from_file(&engine, SFX_FILES[i], MA_SOUND_FLAG_DECODE, NULL, NULL, &sfx[i]) == MA_SUCCESS)
				sfxLoaded[i] = true;
			else
				std::cout << "Could not open " << SFX_FILES[i] << "!\n";
		}
		if (ma_sound_init_from_file(&engine, "music.mp3", MA_SOUND_FLAG_STREAM, NULL, NULL, &music) == MA_SUCCESS) {
			musicLoaded = true;
			ma_sound_set_looping(&music, MA_TRUE);
			ma_sound_set_volume(&music, 0.35f);
			ma_sound_start(&music);
		}
		else {
			std::cout << "Could not open music.mp3!\n";
		}
	}

	// ===Objects
	AstroObjects player, remote, Rope, RopeR, Background;
	player.astroStartObject(ASTRO_PLAYER_OBJECT, 50, 50, (int)spawnX, (int)spawnY, 255, 128, 0);
	remote.astroStartObject(ASTRO_REGULAR_OBJECT, 50, 50, -200, -200, 0, 128, 255);
	Rope.astroStartObject(ASTRO_REGULAR_OBJECT, 10, 100, 1000, 1000, 255, 255, 0);
	RopeR.astroStartObject(ASTRO_REGULAR_OBJECT, 10, 100, 1000, 1000, 255, 255, 0);
	Background.astroStartObject(ASTRO_OBJECT_BACKROUND, w.windowWidth, w.windowHeight, 0, 0, 122, 120, 110);

	player.usePhysics = true;
	Background.isVisible = false;
	remote.isAnchor = true;  remote.isVisible = false;  remote.isCollidable = false;
	Rope.isVisible = false;  Rope.isCollidable = false;
	RopeR.isVisible = false; RopeR.isCollidable = false;

	std::deque<AstroObjects> remoteClones;
	for (int i = 0; i < NET_MAX_CLONES; i++) {
		remoteClones.emplace_back();
		AstroObjects& rc = remoteClones.back();
		rc.astroStartObject(ASTRO_REGULAR_OBJECT, 50, 50, -200, -200, 0, 200, 200);
		rc.isAnchor = true; rc.isVisible = false; rc.isCollidable = false;
	}

	// ===Start screen objects
	AstroObjects menuFloor, menuBoxA, menuBoxB;
	menuFloor.astroStartObject(ASTRO_REGULAR_OBJECT, w.windowWidth, 50, 0, 550, 0, 0, 0);
	menuBoxA.astroStartObject(ASTRO_REGULAR_OBJECT, 50, 50, 70, 500, 0, 0, 0);
	menuBoxB.astroStartObject(ASTRO_REGULAR_OBJECT, 50, 50, 680, 500, 0, 0, 0);

	// ===Textures
	DroneLook playerLook, remoteLook, menuLookA, menuLookB;
	updateDrone(player, playerLook, DP_PLAYER, DF_IDLE, false, false, true);
	updateDrone(remote, remoteLook, DP_REMOTE, DF_IDLE, false, false, true);
	for (AstroObjects& rc : remoteClones) paintDrone(rc, DP_RCLONE, DF_IDLE, false, false, false);
	applyTile(menuFloor, TS_GREEN);
	updateDrone(menuBoxA, menuLookA, DP_PLAYER, DF_IDLE, false, false, true);
	updateDrone(menuBoxB, menuLookB, DP_REMOTE, DF_IDLE, true, false, true);

	// ===Enemy
	Hunter menuHunter;
	menuHunter.body.astroStartObject(ASTRO_OBJECT_AI, 40, 40, 690, 160, 0, 0, 0);

	float remoteFacing = -1.0f, remotePrevX = 0.0f, remotePrevY = 0.0f;
	bool remoteWalking = false, remoteAir = false;
	int remoteLevel = -1;

	loadLevel(0, glfwGetTime());

	std::deque<AstroObjects> clones;
	std::vector<AstroObjects*> objects;
	auto buildObjects = [&]() {
		objects.clear();
		objects.push_back(&Background);
		for (AstroObjects* o : staticObjs) objects.push_back(o);
		for (AstroObjects& c : clones) objects.push_back(&c);
		for (AstroObjects& c : remoteClones) objects.push_back(&c);
		objects.push_back(&remote);
		objects.push_back(&Rope);
		objects.push_back(&RopeR);
		objects.push_back(&player);
		};

	std::srand((unsigned)(glfwGetTime() * 1000.0) + 12345u);

	bool rWasDown = false;
	float respawnX = spawnX, respawnY = spawnY;
	int deaths = 0;
	double lastRecv = -100.0, startTime = glfwGetTime(), winUntil = 0.0, winTime = 0.0;
	double spawnAnimUntil = 0.0, bannerUntil = 0.0;
	std::string lastTitle;

	AppState appState = STATE_MENU;
	int menuSel = 0;
	std::string menuMsg;
	double menuMsgUntil = 0.0;
	double fadeStart = glfwGetTime();
	int lastBounceA = 0, lastBounceB = 0;

	auto pcenterX = [&]() { return player.x_location + player.width / 2.0f; };
	auto pcenterY = [&]() { return player.y_location + player.height / 2.0f; };
	auto playSpawnFx = [&](float px, float py, double nowT, float dur, bool withSound = true) {
		if (withSound) playSfx(SFX_SPAWN);
		fxGather(px + player.width / 2.0f, py + player.height / 2.0f, 1.0f, 0.6f, 0.1f, 30);
		spawnAnimUntil = nowT + dur;
		};

	auto showMenuMsg = [&](const char* msg) {
		menuMsg = msg;
		menuMsgUntil = glfwGetTime() + 3.0;
		playSfx(SFX_DEATH);
		};

	auto hideRemote = [&]() {
		remote.isVisible = false; remote.isCollidable = false;
		RopeR.isVisible = false;
		for (AstroObjects& rc : remoteClones) { rc.isVisible = false; rc.isCollidable = false; }
		};

	auto clearClones = [&]() {
		for (AstroObjects& c : clones) glDeleteTextures(1, &c.texId);
		clones.clear();
		};

	auto resetPlayerToSpawn = [&]() {
		player.x_location = spawnX; player.y_location = spawnY;
		player.vx = 0.0f; player.vy = 0.0f; player.angle = 0.0f;
		respawnX = spawnX; respawnY = spawnY;
		swinging = false;
		Rope.isVisible = false;
		};

	auto startGame = [&](int mode, const std::string& hostIp) -> bool {
		net = AstroNet();
		online = false; isClient = false;
		if (mode == 2) {
			if (!net.startHost()) { net.close(); net = AstroNet(); return false; }
			online = true;
			std::cout << "Host started. UDP port " << NET_PORT << ", waiting for a player...\n";
		}
		else if (mode == 3) {
			if (!net.startClient(hostIp.c_str())) { net.close(); net = AstroNet(); return false; }
			online = true; isClient = true;
			std::cout << "Connecting to " << hostIp << ":" << NET_PORT << "...\n";
		}

		spawnX = isClient ? 110.0f : 50.0f;
		double t = glfwGetTime();
		clearClones();
		hideRemote();
		remoteLevel = -1;
		loadLevel(0, t);
		resetPlayerToSpawn();
		player.isVisible = true;
		fWasDown = false; inPortal = false; facing = 1.0f;
		rWasDown = false;
		deaths = 0;
		startTime = t; winUntil = 0.0; lastRecv = -100.0;
		bannerUntil = t + BANNER_TIME;
		lastTitle.clear();
		buildObjects();

		appState = STATE_GAME;
		ipTyping = false;
		consumeKeys();
		fadeStart = t;
		playSpawnFx(spawnX, spawnY, t, 0.6f);
		return true;
		};

	auto leaveGame = [&]() {
		if (online) net.close();
		net = AstroNet();
		online = false; isClient = false;
		clearClones();
		hideRemote();
		Rope.isVisible = false;
		swinging = false;
		appState = STATE_MENU;
		menuSel = 0;
		ipTyping = false;
		consumeKeys();
		fadeStart = glfwGetTime();
		lastTitle.clear();
		glfwSetWindowTitle(window, "Bounce Back");
		};

	if (argc >= 2) {
		bool ok;
		if (std::strcmp(argv[1], "host") == 0) ok = startGame(2, "");
		else { ipInput = argv[1]; ok = startGame(3, ipInput); }
		if (!ok) showMenuMsg("FAILED TO START NETWORKING!");
	}

	// ===Main loop
	double lastTime = glfwGetTime();
	while (AstroProgramRunning(window))
	{
		double now = glfwGetTime();
		float dt = (float)(now - lastTime);
		lastTime = now;
		if (dt > 0.05f) dt = 0.05f;

		pollKeys();

		if (appState == STATE_GAME && keyHit(GLFW_KEY_ESCAPE)) {
			playSfx(SFX_POP);
			leaveGame();
		}

		if (appState == STATE_GAME)
		{
			player.isVisible = (now >= spawnAnimUntil) || (((int)(now * 25.0)) % 2 == 0);

			// ===Clone cleanup
			while ((int)clones.size() > MAX_CLONES) {
				glDeleteTextures(1, &clones.front().texId);
				clones.pop_front();
			}
			for (auto it = clones.begin(); it != clones.end(); ) {
				if (it->y_location > w.windowHeight + 100 || inAnyAcid(*it)) {
					fxBurst(it->x_location + it->width / 2.0f, it->y_location + it->height / 2.0f,
						0.5f, 1.0f, 0.2f, 14, 140.0f);
					playSfx(SFX_POP);
					glDeleteTextures(1, &it->texId);
					it = clones.erase(it);
				}
				else ++it;
			}

			buildObjects();

			// ===Network receive
			if (online) {
				NetPacket in;
				if (net.receive(in)) {
					float rdx = in.x - remotePrevX, rdy = in.y - remotePrevY;
					remotePrevX = in.x; remotePrevY = in.y;
					if (now - lastRecv < 0.5) {
						remoteWalking = std::fabs(rdx) > 0.3f;
						remoteAir = std::fabs(rdy) > 0.3f;
						if (rdx > 0.3f) remoteFacing = 1.0f;
						else if (rdx < -0.3f) remoteFacing = -1.0f;
					}
					lastRecv = now;
					remoteLevel = in.level;

					if (remoteLevel == currentLevel) {
						remote.x_location = in.x;
						remote.y_location = in.y;
						remote.angle = in.angle;
						remote.isVisible = true;
						remote.isCollidable = true;
						if (in.swinging)
							updateRopeObject(RopeR, in.anchorX, in.anchorY,
								in.x + remote.width / 2.0f, in.y + remote.height / 2.0f);
						else
							RopeR.isVisible = false;

						for (int i = 0; i < NET_MAX_CLONES; i++) {
							AstroObjects& rc = remoteClones[i];
							if (i < in.cloneCount) {
								rc.x_location = in.clones[i].x;
								rc.y_location = in.clones[i].y;
								rc.angle = in.clones[i].angle;
								rc.isVisible = true;
								rc.isCollidable = true;
							}
							else {
								rc.isVisible = false;
								rc.isCollidable = false;
							}
						}
					}
					else {
						hideRemote();
					}
				}
				if (now - lastRecv > 2.0) hideRemote();
			}

			// ===Facing and rope input
			if (astroGetKey(GLFW_KEY_A)) facing = -1.0f;
			if (astroGetKey(GLFW_KEY_D)) facing = 1.0f;

			bool fDown = astroGetKey(GLFW_KEY_F) != 0;
			if (fDown && !fWasDown) {
				float pcx = player.x_location + player.width / 2.0f;
				float pcy = player.y_location + player.height / 2.0f;
				anchorX = pcx + facing * 60.0f;
				anchorY = pcy - 100.0f;
				ropeLen = std::sqrt((pcx - anchorX) * (pcx - anchorX) + (pcy - anchorY) * (pcy - anchorY));
				swinging = true;
				playSfx(SFX_ROPE);
			}
			if (!fDown) swinging = false;
			fWasDown = fDown;

			// ===Movements
			if (!swinging) {
				player.vx = 0.0f;
				if (astroGetKey(GLFW_KEY_A)) player.vx = -MOVE_SPEED;
				if (astroGetKey(GLFW_KEY_D)) player.vx = MOVE_SPEED;
				if ((astroGetKey(GLFW_KEY_W) || astroGetKey(GLFW_KEY_SPACE)) && player.onGround) {
					player.vy = -JUMP_SPEED;
					playSfx(SFX_JUMP);
				}
			}
			else {
				if (astroGetKey(GLFW_KEY_A)) player.vx -= SWING_PUSH * dt;
				if (astroGetKey(GLFW_KEY_D)) player.vx += SWING_PUSH * dt;
			}

			// ===Rotation
			float oldAngle = player.angle;
			if (astroGetKey(GLFW_KEY_Q)) player.angle -= ROTATE_SPEED * dt;
			if (astroGetKey(GLFW_KEY_E)) player.angle += ROTATE_SPEED * dt;
			for (AstroObjects* o : objects)
				if (o != &player && player.collides(*o)) { player.angle = oldAngle; break; }

			// ===Clone
			bool rDown = astroGetKey(GLFW_KEY_R) != 0;
			if (rDown && !rWasDown) {
				float ddx = player.x_location - spawnX, ddy = player.y_location - spawnY;
				bool nearSpawn = (ddx * ddx + ddy * ddy) < 60.0f * 60.0f;
				if (!nearSpawn) {
					fxBurst(pcenterX(), pcenterY(), 1.0f, 0.85f, 0.2f, 26, 220.0f);
					playSfx(SFX_CLONE);

					clones.emplace_back();
					AstroObjects& c = clones.back();
					c.astroStartObject(ASTRO_REGULAR_OBJECT, player.width, player.height,
						(int)player.x_location, (int)player.y_location, 255, 200, 60);
					paintDrone(c, DP_CLONE, DF_IDLE, facing < 0.0f, false, false);
					c.angle = player.angle;
					c.usePhysics = true;
					c.isAnchor = false;
					c.isCollidable = true;

					player.x_location = spawnX;
					player.y_location = spawnY;
					player.vx = 0.0f; player.vy = 0.0f;
					player.angle = 0.0f;
					swinging = false;
					playSpawnFx(spawnX, spawnY, now, 0.6f);
					buildObjects();
				}
			}
			rWasDown = rDown;

			// ===Physics
			astroPhysicsStep(objects, dt);

			if (swinging) {
				float pcx = player.x_location + player.width / 2.0f;
				float pcy = player.y_location + player.height / 2.0f;
				float dx = pcx - anchorX, dy = pcy - anchorY;
				float dist = std::sqrt(dx * dx + dy * dy);
				if (dist > ropeLen) {
					float nx = dx / dist, ny = dy / dist;
					player.x_location -= nx * (dist - ropeLen);
					player.y_location -= ny * (dist - ropeLen);
					float vr = player.vx * nx + player.vy * ny;
					if (vr > 0.0f) { player.vx -= vr * nx; player.vy -= vr * ny; }
				}
				pcx = player.x_location + player.width / 2.0f;
				pcy = player.y_location + player.height / 2.0f;
				updateRopeObject(Rope, anchorX, anchorY, pcx, pcy);
			}
			else {
				Rope.isVisible = false;
			}

			// ===Enemy
			for (Hunter& h : hunters) {
				if (h.update(dt, now, pcenterX(), pcenterY(), staticObjs, clones)) {
					playSfx(SFX_ALERT);
					fxBurst(h.body.x_location + 20.0f, h.body.y_location + 20.0f, 1.0f, 0.15f, 0.15f, 10, 120.0f);
				}
			}

			// ===Portal
			bool inA = PortalA && overlapsTrigger(player, *PortalA);
			bool inB = PortalB && overlapsTrigger(player, *PortalB);
			if (!inPortal) {
				if (inA || inB) {
					AstroObjects& src = inA ? *PortalA : *PortalB;
					AstroObjects& dest = inA ? *PortalB : *PortalA;
					float sr = inA ? 1.0f : 0.0f, sg = inA ? 0.25f : 0.65f, sb = inA ? 0.0f : 1.0f;
					fxBurst(src.x_location + src.width / 2.0f, src.y_location + src.height / 2.0f, sr, sg, sb, 24, 200.0f);

					player.x_location = dest.x_location + ((float)dest.width - (float)player.width) / 2.0f;
					player.y_location = dest.y_location + ((float)dest.height - (float)player.height) / 2.0f;
					inPortal = true;
					playSfx(SFX_PORTAL);

					float dr = inA ? 0.0f : 1.0f, dg = inA ? 0.65f : 0.25f, db = inA ? 1.0f : 0.0f;
					fxGather(pcenterX(), pcenterY(), dr, dg, db, 28);
					fxBurst(pcenterX(), pcenterY(), dr, dg, db, 16, 150.0f);
					spawnAnimUntil = now + 0.35;
				}
			}
			else if (!inA && !inB) {
				inPortal = false;
			}

			// ===Checkpoint
			for (AstroObjects* cp : checkpoints) {
				if (overlapsTrigger(player, *cp)) {
					respawnX = cp->x_location;
					respawnY = cp->y_location - 20.0f;
				}
			}

			// ===Death
			Hunter* catcher = nullptr;
			if (now >= spawnAnimUntil) {
				for (Hunter& h : hunters)
					if (overlapsTrigger(player, h.body)) { catcher = &h; break; }
			}
			if (inAnyAcid(player) || player.y_location > w.windowHeight + 50 || catcher) {
				if (catcher) catcher->reset(now, 1.5);
				float bx = pcenterX();
				float by = std::min(pcenterY(), (float)w.windowHeight - 10.0f);
				fxBurst(bx, by, 1.0f, 0.2f, 0.2f, 34, 260.0f);
				playSfx(SFX_DEATH);

				player.x_location = respawnX;
				player.y_location = respawnY;
				player.vx = 0.0f; player.vy = 0.0f;
				player.angle = 0.0f;
				swinging = false;
				deaths++;
				playSpawnFx(respawnX, respawnY, now, 0.6f);
			}

			// ===Win
			if (overlapsTrigger(player, *ExitDoor)) {
				playSfx(SFX_WIN);
				fxConfetti(ExitDoor->x_location + ExitDoor->width / 2.0f,
					ExitDoor->y_location + ExitDoor->height / 2.0f);

				int next = currentLevel + 1;
				if (next >= LEVEL_COUNT) {
					winTime = now - startTime;
					winUntil = now + 4.0;
					deaths = 0;
					startTime = now;
					next = 0;
				}
				bannerUntil = now + BANNER_TIME;

				clearClones();
				loadLevel(next, now);
				buildObjects();
				resetPlayerToSpawn();
				inPortal = false;
				playSpawnFx(spawnX, spawnY, now, 0.8f, false);
			}

			// ===Effects update
			updateFx(dt);

			// ===Title
			char title[128];
			if (now < winUntil)
				std::snprintf(title, sizeof(title), "Chamber - CONGRATULATIONS! Time: %.1f s", winTime);
			else
				std::snprintf(title, sizeof(title), "Chamber %d%s - %d s | Deaths: %d | Clones: %d/%d",
					currentLevel + 1, online ? (isClient ? " (Client)" : " (Host)") : "",
					(int)(now - startTime), deaths, (int)clones.size(), MAX_CLONES);
			if (lastTitle != title) { lastTitle = title; glfwSetWindowTitle(window, title); }

			// ===Network send
			if (online) {
				NetPacket out;
				std::memset(&out, 0, sizeof(out));
				out.x = player.x_location;
				out.y = player.y_location;
				out.angle = player.angle;
				out.swinging = swinging ? 1 : 0;
				out.anchorX = anchorX;
				out.anchorY = anchorY;
				out.level = (uint8_t)currentLevel;
				int n = 0;
				for (AstroObjects& c : clones) {
					if (n >= NET_MAX_CLONES) break;
					out.clones[n].x = c.x_location;
					out.clones[n].y = c.y_location;
					out.clones[n].angle = c.angle;
					n++;
				}
				out.cloneCount = (uint8_t)n;
				net.send(out);
			}

			// ===Drone animation
			{
				int frame = DF_IDLE;
				if (!player.onGround || swinging) frame = DF_JUMP;
				else if (player.vx != 0.0f && ((int)(now * 8.0)) % 2 == 0) frame = DF_WALK;
				updateDrone(player, playerLook, DP_PLAYER, frame, facing < 0.0f,
					droneBlink(now, 0.0), droneLight(now));

				int rframe = DF_IDLE;
				if (remoteAir) rframe = DF_JUMP;
				else if (remoteWalking && ((int)(now * 8.0)) % 2 == 0) rframe = DF_WALK;
				updateDrone(remote, remoteLook, DP_REMOTE, rframe, remoteFacing < 0.0f,
					droneBlink(now, 1.7), droneLight(now + 0.25));
			}

			// ===Render
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glLoadIdentity();
			drawAnimatedBackground((float)now, w.windowWidth, w.windowHeight);
			for (AstroObjects* o : objects)
				o->astroUpdateObject(w.windowWidth, w.windowHeight);
			for (Hunter& h : hunters) {
				h.body.astroUpdateObject(w.windowWidth, w.windowHeight);
				if (h.alert) {
					float a = 0.6f + 0.4f * std::sin((float)now * 10.0f);
					drawTextCentered("!", h.body.x_location + 20.0f, h.body.y_location - 22.0f,
						3.0f, 1.0f, 0.2f, 0.2f, a);
				}
			}
			drawFx();

			// ===Chamber banner
			if (now < winUntil) {
				float a = (float)std::min(1.0, winUntil - now);
				char line[64];
				std::snprintf(line, sizeof(line), "TIME: %.1f S", winTime);
				drawTextCentered("CONGRATULATIONS!", w.windowWidth / 2.0f, 200.0f, 6.0f, 1.0f, 0.85f, 0.3f, a);
				drawTextCentered(line, w.windowWidth / 2.0f, 260.0f, 3.0f, 1.0f, 1.0f, 1.0f, a);
			}
			else if (now < bannerUntil) {
				float a = (float)std::min(1.0, bannerUntil - now);
				char head[32];
				std::snprintf(head, sizeof(head), "CHAMBER %d", currentLevel + 1);
				drawTextCentered(head, w.windowWidth / 2.0f, 200.0f, 6.0f, 1.0f, 0.55f, 0.1f, a);
				drawTextCentered(LEVEL_NAMES[currentLevel], w.windowWidth / 2.0f, 254.0f, 3.0f, 0.75f, 0.8f, 0.9f, a);
			}

			if (online && now - lastRecv > 2.0) {
				float a = 0.6f + 0.4f * std::sin((float)now * 4.0f);
				drawTextCentered(isClient ? "CONNECTING..." : "WAITING FOR PLAYER...",
					w.windowWidth / 2.0f, 22.0f, 2.0f, 1.0f, 1.0f, 1.0f, a);
			}
			else if (online && remoteLevel >= 0 && remoteLevel != currentLevel) {
				char msg[48];
				std::snprintf(msg, sizeof(msg), "PARTNER IS IN CHAMBER %d", remoteLevel + 1);
				drawTextCentered(msg, w.windowWidth / 2.0f, 22.0f, 2.0f, 0.35f, 0.6f, 1.0f, 0.9f);
			}
		}
		else
		{
			// ===Start screen: input
			if (appState == STATE_MENU) {
				if (keyHit(GLFW_KEY_UP) || keyHit(GLFW_KEY_W)) {
					menuSel = (menuSel + MENU_COUNT - 1) % MENU_COUNT;
					playSfx(SFX_POP);
				}
				if (keyHit(GLFW_KEY_DOWN) || keyHit(GLFW_KEY_S)) {
					menuSel = (menuSel + 1) % MENU_COUNT;
					playSfx(SFX_POP);
				}
				if (keyHit(GLFW_KEY_ENTER) || keyHit(GLFW_KEY_KP_ENTER) || keyHit(GLFW_KEY_SPACE)) {
					float iy = MENU_Y + menuSel * MENU_STEP + 14.0f;
					fxBurst(w.windowWidth / 2.0f, iy, 1.0f, 0.6f, 0.1f, 22, 200.0f);
					playSfx(SFX_PORTAL);
					switch (menuSel) {
					case MENU_SINGLE:
						startGame(1, "");
						break;
					case MENU_HOST:
						if (!startGame(2, "")) showMenuMsg("COULD NOT START HOST!");
						break;
					case MENU_JOIN:
						appState = STATE_IP;
						ipTyping = true;
						consumeKeys();
						break;
					case MENU_QUIT:
						glfwSetWindowShouldClose(window, GLFW_TRUE);
						break;
					}
				}
				else if (keyHit(GLFW_KEY_ESCAPE)) {
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}
			else if (appState == STATE_IP) {
				if (keyHit(GLFW_KEY_BACKSPACE) && !ipInput.empty()) ipInput.pop_back();
				if (keyHit(GLFW_KEY_ENTER) || keyHit(GLFW_KEY_KP_ENTER)) {
					if (ipInput.empty())
						showMenuMsg("ENTER AN IP ADDRESS!");
					else if (!startGame(3, ipInput))
						showMenuMsg("INVALID IP OR NETWORK ERROR!");
				}
				else if (keyHit(GLFW_KEY_ESCAPE)) {
					appState = STATE_MENU;
					ipTyping = false;
					consumeKeys();
					playSfx(SFX_POP);
				}
			}

			// ===Start screen: animation
			const float PI = 3.14159265f;
			float ph = (float)now * 3.2f;
			float bA = std::fabs(std::sin(ph));
			float bB = std::fabs(std::sin(ph + 1.3f));
			menuBoxA.x_location = 70.0f;  menuBoxA.y_location = 500.0f - bA * 130.0f;
			menuBoxB.x_location = 680.0f; menuBoxB.y_location = 500.0f - bB * 130.0f;
			menuBoxA.angle = std::sin(ph) * 10.0f;
			menuBoxB.angle = -std::sin(ph + 1.3f) * 10.0f;
			updateDrone(menuBoxA, menuLookA, DP_PLAYER, bA > 0.12f ? DF_JUMP : DF_IDLE, false,
				droneBlink(now, 0.0), droneLight(now));
			updateDrone(menuBoxB, menuLookB, DP_REMOTE, bB > 0.12f ? DF_JUMP : DF_IDLE, true,
				droneBlink(now, 1.7), droneLight(now + 0.25));

			menuHunter.body.x_location = 690.0f + std::sin((float)now * 0.9f) * 25.0f;
			menuHunter.body.y_location = 165.0f + std::sin((float)now * 2.0f) * 10.0f;
			menuHunter.facing = -1.0f;
			menuHunter.alert = ((int)(now / 2.5)) % 2 == 1;
			menuHunter.updateLook(now);

			int idxA = (int)(ph / PI), idxB = (int)((ph + 1.3f) / PI);
			if (idxA != lastBounceA) { lastBounceA = idxA; fxBurst(95.0f, 550.0f, 0.5f, 0.5f, 0.5f, 6, 70.0f); }
			if (idxB != lastBounceB) { lastBounceB = idxB; fxBurst(705.0f, 550.0f, 0.5f, 0.5f, 0.5f, 6, 70.0f); }

			updateFx(dt);

			// ===Start screen: render
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glLoadIdentity();
			drawAnimatedBackground((float)now, w.windowWidth, w.windowHeight);
			menuFloor.astroUpdateObject(w.windowWidth, w.windowHeight);
			menuBoxA.astroUpdateObject(w.windowWidth, w.windowHeight);
			menuBoxB.astroUpdateObject(w.windowWidth, w.windowHeight);
			menuHunter.body.astroUpdateObject(w.windowWidth, w.windowHeight);

			const char* TITLE = "BOUNCE BACK";
			const float ts = 9.0f;
			float tx = w.windowWidth / 2.0f - textWidth(TITLE, ts) / 2.0f;
			for (int i = 0; TITLE[i]; i++, tx += 6.0f * ts) {
				char ch[2] = { TITLE[i], 0 };
				float ty = 60.0f + std::sin((float)now * 3.0f + i * 0.45f) * 6.0f;
				if (i < 6) drawTextShadow(ch, tx, ty, ts, 1.0f, 0.55f, 0.1f);
				else       drawTextShadow(ch, tx, ty, ts, 0.35f, 0.6f, 1.0f);
			}
			drawTextCentered("THE CHAMBER", w.windowWidth / 2.0f, 145.0f, 3.0f, 0.75f, 0.8f, 0.9f);

			drawRect(190.0f, 228.0f, 420.0f, 212.0f, 0.0f, 0.0f, 0.0f, 0.35f);
			drawRectOutline(190.0f, 228.0f, 420.0f, 212.0f, 1.0f, 1.0f, 1.0f, 0.15f);

			if (appState == STATE_MENU) {
				for (int i = 0; i < MENU_COUNT; i++) {
					float iy = MENU_Y + i * MENU_STEP;
					if (i == menuSel) {
						float pulse = 0.5f + 0.5f * std::sin((float)now * 6.0f);
						float nudge = pulse * 4.0f;
						drawRect(200.0f, iy - 8.0f, 400.0f, 44.0f, 1.0f, 0.55f, 0.1f, 0.10f + 0.06f * pulse);
						drawTextShadow(">", 215.0f + nudge, iy, 4.0f, 1.0f, 0.55f, 0.1f);
						drawTextShadow("<", 561.0f - nudge, iy, 4.0f, 1.0f, 0.55f, 0.1f);
						drawTextCentered(MENU_LABELS[i], w.windowWidth / 2.0f, iy, 4.0f, 1.0f, 0.75f, 0.3f);
					}
					else {
						drawTextCentered(MENU_LABELS[i], w.windowWidth / 2.0f, iy, 4.0f, 0.7f, 0.72f, 0.78f);
					}
				}
				drawTextCentered("W/S: MOVE   ENTER: SELECT   ESC: QUIT",
					w.windowWidth / 2.0f, 505.0f, 2.0f, 0.85f, 0.85f, 0.9f, 0.8f);
			}
			else {
				drawTextCentered("JOIN A ROOM", w.windowWidth / 2.0f, 248.0f, 4.0f, 0.35f, 0.6f, 1.0f);
				drawTextCentered("HOST IP ADDRESS", w.windowWidth / 2.0f, 298.0f, 2.0f, 0.8f, 0.8f, 0.85f);
				drawRect(220.0f, 322.0f, 360.0f, 50.0f, 0.0f, 0.0f, 0.0f, 0.5f);
				drawRectOutline(220.0f, 322.0f, 360.0f, 50.0f, 0.35f, 0.6f, 1.0f, 0.9f);
				std::string shown = ipInput;
				if (((int)(now * 2.0)) % 2 == 0) shown += "_";
				drawTextShadow(shown.c_str(), 234.0f, 335.0f, 3.5f, 1.0f, 1.0f, 1.0f);
				drawTextCentered("ENTER: CONNECT   ESC: BACK",
					w.windowWidth / 2.0f, 400.0f, 2.0f, 0.85f, 0.85f, 0.9f, 0.8f);
			}

			if (now < menuMsgUntil) {
				float a = (float)std::min(1.0, menuMsgUntil - now);
				drawTextCentered(menuMsg.c_str(), w.windowWidth / 2.0f, 462.0f, 2.0f, 1.0f, 0.3f, 0.3f, a);
			}

			drawTextCentered("A/D MOVE  W JUMP  F ROPE  Q/E TURN  R CLONE  ESC MENU",
				w.windowWidth / 2.0f, 569.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.9f);

			char ver[32];
			std::snprintf(ver, sizeof(ver), "ASTRO ENGINE V%.1f", ASTRO_VERSION);
			drawText(ver, 16.0f, 16.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.35f);

			drawFx();
		}

		// ===Fade transition
		float fadeK = (float)(now - fadeStart) / FADE_TIME;
		if (fadeK < 1.0f)
			drawRect(0.0f, 0.0f, (float)w.windowWidth, (float)w.windowHeight, 0.0f, 0.0f, 0.0f, 1.0f - fadeK);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// ===Cleanup
	clearLevel();
	for (int i = 0; i < SFX_COUNT; i++)
		if (sfxLoaded[i]) ma_sound_uninit(&sfx[i]);
	if (musicLoaded) ma_sound_uninit(&music);
	if (engineOk) ma_engine_uninit(&engine);

	if (online) net.close();
	astroUninitWindow(window);
	return 0;
}
