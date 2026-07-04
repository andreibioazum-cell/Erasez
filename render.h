 #ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include "engine.h"
#include "math_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOG_TAG "Render"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

GLuint uiProg = 0;

/* ============= ТЕКСТУРЫ ============= */
static GLuint load_texture_asset(struct android_app* app, const char* filename) {
    AAssetManager* mgr = app->activity->assetManager;
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) { LOGE("Cannot open %s", filename); return 0; }
    size_t len = AAsset_getLength(asset);
    unsigned char* buf = (unsigned char*)malloc(len);
    AAsset_read(asset, buf, len);
    AAsset_close(asset);
    int w, h, ch;
    unsigned char* img = stbi_load_from_memory(buf, (int)len, &w, &h, &ch, 4);
    free(buf);
    if (!img) { LOGE("stbi failed: %s", filename); return 0; }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    return tex;
}

static GLuint make_fallback_texture(void) {
    int sz = 64;
    unsigned char* px = (unsigned char*)malloc(sz * sz * 4);
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            int i = (y * sz + x) * 4;
            int c = ((x / 8) + (y / 8)) % 2;
            px[i] = c ? 200 : 170; px[i+1] = c ? 210 : 180;
            px[i+2] = c ? 220 : 195; px[i+3] = 255;
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
    eng->texPlatform = load_texture_asset(eng->app, "snow.png");
    if (!eng->texPlatform) eng->texPlatform = make_fallback_texture();
}

/* ============= ШЕЙДЕРЫ ============= */
void init_color_shader(struct engine* eng) {
    const char* vS =
        "attribute vec3 aPos; attribute vec3 aNorm;"
        "varying vec3 vNorm; varying vec3 vWorldPos;"
        "uniform mat4 uMVP; uniform mat4 uModel;"
        "void main(){"
        "  gl_Position=uMVP*vec4(aPos,1.0);"
        "  vNorm=mat3(uModel)*aNorm;"
        "  vWorldPos=(uModel*vec4(aPos,1.0)).xyz;}";
    const char* fS =
        "precision mediump float;"
        "varying vec3 vNorm; varying vec3 vWorldPos;"
        "uniform vec3 uColor; uniform vec3 uCamPos;"
        "void main(){"
        "  vec3 n=normalize(vNorm);"
        "  vec3 sun=normalize(vec3(0.4,0.8,0.3));"
        "  float diff=max(dot(n,sun),0.0);"
        "  float amb=0.5;"
        "  float fb=0.0;"
        "  if(n.y>0.5)fb=0.08; if(n.y<-0.5)fb=-0.12;"
        "  if(abs(n.x)>0.5)fb=-0.04; if(abs(n.z)>0.5)fb=-0.06;"
        "  float light=clamp(amb+diff*0.5+fb,0.25,1.0);"
        "  vec3 lit=uColor*light;"
        "  float dist=length((vWorldPos-uCamPos).xz);"
        "  float fog=clamp((dist-35.0)/40.0,0.0,0.8);"
        "  gl_FragColor=vec4(mix(lit,vec3(0.53,0.81,0.98),fog),1.0);}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    eng->colorProgram = glCreateProgram();
    glAttachShader(eng->colorProgram, vs); glAttachShader(eng->colorProgram, fs);
    glBindAttribLocation(eng->colorProgram, 0, "aPos");
    glBindAttribLocation(eng->colorProgram, 1, "aNorm");
    glLinkProgram(eng->colorProgram);
    glDeleteShader(vs); glDeleteShader(fs);
}

void init_tex_shader(struct engine* eng) {
    const char* vS =
        "attribute vec3 aPos; attribute vec3 aNorm; attribute vec2 aUV;"
        "varying vec3 vNorm; varying vec3 vWorldPos; varying vec2 vUV;"
        "uniform mat4 uMVP; uniform mat4 uModel;"
        "void main(){"
        "  gl_Position=uMVP*vec4(aPos,1.0);"
        "  vNorm=mat3(uModel)*aNorm; vWorldPos=(uModel*vec4(aPos,1.0)).xyz; vUV=aUV;}";
    const char* fS =
        "precision mediump float;"
        "varying vec3 vNorm; varying vec3 vWorldPos; varying vec2 vUV;"
        "uniform vec3 uCamPos; uniform sampler2D uTex;"
        "void main(){"
        "  vec3 n=normalize(vNorm); vec3 sun=normalize(vec3(0.4,0.8,0.3));"
        "  float diff=max(dot(n,sun),0.0); float light=clamp(0.5+diff*0.5,0.25,1.0);"
        "  vec4 tc=texture2D(uTex,vUV); vec3 lit=tc.rgb*light;"
        "  float dist=length((vWorldPos-uCamPos).xz);"
        "  float fog=clamp((dist-35.0)/40.0,0.0,0.8);"
        "  gl_FragColor=vec4(mix(lit,vec3(0.53,0.81,0.98),fog),1.0);}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    eng->texProgram = glCreateProgram();
    glAttachShader(eng->texProgram, vs); glAttachShader(eng->texProgram, fs);
    glBindAttribLocation(eng->texProgram, 0, "aPos");
    glBindAttribLocation(eng->texProgram, 1, "aNorm");
    glBindAttribLocation(eng->texProgram, 2, "aUV");
    glLinkProgram(eng->texProgram);
    glDeleteShader(vs); glDeleteShader(fs);
}

void init_ui_shader(void) {
    const char* vS = "attribute vec2 aPos; void main(){gl_Position=vec4(aPos,0.0,1.0);}";
    const char* fS = "precision mediump float; uniform vec4 col; void main(){gl_FragColor=col;}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);
    uiProg = glCreateProgram();
    glAttachShader(uiProg, vs); glAttachShader(uiProg, fs);
    glLinkProgram(uiProg);
    glDeleteShader(vs); glDeleteShader(fs);
}

/* =============================================================
 *  ROUNDED BOX MESH GENERATOR
 *  Генерирует куб со скруглёнными рёбрами и углами.
 *  Каждая вершина: pos(3) + norm(3) = 6 floats.
 *  seg — количество сегментов скругления (4 = хорошо).
 * ============================================================= */

#define RBOX_SEG 4
/* Максимум вершин для rounded box:
   6 граней * (seg+1)^2 * 2 = грани
   12 рёбер * (seg+1) * seg * 2
   8 углов * seg * seg * 2
   Примерно ~2000 вершин при seg=4 */
#define RBOX_MAX_VERTS 4096

typedef struct {
    float* data;   /* pos+norm interleaved */
    int count;     /* vertex count */
} RBoxMesh;

static void rbox_push(RBoxMesh* m, float px, float py, float pz,
                       float nx, float ny, float nz) {
    int i = m->count * 6;
    m->data[i]   = px; m->data[i+1] = py; m->data[i+2] = pz;
    m->data[i+3] = nx; m->data[i+4] = ny; m->data[i+5] = nz;
    m->count++;
}

static void rbox_tri(RBoxMesh* m,
                      float x0,float y0,float z0, float nx0,float ny0,float nz0,
                      float x1,float y1,float z1, float nx1,float ny1,float nz1,
                      float x2,float y2,float z2, float nx2,float ny2,float nz2) {
    rbox_push(m, x0,y0,z0, nx0,ny0,nz0);
    rbox_push(m, x1,y1,z1, nx1,ny1,nz1);
    rbox_push(m, x2,y2,z2, nx2,ny2,nz2);
}

static void rbox_quad(RBoxMesh* m,
                       float x0,float y0,float z0, float nx0,float ny0,float nz0,
                       float x1,float y1,float z1, float nx1,float ny1,float nz1,
                       float x2,float y2,float z2, float nx2,float ny2,float nz2,
                       float x3,float y3,float z3, float nx3,float ny3,float nz3) {
    rbox_tri(m, x0,y0,z0,nx0,ny0,nz0, x1,y1,z1,nx1,ny1,nz1, x2,y2,z2,nx2,ny2,nz2);
    rbox_tri(m, x0,y0,z0,nx0,ny0,nz0, x2,y2,z2,nx2,ny2,nz2, x3,y3,z3,nx3,ny3,nz3);
}

/*
 * Генерирует скруглённый куб размером 1x1x1
 * с радиусом скругления r.
 * Результат масштабируется при рендере.
 */
static RBoxMesh generate_rounded_box(float r) {
    RBoxMesh m;
    m.data = (float*)malloc(RBOX_MAX_VERTS * 6 * sizeof(float));
    m.count = 0;

    int seg = RBOX_SEG;
    float hs = 0.5f;        /* half size */
    float ir = hs - r;      /* inner extent */

    /* === 6 FLAT FACES === */
    /* +Y face */
    rbox_quad(&m,
        -ir, hs, -ir, 0,1,0,   ir, hs, -ir, 0,1,0,
         ir, hs,  ir, 0,1,0,  -ir, hs,  ir, 0,1,0);
    /* -Y face */
    rbox_quad(&m,
        -ir,-hs,  ir, 0,-1,0,  ir,-hs,  ir, 0,-1,0,
         ir,-hs, -ir, 0,-1,0, -ir,-hs, -ir, 0,-1,0);
    /* +X face */
    rbox_quad(&m,
        hs, -ir, -ir, 1,0,0,  hs, -ir,  ir, 1,0,0,
        hs,  ir,  ir, 1,0,0,  hs,  ir, -ir, 1,0,0);
    /* -X face */
    rbox_quad(&m,
        -hs, -ir,  ir, -1,0,0, -hs, -ir, -ir, -1,0,0,
        -hs,  ir, -ir, -1,0,0, -hs,  ir,  ir, -1,0,0);
    /* +Z face */
    rbox_quad(&m,
        -ir, -ir, hs, 0,0,1,   ir, -ir, hs, 0,0,1,
         ir,  ir, hs, 0,0,1,  -ir,  ir, hs, 0,0,1);
    /* -Z face */
    rbox_quad(&m,
         ir, -ir,-hs, 0,0,-1, -ir, -ir,-hs, 0,0,-1,
        -ir,  ir,-hs, 0,0,-1,  ir,  ir,-hs, 0,0,-1);

    /* === 12 EDGE STRIPS === */
    struct { int ax; float cx,cy; float dx,dy; } edges[12];
    /* Каждое ребро: ось скругления, центр дуги, направления */

    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * (PI * 0.5f);
        float a1 = (float)(i + 1) / seg * (PI * 0.5f);
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        /* Edges along Z axis (4 edges) */
        /* +X+Y */
        rbox_quad(&m,
            ir+r*c0, ir+r*s0, -ir, c0,s0,0,  ir+r*c1, ir+r*s1, -ir, c1,s1,0,
            ir+r*c1, ir+r*s1,  ir, c1,s1,0,  ir+r*c0, ir+r*s0,  ir, c0,s0,0);
        /* -X+Y */
        rbox_quad(&m,
            -ir-r*c1, ir+r*s1, -ir, -c1,s1,0, -ir-r*c0, ir+r*s0, -ir, -c0,s0,0,
            -ir-r*c0, ir+r*s0,  ir, -c0,s0,0, -ir-r*c1, ir+r*s1,  ir, -c1,s1,0);
        /* +X-Y */
        rbox_quad(&m,
            ir+r*c1, -ir-r*s1, -ir, c1,-s1,0,  ir+r*c0, -ir-r*s0, -ir, c0,-s0,0,
            ir+r*c0, -ir-r*s0,  ir, c0,-s0,0,  ir+r*c1, -ir-r*s1,  ir, c1,-s1,0);
        /* -X-Y */
        rbox_quad(&m,
            -ir-r*c0, -ir-r*s0, -ir, -c0,-s0,0, -ir-r*c1, -ir-r*s1, -ir, -c1,-s1,0,
            -ir-r*c1, -ir-r*s1,  ir, -c1,-s1,0, -ir-r*c0, -ir-r*s0,  ir, -c0,-s0,0);

        /* Edges along X axis (4 edges) */
        /* +Y+Z */
        rbox_quad(&m,
            -ir, ir+r*c0, ir+r*s0, 0,c0,s0,   ir, ir+r*c0, ir+r*s0, 0,c0,s0,
             ir, ir+r*c1, ir+r*s1, 0,c1,s1,  -ir, ir+r*c1, ir+r*s1, 0,c1,s1);
        /* +Y-Z */
        rbox_quad(&m,
             ir, ir+r*c0, -ir-r*s0, 0,c0,-s0, -ir, ir+r*c0, -ir-r*s0, 0,c0,-s0,
            -ir, ir+r*c1, -ir-r*s1, 0,c1,-s1,  ir, ir+r*c1, -ir-r*s1, 0,c1,-s1);
        /* -Y+Z */
        rbox_quad(&m,
             ir, -ir-r*c0, ir+r*s0, 0,-c0,s0, -ir, -ir-r*c0, ir+r*s0, 0,-c0,s0,
            -ir, -ir-r*c1, ir+r*s1, 0,-c1,s1,  ir, -ir-r*c1, ir+r*s1, 0,-c1,s1);
        /* -Y-Z */
        rbox_quad(&m,
            -ir, -ir-r*c0, -ir-r*s0, 0,-c0,-s0,  ir, -ir-r*c0, -ir-r*s0, 0,-c0,-s0,
             ir, -ir-r*c1, -ir-r*s1, 0,-c1,-s1, -ir, -ir-r*c1, -ir-r*s1, 0,-c1,-s1);

        /* Edges along Y axis (4 edges) */
        /* +X+Z */
        rbox_quad(&m,
            ir+r*s0, -ir, ir+r*c0, s0,0,c0,  ir+r*s0, ir, ir+r*c0, s0,0,c0,
            ir+r*s1,  ir, ir+r*c1, s1,0,c1,  ir+r*s1,-ir, ir+r*c1, s1,0,c1);
        /* -X+Z */
        rbox_quad(&m,
            -ir-r*s0,  ir, ir+r*c0, -s0,0,c0, -ir-r*s0, -ir, ir+r*c0, -s0,0,c0,
            -ir-r*s1, -ir, ir+r*c1, -s1,0,c1, -ir-r*s1,  ir, ir+r*c1, -s1,0,c1);
        /* +X-Z */
        rbox_quad(&m,
            ir+r*s0,  ir, -ir-r*c0, s0,0,-c0, ir+r*s0, -ir, -ir-r*c0, s0,0,-c0,
            ir+r*s1, -ir, -ir-r*c1, s1,0,-c1, ir+r*s1,  ir, -ir-r*c1, s1,0,-c1);
        /* -X-Z */
        rbox_quad(&m,
            -ir-r*s0, -ir, -ir-r*c0, -s0,0,-c0, -ir-r*s0, ir, -ir-r*c0, -s0,0,-c0,
            -ir-r*s1,  ir, -ir-r*c1, -s1,0,-c1, -ir-r*s1,-ir, -ir-r*c1, -s1,0,-c1);
    }

    /* === 8 CORNER PATCHES === */
    float signs[8][3] = {
        { 1, 1, 1}, {-1, 1, 1}, { 1,-1, 1}, {-1,-1, 1},
        { 1, 1,-1}, {-1, 1,-1}, { 1,-1,-1}, {-1,-1,-1}
    };

    for (int ci = 0; ci < 8; ci++) {
        float sx = signs[ci][0], sy = signs[ci][1], sz = signs[ci][2];
        float cx = sx * ir, cy = sy * ir, cz = sz * ir;

        for (int j = 0; j < seg; j++) {
            float phi0 = (float)j / seg * (PI * 0.5f);
            float phi1 = (float)(j + 1) / seg * (PI * 0.5f);
            for (int k = 0; k < seg; k++) {
                float th0 = (float)k / seg * (PI * 0.5f);
                float th1 = (float)(k + 1) / seg * (PI * 0.5f);

                /* 4 нормали на патче */
                float n00x = sx*cosf(phi0)*cosf(th0), n00y = sy*sinf(phi0), n00z = sz*cosf(phi0)*sinf(th0);
                float n10x = sx*cosf(phi1)*cosf(th0), n10y = sy*sinf(phi1), n10z = sz*cosf(phi1)*sinf(th0);
                float n01x = sx*cosf(phi0)*cosf(th1), n01y = sy*sinf(phi0), n01z = sz*cosf(phi0)*sinf(th1);
                float n11x = sx*cosf(phi1)*cosf(th1), n11y = sy*sinf(phi1), n11z = sz*cosf(phi1)*sinf(th1);

                float p00x = cx + r*n00x, p00y = cy + r*n00y, p00z = cz + r*n00z;
                float p10x = cx + r*n10x, p10y = cy + r*n10y, p10z = cz + r*n10z;
                float p01x = cx + r*n01x, p01y = cy + r*n01y, p01z = cz + r*n01z;
                float p11x = cx + r*n11x, p11y = cy + r*n11y, p11z = cz + r*n11z;

                if (sx * sy * sz > 0) {
                    rbox_quad(&m,
                        p00x,p00y,p00z, n00x,n00y,n00z,
                        p10x,p10y,p10z, n10x,n10y,n10z,
                        p11x,p11y,p11z, n11x,n11y,n11z,
                        p01x,p01y,p01z, n01x,n01y,n01z);
                } else {
                    rbox_quad(&m,
                        p00x,p00y,p00z, n00x,n00y,n00z,
                        p01x,p01y,p01z, n01x,n01y,n01z,
                        p11x,p11y,p11z, n11x,n11y,n11z,
                        p10x,p10y,p10z, n10x,n10y,n10z);
                }
            }
        }
    }

    return m;
}

/* VBO для скруглённого куба */
static GLuint rboxVBO = 0;
static int rboxVertCount = 0;

void init_cube_vbo(void) {
    RBoxMesh m = generate_rounded_box(0.08f);
    rboxVertCount = m.count;
    glGenBuffers(1, &rboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, rboxVBO);
    glBufferData(GL_ARRAY_BUFFER, m.count * 6 * sizeof(float), m.data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(m.data);
    LOGI("RoundedBox: %d vertices", rboxVertCount);
}

/* ============= DRAW PART (иерархический) ============= */
static void draw_part(struct engine* eng, float* vpMat, float* parentMat,
                      float pivotX, float pivotY, float pivotZ,
                      float sx, float sy, float sz,
                      float rx, float ry, float rz,
                      float cr, float cg, float cb,
                      float eyeX, float eyeY, float eyeZ,
                      float* outModel) {
    float T[16], S[16], RY[16], RX[16], RZ[16];
    float t1[16], t2[16], t3[16], model[16], mvp[16];

    mat4_translate(T, pivotX, pivotY, pivotZ);
    mat4_rotate_y(RY, ry);
    mat4_rotate_x(RX, rx);
    mat4_rotate_z(RZ, rz);
    mat4_scale(S, sx, sy, sz);

    mat4_mul(t1, RY, RX);
    mat4_mul(t2, t1, RZ);
    mat4_mul(t3, T, t2);

    mat4_mul(model, parentMat, t3);
    if (outModel) memcpy(outModel, model, 64);

    float localFull[16], modelFull[16];
    mat4_mul(localFull, t3, S);
    mat4_mul(modelFull, parentMat, localFull);
    mat4_mul(mvp, vpMat, modelFull);

    glUseProgram(eng->colorProgram);
    glUniformMatrix4fv(glGetUniformLocation(eng->colorProgram, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(eng->colorProgram, "uModel"), 1, GL_FALSE, modelFull);
    glUniform3f(glGetUniformLocation(eng->colorProgram, "uColor"), cr, cg, cb);
    glUniform3f(glGetUniformLocation(eng->colorProgram, "uCamPos"), eyeX, eyeY, eyeZ);
    glBindBuffer(GL_ARRAY_BUFFER, rboxVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, (void*)12); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, rboxVertCount);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ============= R15 РАЗМЕРЫ ============= */
#define HEAD_W   0.55f
#define HEAD_H   0.55f
#define HEAD_D   0.55f
#define UTORSO_W 0.90f
#define UTORSO_H 0.70f
#define UTORSO_D 0.50f
#define LTORSO_W 0.90f
#define LTORSO_H 0.40f
#define LTORSO_D 0.50f
#define UARM_W   0.39f
#define UARM_H   0.40f
#define UARM_D   0.39f
#define LARM_W   0.39f
#define LARM_H   0.65f
#define LARM_D   0.39f
#define ULEG_W   0.40f
#define ULEG_H   0.50f
#define ULEG_D   0.40f
#define LLEG_W   0.40f
#define LLEG_H   0.50f
#define LLEG_D   0.40f

/* Цвета */
#define COL_HEAD_R  0.96f
#define COL_HEAD_G  0.80f
#define COL_HEAD_B  0.19f
#define COL_TORSO_R 0.05f
#define COL_TORSO_G 0.37f
#define COL_TORSO_B 0.74f
#define COL_LIMB_R  0.96f
#define COL_LIMB_G  0.80f
#define COL_LIMB_B  0.19f
#define COL_LEG_R   0.63f
#define COL_LEG_G   0.74f
#define COL_LEG_B   0.07f

/* ============= РЕНДЕР ПЕРСОНАЖА ============= */
static void render_character(struct engine* eng, float* vpMat,
                              float eyeX, float eyeY, float eyeZ) {
    float px = eng->playerPos[0];
    float py = eng->playerPos[1];
    float pz = eng->playerPos[2];
    float rot = eng->playerRot;
    float t = eng->animTime;

    bool walking = eng->isMoving && (eng->moveDirX != 0 || eng->moveDirZ != 0);
    bool inAir = !eng->onGround;

    float legSwing = 0, armSwing = 0, torsoTwist = 0;
    float lKnee = 0, rKnee = 0;

    if (walking) {
        legSwing = sinf(t) * 0.7f;
        armSwing = sinf(t) * 0.5f;
        torsoTwist = sinf(t) * 0.04f;
        float ls = sinf(t), rs = sinf(t + PI);
        lKnee = ls < 0 ? -ls * 0.5f : 0;
        rKnee = rs < 0 ? -rs * 0.5f : 0;
    } else if (inAir) {
        legSwing = -0.3f;
        armSwing = -0.5f;
    }

    float rootY = py + ULEG_H + LLEG_H + LTORSO_H * 0.5f;
    float rootMat[16];
    {
        float T[16], R[16];
        mat4_translate(T, px, rootY, pz);
        mat4_rotate_y(R, rot);
        mat4_mul(rootMat, T, R);
    }

    float ltMat[16];
    draw_part(eng, vpMat, rootMat, 0,0,0,
              LTORSO_W, LTORSO_H, LTORSO_D, 0, torsoTwist, 0,
              COL_TORSO_R, COL_TORSO_G, COL_TORSO_B, eyeX,eyeY,eyeZ, ltMat);

    float utMat[16];
    draw_part(eng, vpMat, ltMat,
              0, LTORSO_H*0.5f+UTORSO_H*0.5f, 0,
              UTORSO_W, UTORSO_H, UTORSO_D, 0, -torsoTwist, 0,
              COL_TORSO_R, COL_TORSO_G, COL_TORSO_B, eyeX,eyeY,eyeZ, utMat);

    draw_part(eng, vpMat, utMat,
              0, UTORSO_H*0.5f+HEAD_H*0.5f, 0,
              HEAD_W, HEAD_H, HEAD_D, 0,0,0,
              COL_HEAD_R, COL_HEAD_G, COL_HEAD_B, eyeX,eyeY,eyeZ, NULL);

    float luaMat[16];
    draw_part(eng, vpMat, utMat,
              -(UTORSO_W*0.5f+UARM_W*0.5f), UTORSO_H*0.35f, 0,
              UARM_W, UARM_H, UARM_D, -armSwing, 0, 0,
              COL_LIMB_R, COL_LIMB_G, COL_LIMB_B, eyeX,eyeY,eyeZ, luaMat);

    draw_part(eng, vpMat, luaMat,
              0, -(UARM_H*0.5f+LARM_H*0.5f), 0,
              LARM_W, LARM_H, LARM_D, 0,0,0,
              COL_LIMB_R, COL_LIMB_G, COL_LIMB_B, eyeX,eyeY,eyeZ, NULL);

    float ruaMat[16];
    draw_part(eng, vpMat, utMat,
              (UTORSO_W*0.5f+UARM_W*0.5f), UTORSO_H*0.35f, 0,
              UARM_W, UARM_H, UARM_D, armSwing, 0, 0,
              COL_LIMB_R, COL_LIMB_G, COL_LIMB_B, eyeX,eyeY,eyeZ, ruaMat);

    draw_part(eng, vpMat, ruaMat,
              0, -(UARM_H*0.5f+LARM_H*0.5f), 0,
              LARM_W, LARM_H, LARM_D, 0,0,0,
              COL_LIMB_R, COL_LIMB_G, COL_LIMB_B, eyeX,eyeY,eyeZ, NULL);

    float lulMat[16];
    draw_part(eng, vpMat, ltMat,
              -0.225f, -(LTORSO_H*0.5f+ULEG_H*0.5f), 0,
              ULEG_W, ULEG_H, ULEG_D, legSwing, 0, 0,
              COL_LEG_R, COL_LEG_G, COL_LEG_B, eyeX,eyeY,eyeZ, lulMat);

    draw_part(eng, vpMat, lulMat,
              0, -(ULEG_H*0.5f+LLEG_H*0.5f), 0,
              LLEG_W, LLEG_H, LLEG_D, lKnee, 0, 0,
              COL_LEG_R, COL_LEG_G, COL_LEG_B, eyeX,eyeY,eyeZ, NULL);

    float rulMat[16];
    draw_part(eng, vpMat, ltMat,
              0.225f, -(LTORSO_H*0.5f+ULEG_H*0.5f), 0,
              ULEG_W, ULEG_H, ULEG_D, -legSwing, 0, 0,
              COL_LEG_R, COL_LEG_G, COL_LEG_B, eyeX,eyeY,eyeZ, rulMat);

    draw_part(eng, vpMat, rulMat,
              0, -(ULEG_H*0.5f+LLEG_H*0.5f), 0,
              LLEG_W, LLEG_H, LLEG_D, rKnee, 0, 0,
              COL_LEG_R, COL_LEG_G, COL_LEG_B, eyeX,eyeY,eyeZ, NULL);
}

/* ============= ПЛАТФОРМА ============= */
static GLuint platVBO = 0;

void init_platform_vbo(void) {
    float hs = PLATFORM_SIZE * 0.5f;
    float uv = PLATFORM_SIZE / 4.0f;
    float data[] = {
        -hs,0,-hs, 0,1,0, 0,0,      hs,0,-hs, 0,1,0, uv,0,
         hs,0, hs, 0,1,0, uv,uv,   -hs,0,-hs, 0,1,0, 0,0,
         hs,0, hs, 0,1,0, uv,uv,   -hs,0, hs, 0,1,0, 0,uv,
    };
    glGenBuffers(1, &platVBO);
    glBindBuffer(GL_ARRAY_BUFFER, platVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void draw_platform(struct engine* eng, float* vpMat,
                           float eyeX, float eyeY, float eyeZ) {
    float T[16], mvp[16];
    mat4_translate(T, 0, 0, 0);
    mat4_mul(mvp, vpMat, T);
    glUseProgram(eng->texProgram);
    glUniformMatrix4fv(glGetUniformLocation(eng->texProgram, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(eng->texProgram, "uModel"), 1, GL_FALSE, T);
    glUniform3f(glGetUniformLocation(eng->texProgram, "uCamPos"), eyeX, eyeY, eyeZ);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texPlatform);
    glUniform1i(glGetUniformLocation(eng->texProgram, "uTex"), 0);
    glBindBuffer(GL_ARRAY_BUFFER, platVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24); glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ============= ДЕКОР ============= */
static void render_decorations(struct engine* eng, float* vpMat,
                                float eyeX, float eyeY, float eyeZ) {
    float ident[16];
    mat4_identity(ident);
    draw_part(eng, vpMat, ident, 10,0.5f,10, 1,1,1, 0,0,0, 0.85f,0.2f,0.2f, eyeX,eyeY,eyeZ, NULL);
    draw_part(eng, vpMat, ident, -8,0.5f,5, 1,1,1, 0,0,0, 0.2f,0.2f,0.85f, eyeX,eyeY,eyeZ, NULL);
    draw_part(eng, vpMat, ident, 5,0.5f,-12, 1,1,1, 0,0,0, 0.85f,0.85f,0.2f, eyeX,eyeY,eyeZ, NULL);
    draw_part(eng, vpMat, ident, -15,1.0f,-8, 2,2,2, 0,0,0, 0.85f,0.5f,0.2f, eyeX,eyeY,eyeZ, NULL);
    draw_part(eng, vpMat, ident, 12,0.75f,-5, 1.5f,1.5f,1.5f, 0,0,0, 0.6f,0.2f,0.7f, eyeX,eyeY,eyeZ, NULL);
    draw_part(eng, vpMat, ident, -5,0.5f,15, 1,1,1, 0,0,0, 0.2f,0.7f,0.7f, eyeX,eyeY,eyeZ, NULL);
}

/* ============= РЕНДЕР СЦЕНЫ ============= */
void render_world(struct engine* eng) {
    glEnable(GL_DEPTH_TEST);
    float camY = eng->camRotY;
    float eyeX = eng->playerPos[0] - sinf(camY) * CAM_DIST;
    float eyeY = eng->playerPos[1] + CAM_HEIGHT;
    float eyeZ = eng->playerPos[2] + cosf(camY) * CAM_DIST;
    float tgtX = eng->playerPos[0];
    float tgtY = eng->playerPos[1] + 1.2f;
    float tgtZ = eng->playerPos[2];

    float proj[16], view[16], vp[16];
    mat4_perspective(proj, GAME_FOV, (float)eng->width / (float)eng->height, 0.1f, 200.0f);
    mat4_lookat_pos(view, eyeX, eyeY, eyeZ, tgtX, tgtY, tgtZ);
    mat4_mul(vp, proj, view);

    draw_platform(eng, vp, eyeX, eyeY, eyeZ);
    render_decorations(eng, vp, eyeX, eyeY, eyeZ);
    render_character(eng, vp, eyeX, eyeY, eyeZ);
}

/* ============= UI ============= */
static void draw_rect_ui(float cx, float cy, float hw, float hh, int sw, int sh,
                          float cr, float cg, float cb, float ca) {
    float nx = (cx/sw)*2.0f-1.0f, ny = 1.0f-(cy/sh)*2.0f;
    float rw = (hw/sw)*2.0f, rh = (hh/sh)*2.0f;
    float v[] = {nx-rw,ny-rh, nx+rw,ny-rh, nx+rw,ny+rh, nx-rw,ny-rh, nx+rw,ny+rh, nx-rw,ny+rh};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr,cg,cb,ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_ring_ui(float cx, float cy, float r, float thick,
                          int w, int h, float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2;
    float rxo = (r/w)*2, ryo = (r/h)*2, rxi = ((r-thick)/w)*2, ryi = ((r-thick)/h)*2;
    float verts[(32+1)*4]; int segs = 32;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i/segs*2*PI, c = cosf(a), s = sinf(a);
        verts[i*4] = ndcX+c*rxo; verts[i*4+1] = ndcY+s*ryo;
        verts[i*4+2] = ndcX+c*rxi; verts[i*4+3] = ndcY+s*ryi;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr,cg,cb,ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs+1)*2);
}

static void draw_circle_ui(float cx, float cy, float r, int w, int h,
                            float cr, float cg, float cb, float ca) {
    float ndcX = (cx/w)*2-1, ndcY = 1-(cy/h)*2, rx = (r/w)*2, ry = (r/h)*2;
    float verts[(24+2)*2]; int segs = 24;
    verts[0] = ndcX; verts[1] = ndcY;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i/segs*2*PI;
        verts[(i+1)*2] = ndcX+cosf(a)*rx; verts[(i+1)*2+1] = ndcY+sinf(a)*ry;
    }
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), cr,cg,cb,ca);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs+2);
}

static void draw_border_ui(float cx, float cy, float hw, float hh, float t,
                            int sw, int sh, float cr, float cg, float cb, float ca) {
    draw_rect_ui(cx, cy-hh+t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx, cy+hh-t*0.5f, hw, t*0.5f, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx-hw+t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
    draw_rect_ui(cx+hw-t*0.5f, cy, t*0.5f, hh, sw, sh, cr,cg,cb,ca);
}

void draw_menu(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    draw_rect_ui(sw/2.0f, sh/2.0f, sw/2.0f, sh/2.0f, sw, sh, 0.15f,0.15f,0.2f, 1);
    float titleY = sh * 0.30f;
    draw_rect_ui(sw/2.0f, titleY, 140, 35, sw, sh, 0.3f,0.5f,0.9f, 0.9f);
    draw_border_ui(sw/2.0f, titleY, 140, 35, 3, sw, sh, 0.5f,0.7f,1.0f, 1);
    float playX = sw/2.0f, playY = sh * 0.55f;
    draw_rect_ui(playX, playY, 100, 40, sw, sh, 0.2f,0.65f,0.2f, 0.9f);
    draw_border_ui(playX, playY, 100, 40, 3, sw, sh, 0.4f,1.0f,0.4f, 1);
    float pnx = (playX/sw)*2-1, pny = 1-(playY/sh)*2;
    float pax = (22.0f/sw)*2, pay = (22.0f/sh)*2;
    float play[] = {pnx-pax,pny+pay, pnx-pax,pny-pay, pnx+pax,pny};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 1,1,1,1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, play); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void draw_ui(struct engine* eng) {
    int sw = eng->width, sh = eng->height;
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (eng->gameState == STATE_MENU) {
        draw_menu(eng); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); return;
    }

    float jx = JOY_X_OFFSET, jy = sh-JOY_Y_OFFSET;
    draw_ring_ui(jx, jy, JOY_RADIUS, 3, sw, sh, 1,1,1, 0.5f);
    draw_circle_ui(jx+eng->moveDirX*JOY_RADIUS*0.6f,
                   jy+eng->moveDirZ*JOY_RADIUS*0.6f, STICK_RADIUS, sw, sh, 1,1,1, 0.6f);

    float bx = sw-JUMP_BTN_OFFSET, by = sh-JUMP_BTN_OFFSET;
    draw_ring_ui(bx, by, JUMP_BTN_SIZE, 3, sw, sh, 1,1,1, 0.5f);
    float as = JUMP_BTN_SIZE*0.3f;
    float anx = (bx/sw)*2-1, any = 1-(by/sh)*2, aax = (as/sw)*2, aay = (as/sh)*2;
    float arrow[] = {anx,any+aay, anx-aax,any-aay*0.5f, anx+aax,any-aay*0.5f};
    glUseProgram(uiProg);
    glUniform4f(glGetUniformLocation(uiProg, "col"), 1,1,1, 0.6f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrow); glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

#endif
