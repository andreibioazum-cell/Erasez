#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

/* ── Процедурная текстура ───────────────────── */

static GLuint make_checker_texture(void) {
    /* Серо-белая шахматка 16x16 */
    unsigned char pixels[16 * 16 * 4];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int i = (y * 16 + x) * 4;
            unsigned char c = ((x / 4 + y / 4) % 2 == 0) ? 200 : 160;
            pixels[i] = c;
            pixels[i + 1] = c;
            pixels[i + 2] = c;
            pixels[i + 3] = 255;
        }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return tex;
}

static void init_textures(struct engine* eng) {
    eng->texPlatform = make_checker_texture();
}

/* ── Меш ────────────────────────────────────── */

static void push_quad(float* buf, int* idx,
    float x0, float y0, float z0, float u0, float v0,
    float x1, float y1, float z1, float u1, float v1,
    float x2, float y2, float z2, float u2, float v2,
    float x3, float y3, float z3, float u3, float v3,
    float nx, float ny, float nz) {
    float vd[6][8] = {
        {x0,y0,z0,u0,v0,nx,ny,nz}, {x1,y1,z1,u1,v1,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz}, {x0,y0,z0,u0,v0,nx,ny,nz},
        {x2,y2,z2,u2,v2,nx,ny,nz}, {x3,y3,z3,u3,v3,nx,ny,nz}
    };
    memcpy(&buf[*idx], vd, sizeof(vd));
    *idx += 48;
}

static void rebuild_vbo(struct engine* eng) {
    update_faces(eng);

    int fc = 0;
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (f & FACE_XP) fc++;
                if (f & FACE_XN) fc++;
                if (f & FACE_YP) fc++;
                if (f & FACE_YN) fc++;
                if (f & FACE_ZP) fc++;
                if (f & FACE_ZN) fc++;
            }

    eng->visibleFaceCount = fc;
    if (fc == 0) { eng->meshDirty = false; return; }

    int cnt = fc * 6 * 8;
    float* buf = (float*)malloc((size_t)cnt * sizeof(float));
    if (!buf) return;

    int idx = 0;
    for (int x = 0; x < WORLD_BUF; x++)
        for (int y = 0; y < CHUNK_H; y++)
            for (int z = 0; z < WORLD_BUF; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (!f) continue;
                float bx = (float)x, by = (float)y, bz = -(float)z;
                float x0 = bx - .5f, x1 = bx + .5f;
                float y0 = by - .5f, y1 = by + .5f;
                float z0 = bz - .5f, z1 = bz + .5f;

                if (f & FACE_XP) push_quad(buf, &idx,
                    x1,y0,z0,0,1, x1,y0,z1,1,1, x1,y1,z1,1,0, x1,y1,z0,0,0, 1,0,0);
                if (f & FACE_XN) push_quad(buf, &idx,
                    x0,y0,z1,0,1, x0,y0,z0,1,1, x0,y1,z0,1,0, x0,y1,z1,0,0, -1,0,0);
                if (f & FACE_YP) push_quad(buf, &idx,
                    x0,y1,z1,0,0, x1,y1,z1,1,0, x1,y1,z0,1,1, x0,y1,z0,0,1, 0,1,0);
                if (f & FACE_YN) push_quad(buf, &idx,
                    x0,y0,z0,0,0, x1,y0,z0,1,0, x1,y0,z1,1,1, x0,y0,z1,0,1, 0,-1,0);
                if (f & FACE_ZP) push_quad(buf, &idx,
                    x1,y0,z0,0,1, x0,y0,z0,1,1, x0,y1,z0,1,0, x1,y1,z0,0,0, 0,0,-1);
                if (f & FACE_ZN) push_quad(buf, &idx,
                    x0,y0,z1,0,1, x1,y0,z1,1,1, x1,y1,z1,1,0, x0,y1,z1,0,0, 0,0,1);
            }

    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cnt * sizeof(float)),
                 buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
    eng->meshDirty = false;
}

static void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_vbo(eng);
    if (eng->visibleFaceCount == 0) return;

    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);

    glUniform3f(eng->uCamPos,
                eng->camPos[0], eng->camPos[1], eng->camPos[2]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texPlatform);
    glUniform1i(eng->uTex, 0);

    float proj[16], view[16], mvp[16];
    mat4_perspective(proj, GAME_FOV,
                     (float)eng->width / (float)eng->height,
                     0.1f, 200.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);
    glUniformMatrix4fv(eng->uMVP, 1, GL_FALSE, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 32, (void*)20);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ── UI ─────────────────────────────────────── */

static GLuint uiProg = 0;
static GLint uColLoc = -1;

static void init_ui_shader(void) {
    const char* vS = "attribute vec4 p; void main(){ gl_Position=p; }";
    const char* fS =
        "precision mediump float; uniform vec4 col;"
        "void main(){ gl_FragColor=col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs); glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs); glDeleteShader(fs);
    uColLoc = glGetUniformLocation(uiProg, "col");
}

static void draw_rect(float cx, float cy, float hw, float hh,
                      int sw, int sh,
                      float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2 - 1, ny = 1 - (cy / sh) * 2;
    float rw = (hw / sw) * 2, rh = (hh / sh) * 2;
    float v[] = {
        nx - rw, ny - rh, nx + rw, ny - rh, nx + rw, ny + rh,
        nx - rw, ny - rh, nx + rw, ny + rh, nx - rw, ny + rh
    };
    glUseProgram(uiProg);
    glUniform4f(uColLoc, cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#define RING_SEGS 32

static void draw_ring(float cx, float cy, float r, float thick,
                      int w, int h,
                      float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2 - 1, ndcY = 1 - (cy / h) * 2;
    float rxo = (r / w) * 2, ryo = (r / h) * 2;
    float rxi = ((r - thick) / w) * 2, ryi = ((r - thick) / h) * 2;
    float verts[(RING_SEGS + 1) * 4];
    for (int i = 0; i <= RING_SEGS; i++) {
        float a = (float)i / RING_SEGS * 2 * PI;
        float c = cosf(a), s = sinf(a);
        verts[i * 4 + 0] = ndcX + c * rxo;
        verts[i * 4 + 1] = ndcY + s * ryo;
        verts[i * 4 + 2] = ndcX + c * rxi;
        verts[i * 4 + 3] = ndcY + s * ryi;
    }
    glUseProgram(uiProg);
    glUniform4f(uColLoc, cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (RING_SEGS + 1) * 2);
}

#define CIRCLE_SEGS 24

static void draw_circle(float cx, float cy, float r,
                        int w, int h,
                        float cr, float cg, float cb, float ca) {
    float ndcX = (cx / w) * 2 - 1, ndcY = 1 - (cy / h) * 2;
    float rx = (r / w) * 2, ry = (r / h) * 2;
    float verts[(CIRCLE_SEGS + 2) * 2];
    verts[0] = ndcX; verts[1] = ndcY;
    for (int i = 0; i <= CIRCLE_SEGS; i++) {
        float a = (float)i / CIRCLE_SEGS * 2 * PI;
        verts[(i + 1) * 2] = ndcX + cosf(a) * rx;
        verts[(i + 1) * 2 + 1] = ndcY + sinf(a) * ry;
    }
    glUseProgram(uiProg);
    glUniform4f(uColLoc, cr, cg, cb, ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGS + 2);
}

static void draw_menu(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    /* Фон */
    draw_rect(sw / 2.0f, sh / 2.0f, sw / 2.0f, sh / 2.0f,
              sw, sh, 0.15f, 0.15f, 0.2f, 1);
    /* "TAP TO START" — полоса */
    draw_rect(sw / 2.0f, sh / 2.0f, 120, 25, sw, sh,
              0.3f, 0.6f, 1.0f, 0.9f);
    /* Стрелка */
    float nx = 0, ny = 0;
    float ax = (20.0f / sw) * 2, ay = (20.0f / sh) * 2;
    float tri[] = { nx - ax, ny + ay, nx - ax, ny - ay, nx + ax, ny };
    glUseProgram(uiProg);
    glUniform4f(uColLoc, 1, 1, 1, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void draw_ui(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Прицел */
    float cx = sw * 0.5f, cy = sh * 0.5f;
    draw_rect(cx, cy, 12, 1.5f, sw, sh, 1, 1, 1, 0.8f);
    draw_rect(cx, cy, 1.5f, 12, sw, sh, 1, 1, 1, 0.8f);

    /* Джойстик */
    float jx = JOY_X_OFFSET, jy = sh - JOY_Y_OFFSET;
    draw_ring(jx, jy, JOY_RADIUS, 3, sw, sh, 1, 1, 1, 0.5f);
    draw_circle(jx + eng->moveDirX * JOY_RADIUS * 0.7f,
                jy + eng->moveDirZ * JOY_RADIUS * 0.7f,
                STICK_RADIUS, sw, sh, 1, 1, 1, 0.5f);

    /* Прыжок */
    float bx = sw - JUMP_BTN_OFFSET, by = sh - JUMP_BTN_OFFSET;
    draw_ring(bx, by, JUMP_BTN_SIZE, 3, sw, sh, 1, 1, 1, 0.5f);
    float as = JUMP_BTN_SIZE * 0.35f;
    float anx = (bx / sw) * 2 - 1, any = 1 - (by / sh) * 2;
    float aax = (as / sw) * 2, aay = (as / sh) * 2;
    float arrow[] = {
        anx, any + aay,
        anx - aax, any - aay * 0.5f,
        anx + aax, any - aay * 0.5f
    };
    glUseProgram(uiProg);
    glUniform4f(uColLoc, 1, 1, 1, 0.6f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
