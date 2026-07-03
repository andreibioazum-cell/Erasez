#ifndef PHYSICS_H
#define PHYSICS_H

#include <math.h>
#include "engine.h"

static void apply_physics(struct engine* eng) {
    /* Гравитация */
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;

    float nextY = eng->playerPos[1] + eng->velY;

    /* Проверка земли — плоская платформа на Y=0 */
    eng->onGround = false;
    if (nextY <= PLATFORM_Y && eng->velY <= 0) {
        nextY = PLATFORM_Y;
        eng->velY = 0;
        eng->onGround = true;
    }

    eng->playerPos[1] = nextY;

    if (eng->playerPos[1] <= PLATFORM_Y) {
        eng->playerPos[1] = PLATFORM_Y;
        eng->onGround = true;
    }

    /* Движение */
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float camY = eng->camRotY;
        float fX = sinf(camY), fZ = -cosf(camY);
        float rX = cosf(camY), rZ = sinf(camY);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        eng->playerPos[0] += dx;
        eng->playerPos[2] += dz;

        /* Поворот персонажа в направлении движения */
        float moveAngle = atan2f(dx, -dz);
        float diff = moveAngle - eng->playerRot;
        while (diff > PI) diff -= 2 * PI;
        while (diff < -PI) diff += 2 * PI;
        eng->playerRot += diff * 0.2f;

        /* Анимация ходьбы */
        eng->animTime += ANIM_SPEED;
        eng->animLegAngle = sinf(eng->animTime) * 0.6f;
        eng->animArmAngle = -sinf(eng->animTime) * 0.5f;
    } else {
        /* Плавное затухание анимации */
        eng->animLegAngle *= 0.85f;
        eng->animArmAngle *= 0.85f;
        if (fabsf(eng->animLegAngle) < 0.01f) {
            eng->animLegAngle = 0;
            eng->animArmAngle = 0;
        }
    }

    /* Ограничение платформой */
    float half = PLATFORM_SIZE * 0.5f - 0.5f;
    if (eng->playerPos[0] > half) eng->playerPos[0] = half;
    if (eng->playerPos[0] < -half) eng->playerPos[0] = -half;
    if (eng->playerPos[2] > half) eng->playerPos[2] = half;
    if (eng->playerPos[2] < -half) eng->playerPos[2] = -half;

    /* Упал за карту */
    if (eng->playerPos[1] < -20.0f) {
        eng->playerPos[0] = 0;
        eng->playerPos[1] = 3.0f;
        eng->playerPos[2] = 0;
        eng->velY = 0;
    }
}

#endif
