#pragma once
#include <windows.h>
#include <vector>

struct Snail {
    int id;
    float x, y;
    float prevX, prevY;
    float speed;
    unsigned long color;
};

class SnailGame {
public:
    SnailGame();
    ~SnailGame();

    void Init(int screenW, int screenH);
    void SpawnSnail();
    void RemoveSnail(int id);
    void Update();
    const std::vector<Snail>& GetSnails() const { return m_snails; }
    bool IsRunning() const { return m_isRunning; }
    
    void Stop() { m_isRunning = false; }
    void Start() { m_isRunning = true; }

private:
    std::vector<Snail> m_snails;
    int m_nextId;
    int m_screenW, m_screenH;
    bool m_isRunning;
};
