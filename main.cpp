#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#include <windows.h>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include <random>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <io.h>

using namespace std;

// Настройки
const int WIDTH = 640;
const int HEIGHT = 360;

// Глобальный флаг для управления циклом
bool game_running = true;

// Массив состояния клавиш (true = нажата)
bool keys[256] = { false };

class Vec2 {
public:
	float x;
	float y;

	Vec2(float x, float y) : x(x), y(y) {}
	Vec2() : x(0.0f), y(0.0f) {}

	bool operator==(const Vec2& other) const {
		return x == other.x && y == other.y;
	}
};

namespace std {
	template<>
	struct hash<Vec2> {
		size_t operator()(const Vec2& v) const noexcept {
			return hash<float>()(v.x) ^ (hash<float>()(v.y) << 1);
		}
	};
}

int random_int(int min, int max) {
	static random_device rd;
	static mt19937 gen(rd());
	uniform_int_distribution<int> dist(min, max);
	return dist(gen);
}

Vec2 mult_digit_vec2(Vec2 vec, float k) {
	return Vec2(vec.x * k, vec.y * k);
}

Vec2 plus_vec2(Vec2 vec, Vec2 vec2) {
	return Vec2(vec.x + vec2.x, vec.y + vec2.y);
}

Vec2 int_vec2(Vec2 vec) {
	return Vec2((int)vec.x, (int)vec.y);
}

float des_round(float x) {
	return roundf(x * 10.0f) / 10.0f;
}

class Vec3 {
public:
	float x;
	float y;
	float z;

	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
	Vec3() : x(0.0f), y(0.0f), z(0.0f) {}

	bool operator==(const Vec3& other) const {
		return x == other.x && y == other.y && z == other.z;
	}
};

namespace std {
	template<>
	struct hash<Vec3> {
		size_t operator()(const Vec3& v) const noexcept {
			return ((hash<float>()(v.x) ^ (hash<float>()(v.y) << 1)) >> 1)
				^ (hash<float>()(v.z) << 1);
		}
	};
}



class Vertex { //НЕ ИСПОЛЬЗУЕТСЯ
public:
	Vec3 pos;
	Vec3 pos_1; // esli rotToX true - k nemu idet 
	Vec3 pos_2;
	Vec3 pos_3;
	Vec3 pos_4;
	bool rotToX;

	Vertex(
		Vec3 pos,
		Vec3 pos_1 = Vec3(0, 0, 0),
		Vec3 pos_2 = Vec3(0, 0, 0),
		Vec3 pos_3 = Vec3(0, 0, 0),
		Vec3 pos_4 = Vec3(0, 0, 0),
		bool rotToX = false
	) : pos(pos), pos_1(pos_1), pos_2(pos_2), pos_3(pos_3), pos_4(pos_4), rotToX(rotToX) {
	}
};

namespace std {
	template<>
	struct hash<Vertex> {
		size_t operator()(const Vertex& v) const noexcept {
			size_t h = hash<Vec3>{}(v.pos);
			h ^= hash<Vec3>{}(v.pos_1) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<Vec3>{}(v.pos_2) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<Vec3>{}(v.pos_3) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<Vec3>{}(v.pos_4) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<bool>{}(v.rotToX) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
}

class Face {
public:
	Vec3 pos1; //верхний левый
	Vec3 pos2; //нижний левый  
	Vec3 pos3; //верхний правый
	Vec3 pos4; //нижний правый
	bool TextureRotatedToX = true; //текстура повёрнута от 1 к 3, если нет то от 1 ко 2

	Face(
		Vec3 p1 = Vec3(),
		Vec3 p2 = Vec3(),
		Vec3 p3 = Vec3(),
		Vec3 p4 = Vec3(),
		bool texRot = true
	) : pos1(p1), pos2(p2), pos3(p3), pos4(p4), TextureRotatedToX(texRot) {
	}

	bool operator==(const Face& other) const {
		return pos1 == other.pos1 && pos2 == other.pos2 &&
			pos3 == other.pos3 && pos4 == other.pos4 &&
			TextureRotatedToX == other.TextureRotatedToX;
	}
};

namespace std {
	template<>
	struct hash<Face> {
		size_t operator()(const Face& f) const noexcept {
			size_t h = hash<Vec3>{}(f.pos1);
			h ^= hash<Vec3>{}(f.pos2) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<Vec3>{}(f.pos3) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<Vec3>{}(f.pos4) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= hash<bool>{}(f.TextureRotatedToX) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
}

// !!== ИГРОВЫЕ ПЕРЕМЕННЫЕ ==!!
unordered_map<Vec3, int> WorldMap;

float anglePlayer = 0;
float anglePlayerY = 0;

Vec3 playerPos = Vec3(0, 12, 0);
int SpeedanglePlayer = 1;
float SpeedMovePlayer = 0.5f;
Vec3 ViewZoneSize = Vec3(10000, 300, 10000);
int FOV = 120;
float FOVY = static_cast<float>(FOV) / (16.0f / 9.0f);

// Коэффициенты без округления
float koef_otstupa_x = static_cast<float>(WIDTH) / static_cast<float>(FOV);
float koef_otstupa_y = static_cast<float>(HEIGHT) / FOVY;

uint32_t texture[8][8]{
	{0xF10101,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xFF0000,0xFF0000,0xFF0100,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xF00000,0xFF0000,0xFF0000,0xFF1000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xF10000,0xFF0011,0xFF0100,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xF01000,0xFF0110,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
	{0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000,0xFF0000},
};

Vec3 indececes_paralelepiped[8]{
	Vec3(0.5f,1.0f,0.5f),   Vec3(0.5f,1.0f,-0.5f),
	Vec3(-0.5f,1.0f,0.5f),  Vec3(-0.5f,1.0f,-0.5f),
	Vec3(0.5f,-1.0f,0.5f),  Vec3(-0.5f,-1.0f,0.5f),
	Vec3(-0.5f,-1.0f,-0.5f), Vec3(0.5f,-1.0f,-0.5f)
};

Face faces_paralelepiped[6]{
	// Передняя грань (Z = +0.5)
	Face(
		Vec3(-0.5f, 1.0f, 0.5f),   // верхний левый (pos3 из индексов[2])
		Vec3(-0.5f, -1.0f, 0.5f),  // нижний левый (pos2 из индексов[5])
		Vec3(0.5f, 1.0f, 0.5f),    // верхний правый (pos1 из индексов[0])
		Vec3(0.5f, -1.0f, 0.5f),   // нижний правый (pos4 из индексов[4])
		true
	),

		// Задняя грань (Z = -0.5) - инвертируем порядок для правильной нормали
	Face(
		Vec3(0.5f, 1.0f, -0.5f),   // верхний левый (pos3 из индексов[1])
		Vec3(0.5f, -1.0f, -0.5f),  // нижний левый (pos2 из индексов[7])
		Vec3(-0.5f, 1.0f, -0.5f),  // верхний правый (pos1 из индексов[3])
		Vec3(-0.5f, -1.0f, -0.5f), // нижний правый (pos4 из индексов[6])
		true
	),

		// Правая грань (X = +0.5)
	Face(
		Vec3(0.5f, 1.0f, -0.5f),   // верхний левый (pos3 из индексов[1])
		Vec3(0.5f, -1.0f, -0.5f),  // нижний левый (pos2 из индексов[7])
		Vec3(0.5f, 1.0f, 0.5f),    // верхний правый (pos1 из индексов[0])
		Vec3(0.5f, -1.0f, 0.5f),   // нижний правый (pos4 из индексов[4])
		false
	),

		// Левая грань (X = -0.5)
	Face(
		Vec3(-0.5f, 1.0f, 0.5f),   // верхний левый (pos3 из индексов[2])
		Vec3(-0.5f, -1.0f, 0.5f),  // нижний левый (pos2 из индексов[5])
		Vec3(-0.5f, 1.0f, -0.5f),  // верхний правый (pos1 из индексов[3])
		Vec3(-0.5f, -1.0f, -0.5f), // нижний правый (pos4 из индексов[6])
		false
	),

		// Верхняя грань (Y = +1.0)
	Face(
		Vec3(-0.5f, 1.0f, -0.5f),  // верхний левый (pos3 из индексов[3])
		Vec3(-0.5f, 1.0f, 0.5f),   // нижний левый (pos2 из индексов[2])
		Vec3(0.5f, 1.0f, -0.5f),   // верхний правый (pos1 из индексов[1])
		Vec3(0.5f, 1.0f, 0.5f),    // нижний правый (pos4 из индексов[0])
		true
	),

		// Нижняя грань (Y = -1.0)
	Face(
		Vec3(-0.5f, -1.0f, 0.5f),  // верхний левый (pos3 из индексов[5])
		Vec3(-0.5f, -1.0f, -0.5f), // нижний левый (pos2 из индексов[6])
		Vec3(0.5f, -1.0f, 0.5f),   // верхний правый (pos1 из индексов[4])
		Vec3(0.5f, -1.0f, -0.5f),  // нижний правый (pos4 из индексов[7])
		true
	)
};

uint32_t framebuffer[WIDTH * HEIGHT];

void set_pixel(int x, int y, uint32_t rgb_color) {
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
		uint8_t r = static_cast<uint8_t>((rgb_color >> 16) & 0xFF);
		uint8_t g = static_cast<uint8_t>((rgb_color >> 8) & 0xFF);
		uint8_t b = static_cast<uint8_t>(rgb_color & 0xFF);
		framebuffer[y * WIDTH + x] = b | (g << 8) | (r << 16); // BGRX
	}
}

void clear_screen() {
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		framebuffer[i] = 0; // чёрный
	}
}

Vec3 CheckNotVisibleCandidat(Vec3 blockPos, Vec3 vertex[8]) {
	Vec3 bestWorld;
	float maxDistSq = -1.0f;
	bool found = false;

	for (int i = 0; i < 8; i++) {
		Vec3 world(
			blockPos.x + vertex[i].x,
			blockPos.y + vertex[i].y,
			blockPos.z + vertex[i].z
		);

		float dx = world.x - playerPos.x;
		float dy = world.y - playerPos.y;
		float dz = world.z - playerPos.z;

		float distSq = dx * dx + dy * dy + dz * dz;

		if (!found || distSq > maxDistSq) {
			maxDistSq = distSq;
			bestWorld = world;
			found = true;
		}
	}

	return found ? bestWorld : Vec3(0, 0, 0);
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;
	int e2;

	while (true) {
		set_pixel(x0, y0, color);
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void line_step(int& x, int& y, int& err, int dx, int dy, int sx, int sy) {
	if (dx == 0 && dy == 0) return;

	int e2 = 2 * err;
	if (e2 > -dy) {
		err -= dy;
		x += sx;
	}
	if (e2 < dx) {
		err += dx;
		y += sy;
	}
}

void dual_line_walk(
	int x0a, int y0a, int x1a, int y1a,
	int x0b, int y0b, int x1b, int y1b
) {
	// Текущие координаты
	int xa = x0a, ya = y0a;
	int xb = x0b, yb = y0b;

	// Параметры линии A
	int dx_a = abs(x1a - x0a);
	int dy_a = abs(y1a - y0a);
	int sx_a = (x0a < x1a) ? 1 : -1;
	int sy_a = (y0a < y1a) ? 1 : -1;
	int err_a = dx_a - dy_a;

	// Параметры линии B
	int dx_b = abs(x1b - x0b);
	int dy_b = abs(y1b - y0b);
	int sx_b = (x0b < x1b) ? 1 : -1;
	int sy_b = (y0b < y1b) ? 1 : -1;
	int err_b = dx_b - dy_b;

	bool done_a = (x0a == x1a && y0a == y1a);
	bool done_b = (x0b == x1b && y0b == y1b);

	while (true) {
		Vec2 posA(static_cast<float>(xa), static_cast<float>(ya));
		Vec2 posB(static_cast<float>(xb), static_cast<float>(yb));

		if (done_a && done_b) break;

		// Шаг по линии A
		if (!done_a) {
			int e2 = 2 * err_a;
			if (e2 > -dy_a) {
				err_a -= dy_a;
				xa += sx_a;
			}
			if (e2 < dx_a) {
				err_a += dx_a;
				ya += sy_a;
			}
			done_a = (xa == x1a && ya == y1a);
		}

		// Шаг по линии B
		if (!done_b) {
			int e2 = 2 * err_b;
			if (e2 > -dy_b) {
				err_b -= dy_b;
				xb += sx_b;
			}
			if (e2 < dx_b) {
				err_b += dx_b;
				yb += sy_b;
			}
			done_b = (xb == x1b && yb == y1b);
		}
	}
}

void draw_framebuffer(HWND hwnd) {
	HDC hdc = GetDC(hwnd);
	if (hdc) {
		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = WIDTH;
		bmi.bmiHeader.biHeight = -HEIGHT; // top-down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		StretchDIBits(hdc,
			0, 0, WIDTH * 2, HEIGHT * 2,
			0, 0, WIDTH, HEIGHT,
			framebuffer, &bmi,
			DIB_RGB_COLORS, SRCCOPY);

		ReleaseDC(hwnd, hdc);
	}
}

// --- ОСНОВНАЯ ФУНКЦИЯ РЕНДЕРА ---
void Draw() {
	int startX = static_cast<int>(playerPos.x - ViewZoneSize.x);
	int endX = static_cast<int>(playerPos.x + ViewZoneSize.x);
	int startZ = static_cast<int>(playerPos.z - ViewZoneSize.z);
	int endZ = static_cast<int>(playerPos.z + ViewZoneSize.z);
	int startY = static_cast<int>(playerPos.y - ViewZoneSize.y);
	int endY = static_cast<int>(playerPos.y + ViewZoneSize.y);

	for (const auto& [key, value] : WorldMap) {
		Vec3 not_check = CheckNotVisibleCandidat(key, indececes_paralelepiped);

		for (int i = 0; i < 6; i++) {
			if (faces_paralelepiped[i].pos1 == not_check ||
				faces_paralelepiped[i].pos2 == not_check ||
				faces_paralelepiped[i].pos3 == not_check ||
				faces_paralelepiped[i].pos4 == not_check) {
				continue;
			}

			Vec2 toDrawVertexScreen[4];
			bool validVertex[4] = { false, false, false, false };

			Vec3 reference[4] = {
				faces_paralelepiped[i].pos1,
				faces_paralelepiped[i].pos2,
				faces_paralelepiped[i].pos3,
				faces_paralelepiped[i].pos4
			};

			for (int v = 0; v < 4; v++) {
				Vec3 currentWorld(
					key.x + reference[v].x,
					key.y + reference[v].y,
					key.z + reference[v].z
				);

				//Проверка зоны видимости
				if (currentWorld.x < static_cast<float>(startX) || currentWorld.x > static_cast<float>(endX) ||
					currentWorld.z < static_cast<float>(startZ) || currentWorld.z > static_cast<float>(endZ) ||
					currentWorld.y < static_cast<float>(startY) || currentWorld.y > static_cast<float>(endY)) {
					continue;
				}

				float dx = currentWorld.x - playerPos.x;
				float dz = currentWorld.z - playerPos.z;
				float dy = currentWorld.y - playerPos.y;

				float horizontal_dist = sqrtf(dx * dx + dz * dz);
				if (horizontal_dist < 0.001f) continue;

				float angle_rad = atan2f(dz, dx);
				float angle_deg = angle_rad * 180.0f / static_cast<float>(M_PI);
				if (angle_deg < 0.0f) angle_deg += 360.0f;
				
				float angle_to_obj = angle_deg - anglePlayer;
				if (angle_to_obj > 180.0f) angle_to_obj -= 360.0f;
				if (angle_to_obj < -180.0f) angle_to_obj += 360.0f;

				if (fabsf(angle_to_obj) > static_cast<float>(FOV) / 2.0f) {
					continue;
				}
				float angle_radY = atan2f(dy, horizontal_dist);
				
				float angle_degY = angle_radY * 180.0f / static_cast<float>(M_PI);

				float angle_to_objY = angle_degY - anglePlayerY;
				if (angle_to_objY > 180.0f) angle_to_objY -= 360.0f;
				if (angle_to_objY < -180.0f) angle_to_objY += 360.0f;

				int x = WIDTH / 2 + static_cast<int>(koef_otstupa_x * angle_to_obj);
				int y = HEIGHT / 2 - static_cast<int>(koef_otstupa_y * angle_to_objY);

				toDrawVertexScreen[v] = Vec2(static_cast<float>(x), static_cast<float>(y));
				validVertex[v] = true;
			}

			if (!validVertex[0] || !validVertex[1] || !validVertex[2] || !validVertex[3]) {
				continue;
			}

			//ЗАПОЛНЕНИЕ ГРАНИ 
			if (faces_paralelepiped[i].TextureRotatedToX) {
				int x0a = static_cast<int>(toDrawVertexScreen[0].x);
				int y0a = static_cast<int>(toDrawVertexScreen[0].y);
				int x1a = static_cast<int>(toDrawVertexScreen[1].x);
				int y1a = static_cast<int>(toDrawVertexScreen[1].y);

				int x0b = static_cast<int>(toDrawVertexScreen[2].x);
				int y0b = static_cast<int>(toDrawVertexScreen[2].y);
				int x1b = static_cast<int>(toDrawVertexScreen[3].x);
				int y1b = static_cast<int>(toDrawVertexScreen[3].y);

				int dxA = abs(x1a - x0a); //дельта для расстояния по X
				int dyA = abs(y1a - y0a); //дельта для расстояния по Y
				int sxA = (x0a < x1a) ? 1 : -1; //куда идет брезенхем по X
				int syA = (y0a < y1a) ? 1 : -1; //куда идет брезенхем по Y
				int errA = dxA - dyA; //Ошибка для брезенхема
				int curXa = x0a;
				int curYa = y0a;

				int dxB = abs(x1b - x0b);
				int dyB = abs(y1b - y0b);
				int sxB = (x0b < x1b) ? 1 : -1;
				int syB = (y0b < y1b) ? 1 : -1;
				int errB = dxB - dyB;
				int curXb = x0b, curYb = y0b;

				while (true) {
					draw_line(curXa, curYa, curXb, curYb, 0xFF0000);

					bool doneA = (curXa == x1a && curYa == y1a);
					bool doneB = (curXb == x1b && curYb == y1b);

					if (doneA && doneB) break;

					if (!doneA) {
						line_step(curXa, curYa, errA, dxA, dyA, sxA, syA);
					}
					if (!doneB) {
						line_step(curXb, curYb, errB, dxB, dyB, sxB, syB);
					}
				}
			}
			else {
				int x0a = static_cast<int>(toDrawVertexScreen[0].x);
				int y0a = static_cast<int>(toDrawVertexScreen[0].y);
				int x1a = static_cast<int>(toDrawVertexScreen[2].x);
				int y1a = static_cast<int>(toDrawVertexScreen[2].y);

				int x0b = static_cast<int>(toDrawVertexScreen[1].x);
				int y0b = static_cast<int>(toDrawVertexScreen[1].y);
				int x1b = static_cast<int>(toDrawVertexScreen[3].x);
				int y1b = static_cast<int>(toDrawVertexScreen[3].y);

				int dxA = abs(x1a - x0a);
				int dyA = abs(y1a - y0a);
				int sxA = (x0a < x1a) ? 1 : -1;
				int syA = (y0a < y1a) ? 1 : -1;
				int errA = dxA - dyA;
				int curXa = x0a, curYa = y0a;

				int dxB = abs(x1b - x0b);
				int dyB = abs(y1b - y0b);
				int sxB = (x0b < x1b) ? 1 : -1;
				int syB = (y0b < y1b) ? 1 : -1;
				int errB = dxB - dyB;
				int curXb = x0b, curYb = y0b;

				while (true) {
					draw_line(curXa, curYa, curXb, curYb, 0xFF0000);

					bool doneA = (curXa == x1a && curYa == y1a);
					bool doneB = (curXb == x1b && curYb == y1b);

					if (doneA && doneB) break;

					if (!doneA) {
						line_step(curXa, curYa, errA, dxA, dyA, sxA, syA);
					}
					if (!doneB) {
						line_step(curXb, curYb, errB, dxB, dyB, sxB, syB);
					}
				}
			}
		}
	}
}

// == ОСНОВНАЯ ИГРОВАЯ ФУНКЦИЯ ==
void Game_Cycle() {
	clear_screen();

	if (keys[VK_LEFT]) {
		anglePlayer -= static_cast<float>(SpeedanglePlayer);
		if (anglePlayer < 0.0f) anglePlayer += 360.0f;
	}
	else if (keys[VK_RIGHT]) {
		anglePlayer += static_cast<float>(SpeedanglePlayer);
		if (anglePlayer >= 360.0f) anglePlayer -= 360.0f;
	}

	if (keys[VK_UP]) {
		anglePlayerY += static_cast<float>(SpeedanglePlayer);
	}
	else if (keys[VK_DOWN]) {
		anglePlayerY -= static_cast<float>(SpeedanglePlayer);
	}

	if (anglePlayerY > 89.0f) anglePlayerY = 89.0f;
	if (anglePlayerY < -89.0f) anglePlayerY = -89.0f;

	// === ДВИЖЕНИЕ ИГРОКА (WASD) ===
	float angleRad = anglePlayer * static_cast<float>(M_PI) / 180.0f;
	float forwardX = cosf(angleRad);
	float forwardZ = sinf(angleRad);
	float rightX = -sinf(angleRad);
	float rightZ = cosf(angleRad);

	Vec2 moveDir = Vec2(0, 0);

	if (keys['W']) {
		moveDir.x += forwardX;
		moveDir.y += forwardZ;
	}
	if (keys['S']) {
		moveDir.x -= forwardX;
		moveDir.y -= forwardZ;
	}
	if (keys['A']) {
		moveDir.x -= rightX;
		moveDir.y -= rightZ;
	}
	if (keys['D']) {
		moveDir.x += rightX;
		moveDir.y += rightZ;
	}

	float len = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
	if (len > 0.0f) {
		moveDir.x /= len;
		moveDir.y /= len;
	}

	playerPos.x += moveDir.x * SpeedMovePlayer;
	playerPos.z += moveDir.y * SpeedMovePlayer;

	Draw();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			game_running = false;
			return 0;
		}
		if (wParam < 256) {
			keys[wParam] = true;
		}
		return 0;

	case WM_KEYUP:
		if (wParam < 256) {
			keys[wParam] = false;
		}
		return 0;

	case WM_DESTROY:
		game_running = false;
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		draw_framebuffer(hwnd);
		ValidateRect(hwnd, NULL);
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrev, _In_ LPSTR cmd, _In_ int show) {
#ifdef _DEBUG
	AllocConsole();
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);
	freopen_s(&f, "CONOUT$", "w", stderr);
	std::cout.clear();
	std::cerr.clear();
#endif

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"PixelWindow";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0, L"PixelWindow", L"Horror_survival",
		WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		WIDTH * 2, HEIGHT * 2,
		NULL, NULL, hInst, NULL
	);

	ShowWindow(hwnd, show);
	UpdateWindow(hwnd);

	// Инициализация мира
	//WorldMap[Vec3(0, 0, 10)] = 1;
	//WorldMap[Vec3(1, 0, 10)] = 1;
	//WorldMap[Vec3(2, 0, 10)] = 1;
	//WorldMap[Vec3(0, 0, 5)] = 1;
	//WorldMap[Vec3(0, 2, 10)] = 1;
	//WorldMap[Vec3(1, 2, 10)] = 1;
	//WorldMap[Vec3(2, 2, 10)] = 1;
	//WorldMap[Vec3(0, 2, 5)] = 1;

	// Спинка (2x3 блока за сиденьем, Z=9)
	WorldMap[Vec3(3.0f, 0.0f, 3.0f)] = 1;
	WorldMap[Vec3(3.0f, 2.0f, 3.0f)] = 1;
	WorldMap[Vec3(3.0f, 4.0f, 3.0f)] = 1;

	WorldMap[Vec3(3.0f, 0.0f, -3.0f)] = 1;
	WorldMap[Vec3(3.0f, 2.0f, -3.0f)] = 1;
	WorldMap[Vec3(3.0f, 4.0f, -3.0f)] = 1;

	WorldMap[Vec3(-3.0f, 0.0f, 3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 2.0f, 3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 4.0f, 3.0f)] = 1;

	WorldMap[Vec3(-3.0f, 0.0f, -3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 2.0f, -3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 4.0f, -3.0f)] = 1;

	int startX = -3;
	int endX = 4;
	int startZ = -3;
	int endZ = 4;
	for (int x = startX; x < endX; x++) {
		for (int z = startZ; z < endZ; z++) {
			WorldMap[Vec3(x, 6, z)] = 1;
		}
	}
	for (int x = -30; x < 21; x++) {
		for (int z = -30; z < 21; z++) {
			WorldMap[Vec3(x, 1, z)] = 1;
		}
	}
	WorldMap[Vec3(-3.0f, 8.0f, 3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 10.0f, 3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, 3.0f)] = 1;

	WorldMap[Vec3(-3.0f, 8.0f, -3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 10.0f, -3.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, -3.0f)] = 1;

	WorldMap[Vec3(-3.0f, 12.0f, -1.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, 1.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, 0.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, 2.0f)] = 1;
	WorldMap[Vec3(-3.0f, 12.0f, -2.0f)] = 1;


	//for (int i = 0; i < 100; i++) {
	//	WorldMap[Vec3(i, 0, 6)] = 1;
	//}

	// Игровой цикл
	const DWORD TARGET_FRAME_TIME_MS = 16; // ~60 FPS
	DWORD last_frame_time = GetTickCount();

	while (game_running) {
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				game_running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!game_running) break;

		DWORD current_time = GetTickCount();
		if (current_time - last_frame_time < TARGET_FRAME_TIME_MS) {
			Sleep(1);
			continue;
		}

		Game_Cycle();
		draw_framebuffer(hwnd);

		last_frame_time = current_time;
	}

	return 0;
}