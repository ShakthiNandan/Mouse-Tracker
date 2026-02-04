/*
    Tron Game Logic Header
    Handles Game State, Entities, and Collision Logic
*/

#ifndef TRON_GAME_H
#define TRON_GAME_H

#include <windows.h>
#include <vector>
#include <deque>

enum Direction { UP, DOWN, LEFT, RIGHT };
enum BikeState { ALIVE, DEAD };

struct Bike {
    float x, y;             // Float for smoother movement
    Direction dir;
    BikeState state;
    bool isPlayer;
    COLORREF color;
    
    // Trail stores key turning points for efficient collision
    std::vector<POINT> trail; 
    POINT lastPos;          // Last drawn position to minimize point count
};

enum GameState {
    GAME_IDLE,
    GAME_RUNNING,
    GAME_OVER
};

class TronGame {
public:
    TronGame();
    ~TronGame();

    void Init(int screenW, int screenH);
    void StartGame(int aiCount);
    void Update();
    void HandleInput(int key);
    
    // Getters for rendering
    const std::vector<Bike>& GetBikes() const { return m_bikes; }
    GameState GetState() const { return m_state; }

private:
    void UpdateAIBike(Bike& b);
    void UpdatePlayerBike(Bike& b);
    void CheckCollisions();
    bool LineSegmentIntersection(POINT p1, POINT p2, POINT p3, POINT p4);

    int m_screenW;
    int m_screenH;
    int m_speed;
    GameState m_state;
    std::vector<Bike> m_bikes;
};

#endif
