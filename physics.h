#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdbool.h>
#include <math.h>
#include "engine.h"

static bool is_solid(struct engine* eng, float rx, float ry, float rz) {
    /* Рендер → буфер координаты */
    int bx = (int)floorf(rx + 0.5f);
    int by = (int)floorf(ry + 0.5f);
    int bz = (int)floorf(-rz + 0.5f);
    if (bx < 0 || bx >= WORLD_BUF) return false;
    if (bz < 0 || bz >= WORLD_BUF) return false;
    if (by < 0 || by >= CHUNK_H) return false;
    return eng->blocks[bx][by][bz] > 0;
}

static bool check_box(struct engine* eng, float x, float y, float z) {
    return is_solid(eng, x - PLAYER_W, y, z - PLAYER_W) ||
           is_solid(eng, x + PLAYER_W, y, z - PLAYER_W) ||
           is_solid(eng, x - PLAYER_W, y, z + PLAYER_W) ||
           is_solid(eng, x + PLAYER_W, y, z + PLAYER_W);
}

static bool check_ground(struct engine* eng, float x, float footY, float z) {
    return check_box(eng, x, footY - 0.05f, z);
}

static bool check_ceiling(struct engine* eng, float x, float headY, float z) {
    return check_box(eng, x, headY, z);
}

static bool check_wall(struct engine* eng, float x, float eyeY, float z) {
    float foot = eyeY - EYE_H;
    return check_box(eng, x, foot + 0.1f, z) ||
           check_box(eng, x, foot + EYE_H * 0.4f, z) ||
           check_box(eng, x, foot + EYE_H * 0.8f, z) ||
           check_box(eng, x, foot + EYE_H, z);
}

static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;

    float nextY = eng->camPos[1] + eng->velY;
    eng->onGround = false;

    if (check_ground(eng, eng->camPos[0], eng->camPos[1] - EYE_H, eng->camPos[2]))
        eng->onGround = true;

    if (eng->velY <= 0) {
        if (check_ground(eng, eng->camPos[0], nextY - EYE_H, eng->camPos[2])) {
            eng->velY = 0;
            eng->onGround = true;
        } else {
            eng->camPos[1] = nextY;
        }
    } else {
        if (check_ceiling(eng, eng->camPos[0], nextY + HEAD_MARGIN, eng->camPos[2]))
            eng->velY = 0;
        else
            eng->camPos[1] = nextY;
    }

    /* Движение по горизонтали */
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float yaw = eng->camRot[1];
        float fX = sinf(yaw), fZ = -cosf(yaw);
        float rX = cosf(yaw), rZ = sinf(yaw);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        if (!check_wall(eng, eng->camPos[0] + dx, eng->camPos[1], eng->camPos[2]))
            eng->camPos[0] += dx;
        if (!check_wall(eng, eng->camPos[0], eng->camPos[1], eng->camPos[2] + dz))
            eng->camPos[2] += dz;
    }

    /* Упал — респавн */
    if (eng->camPos[1] < -20.0f) {
        eng->camPos[0] = (float)(PLATFORM_SIZE / 2);
        eng->camPos[1] = (float)(PLATFORM_Y) + EYE_H + 1.0f;
        eng->camPos[2] = -(float)(PLATFORM_SIZE / 2);
        eng->velY = 0;
    }
}

#endif
