#ifndef WORLD_H
#define WORLD_H

#include <string.h>
#include "engine.h"

static int buf_block(struct engine* eng, int bx, int by, int bz) {
    if (by < 0 || by >= CHUNK_H) return 0;
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][by][bz];
}

static void generate_platform(struct engine* eng) {
    memset(eng->blocks, 0, sizeof(eng->blocks));
    for (int x = 0; x < PLATFORM_SIZE; x++)
        for (int z = 0; z < PLATFORM_SIZE; z++)
            eng->blocks[x][PLATFORM_Y][z] = BLOCK_SOLID;
    eng->worldLoaded = true;
    eng->meshDirty = true;
}

static void update_faces(struct engine* eng) {
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                if (!eng->blocks[x][y][z]) {
                    eng->faces[x][y][z] = 0; continue;
                }
                unsigned char f = 0;
                if (!buf_block(eng,x+1,y,z)) f |= FACE_XP;
                if (!buf_block(eng,x-1,y,z)) f |= FACE_XN;
                if (!buf_block(eng,x,y+1,z)) f |= FACE_YP;
                if (!buf_block(eng,x,y-1,z)) f |= FACE_YN;
                if (!buf_block(eng,x,y,z+1)) f |= FACE_ZP;
                if (!buf_block(eng,x,y,z-1)) f |= FACE_ZN;
                eng->faces[x][y][z] = f;
            }
}

static void update_world(struct engine* eng) {
    if (!eng->worldLoaded)
        generate_platform(eng);
}

#endif
