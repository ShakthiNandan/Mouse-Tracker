#include "tron_game.h"
#include <math.h>
#include <time.h>

TronGame::TronGame() : m_state(GAME_IDLE), m_speed(5) {
    srand((unsigned int)time(NULL));
}

TronGame::~TronGame() {}

void TronGame::Init(int screenW, int screenH) {
    m_screenW = screenW;
    m_screenH = screenH;
    m_collisionGrid.resize(m_screenW * m_screenH);
}

void TronGame::StartGame(int aiCount) {
    m_bikes.clear();
    m_state = GAME_RUNNING;
    // Reset Grid
    std::fill(m_collisionGrid.begin(), m_collisionGrid.end(), -1);

    // 1. Add Player Bike (Starts at mouse pos)
    POINT p;
    GetCursorPos(&p);
    Bike player;
    player.x = (float)p.x;
    player.y = (float)p.y;
    player.lastPos = p;
    player.dir = RIGHT; 
    player.state = ALIVE;
    player.isPlayer = true;
    player.color = RGB(0, 255, 255); // Cyan
    player.trail.push_back(p);
    m_bikes.push_back(player);

    // 2. Add AI Bikes
    for (int i = 0; i < aiCount; i++) {
        Bike b;
        b.x = (float)(rand() % (m_screenW - 100) + 50);
        b.y = (float)(rand() % (m_screenH - 100) + 50);
        b.lastPos = { (long)b.x, (long)b.y };
        b.dir = (Direction)(rand() % 4);
        b.state = ALIVE;
        b.isPlayer = false;
        b.color = RGB(100 + rand() % 155, 100 + rand() % 155, 100 + rand() % 155);
        b.trail.push_back(b.lastPos);
        m_bikes.push_back(b);
    }
    
    // Mark initial positions on grid
    for(size_t i=0; i<m_bikes.size(); ++i) {
        int idx = (int)m_bikes[i].x + (int)m_bikes[i].y * m_screenW;
        if(idx >= 0 && idx < (int)m_collisionGrid.size()) {
            m_collisionGrid[idx] = (int)i;
        }
    }
}

void TronGame::Update() {
    if (m_state != GAME_RUNNING) return;

    bool anyAiAlive = false;

    for (size_t i = 0; i < m_bikes.size(); ++i) {
        Bike& b = m_bikes[i];
        if (b.state == DEAD) continue;
        if (!b.isPlayer) anyAiAlive = true;

        float oldX = b.x;
        float oldY = b.y;

        if (b.isPlayer) {
            UpdatePlayerBike(b);
        } else {
            UpdateAIBike(b);
        }

        // Trace movement on Grid (Bresenham-like or simple stepping)
        // We walk from (oldX, oldY) to (b.x, b.y)
        int x0 = (int)oldX;
        int y0 = (int)oldY;
        int x1 = (int)b.x;
        int y1 = (int)b.y;

        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        while (true) {
            // Check Collision (Skip start point to avoid self-collision immediately if desired, 
            // but usually safe as start point is 'owned' by us from previous frame)
            // However, if we just marked it, it returns our ID.
            
            int gridIdx = x0 + y0 * m_screenW;
            if (gridIdx >= 0 && gridIdx < (int)m_collisionGrid.size()) {
                 int curId = m_collisionGrid[gridIdx];
                 if (curId != -1 && curId != (int)i) {
                     // Hit something!
                     // Check if that something is ALIVE
                     if (m_bikes[curId].state == ALIVE) {
                         b.state = DEAD;
                         if (b.isPlayer) m_state = GAME_OVER;
                         break; // Stop tracing
                     }
                     // If DEAD, it's a ghost trail, ignore.
                 }
                 // Mark it
                 m_collisionGrid[gridIdx] = (int)i;
            } else {
                // Out of bounds - Kill? Or clamp? 
                // AI logic clamps, so we shouldn't be here. Player might be.
                if (b.isPlayer) { // Player went off screen
                     b.state = DEAD; m_state = GAME_OVER; break;
                }
            }

            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }

        // Add trail point if moved enough (rendering optimization)
        POINT curr = { (long)b.x, (long)b.y };
        float dxx = b.x - b.lastPos.x;
        float dyy = b.y - b.lastPos.y;
        if (dxx*dxx + dyy*dyy > 4) { 
            b.trail.push_back(curr);
            b.lastPos = curr;
        }
    }

    // Check Win/Loss conditions - keeping simple
}

void TronGame::UpdatePlayerBike(Bike& b) {
    POINT p;
    GetCursorPos(&p);
    
    // Calculate direction based on movement for visuals
    if (abs(p.x - (long)b.x) > abs(p.y - (long)b.y)) {
        b.dir = (p.x > b.x) ? RIGHT : LEFT;
    } else {
        b.dir = (p.y > b.y) ? DOWN : UP;
    }

    b.x = (float)p.x;
    b.y = (float)p.y;
}

void TronGame::UpdateAIBike(Bike& b) {
    // Basic movement
    float spd = (float)m_speed;
    switch (b.dir) {
        case UP:    b.y -= spd; break;
        case DOWN:  b.y += spd; break;
        case LEFT:  b.x -= spd; break;
        case RIGHT: b.x += spd; break;
    }

    // Bounds checking - wrap around or bounce? Tron usually kills. Let's Bounce for now to keep them alive longer.
    if (b.x < 0) { b.x = 0; b.dir = RIGHT; }
    if (b.x > m_screenW) { b.x = (float)m_screenW; b.dir = LEFT; }
    if (b.y < 0) { b.y = 0; b.dir = DOWN; }
    if (b.y > m_screenH) { b.y = (float)m_screenH; b.dir = UP; }

    // Random turn logic
    if (rand() % 50 == 0) {
        // Turn 90 degrees left or right
        if (b.dir == UP || b.dir == DOWN) {
            b.dir = (rand() % 2 == 0) ? LEFT : RIGHT;
        } else {
            b.dir = (rand() % 2 == 0) ? UP : DOWN;
        }
    }
}

// Deprecated O(N^2) check
void TronGame::CheckCollisions() {
    // Logic moved to Update for efficiency
}

// Helper NOT strictly needed anymore but left for compatibility
bool PointNearSegment(POINT p, POINT s1, POINT s2, int threshold = 5) {
     return false; 
}

void TronGame::HandleInput(int key) {
    // Could handle arrow keys to force direction
}
