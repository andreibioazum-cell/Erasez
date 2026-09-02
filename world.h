#ifndef WORLD_H
#define WORLD_H

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "engine.h"

/* ============================================================
   Процедурная генерация мира: фрактальный шум -> высоты,
   биомы (снег / трава / пустыня), деревья, камни.
   Блок (x,y,z): x,z - WORLD_X/WORLD_Z, y - WORLD_Y.
   ============================================================ */

static inline unsigned char world_get(const struct engine* e,
                                       int x, int y, int z) {
    if (y < 0 || y >= WORLD_Y) return BLOCK_AIR;
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return BLOCK_AIR;
    return e->blocks[(y * WORLD_Z + z) * WORLD_X + x];
}

static inline bool world_solid(const struct engine* e, int x, int y, int z) {
    return world_get(e, x, y, z) != BLOCK_AIR;
}

static inline void world_set(struct engine* e, int x, int y, int z,
                             unsigned char b) {
    if (y < 0 || y >= WORLD_Y) return;
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return;
    e->blocks[(y * WORLD_Z + z) * WORLD_X + x] = b;
}

/* Мир по XZ центрирован относительно (0, 0). */
static inline float col_cx(int x) { return (float)x + 0.5f - WORLD_X * 0.5f; }
static inline float col_cz(int z) { return (float)z + 0.5f - WORLD_Z * 0.5f; }

/* Позиция игрока -> индекс колонки/блока. */
static inline int world_vox(float w) {
    return (int)floorf(w + WORLD_X * 0.5f);
}

/* ---------- Детерминированный шум ---------- */

static unsigned int w_hash(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + 982451653u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float w_rand01(int x, int z) {
    return (float)(w_hash(x, z) & 0xFFFFFF) * (1.0f / 16777216.0f);
}

static float w_smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static float w_noise2(float fx, float fz) {
    int x0 = (int)floorf(fx), z0 = (int)floorf(fz);
    float tx = w_smooth(fx - (float)x0);
    float tz = w_smooth(fz - (float)z0);
    float n00 = w_rand01(x0, z0);
    float n10 = w_rand01(x0 + 1, z0);
    float n01 = w_rand01(x0, z0 + 1);
    float n11 = w_rand01(x0 + 1, z0 + 1);
    float a = n00 + (n10 - n00) * tx;
    float b = n01 + (n11 - n01) * tx;
    return a + (b - a) * tz;
}

/* fBm: несколько октав, результат примерно в [0, 1]. */
static float w_fbm2(float fx, float fz) {
    float sum = 0.0f, amp = 0.5f, tot = 0.0f;
    for (int o = 0; o < 4; o++) {
        sum += w_noise2(fx, fz) * amp;
        tot += amp;
        amp *= 0.5f;
        fx *= 2.03f;
        fz *= 1.97f;
    }
    float v = sum / tot;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

/* ---------- Генерация рельефа ---------- */

static void w_fill_column(struct engine* e, int x, int z, int h,
                          bool cold, bool hot) {
    for (int y = 0; y < WORLD_Y; y++) {
        unsigned char b;
        if (y > h) b = BLOCK_AIR;
        else if (y == h) {
            if (cold)      b = BLOCK_SNOW;
            else if (hot)  b = BLOCK_SAND;
            else           b = BLOCK_GRASS;
        } else if (y >= h - 3) {
            if (hot) b = BLOCK_SAND;
            else     b = BLOCK_DIRT;
        } else {
            b = BLOCK_STONE;
        }
        world_set(e, x, y, z, b);
    }
}

static void w_place_tree(struct engine* e, int x, int z, int g) {
    if (g + 9 >= WORLD_Y) return;                 /* мало высоты */
    if (g < 6) return;
    /* ствол: 3 блока над землёй (+1 внутри кроны) */
    for (int y = g + 1; y <= g + 4; y++)
        world_set(e, x, y, z, BLOCK_WOOD);
    /* крона: два яруса */
    int hy = g + 5;                               /* нижний ярус кроны */
    for (int dx = -2; dx <= 2; dx++)
        for (int dz = -2; dz <= 2; dz++) {
            if (abs(dx) == 2 && abs(dz) == 2) continue;
            if (world_get(e, x + dx, hy, z + dz) == BLOCK_AIR)
                world_set(e, x + dx, hy, z + dz, BLOCK_LEAVES);
        }
    hy = g + 6;
    for (int dx = -1; dx <= 1; dx++)
        for (int dz = -1; dz <= 1; dz++) {
            if (world_get(e, x + dx, hy, z + dz) == BLOCK_AIR)
                world_set(e, x + dx, hy, z + dz, BLOCK_LEAVES);
        }
    if (world_get(e, x, g + 7, z) == BLOCK_AIR)
        world_set(e, x, g + 7, z, BLOCK_LEAVES);
}

static void w_gen(struct engine* e) {
    if (!e->blocks) {
        e->blocks = (unsigned char*)malloc(WORLD_X * WORLD_Y * WORLD_Z);
        if (!e->blocks) return;
    }
    memset(e->blocks, 0, WORLD_X * WORLD_Y * WORLD_Z);

    int* gh = (int*)malloc(WORLD_X * WORLD_Z * sizeof(int));
    if (!gh) return;

    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            float fx = (float)x, fz = (float)z;
            float mainN = w_fbm2(fx * 0.040f, fz * 0.040f);   /* холмы  */
            float detail = w_fbm2(fx * 0.14f + 40.0f, fz * 0.14f - 20.0f);
            float h = 16.0f + mainN * 13.0f + (detail - 0.5f) * 3.0f;
            if (h < 8.0f) h = 8.0f;
            if (h > 34.0f) h = 34.0f;
            gh[x * WORLD_Z + z] = (int)(h + 0.5f);
        }
    }

    /* Ровная стартовая поляна в центре. */
    int cxm = WORLD_X / 2, czm = WORLD_Z / 2;
    int hc = gh[cxm * WORLD_Z + czm];
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            float dx = (float)(x - cxm), dz = (float)(z - czm);
            float d = sqrtf(dx * dx + dz * dz);
            float m = (d - 3.0f) / 8.0f;
            if (m < 0.0f) m = 0.0f;
            if (m > 1.0f) m = 1.0f;
            float mix = 1.0f - m * m * (3.0f - 2.0f * m);   /* плавно к центру */
            gh[x * WORLD_Z + z] =
                (int)((float)gh[x * WORLD_Z + z] * mix + (float)hc * (1.0f - mix) + 0.5f);
        }
    }

    /* Заливаем блоки + биомы. */
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            int h = gh[x * WORLD_Z + z];
            float cold = w_fbm2((float)x * 0.009f + 100.0f,
                                (float)z * 0.009f - 50.0f);
            float hot  = w_fbm2((float)x * 0.009f + 77.0f,
                                (float)z * 0.009f + 31.0f);
            bool isCold = cold > 0.63f;
            bool isHot  = hot  > 0.68f;
            if (isCold && isHot) isHot = false;
            w_fill_column(e, x, z, h, isCold, isHot);

            float rough = w_fbm2((float)x * 0.06f + 200.0f,
                                 (float)z * 0.06f - 100.0f);
            /* Каменный выступ. */
            if (!isCold && !isHot && rough > 0.87f && h < 33) {
                world_set(e, x, h + 1, z, BLOCK_STONE);
                if (w_rand01(x + 7, z - 3) > 0.5f)
                    world_set(e, x + 1, h + 1, z, BLOCK_STONE);
                if (w_rand01(x - 5, z + 9) > 0.5f)
                    world_set(e, x, h + 1, z + 1, BLOCK_STONE);
            }

            /* Деревья — в умеренном поясе, не на поляне. */
            float dx = (float)(x - cxm), dz = (float)(z - czm);
            float dist2 = dx * dx + dz * dz;
            if (!isCold && !isHot && rough < 0.75f &&
                w_rand01(x * 3 + 1, z * 5 - 2) < 0.028f &&
                dist2 > 12.0f * 12.0f && h > 12) {
                w_place_tree(e, x, z, h);
            }
        }
    }

    e->spawnY = (float)hc + 1.0f;
    free(gh);
    e->worldReady = true;
}

#endif
