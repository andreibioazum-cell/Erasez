#ifndef PHYSICS_H
#define PHYSICS_H

#include <math.h>
#include "world.h"

/* ============================================================
   Физика: коллизии игрока (AABB) с воксельным миром.
   Позиция py - ноги; тело от py до py+P_HEIGHT.
   ============================================================ */

/* Пересекает ли AABB игрока в точке (x, y, z) твёрдый блок. */
static bool box_collide(struct engine* e, float x, float y, float z) {
    float x0 = x - P_HALF_W, x1 = x + P_HALF_W;
    float y1 = y + P_HEIGHT;
    float z0 = z - P_HALF_W, z1 = z + P_HALF_W;

    int ix0 = world_vox(x0), ix1 = world_vox(x1);
    int iy0 = (int)floorf(y), iy1 = (int)floorf(y1);
    int iz0 = world_vox(z0), iz1 = world_vox(z1);

    if (ix0 < 0) ix0 = 0; if (ix1 >= WORLD_X) ix1 = WORLD_X - 1;
    if (iz0 < 0) iz0 = 0; if (iz1 >= WORLD_Z) iz1 = WORLD_Z - 1;
    if (iy0 < 0) iy0 = 0; if (iy1 >= WORLD_Y) iy1 = WORLD_Y - 1;

    for (int iy = iy0; iy <= iy1; iy++)
        for (int ix = ix0; ix <= ix1; ix++)
            for (int iz = iz0; iz <= iz1; iz++)
                if (world_solid(e, ix, iy, iz)) return true;
    return false;
}

/* Максимальная «крыша над головой» по вертикали при движении вверх.
   Возвращает y ног, при котором голова упирается в самый низкий потолок. */
static float resolve_ceiling(struct engine* e, float x, float z, float y) {
    float x0 = x - P_HALF_W, x1 = x + P_HALF_W;
    float z0 = z - P_HALF_W, z1 = z + P_HALF_W;
    int ix0 = world_vox(x0), ix1 = world_vox(x1);
    int iz0 = world_vox(z0), iz1 = world_vox(z1);
    if (ix0 < 0) ix0 = 0; if (ix1 >= WORLD_X) ix1 = WORLD_X - 1;
    if (iz0 < 0) iz0 = 0; if (iz1 >= WORLD_Z) iz1 = WORLD_Z - 1;

    float head = y + P_HEIGHT;
    int iy0 = (int)floorf(head);
    float best = 1e9f;
    for (int iy = iy0; iy < WORLD_Y; iy++)
        for (int ix = ix0; ix <= ix1; ix++)
            for (int iz = iz0; iz <= iz1; iz++)
                if (world_solid(e, ix, iy, iz)) {
                    float b = (float)iy;
                    if (b < best) best = b;
                }
    if (best > 1e8f) return y;
    return best - P_HEIGHT;
}

/* Самый высокий пол под ногами (макс. top блока <= y + eps). */
static float ground_below(struct engine* e, float x, float z, float y) {
    float x0 = x - P_HALF_W, x1 = x + P_HALF_W;
    float z0 = z - P_HALF_W, z1 = z + P_HALF_W;
    int ix0 = world_vox(x0), ix1 = world_vox(x1);
    int iz0 = world_vox(z0), iz1 = world_vox(z1);
    if (ix0 < 0) ix0 = 0; if (ix1 >= WORLD_X) ix1 = WORLD_X - 1;
    if (iz0 < 0) iz0 = 0; if (iz1 >= WORLD_Z) iz1 = WORLD_Z - 1;

    float eps = 0.01f;
    float best = -1e9f;
    int iy0 = (int)floorf(y + eps);
    if (iy0 < 0) iy0 = 0;
    for (int iy = 0; iy <= iy0 && iy < WORLD_Y; iy++)
        for (int ix = ix0; ix <= ix1; ix++)
            for (int iz = iz0; iz <= iz1; iz++)
                if (world_solid(e, ix, iy, iz)) {
                    float t = (float)iy + 1.0f;
                    if (t > best) best = t;
                }
    return best;   /* -1e9 => опоры нет */
}

static void respawn_player(struct engine* e) {
    e->px = col_cx(WORLD_X / 2);
    e->pz = col_cz(WORLD_Z / 2);
    e->py = e->spawnY;
    e->velY = 0.0f;
    e->onGround = false;
}

static void apply_physics(struct engine* e, float dt) {
    if (dt <= 0.0f) return;
    float spd = e->flying ? FLY_SPEED : WALK_SPEED;

    /* ---- Горизонтальное движение (в плоскости XZ) ---- */
    float fx = sinf(e->yaw), fz = -cosf(e->yaw);
    float rxv = cosf(e->yaw), rzv = sinf(e->yaw);

    float mx = 0.0f, mz = 0.0f;
    if (e->isMoving && (e->moveDirX != 0.0f || e->moveDirZ != 0.0f)) {
        /* moveDirZ < 0 => стик вверх => вперёд */
        mx = (fx * -e->moveDirZ + rxv * e->moveDirX) * spd;
        mz = (fz * -e->moveDirZ + rzv * e->moveDirX) * spd;
    }

    /* X */
    float nx = e->px + mx * dt;
    if (!box_collide(e, nx, e->py, e->pz)) {
        e->px = nx;
    } else {
        bool done = false;
        if (e->onGround) {
            for (float st = 0.25f; st <= MAX_STEP + 0.001f && !done; st += 0.25f) {
                if (!box_collide(e, nx, e->py + st, e->pz)) {
                    e->px = nx;
                    e->py += st;
                    done = true;
                }
            }
        }
        (void)done;
    }
    /* Z */
    float nz = e->pz + mz * dt;
    if (!box_collide(e, e->px, e->py, nz)) {
        e->pz = nz;
    } else {
        if (e->onGround) {
            for (float st = 0.25f; st <= MAX_STEP + 0.001f; st += 0.25f) {
                if (!box_collide(e, e->px, e->py + st, nz)) {
                    e->pz = nz;
                    e->py += st;
                    break;
                }
            }
        }
    }

    /* ---- Вертикаль ---- */
    if (e->flying) {
        float want = 0.0f;
        if (e->jumpHeld) want += FLY_VERT;
        if (e->downHeld) want -= FLY_VERT;
        e->velY += (want - e->velY) * fminf(1.0f, dt * 8.0f);
    } else {
        e->velY -= GRAVITY * dt;
        if (e->velY < TERMINAL_V) e->velY = TERMINAL_V;
    }

    float ny = e->py + e->velY * dt;
    e->onGround = false;
    if (!box_collide(e, e->px, ny, e->pz)) {
        e->py = ny;
    } else {
        if (e->velY <= 0.0f) {
            /* падение/спуск: приземляемся на опору */
            float g = ground_below(e, e->px, e->pz, ny);
            if (g > -1e8f && g <= ny + 1.0f) {
                e->py = g;
                e->velY = 0.0f;
                e->onGround = !e->flying;
            } else {
                e->py = ny;
                e->velY = 0.0f;
            }
        } else {
            /* удар головой о потолок */
            e->py = resolve_ceiling(e, e->px, e->pz, ny);
            e->velY = 0.0f;
        }
    }

    /* Шаг анимации ходьбы */
    if (e->onGround && e->isMoving) e->walkPhase += dt * 9.0f;
    else if (e->walkPhase > 0.0f) e->walkPhase = 0.0f;

    /* ---- Границы мира и возрождение ---- */
    float lim = WORLD_X * 0.5f - P_HALF_W - 0.4f;
    if (e->px >  lim) e->px =  lim;
    if (e->px < -lim) e->px = -lim;
    if (e->pz >  lim) e->pz =  lim;
    if (e->pz < -lim) e->pz = -lim;

    if (e->py < -30.0f) respawn_player(e);
}

#endif
