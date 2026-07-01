#ifndef MODELS_H
#define MODELS_H

#include <GLES2/gl2.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

/*
 * Все модели строятся программно из примитивов.
 * Каждая функция generate_xxx заполняет буфер вершин:
 *   [x, y, z, nx, ny, nz] на вершину (stride=24 байт)
 * Возвращает количество вершин.
 */

/* ── Утилита: куб ─────────────────────────────── */

static int gen_box(float* buf, int idx,
                   float cx, float cy, float cz,
                   float sx, float sy, float sz) {
    float x0 = cx - sx, x1 = cx + sx;
    float y0 = cy - sy, y1 = cy + sy;
    float z0 = cz - sz, z1 = cz + sz;

    /* 6 граней × 2 треугольника × 3 вершины = 36 вершин */
    float faces[36][6] = {
        /* +X */ {x1,y0,z0, 1,0,0},{x1,y0,z1, 1,0,0},{x1,y1,z1, 1,0,0},
                 {x1,y0,z0, 1,0,0},{x1,y1,z1, 1,0,0},{x1,y1,z0, 1,0,0},
        /* -X */ {x0,y0,z1,-1,0,0},{x0,y0,z0,-1,0,0},{x0,y1,z0,-1,0,0},
                 {x0,y0,z1,-1,0,0},{x0,y1,z0,-1,0,0},{x0,y1,z1,-1,0,0},
        /* +Y */ {x0,y1,z1, 0,1,0},{x1,y1,z1, 0,1,0},{x1,y1,z0, 0,1,0},
                 {x0,y1,z1, 0,1,0},{x1,y1,z0, 0,1,0},{x0,y1,z0, 0,1,0},
        /* -Y */ {x0,y0,z0, 0,-1,0},{x1,y0,z0, 0,-1,0},{x1,y0,z1, 0,-1,0},
                 {x0,y0,z0, 0,-1,0},{x1,y0,z1, 0,-1,0},{x0,y0,z1, 0,-1,0},
        /* +Z */ {x0,y0,z1, 0,0,1},{x1,y0,z1, 0,0,1},{x1,y1,z1, 0,0,1},
                 {x0,y0,z1, 0,0,1},{x1,y1,z1, 0,0,1},{x0,y1,z1, 0,0,1},
        /* -Z */ {x1,y0,z0, 0,0,-1},{x0,y0,z0, 0,0,-1},{x0,y1,z0, 0,0,-1},
                 {x1,y0,z0, 0,0,-1},{x0,y1,z0, 0,0,-1},{x1,y1,z0, 0,0,-1},
    };
    memcpy(&buf[idx], faces, sizeof(faces));
    return 36;
}

/* ── Утилита: цилиндр ─────────────────────────── */

#define CYL_SEGS 12

static int gen_cylinder(float* buf, int idx,
                        float cx, float cy, float cz,
                        float radius, float height, int segs) {
    int verts = 0;
    float halfH = height * 0.5f;

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * 2 * PI;
        float a1 = (float)(i + 1) / segs * 2 * PI;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        float nx0 = c0, nz0 = s0;
        float nx1 = c1, nz1 = s1;

        /* Боковая грань — два треугольника */
        int p = idx + verts * 6;
        /* tri 1 */
        buf[p+ 0]=cx+c0*radius; buf[p+ 1]=cy-halfH; buf[p+ 2]=cz+s0*radius;
        buf[p+ 3]=nx0; buf[p+ 4]=0; buf[p+ 5]=nz0;
        buf[p+ 6]=cx+c1*radius; buf[p+ 7]=cy-halfH; buf[p+ 8]=cz+s1*radius;
        buf[p+ 9]=nx1; buf[p+10]=0; buf[p+11]=nz1;
        buf[p+12]=cx+c1*radius; buf[p+13]=cy+halfH; buf[p+14]=cz+s1*radius;
        buf[p+15]=nx1; buf[p+16]=0; buf[p+17]=nz1;
        /* tri 2 */
        buf[p+18]=cx+c0*radius; buf[p+19]=cy-halfH; buf[p+20]=cz+s0*radius;
        buf[p+21]=nx0; buf[p+22]=0; buf[p+23]=nz0;
        buf[p+24]=cx+c1*radius; buf[p+25]=cy+halfH; buf[p+26]=cz+s1*radius;
        buf[p+27]=nx1; buf[p+28]=0; buf[p+29]=nz1;
        buf[p+30]=cx+c0*radius; buf[p+31]=cy+halfH; buf[p+32]=cz+s0*radius;
        buf[p+33]=nx0; buf[p+34]=0; buf[p+35]=nz0;
        verts += 6;

        /* Верхний круг */
        p = idx + verts * 6;
        buf[p+ 0]=cx;            buf[p+ 1]=cy+halfH; buf[p+ 2]=cz;
        buf[p+ 3]=0; buf[p+ 4]=1; buf[p+ 5]=0;
        buf[p+ 6]=cx+c0*radius;  buf[p+ 7]=cy+halfH; buf[p+ 8]=cz+s0*radius;
        buf[p+ 9]=0; buf[p+10]=1; buf[p+11]=0;
        buf[p+12]=cx+c1*radius;  buf[p+13]=cy+halfH; buf[p+14]=cz+s1*radius;
        buf[p+15]=0; buf[p+16]=1; buf[p+17]=0;
        verts += 3;

        /* Нижний круг */
        p = idx + verts * 6;
        buf[p+ 0]=cx;            buf[p+ 1]=cy-halfH; buf[p+ 2]=cz;
        buf[p+ 3]=0; buf[p+ 4]=-1; buf[p+ 5]=0;
        buf[p+ 6]=cx+c1*radius;  buf[p+ 7]=cy-halfH; buf[p+ 8]=cz+s1*radius;
        buf[p+ 9]=0; buf[p+10]=-1; buf[p+11]=0;
        buf[p+12]=cx+c0*radius;  buf[p+13]=cy-halfH; buf[p+14]=cz+s0*radius;
        buf[p+15]=0; buf[p+16]=-1; buf[p+17]=0;
        verts += 3;
    }
    return verts;
}

/* ── Утилита: конус ────────────────────────────── */

static int gen_cone(float* buf, int idx,
                    float cx, float cy, float cz,
                    float radius, float height, int segs) {
    int verts = 0;
    float tipY = cy + height;

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * 2 * PI;
        float a1 = (float)(i + 1) / segs * 2 * PI;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        /* Нормаль для конуса (приблизительная) */
        float slope = radius / height;
        float ny = slope;
        float nx0 = c0, nz0 = s0;
        float nx1 = c1, nz1 = s1;
        float nl = sqrtf(1 + ny * ny);
        ny /= nl;

        int p = idx + verts * 6;
        /* Боковой треугольник */
        buf[p+ 0]=cx; buf[p+ 1]=tipY; buf[p+ 2]=cz;
        buf[p+ 3]=0; buf[p+ 4]=ny; buf[p+ 5]=0;
        buf[p+ 6]=cx+c1*radius; buf[p+ 7]=cy; buf[p+ 8]=cz+s1*radius;
        buf[p+ 9]=nx1/nl; buf[p+10]=ny; buf[p+11]=nz1/nl;
        buf[p+12]=cx+c0*radius; buf[p+13]=cy; buf[p+14]=cz+s0*radius;
        buf[p+15]=nx0/nl; buf[p+16]=ny; buf[p+17]=nz0/nl;
        verts += 3;

        /* Дно */
        p = idx + verts * 6;
        buf[p+ 0]=cx; buf[p+ 1]=cy; buf[p+ 2]=cz;
        buf[p+ 3]=0; buf[p+ 4]=-1; buf[p+ 5]=0;
        buf[p+ 6]=cx+c0*radius; buf[p+ 7]=cy; buf[p+ 8]=cz+s0*radius;
        buf[p+ 9]=0; buf[p+10]=-1; buf[p+11]=0;
        buf[p+12]=cx+c1*radius; buf[p+13]=cy; buf[p+14]=cz+s1*radius;
        buf[p+15]=0; buf[p+16]=-1; buf[p+17]=0;
        verts += 3;
    }
    return verts;
}

/* ── Утилита: сфера ────────────────────────────── */

#define SPHERE_RINGS 8
#define SPHERE_SEGS  12

static int gen_sphere(float* buf, int idx,
                      float cx, float cy, float cz, float r) {
    int verts = 0;

    for (int ring = 0; ring < SPHERE_RINGS; ring++) {
        float phi0 = (float)ring / SPHERE_RINGS * PI;
        float phi1 = (float)(ring + 1) / SPHERE_RINGS * PI;
        float sp0 = sinf(phi0), cp0 = cosf(phi0);
        float sp1 = sinf(phi1), cp1 = cosf(phi1);

        for (int seg = 0; seg < SPHERE_SEGS; seg++) {
            float th0 = (float)seg / SPHERE_SEGS * 2 * PI;
            float th1 = (float)(seg + 1) / SPHERE_SEGS * 2 * PI;
            float st0 = sinf(th0), ct0 = cosf(th0);
            float st1 = sinf(th1), ct1 = cosf(th1);

            /* 4 точки на сфере */
            float x00 = sp0*ct0, y00 = cp0, z00 = sp0*st0;
            float x10 = sp0*ct1, y10 = cp0, z10 = sp0*st1;
            float x01 = sp1*ct0, y01 = cp1, z01 = sp1*st0;
            float x11 = sp1*ct1, y11 = cp1, z11 = sp1*st1;

            int p = idx + verts * 6;
            /* tri 1 */
            buf[p+ 0]=cx+x00*r; buf[p+ 1]=cy+y00*r; buf[p+ 2]=cz+z00*r;
            buf[p+ 3]=x00; buf[p+ 4]=y00; buf[p+ 5]=z00;
            buf[p+ 6]=cx+x01*r; buf[p+ 7]=cy+y01*r; buf[p+ 8]=cz+z01*r;
            buf[p+ 9]=x01; buf[p+10]=y01; buf[p+11]=z01;
            buf[p+12]=cx+x11*r; buf[p+13]=cy+y11*r; buf[p+14]=cz+z11*r;
            buf[p+15]=x11; buf[p+16]=y11; buf[p+17]=z11;
            verts += 3;

            p = idx + verts * 6;
            /* tri 2 */
            buf[p+ 0]=cx+x00*r; buf[p+ 1]=cy+y00*r; buf[p+ 2]=cz+z00*r;
            buf[p+ 3]=x00; buf[p+ 4]=y00; buf[p+ 5]=z00;
            buf[p+ 6]=cx+x11*r; buf[p+ 7]=cy+y11*r; buf[p+ 8]=cz+z11*r;
            buf[p+ 9]=x11; buf[p+10]=y11; buf[p+11]=z11;
            buf[p+12]=cx+x10*r; buf[p+13]=cy+y10*r; buf[p+14]=cz+z10*r;
            buf[p+15]=x10; buf[p+16]=y10; buf[p+17]=z10;
            verts += 3;
        }
    }
    return verts;
}

/* ═══════════════════════════════════════════════
 *                МОДЕЛИ
 * ═══════════════════════════════════════════════ */

/* Максимум вершин на модель */
#define MODEL_MAX_VERTS 4096

/* ── Дерево: ствол (цилиндр) + 3 конуса (крона) ── */

static int generate_tree(float* buf) {
    int v = 0;
    /* Ствол — коричневый цилиндр */
    v += gen_cylinder(buf, v * 6, 0, 1.0f, 0, 0.15f, 2.0f, CYL_SEGS);
    /* Крона — 3 конуса, поднимающихся */
    v += gen_cone(buf, v * 6, 0, 1.5f, 0, 1.2f, 1.5f, CYL_SEGS);
    v += gen_cone(buf, v * 6, 0, 2.2f, 0, 1.0f, 1.3f, CYL_SEGS);
    v += gen_cone(buf, v * 6, 0, 2.8f, 0, 0.7f, 1.1f, CYL_SEGS);
    return v;
}

/* ── Камень: сплюснутая сфера ──────────────────── */

static int generate_rock(float* buf) {
    int v = gen_sphere(buf, 0, 0, 0.3f, 0, 0.5f);
    /* Сплющиваем по Y */
    for (int i = 0; i < v; i++) {
        buf[i * 6 + 1] *= 0.5f; /* y позиция */
        /* нормаль тоже корректируем */
        buf[i * 6 + 4] *= 2.0f;
        float nx = buf[i*6+3], ny = buf[i*6+4], nz = buf[i*6+5];
        float l = sqrtf(nx*nx+ny*ny+nz*nz);
        if (l > 0.001f) { buf[i*6+3]/=l; buf[i*6+4]/=l; buf[i*6+5]/=l; }
    }
    return v;
}

/* ── Снеговик: 3 сферы + нос (конус) ──────────── */

static int generate_snowman(float* buf) {
    int v = 0;
    /* Тело нижнее */
    v += gen_sphere(buf, v * 6, 0, 0.5f, 0, 0.5f);
    /* Тело среднее */
    v += gen_sphere(buf, v * 6, 0, 1.2f, 0, 0.38f);
    /* Голова */
    v += gen_sphere(buf, v * 6, 0, 1.75f, 0, 0.28f);
    /* Нос-морковка */
    v += gen_cone(buf, v * 6, 0, 1.75f, 0.28f, 0.05f, 0.25f, 8);
    return v;
}

/* ── Домик: основание (куб) + крыша (2 наклонных плоскости) ── */

static int generate_house(float* buf) {
    int v = 0;
    /* Стены */
    v += gen_box(buf, v * 6, 0, 1.0f, 0, 1.5f, 1.0f, 1.2f);
    /* Крыша — два наклонных ската как тонкие кубы */
    /* Левый скат */
    v += gen_box(buf, v * 6, -0.8f, 2.3f, 0, 0.1f, 0.8f, 1.4f);
    /* Правый скат */
    v += gen_box(buf, v * 6, 0.8f, 2.3f, 0, 0.1f, 0.8f, 1.4f);
    /* Конёк */
    v += gen_box(buf, v * 6, 0, 2.8f, 0, 0.15f, 0.15f, 1.5f);
    /* Дверь (углубление) */
    v += gen_box(buf, v * 6, 0, 0.5f, 1.2f, 0.35f, 0.5f, 0.05f);
    return v;
}

/* ── Цвета моделей ─────────────────────────────── */

static void get_model_colors(ModelType type, int part,
                             float* r, float* g, float* b) {
    switch (type) {
        case MODEL_TREE:
            if (part == 0) { *r=0.45f; *g=0.30f; *b=0.15f; } /* ствол */
            else           { *r=0.15f; *g=0.55f; *b=0.15f; } /* хвоя */
            break;
        case MODEL_ROCK:
            *r=0.5f; *g=0.5f; *b=0.52f;
            break;
        case MODEL_SNOWMAN:
            if (part < 3)  { *r=0.95f; *g=0.95f; *b=0.97f; } /* снег */
            else           { *r=0.9f;  *g=0.4f;  *b=0.1f;  } /* нос */
            break;
        case MODEL_HOUSE:
            if (part == 0)      { *r=0.7f;  *g=0.55f; *b=0.35f; } /* стены */
            else if (part <= 2) { *r=0.6f;  *g=0.15f; *b=0.1f;  } /* крыша */
            else if (part == 3) { *r=0.5f;  *g=0.45f; *b=0.3f;  } /* конёк */
            else                { *r=0.25f; *g=0.15f; *b=0.1f;  } /* дверь */
            break;
        default:
            *r=1; *g=0; *b=1;
    }
}

/* ── Генератор VBO для одного типа модели ──────── */

typedef struct {
    GLuint vbo;
    int vertCount;
    int parts;           /* количество частей с разными цветами */
    int partStart[8];    /* начальная вершина каждой части */
    int partCount[8];    /* количество вершин каждой части */
    float partColor[8][3];
} ModelMesh;

static ModelMesh modelMeshes[5]; /* индекс = ModelType */
static bool modelMeshesReady = false;

static void build_model_mesh(ModelType type) {
    float* buf = (float*)malloc(MODEL_MAX_VERTS * 6 * sizeof(float));
    if (!buf) return;

    ModelMesh* mm = &modelMeshes[type];
    memset(mm, 0, sizeof(ModelMesh));

    int totalVerts = 0;

    switch (type) {
        case MODEL_TREE: {
            /* Часть 0: ствол */
            int v0 = gen_cylinder(buf, 0, 0, 1.0f, 0, 0.15f, 2.0f, CYL_SEGS);
            mm->partStart[0] = 0; mm->partCount[0] = v0;
            get_model_colors(type, 0, &mm->partColor[0][0],
                             &mm->partColor[0][1], &mm->partColor[0][2]);
            /* Часть 1: крона */
            int v1 = gen_cone(buf, v0 * 6, 0, 1.5f, 0, 1.2f, 1.5f, CYL_SEGS);
            int v2 = gen_cone(buf, (v0+v1) * 6, 0, 2.2f, 0, 1.0f, 1.3f, CYL_SEGS);
            int v3 = gen_cone(buf, (v0+v1+v2) * 6, 0, 2.8f, 0, 0.7f, 1.1f, CYL_SEGS);
            mm->partStart[1] = v0; mm->partCount[1] = v1+v2+v3;
            get_model_colors(type, 1, &mm->partColor[1][0],
                             &mm->partColor[1][1], &mm->partColor[1][2]);
            totalVerts = v0+v1+v2+v3;
            mm->parts = 2;
        } break;

        case MODEL_ROCK: {
            int v = generate_rock(buf);
            mm->partStart[0] = 0; mm->partCount[0] = v;
            get_model_colors(type, 0, &mm->partColor[0][0],
                             &mm->partColor[0][1], &mm->partColor[0][2]);
            totalVerts = v;
            mm->parts = 1;
        } break;

        case MODEL_SNOWMAN: {
            int v0 = gen_sphere(buf, 0, 0, 0.5f, 0, 0.5f);
            int v1 = gen_sphere(buf, v0*6, 0, 1.2f, 0, 0.38f);
            int v2 = gen_sphere(buf, (v0+v1)*6, 0, 1.75f, 0, 0.28f);
            mm->partStart[0] = 0; mm->partCount[0] = v0+v1+v2;
            get_model_colors(type, 0, &mm->partColor[0][0],
                             &mm->partColor[0][1], &mm->partColor[0][2]);
            int v3 = gen_cone(buf, (v0+v1+v2)*6, 0, 1.75f, 0.28f, 0.05f, 0.25f, 8);
            mm->partStart[1] = v0+v1+v2; mm->partCount[1] = v3;
            get_model_colors(type, 3, &mm->partColor[1][0],
                             &mm->partColor[1][1], &mm->partColor[1][2]);
            totalVerts = v0+v1+v2+v3;
            mm->parts = 2;
        } break;

        case MODEL_HOUSE: {
            int v0 = gen_box(buf, 0, 0, 1.0f, 0, 1.5f, 1.0f, 1.2f);
            mm->partStart[0] = 0; mm->partCount[0] = v0;
            get_model_colors(type, 0, &mm->partColor[0][0],
                             &mm->partColor[0][1], &mm->partColor[0][2]);

            int v1 = gen_box(buf, v0*6, -0.8f, 2.3f, 0, 0.1f, 0.8f, 1.4f);
            int v2 = gen_box(buf, (v0+v1)*6, 0.8f, 2.3f, 0, 0.1f, 0.8f, 1.4f);
            mm->partStart[1] = v0; mm->partCount[1] = v1+v2;
            get_model_colors(type, 1, &mm->partColor[1][0],
                             &mm->partColor[1][1], &mm->partColor[1][2]);

            int v3 = gen_box(buf, (v0+v1+v2)*6, 0, 2.8f, 0, 0.15f, 0.15f, 1.5f);
            mm->partStart[2] = v0+v1+v2; mm->partCount[2] = v3;
            get_model_colors(type, 3, &mm->partColor[2][0],
                             &mm->partColor[2][1], &mm->partColor[2][2]);

            int v4 = gen_box(buf, (v0+v1+v2+v3)*6, 0, 0.5f, 1.2f, 0.35f, 0.5f, 0.05f);
            mm->partStart[3] = v0+v1+v2+v3; mm->partCount[3] = v4;
            get_model_colors(type, 4, &mm->partColor[3][0],
                             &mm->partColor[3][1], &mm->partColor[3][2]);

            totalVerts = v0+v1+v2+v3+v4;
            mm->parts = 4;
        } break;

        default: break;
    }

    mm->vertCount = totalVerts;
    glGenBuffers(1, &mm->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mm->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(totalVerts * 6 * sizeof(float)),
                 buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
}

static void init_all_models(void) {
    if (modelMeshesReady) return;
    build_model_mesh(MODEL_TREE);
    build_model_mesh(MODEL_ROCK);
    build_model_mesh(MODEL_SNOWMAN);
    build_model_mesh(MODEL_HOUSE);
    modelMeshesReady = true;
}

/* ── Расстановка моделей на платформе ──────────── */

static unsigned int model_hash(int x, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + 42);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void place_models(struct engine* eng) {
    eng->modelCount = 0;

    for (int x = 2; x < PLATFORM_SIZE - 2; x += 4)
        for (int z = 2; z < PLATFORM_SIZE - 2; z += 4) {
            if (eng->modelCount >= MAX_MODELS) return;

            unsigned int h = model_hash(x, z);
            if ((h & 7) > 2) continue; /* ~37% шанс */

            struct model_instance* m = &eng->models[eng->modelCount++];
            m->x = (float)x + (float)(h & 3) * 0.25f;
            m->y = (float)PLATFORM_Y + 0.5f;
            m->z = -(float)z - (float)((h >> 2) & 3) * 0.25f;
            m->rotY = (float)(h & 0xFF) / 255.0f * 2 * PI;
            m->scale = 0.8f + (float)((h >> 8) & 0xFF) / 255.0f * 0.5f;

            switch ((h >> 4) & 3) {
                case 0: m->type = MODEL_TREE; break;
                case 1: m->type = MODEL_ROCK; break;
                case 2: m->type = MODEL_SNOWMAN; break;
                case 3: m->type = MODEL_HOUSE; m->scale *= 0.7f; break;
            }
        }
}

/* ── Отрисовка всех моделей ────────────────────── */

static void render_models(struct engine* eng, const float* vpMatrix) {
    glUseProgram(eng->modelProgram);
    glEnable(GL_DEPTH_TEST);

    for (int i = 0; i < eng->modelCount; i++) {
        struct model_instance* mi = &eng->models[i];
        ModelMesh* mm = &modelMeshes[mi->type];
        if (mm->vertCount == 0) continue;

        /* Матрица модели: translate * rotateY * scale */
        float T[16], R[16], S[16], TR[16], TRS[16], mvp[16];
        mat4_translate(T, mi->x, mi->y, mi->z);
        mat4_rotate_y(R, mi->rotY);
        mat4_scale_uniform(S, mi->scale);
        mat4_mul(TR, T, R);
        mat4_mul(TRS, TR, S);
        mat4_mul(mvp, vpMatrix, TRS);

        glUniformMatrix4fv(eng->umMVP, 1, GL_FALSE, mvp);

        glBindBuffer(GL_ARRAY_BUFFER, mm->vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12);
        glEnableVertexAttribArray(1);

        for (int p = 0; p < mm->parts; p++) {
            glUniform3f(eng->umColor,
                        mm->partColor[p][0],
                        mm->partColor[p][1],
                        mm->partColor[p][2]);
            glDrawArrays(GL_TRIANGLES,
                         mm->partStart[p], mm->partCount[p]);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

#endif
