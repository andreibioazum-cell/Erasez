#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>
#include "engine.h"
#include "math_utils.h"

#define LOG_TAG "Render"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

GLuint uiProg = 0;

/* ============= ТЕКСТУРЫ ============= */
static GLuint make_checker_texture(void) {
    int sz = 64;
    unsigned char* px = (unsigned char*)malloc(sz * sz * 4);
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int i = (y * sz + x) * 4;
            int check = ((x / 8) + (y / 8)) % 2;
            if (check) {
                px[i] = 90; px[i+1] = 170; px[i+2] = 90; px[i+3] = 255;
            } else {
                px[i] = 70; px[i+1] = 140; px[i+2] = 70; px[i+3] = 255;
            }
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    free(px);
    return tex;
}

static void init_textures(struct engine* eng) {
    eng->texPlatform = make_checker_texture();
}

/* ============= ШЕЙДЕРЫ ============= */
void init_color_shader(struct engine* eng) {
    const char* vS =
        "attribute vec3 aPos;"
        "attribute vec3 aNorm;"
        "varying vec3 vNorm;"
        "varying vec3 vWorldPos;"
        "uniform mat4 uMVP;"
        "uniform mat4 uModel;"
        "void main(){"
        "  gl_Position = uMVP * vec4(aPos, 1.0);"
        "  vNorm = mat3(uModel) * aNorm;"
        "  vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;"
        "}";
    const char* fS =
        "precision mediump float;"
        "varying vec3 vNorm;"
        "varying vec3 vWorldPos;"
        "uniform vec3 uColor;"
        "uniform vec3 uCamPos;"
        "void main(){"
        "  vec3 n = normalize(vNorm);"
        "  vec3 sun = normalize(vec3(0.4, 0.8, 0.3));"
        "  float diff = max(dot(n, sun), 0.0);"
        "  float amb = 0.45;"
        "  float fb = 0.0;"
        "  if(n.y > 0.5) fb = 0.1;"
        "  if(n.y < -0.5) fb = -0.15;"
        "  if(abs(n.x) > 0.5) fb = -0.05;"
        "  if(abs(n.z) > 0.5) fb = -0.08;"
        "  float light = clamp(amb + diff * 0.55 + fb, 0.2, 1.0);"
        "  vec3 lit = uColor * light;"
        "  float dist = length((vWorldPos - uCamPos).xz);"
        "  float fog = clamp((dist - 30.0) / 35.0, 0.0, 0.85);"
        "  gl_FragColor = vec4(mix(lit, vec3(0.53, 0.81, 0.98), fog), 1.0);"
        "}";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    eng->colorProgram = glCreateProgram();
    glAttachShader(eng->colorProgram, vs);
    glAttachShader(eng->colorProgram, fs);
    glBindAttribLocation(eng->colorProgram, 0, "aPos");
    glBindAttribLocation(eng->colorProgram, 1, "aNorm");
    glLinkProgram(eng->colorProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void init_ui_shader(void) {
    const char* vS =
        "attribute vec2 aPos;"
        "void main(){ gl_Position = vec4(aPos, 0.0, 1.0); }";
    const char* fS =
        "precision mediump float;"
        "uniform vec4 col;"
        "void main(){ gl_FragColor = col; }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs);
    glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

/* ============= КУБ ============= */
static const float cube_data[] = {
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f, 0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f, 0.5f, 0.5f, 1,0,0,  0.5f, 0.5f,-0.5f, 1,0,0,
    -0.5f,-0.5f, 0.5f,-1,0,0, -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f, 0.5f,-0.5f,-1,0,0,
    -0.5f,-0.5f, 0.5f,-1,0,0, -0.5f, 0.5f,-0.5f,-1,0,0, -0.5f, 0.5f, 0.5f,-1,0,0,
    -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0,
    -0.5f, 0.5f, 0.5f, 0,1,0,  0.5f, 0.5f,-0.5f, 0,1,0, -0.5f, 0.5f,-0.5f, 0,1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
    -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0,
    -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f, 0.5f, 0.5f, 0,0,1,
     0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1,
     0.5f,-0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,
};

static GLuint cubeVBO = 0;

void init_cube_vbo(void) {
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data), cube_data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void draw_cube(struct engine* eng, float* vpMat,
                      float tx, float ty, float tz,
                      float sx, float sy, float sz,
                      float rotY, float rotX, float rotZ,
                      float cr, float cg, float cb,
                      float eyeX, float eyeY, float eyeZ) {
    float T[16], S[16], RY[16], RX[16], RZ[16], tmp1[16], tmp2[16], model[16], mvp[16];

    mat4_translate(T, tx, ty, tz);
    mat4_scale(S, sx, sy, sz);
    mat4_rotate_y(RY, rotY);
    mat4_rotate_x(RX, rotX);
    mat4_rotate_z(RZ, rotZ);

    mat4_mul(tmp1, RY, RX);
    mat4_mul(tmp2, tmp1, RZ);
    mat4_mul(tmp1, tmp2, S);
    mat4_mul(model, T, tmp1);
    mat4_mul(mvp, vpMat, model);

    glUseProgram(eng->colorProgram);
    glUniformMatrix4fv(glGetUniformLocation(eng->colorProgram, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(eng->colorProgram, "uModel"), 1, GL_FALSE, model);
    glUniform3f(glGetUniformLocation(eng->colorProgram, "uColor"), cr, cg, cb);
    glUniform3f(glGetUniformLocation(eng->colorProgram, "uCamPos"), eyeX, eyeY, eyeZ);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ============= ПЕРСОНАЖ ============= */
static void render_character(struct engine* eng, float* vpMat,
                              float eyeX, float eyeY, float eyeZ) {
    float px = eng->playerPos[0];
    float py = eng->playerPos[1];
    float pz = eng->playerPos[2];
    float rot = eng->playerRot;
    float legA = eng->animLegAngle;
    float armA = eng->animArmAngle;

    /* Тело */
    draw_cube(eng, vpMat,
              px, py + 0.9f, pz,
              0.5f, 0.6f, 0.25f,
              rot, 0, 0,
              0.2f, 0.6f, 0.85f,
              eyeX, eyeY, eyeZ);

    /* Голова */
    draw_cube(eng, vpMat,
              px, py + 1.45f, pz,
              0.4f, 0.4f, 0.4f,
              rot, 0, 0,
              1.0f, 0.85f, 0.65f,
              eyeX, eyeY, eyeZ);

    /* Левая рука */
    float laOX = -cosf(rot) * 0.38f;
    float laOZ = -sinf(rot) * 0.38f;
    draw_cube(eng, vpMat,
              px + laOX, py + 0.85f, pz + laOZ,
              0.15f, 0.55f, 0.15f,
              rot, armA, 0,
              0.2f, 0.6f, 0.85f,
              eyeX, eyeY, eyeZ);

    /* Правая рука */
    float raOX = cosf(rot) * 0.38f;
    float raOZ = sinf(rot) * 0.38f;
    draw_cube(eng, vpMat,
              px + raOX, py + 0.85f, pz + raOZ,
              0.15f, 0.55f, 0.15f,
              rot, -armA, 0,
              0.2f, 0.6f, 0.85f,
              eyeX, eyeY, eyeZ);

    /* Левая нога */
    float llOX = -cosf(rot) * 0.13f;
    float llOZ = -sinf(rot) * 0.13f;
    draw_cube(eng, vpMat,
              px + llOX, py + 0.3f, pz + llOZ,
              0.18f, 0.55f, 0.18f,
              rot, legA, 0,
              0.15f, 0.35f, 0.15f,
              eyeX, eyeY, eyeZ);

    /* Правая нога */
    float rlOX = cosf(rot) * 0.13f;
    float rlOZ = sinf(rot) * 0.13f;
    draw_cube(eng, vpMat,
              px + rlOX, py + 0.3f, pz + rlOZ,
              0.18f, 0.55f, 0.18f,
              rot, -legA, 0,
              0.15f, 0.35f, 0.15f,
              eyeX, eyeY, eyeZ);
}

/* ============= ПЛАТФОРМА ============= */
static void render_platform(struct engine* eng, float* vpMat,
                             float eyeX, float eyeY, float eyeZ) {
    /* Основная платформа */
    draw_cube(eng, vpMat,
              0, -0.5f, 0,
              PLATFORM_SIZE, 1.0f, PLATFORM_SIZE,
              0, 0, 0,
              0.35f, 0.75f, 0.35f,
              eyeX, eyeY, eyeZ);

    /* Декоративные кубики */
    draw_cube(eng, vpMat,
              10, 0.5f, 10,
              1.0f, 1.0f, 1.0f,
              0, 0, 0,
              0.85f, 0.2f, 0.2f,
              eyeX, eyeY, eyeZ);

    draw_cube(eng, vpMat,
              -8, 0.5f, 5,
              1.0f, 1.0f, 1.0f,
              0.5f, 0, 0,
              0.2f, 0.2f, 0.85f,
              eyeX, eyeY, eyeZ);

    draw_cube(eng, vpMat,
              5, 0.5f, -12,
              1.0f, 1.0f, 1.0f,
              0.8f, 0, 0,
              0.85f, 0.85f, 0.2f,
              eyeX, eyeY, eyeZ);

    draw_cube(eng, vpMat,
              -15, 1.0f, -8,
              2.0f, 2.0f, 2.0f,
              0.3f, 0, 0,
              0.85f, 0.5f, 0.2f,
              eyeX, eyeY, eyeZ);

    draw_cube(eng, vpMat,
              12, 0.75f, -5,
              1.5f, 1.5f, 1.5f,
              1.0f, 0, 0,
              0.6f, 0.2f, 0.7f,
              eyeX, eyeY, eyeZ);

    draw_cube(eng, vpMat,
              -5, 0.5f, 15,
              1.0f, 1.0f, 1.0f,
              0, 0, 0,
              0.2f, 0.7f, 0.7f,
              eyeX, eyeY, eyeZ);
}

/* ============= РЕНДЕР СЦЕНЫ ============= */
void render_world(struct engine* eng) {
    glEnable(GL_DEPTH_TEST);

    float camY = eng->camRotY;
    float eyeX = eng->playerPos[0] - sinf(camY) * CAM_DIST;
    float eyeY = eng->playerPos[1] + CAM_HEIGHT;
    float eyeZ = eng->playerPos[2] + cosf(camY) * CAM_DIST;
    float tgtX = eng->playerPos[0];
    float tgtY = eng->playerPos[1] + 1.0f;
    float tgtZ = eng->playerPos[2];

    float proj[16], view[16], vp[16];
    mat4_perspective(proj, GAME_FOV, (float)eng->width / (float)eng->height, 0.1f, 200.0f);
    mat4_lookat_pos(view, eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ);
    mat4_mul(vp, proj, view);

    render_platform(eng, vp, eyeX, eyeY, eyeZ);
    render_character(eng, vp, eyeX, eyeY, eyeZ);
}

/* ============= UI УТИЛИТЫ ============= */
static void draw_rect_ui(float cx, float cy, float hw, float hh, int sw, int sh,
                          float cr, float cg, float cb, float ca) {
    float nx = (cx/sw)*2.0f-1.0f, ny = 1.0f-(cy/sh)*2.0f;
    float rw = (hw/sw)*2.0f, rh = (hh/sh)*2.0f;
    float v[] = {nx-rw,ny-rh, nx+rw,ny-rh, nx+rw,ny+rh,
                 nx-rw,ny-rh, nx+rw,ny+rh, nx-rw,ny+rh};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,6);
}

static void draw_ring_ui(float cx, float cy, float r, float thick,
                          int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2;
    float rxo = (r/w)*2, ryo = (r/h)*2;
    float rxi = ((r-thick)/w)*2, ryi = ((r-thick)/h)*2;
    float verts[(32+1)*4];
    int segs = 32;
    for(int i=0;i<=segs;i++){
        float a = (float)i/segs*2*PI;
        float c = cosf(a), s = sinf(a);
        verts[i*4] = ndcX + c*rxo;
        verts[i*4+1] = ndcY + s*ryo;
        verts[i*4+2] = ndcX + c*rxi;
        verts[i*4+3] = ndcY + s*ryi;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP,0,(segs+1)*2);
}

static void draw_circle_ui(float cx, float cy, float r, int w, int h,
                            float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2;
    float rx = (r/w)*2, ry = (r/h)*2;
    float verts[(24+2)*2];
    int segs = 24;
    verts[0] = ndcX; verts[1] = ndcY;
    for(int i=0;i<=segs;i++){
        float a = (float)i/segs*2*PI;
        verts[(i+1)*2] = ndcX + cosf(a)*rx;
        verts[(i+1)*2+1] = ndcY + sinf(a)*ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"), cr,cg,cb,ca);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN,0,segs+2);
}

static void draw_border_ui(float cx, float cy, float hw, float hh, float t,
                            int sw, int sh, float cr, float cg, float cb, float ca) {
    draw_rect_ui(cx, cy-hh+t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx, cy+hh-t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx-hw+t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx+hw-t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
}

/* ============= МЕНЮ ============= */
void draw_menu(struct engine* eng) {
    int sw = eng->width, sh = eng->height;

    draw_rect_ui(sw/2.0f, sh/2.0f, sw/2.0f, sh/2.0f, sw, sh, 0.15f,0.15f,0.2f, 1);

    float titleY = sh * 0.30f;
    draw_rect_ui(sw/2.0f, titleY, 140, 35, sw, sh, 0.3f,0.5f,0.9f, 0.9f);
    draw_border_ui(sw/2.0f, titleY, 140, 35, 3, sw, sh, 0.5f,0.7f,1.0f, 1);

    float playX = sw / 2.0f, playY = sh * 0.55f;
    draw_rect_ui(playX, playY, 100, 40, sw, sh, 0.2f,0.65f,0.2f, 0.9f);
    draw_border_ui(playX, playY, 100, 40, 3, sw, sh, 0.4f,1.0f,0.4f, 1);

    float pnx = (playX/sw)*2-1, pny = 1-(playY/sh)*2;
    float pax = (22.0f/sw)*2, pay = (22.0f/sh)*2;
    float play[] = {pnx-pax,pny+pay, pnx-pax,pny-pay, pnx+pax,pny};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"),1,1,1,1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,play);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,3);
}

/* ============= ИГРОВОЙ UI ============= */
void draw_ui(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (eng->gameState == STATE_MENU) {
        draw_menu(eng);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    float jx = JOY_X_OFFSET, jy = sh - JOY_Y_OFFSET;
    draw_ring_ui(jx, jy, JOY_RADIUS, 3, sw, sh, 1,1,1,0.5f);
    draw_circle_ui(jx + eng->moveDirX*JOY_RADIUS*0.6f,
                   jy + eng->moveDirZ*JOY_RADIUS*0.6f,
                   STICK_RADIUS, sw, sh, 1,1,1,0.6f);

    float bx = sw - JUMP_BTN_OFFSET, by = sh - JUMP_BTN_OFFSET;
    draw_ring_ui(bx, by, JUMP_BTN_SIZE, 3, sw, sh, 1,1,1,0.5f);

    float as = JUMP_BTN_SIZE*0.3f;
    float anx=(bx/sw)*2-1, any=1-(by/sh)*2;
    float aax=(as/sw)*2, aay=(as/sh)*2;
    float arrow[] = {anx, any+aay, anx-aax, any-aay*0.5f, anx+aax, any-aay*0.5f};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg,"col"),1,1,1,0.6f);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,arrow);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,3);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
