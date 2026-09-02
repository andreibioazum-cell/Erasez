#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>
#include "engine.h"
#include "world.h"
#include "math_utils.h"

#define LOG_TAG "Render"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* ================= ШЕЙДЕРЫ ================= */

static GLuint rb_compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        LOGE("shader error: %s", log);
    }
    return s;
}

static GLuint rb_program(const char* vs, const char* fs) {
    GLuint v = rb_compile(GL_VERTEX_SHADER, vs);
    GLuint f = rb_compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    /* Явно закрепляем атрибуты; несуществующие имена линкер игнорирует. */
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aNorm");
    glBindAttribLocation(p, 2, "aCol");
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LOGE("program error: %s", log);
    }
    return p;
}

static void init_game_programs(struct engine* e) {
    const char* worldVS =
        "attribute vec3 aPos; attribute vec3 aNorm; attribute vec3 aCol;"
        "uniform mat4 uMVP;"
        "varying vec3 vN; varying vec3 vC; varying vec3 vW;"
        "void main(){ vW=aPos; vN=aNorm; vC=aCol;"
        " gl_Position=uMVP*vec4(aPos,1.0); }";
    const char* worldFS =
        "precision mediump float;"
        "varying vec3 vN; varying vec3 vC; varying vec3 vW;"
        "uniform vec3 uCamPos; uniform vec3 uSunDir;"
        "void main(){"
        " vec3 n=normalize(vN);"
        " float dif=max(dot(n,normalize(uSunDir)),0.0);"
        " float light=clamp(0.56+dif*0.62,0.34,1.2);"
        " vec3 col=vC*light;"
        " float d=length(vW-uCamPos);"
        " float fog=clamp((d-24.0)/42.0,0.0,0.96);"
        " vec3 sh=vec3(0.72,0.83,0.94);"
        " gl_FragColor=vec4(mix(col,sh,fog),1.0); }";
    e->worldProgram = rb_program(worldVS, worldFS);

    const char* skyVS =
        "attribute vec2 aPos; varying vec2 vP;"
        "void main(){ vP=aPos; gl_Position=vec4(aPos,0.9999,1.0); }";
    const char* skyFS =
        "precision mediump float;"
        "varying vec2 vP;"
        "uniform vec3 uFwd; uniform vec3 uRight; uniform vec3 uUp;"
        "uniform float uTanX; uniform float uTanY;"
        "uniform vec3 uSunDir;"
        "void main(){"
        " vec3 d=normalize(uFwd + uRight*vP.x*uTanX + uUp*vP.y*uTanY);"
        " vec3 horizon=vec3(0.72,0.83,0.94);"
        " vec3 zenith =vec3(0.28,0.52,0.92);"
        " float h=clamp(d.y*1.7+0.18,0.0,1.0);"
        " vec3 col=mix(horizon,zenith,h*h);"
        " if(d.y<0.0){ vec3 gnd=vec3(0.46,0.56,0.64);"
        "   col=mix(col,gnd,clamp(-d.y*2.2,0.0,1.0)); }"
        " float s=max(dot(d,normalize(uSunDir)),0.0);"
        " col+=vec3(1.0,0.95,0.8)*pow(s,140.0)*2.2;"
        " col+=vec3(1.0,0.97,0.88)*pow(s,10.0)*0.22;"
        " gl_FragColor=vec4(col,1.0); }";
    e->skyProgram = rb_program(skyVS, skyFS);

    const char* uiVS =
        "attribute vec2 aPos; void main(){ gl_Position=vec4(aPos,0.0,1.0); }";
    const char* uiFS =
        "precision mediump float; uniform vec4 col;"
        "void main(){ gl_FragColor=col; }";
    e->uiProgram = rb_program(uiVS, uiFS);

    /* Полноэкранный треугольник для неба */
    float skyVerts[] = { -1, -1, 3, -1, -1, 3 };
    glGenBuffers(1, &e->skyVBO);
    glBindBuffer(GL_ARRAY_BUFFER, e->skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyVerts), skyVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ================= ПОСТРОЕНИЕ МЕША МИРА ================= */

typedef struct { float* d; int n; int cap; } VBuf;

static void vb_need(VBuf* m, int add) {
    if (m->n + add > m->cap) {
        m->cap = m->cap ? m->cap * 2 : 1 << 16;
        while (m->cap < m->n + add) m->cap *= 2;
        m->d = (float*)realloc(m->d, (size_t)m->cap * 9 * sizeof(float));
    }
}

static void vb_vert(VBuf* m, float x, float y, float z,
                    float nx, float ny, float nz,
                    float r, float g, float b) {
    vb_need(m, 1);
    float* p = m->d + (size_t)m->n * 9;
    p[0] = x; p[1] = y; p[2] = z;
    p[3] = nx; p[4] = ny; p[5] = nz;
    p[6] = r; p[7] = g; p[8] = b;
    m->n++;
}

/* Цвет блока по типу и стороне света (верх/низ/бок) */
static void block_color(int type, int axis, int sign,
                        float* r, float* g, float* b) {
    float top = (axis == 1 && sign > 0) ? 1.0f : 0.0f;
    switch (type) {
        case BLOCK_GRASS:
            if (top) { *r = 0.40f; *g = 0.70f; *b = 0.30f; }
            else     { *r = 0.58f; *g = 0.44f; *b = 0.29f; }
            break;
        case BLOCK_DIRT:  *r = 0.60f; *g = 0.45f; *b = 0.30f; break;
        case BLOCK_STONE: *r = 0.56f; *g = 0.57f; *b = 0.61f; break;
        case BLOCK_SAND:  *r = 0.93f; *g = 0.86f; *b = 0.62f; break;
        case BLOCK_SNOW:
            *r = 0.93f; *g = 0.95f; *b = 1.0f;
            if (!top) { *r *= 0.94f; *g *= 0.96f; }
            break;
        case BLOCK_WOOD:
            if (axis == 1) { *r = 0.62f; *g = 0.48f; *b = 0.28f; }
            else           { *r = 0.48f; *g = 0.34f; *b = 0.20f; }
            break;
        case BLOCK_LEAVES:
            *r = 0.21f; *g = 0.50f; *b = 0.16f;
            if (top) { *r *= 1.15f; *g *= 1.15f; }
            break;
        default: *r = 0.9f; *g = 0.2f; *b = 0.9f; break;
    }
}

/* Добавить грань куба. a - ось (0=X,1=Y,2=Z), s - направление (+-1). */
static void push_face(VBuf* m, int bx, int by, int bz, int a, int s,
                      float tint, int type) {
    static const int UAX[3] = {1, 2, 0};   /* u = (a+1)%3  */
    static const int VAX[3] = {2, 0, 1};   /* v = (a+2)%3  */
    static const float NB[3][3] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
    };
    float uu[3] = {0, 0, 0}, vv[3] = {0, 0, 0};
    uu[UAX[a]] = 1.0f;
    vv[VAX[a]] = 1.0f;

    float cx = (float)bx + 0.5f - WORLD_X * 0.5f;
    float cy = (float)by + 0.5f;
    float cz = (float)bz + 0.5f - WORLD_Z * 0.5f;

    float n[3] = { NB[a][0] * s, NB[a][1] * s, NB[a][2] * s };

    float r, g, b;
    block_color(type, a, s, &r, &g, &b);
    r *= tint; g *= tint; b *= tint;
    if (s < 0) { r *= 0.72f; g *= 0.72f; b *= 0.72f; }

    /* 4 угла: (su,sv) = (--, +-, ++, -+) */
    int su[4] = {-1, 1, 1, -1};
    int sv[4] = {-1, -1, 1, 1};

    float c[4][3];
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) {
            float off = 0.5f * ((float)su[i] * uu[k] + (float)sv[i] * vv[k]);
            if (k == 0) c[i][k] = cx + off;
            else if (k == 1) c[i][k] = cy + off;
            else c[i][k] = cz + off;
        }
    }

    if (s > 0) {
        vb_vert(m, c[0][0], c[0][1], c[0][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[1][0], c[1][1], c[1][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[2][0], c[2][1], c[2][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[0][0], c[0][1], c[0][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[2][0], c[2][1], c[2][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[3][0], c[3][1], c[3][2], n[0], n[1], n[2], r, g, b);
    } else {
        vb_vert(m, c[0][0], c[0][1], c[0][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[3][0], c[3][1], c[3][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[2][0], c[2][1], c[2][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[0][0], c[0][1], c[0][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[2][0], c[2][1], c[2][2], n[0], n[1], n[2], r, g, b);
        vb_vert(m, c[1][0], c[1][1], c[1][2], n[0], n[1], n[2], r, g, b);
    }
}

static int mesh_neighbor(const struct engine* e, int bx, int by, int bz,
                         int a, int s) {
    int dx = 0, dy = 0, dz = 0;
    if (a == 0) dx = s; else if (a == 1) dy = s; else dz = s;
    return world_get(e, bx + dx, by + dy, bz + dz) != BLOCK_AIR;
}

/* Полная сборка статичного меша мира (мир не меняется во время игры). */
static void build_world_mesh(struct engine* e) {
    VBuf m;
    m.d = NULL; m.n = 0; m.cap = 0;

    for (int y = 0; y < WORLD_Y; y++) {
        for (int z = 0; z < WORLD_Z; z++) {
            for (int x = 0; x < WORLD_X; x++) {
                int b = world_get(e, x, y, z);
                if (b == BLOCK_AIR) continue;

                float tint = 0.90f + 0.20f * w_rand01(x, z);
                for (int a = 0; a < 3; a++) {
                    for (int s = -1; s <= 1; s += 2) {
                        if (mesh_neighbor(e, x, y, z, a, s)) continue;
                        push_face(&m, x, y, z, a, s, tint, b);
                    }
                }
            }
        }
    }

    if (e->worldVBO) glDeleteBuffers(1, &e->worldVBO);
    glGenBuffers(1, &e->worldVBO);
    glBindBuffer(GL_ARRAY_BUFFER, e->worldVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m.n * 9 * sizeof(float),
                 m.d, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    e->worldVerts = m.n;
    free(m.d);
    LOGI("world mesh verts: %d", e->worldVerts);
}

/* ================= ОТРИСОВКА СЦЕНЫ ================= */

static void draw_sky(struct engine* e, const float* dir,
                     const float* right, const float* up, float aspect) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(e->skyProgram);
    float tf = tanf(GAME_FOV * 0.5f);
    glUniform3f(glGetUniformLocation(e->skyProgram, "uFwd"), dir[0], dir[1], dir[2]);
    glUniform3f(glGetUniformLocation(e->skyProgram, "uRight"), right[0], right[1], right[2]);
    glUniform3f(glGetUniformLocation(e->skyProgram, "uUp"), up[0], up[1], up[2]);
    glUniform1f(glGetUniformLocation(e->skyProgram, "uTanX"), tf * aspect);
    glUniform1f(glGetUniformLocation(e->skyProgram, "uTanY"), tf);
    glUniform3f(glGetUniformLocation(e->skyProgram, "uSunDir"), 0.45f, 0.68f, 0.58f);
    glBindBuffer(GL_ARRAY_BUFFER, e->skyVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static void draw_world(struct engine* e, const float* vp,
                       float eyeX, float eyeY, float eyeZ) {
    glUseProgram(e->worldProgram);
    glUniformMatrix4fv(glGetUniformLocation(e->worldProgram, "uMVP"),
                       1, GL_FALSE, vp);
    glUniform3f(glGetUniformLocation(e->worldProgram, "uCamPos"), eyeX, eyeY, eyeZ);
    glUniform3f(glGetUniformLocation(e->worldProgram, "uSunDir"),
                0.45f, 0.68f, 0.58f);
    glBindBuffer(GL_ARRAY_BUFFER, e->worldVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 36, (void*)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 36, (void*)24);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, e->worldVerts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void render_scene(struct engine* e) {
    int w = e->width, h = e->height;
    if (w <= 0 || h <= 0) return;
    float aspect = (float)w / (float)h;

    /* Направление взгляда */
    float cp = cosf(e->pitch), sp = sinf(e->pitch);
    float fx = sinf(e->yaw), fz = -cosf(e->yaw);
    float dir[3] = { fx * cp, sp, fz * cp };

    /* Лёгкое покачивание камеры при ходьбе */
    float bob = 0.0f;
    if (e->onGround && e->isMoving)
        bob = 0.05f * sinf(e->walkPhase * 2.0f);

    float eyeX = e->px;
    float eyeY = e->py + EYE_H + bob;
    float eyeZ = e->pz;

    /* Базис камеры */
    float right[3] = { cosf(e->yaw), 0.0f, sinf(e->yaw) };
    float up[3];
    up[0] = -right[2] * dir[1];
    up[1] = right[2] * dir[0] - right[0] * dir[2];
    up[2] = right[0] * dir[1];
    float ul = sqrtf(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    if (ul < 0.0001f) { up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f; ul = 1.0f; }
    up[0] /= ul; up[1] /= ul; up[2] /= ul;

    /* Небо */
    glDepthMask(GL_FALSE);
    draw_sky(e, dir, right, up, aspect);

    /* Матрицы */
    float proj[16], view[16], vp[16];
    mat4_perspective(proj, GAME_FOV, aspect, 0.1f, 320.0f);
    float tgt[3] = { eyeX + dir[0], eyeY + dir[1], eyeZ + dir[2] };
    mat4_lookat_pos(view, eyeX, eyeY, eyeZ, tgt[0], tgt[1], tgt[2]);
    mat4_mul(vp, proj, view);

    /* Мир */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    draw_world(e, vp, eyeX, eyeY, eyeZ);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
}

/* ================= UI (2D) ================= */

static void ui_rect(struct engine* e, float cx, float cy, float hw, float hh,
                    int sw, int sh, float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2.0f - 1.0f, ny = 1.0f - (cy / sh) * 2.0f;
    float rw = (hw / sw) * 2.0f, rh = (hh / sh) * 2.0f;
    float v[] = { nx - rw, ny - rh, nx + rw, ny - rh, nx + rw, ny + rh,
                  nx - rw, ny - rh, nx + rw, ny + rh, nx - rw, ny + rh };
    glUseProgram(e->uiProgram);
    glUniform4f(glGetUniformLocation(e->uiProgram, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void ui_tri(struct engine* e, float cx, float cy, float sx, float sy,
                   int dir, int sw, int sh,
                   float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2 - 1, ny = 1 - (cy / sh) * 2;
    float ax = (sx / sw) * 2, ay = (sy / sh) * 2;
    float v[6];
    if (dir == 1) {  /* вверх */
        v[0] = nx; v[1] = ny + ay;
        v[2] = nx - ax; v[3] = ny - ay * 0.6f;
        v[4] = nx + ax; v[5] = ny - ay * 0.6f;
    } else {         /* вниз */
        v[0] = nx; v[1] = ny - ay;
        v[2] = nx - ax; v[3] = ny + ay * 0.6f;
        v[4] = nx + ax; v[5] = ny + ay * 0.6f;
    }
    glUseProgram(e->uiProgram);
    glUniform4f(glGetUniformLocation(e->uiProgram, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void ui_circle(struct engine* e, float cx, float cy, float r, int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2 - 1, ndcY = 1 - (cy / h) * 2;
    float rx = (r / w) * 2, ry = (r / h) * 2;
    float verts[(24 + 2) * 2];
    int segs = 24;
    verts[0] = ndcX; verts[1] = ndcY;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        verts[(i + 1) * 2] = ndcX + cosf(a) * rx;
        verts[(i + 1) * 2 + 1] = ndcY + sinf(a) * ry;
    }
    glUseProgram(e->uiProgram);
    glUniform4f(glGetUniformLocation(e->uiProgram, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs + 2);
}

static void ui_ring(struct engine* e, float cx, float cy, float r, float thick,
                    int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2 - 1, ndcY = 1 - (cy / h) * 2;
    float rxo = (r / w) * 2, ryo = (r / h) * 2;
    float rxi = ((r - thick) / w) * 2, ryi = ((r - thick) / h) * 2;
    float verts[(32 + 1) * 4];
    int segs = 32;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI, c = cosf(a), s = sinf(a);
        verts[i * 4] = ndcX + c * rxo; verts[i * 4 + 1] = ndcY + s * ryo;
        verts[i * 4 + 2] = ndcX + c * rxi; verts[i * 4 + 3] = ndcY + s * ryi;
    }
    glUseProgram(e->uiProgram);
    glUniform4f(glGetUniformLocation(e->uiProgram, "col"), cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs + 1) * 2);
}

static void ui_controls(struct engine* e) {
    int sw = e->width, sh = e->height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Перекрестие */
    float cx = sw * 0.5f, cy = sh * 0.5f;
    ui_rect(e, cx - 8, cy, 2.5f, 1.2f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(e, cx + 6, cy, 2.5f, 1.2f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(e, cx, cy - 8, 1.2f, 2.5f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(e, cx, cy + 6, 1.2f, 2.5f, sw, sh, 1, 1, 1, 0.85f);

    /* Джойстик */
    float jx = JOY_OFFSET, jy = sh - JOY_OFFSET;
    ui_circle(e, jx, jy, JOY_RADIUS, sw, sh, 0.08f, 0.10f, 0.14f, 0.30f);
    ui_ring(e, jx, jy, JOY_RADIUS, 3, sw, sh, 1, 1, 1, 0.35f);
    ui_circle(e, jx + e->moveDirX * JOY_RADIUS * 0.6f,
              jy + e->moveDirZ * JOY_RADIUS * 0.6f,
              STICK_RADIUS, sw, sh, 1, 1, 1, 0.55f);

    /* Прыжок / вверх (правый нижний угол) */
    float bx = sw - BTN_OFFSET, by = sh - BTN_OFFSET;
    ui_circle(e, bx, by, BTN_RADIUS, sw, sh, 0.08f, 0.10f, 0.14f, 0.32f);
    ui_ring(e, bx, by, BTN_RADIUS, 3, sw, sh, 1, 1, 1, 0.4f);
    ui_tri(e, bx, by, 16, 18, 1, sw, sh, 1, 1, 1, 0.95f);

    if (e->flying) {
        /* Спуск - левее прыжка */
        float dx2 = bx - BTN_GAP, dy2 = by;
        ui_circle(e, dx2, dy2, BTN_RADIUS * 0.9f, sw, sh, 0.08f, 0.10f, 0.14f, 0.32f);
        ui_ring(e, dx2, dy2, BTN_RADIUS * 0.9f, 3, sw, sh, 1, 1, 1, 0.4f);
        ui_tri(e, dx2, dy2, 15, 17, -1, sw, sh, 1, 1, 1, 0.95f);
    }

    /* Переключатель полёта - над прыжком */
    float fx2 = bx, fy2 = by - BTN_GAP;
    ui_circle(e, fx2, fy2, FLY_RADIUS, sw, sh, 0.10f, 0.18f, 0.30f,
              e->flying ? 0.75f : 0.35f);
    ui_ring(e, fx2, fy2, FLY_RADIUS, 3, sw, sh, 1, 1, 1, 0.55f);
    /* иконка: две стрелки (вверх/вниз) */
    ui_tri(e, fx2, fy2 - 4, 9, 9, 1, sw, sh, 1, 1, 1, 0.95f);
    ui_tri(e, fx2, fy2 + 4, 9, 9, -1, sw, sh, 1, 1, 1, 0.95f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

static void draw_menu(struct engine* e) {
    int sw = e->width, sh = e->height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ui_rect(e, sw / 2.0f, sh / 2.0f, sw / 2.0f, sh / 2.0f, sw, sh,
            0.10f, 0.13f, 0.18f, 0.92f);

    /* Псевдо-логотип: горы из блоков */
    float ly = sh * 0.30f;
    float bx = sw / 2.0f;
    ui_rect(e, bx, ly + 24, 150, 34, sw, sh, 0.16f, 0.24f, 0.34f, 1.0f);
    for (int i = -3; i <= 3; i++) {
        float hgt = 8.0f + (i < 0 ? -i * 4.0f : i * 4.0f) * 0.5f;
        ui_rect(e, bx + i * 20, ly + 24 - hgt * 0.5f - 17, 10, hgt, sw, sh,
                0.42f, 0.72f, 0.32f, 1.0f);
    }

    /* Кнопка ИГРАТЬ */
    float playX = bx, playY = sh * 0.58f;
    ui_circle(e, playX, playY, 56, sw, sh, 0.13f, 0.55f, 0.22f, 1.0f);
    ui_ring(e, playX, playY, 56, 4, sw, sh, 0.45f, 1.0f, 0.5f, 1.0f);
    ui_tri(e, playX, playY, 18, 20, 1, sw, sh, 1, 1, 1, 1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void draw_ui(struct engine* e) {
    if (e->gameState == STATE_MENU) draw_menu(e);
    else ui_controls(e);
}

#endif
