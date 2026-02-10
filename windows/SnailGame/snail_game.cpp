#include "snail_game.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <algorithm>

SnailGame::SnailGame() : m_isRunning(false), m_screenW(0), m_screenH(0), m_nextId(1) {
    srand((unsigned int)time(NULL));
}

SnailGame::~SnailGame() {}

void SnailGame::Init(int screenW, int screenH) {
    m_screenW = screenW;
    m_screenH = screenH;
}

void SnailGame::SpawnSnail() {
    Snail s;
    s.id = m_nextId++;
    s.speed = 0.5f + ((rand() % 100) / 200.0f); // Random speed between 0.5 and 1.0
    
    // Spawn randomly
    s.x = (float)(rand() % m_screenW);
    s.y = (float)(rand() % m_screenH);
    
    // Random Color (pastel/bright, avoid too dark)
    int r = 50 + rand() % 205;
    int g = 50 + rand() % 205;
    int b = 50 + rand() % 205;
    // Format is ARGB for GDI+ Color(alpha, r, g, b) constructor
    // BUT we store as DWORD ARGB: 0xAARRGGBB
    // Gdiplus::Color::MakeARGB(a, r, g, b) creates this.
    // Let's just store as raw bytes or make it simple.
    // To match Gdiplus Color constructor later: Color(255, r, g, b).GetValue()
    // Let's just assume we store the packed excessive value.
    // Actually, let's just use Gdiplus helper if possible, but better to keep game logic separate from GDI+ types directly if possible.
    // We'll store it as 0xFFRRGGBB.
    s.color = (0xFF << 24) | (r << 16) | (g << 8) | b;

    s.prevX = s.x;
    s.prevY = s.y;
    
    m_snails.push_back(s);
    m_isRunning = true; // Ensure game is running if we spawn
}

void SnailGame::RemoveSnail(int id) {
    // Standard remove-erase idiom
    auto it = std::remove_if(m_snails.begin(), m_snails.end(), 
        [id](const Snail& s){ return s.id == id; });
    m_snails.erase(it, m_snails.end());
}


void SnailGame::Update() {
    if (!m_isRunning) return;

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    for (auto& snail : m_snails) {
        float dx = (float)cursorPos.x - snail.x;
        float dy = (float)cursorPos.y - snail.y;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist > 1.0f) {
            // Move towards mouse
            float moveX = (dx / dist) * snail.speed;
            float moveY = (dy / dist) * snail.speed;

            snail.prevX = snail.x;
            snail.prevY = snail.y;
            snail.x += moveX;
            snail.y += moveY;
        }
    }
}
