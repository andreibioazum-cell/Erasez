#ifndef PHYSICS_H
#define PHYSICS_H

#include <math.h>
#include "engine.h"

static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;

    float nextY = eng->playerPos[1] + eng->velY;
    eng->onGround = false;

    if (nextY <= PLATFORM_Y && eng->velY <= 0) {
        nextY = PLATFORM_Y;
        eng->velY = 0;
        eng->onGround = true;
    }
    eng->playerPos[1] = nextY;

    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float camY = eng->camRotY;
        float fX = sinf(camY), fZ = -cosf(camY);
        float rX = cosf(camY), rZ = sinf(camY);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        eng->playerPos[0] += dx;
        eng->playerPos[2] += dz;

        /* Мгновенный поворот — направление куда идём */
        eng->playerRot = atan2f(dx, dz);

        eng->animTime += 0.15f;
    } else if (eng->onGround) {
        eng->animTime += 0.03f;
    }

    float half = PLATFORM_SIZE * 0.5f - 0.5f;
    if (eng->playerPos[0] > half) eng->playerPos[0] = half;
    if (eng->playerPos[0] < -half) eng->playerPos[0] = -half;
    if (eng->playerPos[2] > half) eng->playerPos[2] = half;
    if (eng->playerPos[2] < -half) eng->playerPos[2] = -half;

    if (eng->playerPos[1] < -20.0f) {
        eng->playerPos[0] = 0;
        eng->playerPos[1] = 3.0f;
        eng->playerPos[2] = 0;
        eng->velY = 0;
    }
}

#endif
