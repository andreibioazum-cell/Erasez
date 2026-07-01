#ifndef PHYSICS_H
#define PHYSICS_H

#include "engine.h"
#include "world.h"

static inline bool is_solid(struct engine* eng, float x, float y, float z) {
    int wx, wy, wz;
    pos_to_block(x, y, z, &wx, &wy, &wz);
    return world_block_at(eng, wx, wy, wz) > 0;
}

static inline bool is_colliding(struct engine* eng, float x, float y, float z) {
    float r = PLAYER_W;
    // Проверяем 8 углов AABB
    return is_solid(eng, x-r, y-r, z-r) || is_solid(eng, x+r, y-r, z-r) ||
           is_solid(eng, x-r, y-r, z+r) || is_solid(eng, x+r, y-r, z+r) ||
           is_solid(eng, x-r, y+r, z-r) || is_solid(eng, x+r, y+r, z-r) ||
           is_solid(eng, x-r, y+r, z+r) || is_solid(eng, x+r, y+r, z+r);
}

static inline bool check_axis(struct engine* eng, float x, float y, float z) {
    float r = PLAYER_W * 0.95f;
    return is_solid(eng, x-r, y-r, z-r) || is_solid(eng, x+r, y-r, z-r) ||
           is_solid(eng, x-r, y-r, z+r) || is_solid(eng, x+r, y-r, z+r) ||
           is_solid(eng, x-r, y+r, z-r) || is_solid(eng, x+r, y+r, z-r) ||
           is_solid(eng, x-r, y+r, z+r) || is_solid(eng, x+r, y+r, z+r);
}

static void apply_physics(struct engine* eng) {
    // Гравитация
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;
    
    // Сохраняем позицию для отката
    float oldX = eng->camPos[0];
    float oldY = eng->camPos[1];
    float oldZ = eng->camPos[2];
    float newX = oldX, newY = oldY, newZ = oldZ;
    
    // Вертикальное движение с точной коллизией
    newY = oldY + eng->velY;
    if (!check_axis(eng, oldX, newY - EYE_H, oldZ) && 
        !check_axis(eng, oldX, newY + HEAD_MARGIN, oldZ)) {
        eng->camPos[1] = newY;
    } else {
        if (eng->velY < 0) {
            eng->onGround = true;
        }
        eng->velY = 0;
    }
    
    // Горизонтальное движение
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw);
        float rX = cosf(yaw), rZ = sinf(yaw);
        float speed = PLAYER_SPEED;
        
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;
        
        // Движение по X
        if (!check_axis(eng, oldX + dx, eng->camPos[1] - EYE_H, oldZ)) {
            eng->camPos[0] = oldX + dx;
        }
        // Движение по Z
        if (!check_axis(eng, eng->camPos[0], eng->camPos[1] - EYE_H, oldZ + dz)) {
            eng->camPos[2] = oldZ + dz;
        }
    }
    
    // Проверка на падение в пустоту
    if (eng->camPos[1] < -10.0f) {
        eng->camPos[0] = WORLD_SIZE_X / 2.0f;
        eng->camPos[1] = 5.0f;
        eng->camPos[2] = WORLD_SIZE_Z / 2.0f;
        eng->velY = 0;
    }
}

#endif
