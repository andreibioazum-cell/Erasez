#ifndef PHYSICS_H
#define PHYSICS_H

#include <math.h>
#include <stdbool.h>
#include "engine.h"

/* Радиус игрока для коллизии */
#define PLAYER_RADIUS   0.4f
#define PLAYER_HEIGHT   1.5f

/* Проверка пересечения игрока с блоком по XZ */
static bool block_collide_xz(struct engine* eng, float px, float pz,
                              struct BlockCollider* b, float playerBottom, float playerTop) {
    float bMinY = b->y - b->sy * 0.5f;
    float bMaxY = b->y + b->sy * 0.5f;
    /* По вертикали пересекаются? */
    if (playerTop <= bMinY || playerBottom >= bMaxY) return false;

    float bMinX = b->x - b->sx * 0.5f - PLAYER_RADIUS;
    float bMaxX = b->x + b->sx * 0.5f + PLAYER_RADIUS;
    float bMinZ = b->z - b->sz * 0.5f - PLAYER_RADIUS;
    float bMaxZ = b->z + b->sz * 0.5f + PLAYER_RADIUS;

    return (px > bMinX && px < bMaxX && pz > bMinZ && pz < bMaxZ);
}

/* Проверка на верх блока (можно стоять) */
static float get_ground_y(struct engine* eng, float px, float pz, float currentY) {
    float bestY = PLATFORM_Y;

    for (int i = 0; i < eng->blockCount; i++) {
        struct BlockCollider* b = &eng->blocks[i];
        float bMinX = b->x - b->sx * 0.5f;
        float bMaxX = b->x + b->sx * 0.5f;
        float bMinZ = b->z - b->sz * 0.5f;
        float bMaxZ = b->z + b->sz * 0.5f;
        float bTop = b->y + b->sy * 0.5f;

        /* Проекция игрока на блок */
        if (px + PLAYER_RADIUS > bMinX && px - PLAYER_RADIUS < bMaxX &&
            pz + PLAYER_RADIUS > bMinZ && pz - PLAYER_RADIUS < bMaxZ) {
            /* Игрок над блоком или чуть выше */
            if (currentY >= bTop - 0.1f && bTop > bestY) {
                bestY = bTop;
            }
        }
    }
    return bestY;
}

/* Проверка потолка при прыжке */
static float get_ceiling_y(struct engine* eng, float px, float pz, float playerTop) {
    float bestY = 1000.0f;

    for (int i = 0; i < eng->blockCount; i++) {
        struct BlockCollider* b = &eng->blocks[i];
        float bMinX = b->x - b->sx * 0.5f;
        float bMaxX = b->x + b->sx * 0.5f;
        float bMinZ = b->z - b->sz * 0.5f;
        float bMaxZ = b->z + b->sz * 0.5f;
        float bBottom = b->y - b->sy * 0.5f;

        if (px + PLAYER_RADIUS > bMinX && px - PLAYER_RADIUS < bMaxX &&
            pz + PLAYER_RADIUS > bMinZ && pz - PLAYER_RADIUS < bMaxZ) {
            if (bBottom > playerTop - 0.1f && bBottom < bestY) {
                bestY = bBottom;
            }
        }
    }
    return bestY;
}

static void apply_physics(struct engine* eng) {
    eng->velY -= GRAVITY;
    if (eng->velY < TERM_VEL) eng->velY = TERM_VEL;

    float nextY = eng->playerPos[1] + eng->velY;
    eng->onGround = false;

    float groundY = get_ground_y(eng, eng->playerPos[0], eng->playerPos[2], eng->playerPos[1]);

    /* Приземление */
    if (nextY <= groundY && eng->velY <= 0) {
        nextY = groundY;
        eng->velY = 0;
        eng->onGround = true;
    }

    /* Проверка потолка */
    if (eng->velY > 0) {
        float ceilY = get_ceiling_y(eng, eng->playerPos[0], eng->playerPos[2],
                                     eng->playerPos[1] + PLAYER_HEIGHT);
        if (nextY + PLAYER_HEIGHT >= ceilY) {
            nextY = ceilY - PLAYER_HEIGHT;
            eng->velY = 0;
        }
    }

    eng->playerPos[1] = nextY;

    /* Движение по XZ с коллизией */
    if (eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0)) {
        float speed = 0.08f;
        float camY = eng->camRotY;
        float fX = sinf(camY), fZ = -cosf(camY);
        float rX = cosf(camY), rZ = sinf(camY);
        float dx = (fX * -eng->moveDirZ + rX * eng->moveDirX) * speed;
        float dz = (fZ * -eng->moveDirZ + rZ * eng->moveDirX) * speed;

        float playerBottom = eng->playerPos[1] + 0.1f;
        float playerTop = eng->playerPos[1] + PLAYER_HEIGHT - 0.1f;

        /* Проверка по X */
        float newX = eng->playerPos[0] + dx;
        bool blockedX = false;
        for (int i = 0; i < eng->blockCount; i++) {
            if (block_collide_xz(eng, newX, eng->playerPos[2],
                                  &eng->blocks[i], playerBottom, playerTop)) {
                blockedX = true; break;
            }
        }
        if (!blockedX) eng->playerPos[0] = newX;

        /* Проверка по Z */
        float newZ = eng->playerPos[2] + dz;
        bool blockedZ = false;
        for (int i = 0; i < eng->blockCount; i++) {
            if (block_collide_xz(eng, eng->playerPos[0], newZ,
                                  &eng->blocks[i], playerBottom, playerTop)) {
                blockedZ = true; break;
            }
        }
        if (!blockedZ) eng->playerPos[2] = newZ;

        /* Мгновенный поворот */
        eng->playerRot = atan2f(dx, dz);
        eng->animTime += 0.15f;

        /* Плавно увеличиваем размах анимации */
        eng->animArmSwing += (0.5f - eng->animArmSwing) * 0.15f;
        eng->animLegSwing += (0.6f - eng->animLegSwing) * 0.15f;
    } else {
        /* Плавное возвращение в позу покоя */
        eng->animArmSwing *= 0.85f;
        eng->animLegSwing *= 0.85f;

        /* Продолжаем крутить фазу чтобы плавно вернуться к 0 */
        if (eng->animArmSwing > 0.01f) {
            eng->animTime += 0.15f;
        } else {
            eng->animArmSwing = 0;
            eng->animLegSwing = 0;
            /* Замедляем фазу до 0 */
            float phase = fmodf(eng->animTime, 2.0f * PI);
            eng->animTime -= phase * 0.1f;
        }
    }

    /* Границы платформы */
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
