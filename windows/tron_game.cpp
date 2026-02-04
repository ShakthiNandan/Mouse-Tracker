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
}

void TronGame::StartGame(int aiCount) {
    m_bikes.clear();
    m_state = GAME_RUNNING;

    // 1. Add Player Bike (Starts at mouse pos)
    POINT p;
    GetCursorPos(&p);
    Bike player;
    player.x = (float)p.x;
    player.y = (float)p.y;
    player.lastPos = p;
    player.dir = RIGHT; // Default, but controllable
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
        // Random intense colors (Avoid black/dark)
        b.color = RGB(100 + rand() % 155, 100 + rand() % 155, 100 + rand() % 155);
        b.trail.push_back(b.lastPos);
        m_bikes.push_back(b);
    }
}

void TronGame::Update() {
    if (m_state != GAME_RUNNING) return;

    bool anyAiAlive = false;

    for (auto& b : m_bikes) {
        if (b.state == DEAD) continue;
        if (!b.isPlayer) anyAiAlive = true;

        if (b.isPlayer) {
            UpdatePlayerBike(b);
        } else {
            UpdateAIBike(b);
        }

        // Add trail point if moved enough (optimization)
        POINT curr = { (long)b.x, (long)b.y };
        float dx = b.x - b.lastPos.x;
        float dy = b.y - b.lastPos.y;
        if (dx*dx + dy*dy > 4) { // Only add if moved > 2 pixels
            b.trail.push_back(curr);
            b.lastPos = curr;
        }
    }

    CheckCollisions();

    // Check Win/Loss conditions
    if (!anyAiAlive) {
        // Player wins? Or infinite mode? keeping running for now
    }
}

void TronGame::UpdatePlayerBike(Bike& b) {
    // Player controls direction, but movement is automatic constant speed (Tron style)
    // OR: We can follow the mouse cursor directly?
    // User requested: "Cursor player -> treated like a bike"
    // To match "Tron", constant automatic movement is better.
    // BUT to match "Mouse Tracker", maybe we just use Cursor Position directly?
    // Let's stick to CURSOR POSITION directly as the user implies "GetCursorPos -> bike".
    
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

// Simple point-to-segment distance check
bool PointNearSegment(POINT p, POINT s1, POINT s2, int threshold = 5) {
    // vertical segment
    if (abs(s1.x - s2.x) < threshold) { 
        if (abs(p.x - s1.x) < threshold) {
            long minY = std::min(s1.y, s2.y) - threshold;
            long maxY = std::max(s1.y, s2.y) + threshold;
            if (p.y >= minY && p.y <= maxY) return true;
        }
    }
    // horizontal segment
    else if (abs(s1.y - s2.y) < threshold) {
        if (abs(p.y - s1.y) < threshold) {
            long minX = std::min(s1.x, s2.x) - threshold;
            long maxX = std::max(s1.x, s2.x) + threshold;
            if (p.x >= minX && p.x <= maxX) return true;
        }
    }
    // diagonal or general (simplified rect check)
    else {
        // Just checking bounding box for speed
        long minX = std::min(s1.x, s2.x) - threshold;
        long maxX = std::max(s1.x, s2.x) + threshold;
        long minY = std::min(s1.y, s2.y) - threshold;
        long maxY = std::max(s1.y, s2.y) + threshold;
        if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) return true;
    }
    return false;
}

void TronGame::CheckCollisions() {
    for (size_t i = 0; i < m_bikes.size(); i++) {
        if (m_bikes[i].state == DEAD) continue;

        POINT head = { (long)m_bikes[i].x, (long)m_bikes[i].y };

        // Check against ALL trails (including self, but ignore recent points)
        for (size_t j = 0; j < m_bikes.size(); j++) {
            if (m_bikes[j].state == DEAD) continue; // Dead trails don't kill (user requirement)

            const auto& trail = m_bikes[j].trail;
            
            // Check segments
            // Skip the very last few points of own trail to avoid self-collision immediately
            size_t k_end = trail.size();
            if (i == j && k_end > 5) k_end -= 5; 
            else if (i == j) k_end = 0; // Trail too short to hit self

            if (trail.size() < 2) continue;

            for (size_t k = 1; k < k_end; k++) {
                if (PointNearSegment(head, trail[k-1], trail[k])) {
                    m_bikes[i].state = DEAD;
                    if (m_bikes[i].isPlayer) {
                        // MessageBox handled in main loop or just flag state
                        m_state = GAME_OVER;
                    }
                }
            }
        }
    }
}

void TronGame::HandleInput(int key) {
    // Could handle arrow keys to force direction
}
