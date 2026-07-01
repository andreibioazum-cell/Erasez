#ifndef WORLD_H
#define WORLD_H

#include <string.h>  // ДОБАВЛЕНО
#include <math.h>    // ДОБАВЛЕНО
#include "engine.h"

static void init_world(struct engine* eng) {
    memset(eng->blocks, 0, sizeof(eng->blocks));
    
    // Плоский мир из травы с ямой
    for (int x = 0; x < WORLD_SIZE_X; x++) {
        for (int z = 0; z < WORLD_SIZE_Z; z++) {
            int dx = x - WORLD_SIZE_X/2;
            int dz = z - WORLD_SIZE_Z/2;
            float dist = sqrtf(dx*dx + dz*dz);
            int height = 3;
            
            if (dist < 3) height = 0;
            else if (dist < 5) height = 1;
            else if (dist < 7) height = 2;
            
            for (int y = 0; y < height; y++) {
                if (y == height - 1 && height > 0)
                    eng->blocks[x][y][z] = BLOCK_GRASS;
                else if (height > 0)
                    eng->blocks[x][y][z] = BLOCK_DIRT;
            }
        }
    }
    
    // Небольшой домик
    int hx = WORLD_SIZE_X/2 + 4;
    int hz = WORLD_SIZE_Z/2;
    
    for (int x = hx-2; x <= hx+2; x++) {
        for (int z = hz-2; z <= hz+2; z++) {
            for (int y = 0; y < 3; y++) {
                if (x == hx-2 || x == hx+2 || z == hz-2 || z == hz+2) {
                    if (y == 0) eng->blocks[x][y][z] = BLOCK_STONE;
                    else eng->blocks[x][y][z] = BLOCK_WOOD;
                }
            }
        }
    }
    
    for (int x = hx-3; x <= hx+3; x++) {
        for (int z = hz-3; z <= hz+3; z++) {
            if (abs(x-hx) <= 2 && abs(z-hz) <= 2) continue;
            eng->blocks[x][3][z] = BLOCK_LEAVES;
        }
    }
    
    for (int i = 0; i < 8; i++) {
        int x = hx + 3 + i;
        int z = hz;
        for (int y = 0; y < 2; y++) {
            if (x < WORLD_SIZE_X && z < WORLD_SIZE_Z)
                eng->blocks[x][y][z] = BLOCK_SAND;
        }
    }
    
    eng->meshDirty = true;
}

static inline int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wx < 0 || wx >= WORLD_SIZE_X || wy < 0 || wy >= WORLD_SIZE_Y || wz < 0 || wz >= WORLD_SIZE_Z)
        return 0;
    return eng->blocks[wx][wy][wz];
}

static inline void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    if (wx < 0 || wx >= WORLD_SIZE_X || wy < 0 || wy >= WORLD_SIZE_Y || wz < 0 || wz >= WORLD_SIZE_Z)
        return;
    eng->blocks[wx][wy][wz] = val;
    eng->meshDirty = true;
    
    if (eng->editCount < MAX_EDITS) {
        eng->edits[eng->editCount].wx = wx;
        eng->edits[eng->editCount].wy = wy;
        eng->edits[eng->editCount].wz = wz;
        eng->edits[eng->editCount].val = val;
        eng->editCount++;
    }
}

static inline void pos_to_block(float x, float y, float z, int* wx, int* wy, int* wz) {
    *wx = (int)floorf(x + 0.5f);
    *wy = (int)floorf(y + 0.5f);
    *wz = (int)floorf(z + 0.5f);
}

static void get_look_dir(struct engine* eng, float* dx, float* dy, float* dz) {
    float pitch = eng->camRot[0];
    float yaw = eng->camRot[1];
    *dx = -sinf(yaw) * cosf(pitch);
    *dy = -sinf(pitch);
    *dz = -cosf(yaw) * cosf(pitch);
}

static bool raycast(struct engine* eng, int* hitX, int* hitY, int* hitZ,
                    int* prevX, int* prevY, int* prevZ) {
    float dx, dy, dz;
    get_look_dir(eng, &dx, &dy, &dz);
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 0.001f) return false;
    dx /= len; dy /= len; dz /= len;
    
    int lx = -9999, ly = -9999, lz = -9999;
    *prevX = -9999; *prevY = -9999; *prevZ = -9999;
    
    for (float t = 0.1f; t < RAY_DIST; t += RAY_STEP) {
        int wx, wy, wz;
        pos_to_block(eng->camPos[0] + dx*t, eng->camPos[1] + dy*t, eng->camPos[2] + dz*t, &wx, &wy, &wz);
        if (wx == lx && wy == ly && wz == lz) continue;
        if (world_block_at(eng, wx, wy, wz) > 0) {
            *hitX = wx; *hitY = wy; *hitZ = wz;
            *prevX = lx; *prevY = ly; *prevZ = lz;
            return true;
        }
        lx = wx; ly = wy; lz = wz;
    }
    return false;
}

#endif
