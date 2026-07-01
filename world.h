#ifndef WORLD_H
#define WORLD_H

#include <math.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

static unsigned int game_seed = 0;

static unsigned int hash2d(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + game_seed * 1103515245);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noise2d(int x, int z) {
    return (float)(hash2d(x, z) & 0xFFFF) / 65535.0f;
}

static float smooth_noise(float fx, float fz) {
    int ix = (int)floorf(fx);
    int iz = (int)floorf(fz);
    float dx = fx - ix;
    float dz = fz - iz;
    dx = dx * dx * (3.0f - 2.0f * dx);
    dz = dz * dz * (3.0f - 2.0f * dz);
    float v00 = noise2d(ix, iz);
    float v10 = noise2d(ix + 1, iz);
    float v01 = noise2d(ix, iz + 1);
    float v11 = noise2d(ix + 1, iz + 1);
    float i0 = v00 + (v10 - v00) * dx;
    float i1 = v01 + (v11 - v01) * dx;
    return i0 + (i1 - i0) * dz;
}

static float fbm_noise(float x, float z) {
    float val = 0;
    val += smooth_noise(x * 0.015f, z * 0.015f) * 16.0f;
    val += smooth_noise(x * 0.03f,  z * 0.03f)  * 8.0f;
    val += smooth_noise(x * 0.06f,  z * 0.06f)  * 4.0f;
    val += smooth_noise(x * 0.12f,  z * 0.12f)  * 2.0f;
    val += smooth_noise(x * 0.25f,  z * 0.25f)  * 1.0f;
    return val;
}

static int get_height(int wx, int wz) {
    float h = fbm_noise((float)wx, (float)wz);
    int height = (int)(h) + 8;
    if (height < 2) height = 2;
    if (height >= CHUNK_H - 1) height = CHUNK_H - 2;
    return height;
}

static void pos_to_block(float rx, float ry, float rz, int* wx, int* wy, int* wz) {
    *wx = (int)floorf(rx + 0.5f);
    *wy = (int)floorf(ry + 0.5f);
    *wz = (int)floorf(-rz + 0.5f);
}

static void world_to_buf(struct engine* eng, int wx, int wz, int* bx, int* bz) {
    *bx = wx - eng->loadCenterX + LOAD_RADIUS;
    *bz = wz - eng->loadCenterZ + LOAD_RADIUS;
}

static int buf_block(struct engine* eng, int bx, int by, int bz) {
    if (by < 0 || by >= CHUNK_H) return 0;
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][by][bz];
}

static int world_block_at(struct engine* eng, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_H) return 0;
    int bx, bz;
    world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return 0;
    return eng->blocks[bx][wy][bz];
}

static void load_blocks_around(struct engine* eng, int cx, int cz) {
    eng->loadCenterX = cx;
    eng->loadCenterZ = cz;
    memset(eng->blocks, 0, sizeof(eng->blocks));
    for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
        for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++) {
            int bx = dx + LOAD_RADIUS, bz = dz + LOAD_RADIUS;
            int h = get_height(cx + dx, cz + dz);
            for (int y = 0; y < h; y++)
                eng->blocks[bx][y][bz] = BLOCK_GRASS;
        }
    for (int i = 0; i < eng->editCount; i++) {
        struct block_edit* e = &eng->edits[i];
        int bx, bz;
        world_to_buf(eng, e->wx, e->wz, &bx, &bz);
        if (bx >= 0 && bx < WORLD_BUF && bz >= 0 && bz < WORLD_BUF &&
            e->wy >= 0 && e->wy < CHUNK_H)
            eng->blocks[bx][e->wy][bz] = e->val;
    }
    eng->worldLoaded = true;
    eng->meshDirty = true;
}

static void world_set_block(struct engine* eng, int wx, int wy, int wz, unsigned char val) {
    if (wy < 0 || wy >= CHUNK_H) return;
    int bx, bz;
    world_to_buf(eng, wx, wz, &bx, &bz);
    if (bx < 0 || bx >= WORLD_BUF || bz < 0 || bz >= WORLD_BUF) return;
    eng->blocks[bx][wy][bz] = val;
    for (int i = 0; i < eng->editCount; i++) {
        if (eng->edits[i].wx == wx && eng->edits[i].wy == wy && eng->edits[i].wz == wz) {
            eng->edits[i].val = val; eng->meshDirty = true; return;
        }
    }
    if (eng->editCount < MAX_EDITS) {
        struct block_edit* e = &eng->edits[eng->editCount++];
        e->wx = wx; e->wy = wy; e->wz = wz; e->val = val;
    }
    eng->meshDirty = true;
}

static void update_faces(struct engine* eng) {
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                if (!eng->blocks[x][y][z]) { eng->faces[x][y][z] = 0; continue; }
                unsigned char f = 0;
                if (!buf_block(eng,x+1,y,z)) f|=FACE_XP;
                if (!buf_block(eng,x-1,y,z)) f|=FACE_XN;
                if (!buf_block(eng,x,y+1,z)) f|=FACE_YP;
                if (!buf_block(eng,x,y-1,z)) f|=FACE_YN;
                if (!buf_block(eng,x,y,z+1)) f|=FACE_ZP;
                if (!buf_block(eng,x,y,z-1)) f|=FACE_ZN;
                eng->faces[x][y][z] = f;
            }
}

static void update_world(struct engine* eng) {
    int px = (int)floorf(eng->camPos[0]);
    int pz = (int)floorf(-eng->camPos[2]);
    if (!eng->worldLoaded) { load_blocks_around(eng, px, pz); return; }
    int dx = px - eng->loadCenterX, dz = pz - eng->loadCenterZ;
    if (dx * dx + dz * dz > 36) load_blocks_around(eng, px, pz);
}

static void get_look_dir(struct engine* eng, float* dx, float* dy, float* dz) {
    float view[16];
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    *dx = -view[2]; *dy = -view[6]; *dz = -view[10];
}

static bool raycast(struct engine* eng, int* hitX, int* hitY, int* hitZ,
                    int* prevX, int* prevY, int* prevZ) {
    float dx, dy, dz;
    get_look_dir(eng, &dx, &dy, &dz);
    float len = sqrtf(dx*dx+dy*dy+dz*dz);
    if (len < 0.001f) return false;
    dx/=len; dy/=len; dz/=len;
    *prevX=-9999;*prevY=-9999;*prevZ=-9999;
    int lx=-9999,ly=-9999,lz=-9999;
    for (float t=0.1f; t<RAY_DIST; t+=RAY_STEP) {
        int wx,wy,wz;
        pos_to_block(eng->camPos[0]+dx*t,eng->camPos[1]+dy*t,eng->camPos[2]+dz*t,&wx,&wy,&wz);
        if (wx==lx&&wy==ly&&wz==lz) continue;
        if (world_block_at(eng,wx,wy,wz)>0) {
            *hitX=wx;*hitY=wy;*hitZ=wz;*prevX=lx;*prevY=ly;*prevZ=lz; return true;
        }
        lx=wx;ly=wy;lz=wz;
    }
    return false;
}

static void inv_add_block(struct engine* eng, unsigned char type) {
    for (int i = 0; i < INV_SLOTS; i++)
        if (eng->invSlots[i] == BLOCK_AIR) { eng->invSlots[i] = type; return; }
}

static bool inv_use_block(struct engine* eng) {
    if (eng->invSlots[eng->selectedSlot] == BLOCK_AIR) return false;
    eng->invSlots[eng->selectedSlot] = BLOCK_AIR; return true;
}

static void start_block_anim(struct engine* eng, int wx, int wy, int wz, bool breaking) {
    eng->animActive = true;
    eng->animBlockX = (float)wx;
    eng->animBlockY = (float)wy;
    eng->animBlockZ = -(float)wz;
    eng->animIsBreak = breaking;
    if (breaking) eng->animBreakTimer = ANIM_BREAK_FRAMES;
    else eng->animPlaceTimer = ANIM_PLACE_FRAMES;
}

static void break_block(struct engine* eng) {
    int hx,hy,hz,px,py,pz;
    if (raycast(eng,&hx,&hy,&hz,&px,&py,&pz)) {
        if (hy<=0) return;
        unsigned char type = (unsigned char)world_block_at(eng, hx, hy, hz);
        start_block_anim(eng, hx, hy, hz, true);
        world_set_block(eng,hx,hy,hz,BLOCK_AIR);
        inv_add_block(eng, type);
    }
}

static void place_block(struct engine* eng) {
    int hx,hy,hz,px,py,pz;
    if (!raycast(eng,&hx,&hy,&hz,&px,&py,&pz)) return;
    if (px==-9999||py<0||py>=CHUNK_H) return;
    if (world_block_at(eng,px,py,pz)>0) return;
    float bRx=(float)px,bRy=(float)py,bRz=-(float)pz;
    float footY=eng->camPos[1]-EYE_H,headY=eng->camPos[1]+HEAD_MARGIN;
    if ((eng->camPos[0]+PLAYER_W>bRx-0.5f)&&(eng->camPos[0]-PLAYER_W<bRx+0.5f)&&
        (eng->camPos[2]+PLAYER_W>bRz-0.5f)&&(eng->camPos[2]-PLAYER_W<bRz+0.5f)&&
        (headY>bRy-0.5f)&&(footY<bRy+0.5f)) return;
    if (!inv_use_block(eng)) return;
    start_block_anim(eng, px, py, pz, false);
    world_set_block(eng,px,py,pz,BLOCK_GRASS);
}

#endif
