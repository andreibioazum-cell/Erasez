#ifndef RENDER_H
#define RENDER_H

#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "math_utils.h"

// ===== ТЕКСТУРНЫЙ АТЛАС =====
static GLuint create_texture_atlas(struct engine* eng) {
    int atlasSize = 128;
    int blockSize = 16;
    int blocksPerRow = atlasSize / blockSize;
    
    unsigned char* atlas = (unsigned char*)calloc(atlasSize * atlasSize * 4, 1);
    if (!atlas) return 0;
    
    unsigned char colors[][3] = {
        {0, 0, 0},
        {100, 200, 80},
        {160, 130, 80},
        {180, 180, 180},
        {180, 140, 80},
        {80, 180, 80},
        {210, 200, 150},
    };
    
    for (int b = 1; b < sizeof(colors)/sizeof(colors[0]); b++) {
        int bx = (b % blocksPerRow) * blockSize;
        int by = (b / blocksPerRow) * blockSize;
        
        for (int y = 0; y < blockSize; y++) {
            for (int x = 0; x < blockSize; x++) {
                int idx = ((by + y) * atlasSize + (bx + x)) * 4;
                float noise = (rand() % 20 - 10) / 255.0f;
                atlas[idx] = colors[b][0] + noise * 30;
                atlas[idx+1] = colors[b][1] + noise * 30;
                atlas[idx+2] = colors[b][2] + noise * 30;
                atlas[idx+3] = 255;
                
                if (b == BLOCK_GRASS && y > blockSize * 0.7f) {
                    atlas[idx] = 130;
                    atlas[idx+1] = 220;
                    atlas[idx+2] = 70;
                }
            }
        }
    }
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasSize, atlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    free(atlas);
    return tex;
}

// ===== ПОСТРОЕНИЕ МЕША =====
static void rebuild_mesh(struct engine* eng) {
    for (int x = 0; x < WORLD_SIZE_X; x++) {
        for (int y = 0; y < WORLD_SIZE_Y; y++) {
            for (int z = 0; z < WORLD_SIZE_Z; z++) {
                if (!eng->blocks[x][y][z]) {
                    eng->faces[x][y][z] = 0;
                    continue;
                }
                unsigned char f = 0;
                if (x+1 >= WORLD_SIZE_X || !eng->blocks[x+1][y][z]) f |= FACE_XP;
                if (x-1 < 0 || !eng->blocks[x-1][y][z]) f |= FACE_XN;
                if (y+1 >= WORLD_SIZE_Y || !eng->blocks[x][y+1][z]) f |= FACE_YP;
                if (y-1 < 0 || !eng->blocks[x][y-1][z]) f |= FACE_YN;
                if (z+1 >= WORLD_SIZE_Z || !eng->blocks[x][y][z+1]) f |= FACE_ZP;
                if (z-1 < 0 || !eng->blocks[x][y][z-1]) f |= FACE_ZN;
                eng->faces[x][y][z] = f;
            }
        }
    }
    
    int fc = 0;
    for (int x = 0; x < WORLD_SIZE_X; x++)
        for (int y = 0; y < WORLD_SIZE_Y; y++)
            for (int z = 0; z < WORLD_SIZE_Z; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (f & FACE_XP) fc++;
                if (f & FACE_XN) fc++;
                if (f & FACE_YP) fc++;
                if (f & FACE_YN) fc++;
                if (f & FACE_ZP) fc++;
                if (f & FACE_ZN) fc++;
            }
    
    eng->visibleFaceCount = fc;
    if (fc == 0) {
        eng->meshDirty = false;
        return;
    }
    
    int vertCount = fc * 6;
    float* buf = (float*)malloc(vertCount * 8 * sizeof(float));
    if (!buf) return;
    
    int idx = 0;
    for (int x = 0; x < WORLD_SIZE_X; x++) {
        for (int y = 0; y < WORLD_SIZE_Y; y++) {
            for (int z = 0; z < WORLD_SIZE_Z; z++) {
                unsigned char f = eng->faces[x][y][z];
                if (!f) continue;
                
                float bx = (float)x - 0.5f;
                float by = (float)y - 0.5f;
                float bz = (float)z - 0.5f;
                float x0 = bx, x1 = bx + 1.0f;
                float y0 = by, y1 = by + 1.0f;
                float z0 = bz, z1 = bz + 1.0f;
                
                unsigned char blockType = eng->blocks[x][y][z];
                float u = (blockType % 8) / 8.0f;
                float v = (blockType / 8) / 8.0f;
                float du = 1.0f / 8.0f;
                float dv = 1.0f / 8.0f;
                
                // Добавление квада (исправленный макрос)
                #define ADD_QUAD(ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, nx,ny,nz) do { \
                    float verts[6][8] = { \
                        {ax,ay,az, u, v, nx,ny,nz}, \
                        {bx,by,bz, u+du, v, nx,ny,nz}, \
                        {cx,cy,cz, u+du, v+dv, nx,ny,nz}, \
                        {ax,ay,az, u, v, nx,ny,nz}, \
                        {cx,cy,cz, u+du, v+dv, nx,ny,nz}, \
                        {dx,dy,dz, u, v+dv, nx,ny,nz} \
                    }; \
                    memcpy(&buf[idx*8], verts, sizeof(verts)); \
                    idx += 6; \
                } while(0)
                
                if (f & FACE_XP) ADD_QUAD(x1,y0,z0, x1,y0,z1, x1,y1,z1, x1,y1,z0, 1,0,0);
                if (f & FACE_XN) ADD_QUAD(x0,y0,z1, x0,y0,z0, x0,y1,z0, x0,y1,z1, -1,0,0);
                if (f & FACE_YP) {
                    float u2 = u + du * 0.5f;
                    float v2 = v;
                    float verts[6][8] = {
                        {x0,y1,z1, u2, v2, 0,1,0},
                        {x1,y1,z1, u2+du, v2, 0,1,0},
                        {x1,y1,z0, u2+du, v2+dv, 0,1,0},
                        {x0,y1,z1, u2, v2, 0,1,0},
                        {x1,y1,z0, u2+du, v2+dv, 0,1,0},
                        {x0,y1,z0, u2, v2+dv, 0,1,0}
                    };
                    memcpy(&buf[idx*8], verts, sizeof(verts));
                    idx += 6;
                }
                if (f & FACE_YN) ADD_QUAD(x0,y0,z0, x1,y0,z0, x1,y0,z1, x0,y0,z1, 0,-1,0);
                if (f & FACE_ZP) ADD_QUAD(x1,y0,z0, x0,y0,z0, x0,y1,z0, x1,y1,z0, 0,0,-1);
                if (f & FACE_ZN) ADD_QUAD(x0,y0,z1, x1,y0,z1, x1,y1,z1, x0,y1,z1, 0,0,1);
                
                #undef ADD_QUAD
            }
        }
    }
    
    if (!eng->vbo) glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, idx * 8 * sizeof(float), buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    free(buf);
    eng->meshDirty = false;
}

static void render_world(struct engine* eng) {
    if (eng->meshDirty) rebuild_mesh(eng);
    if (eng->visibleFaceCount == 0) return;
    
    glEnable(GL_DEPTH_TEST);
    glUseProgram(eng->program);
    
    float proj[16], view[16], mvp[16];
    float aspect = (float)eng->width / (float)eng->height;
    mat4_perspective(proj, GAME_FOV, aspect, 0.1f, 100.0f);
    mat4_lookat(view, eng->camPos, eng->camRot[0], eng->camRot[1]);
    mat4_mul(mvp, proj, view);
    
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "uMVP"), 1, GL_FALSE, mvp);
    glUniform3fv(glGetUniformLocation(eng->program, "uCamPos"), 1, eng->camPos);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texAtlas);
    glUniform1i(glGetUniformLocation(eng->program, "uTexture"), 0);
    
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glDrawArrays(GL_TRIANGLES, 0, eng->visibleFaceCount * 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ===== UI =====
static void init_ui_shader(struct engine* eng) {
    const char* vS = "attribute vec2 aPos; void main() { gl_Position = vec4(aPos, 0.0, 1.0); }";
    const char* fS = "precision highp float; uniform vec4 uColor; void main() { gl_FragColor = uColor; }";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vS, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fS, NULL);
    glCompileShader(fs);
    
    eng->uiProgram = glCreateProgram();
    glAttachShader(eng->uiProgram, vs);
    glAttachShader(eng->uiProgram, fs);
    glLinkProgram(eng->uiProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

static void draw_rect(struct engine* eng, float x, float y, float w, float h, float r, float g, float b, float a) {
    float x1 = (x / eng->width) * 2.0f - 1.0f;
    float y1 = 1.0f - (y / eng->height) * 2.0f;
    float w2 = (w / eng->width) * 2.0f;
    float h2 = (h / eng->height) * 2.0f;
    
    float verts[] = {
        x1-w2, y1-h2, x1+w2, y1-h2, x1+w2, y1+h2,
        x1-w2, y1-h2, x1+w2, y1+h2, x1-w2, y1+h2
    };
    
    glUseProgram(eng->uiProgram);
    glUniform4f(glGetUniformLocation(eng->uiProgram, "uColor"), r, g, b, a);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_ui(struct engine* eng) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Прицел
    float cx = eng->width / 2.0f;
    float cy = eng->height / 2.0f;
    draw_rect(eng, cx, cy, 12.0f, 1.5f, 1, 1, 1, 0.8f);
    draw_rect(eng, cx, cy, 1.5f, 12.0f, 1, 1, 1, 0.8f);
    
    // Джойстик
    float jx = JOY_X_OFFSET;
    float jy = eng->height - JOY_Y_OFFSET;
    draw_rect(eng, jx, jy, JOY_RADIUS*2, JOY_RADIUS*2, 0, 0, 0, 0.3f);
    draw_rect(eng, jx, jy, JOY_RADIUS*2-4, JOY_RADIUS*2-4, 0, 0, 0, 0.1f);
    
    if (eng->isMoving) {
        float sx = jx + eng->moveDirX * JOY_RADIUS * 0.6f;
        float sy = jy + eng->moveDirZ * JOY_RADIUS * 0.6f;
        draw_rect(eng, sx, sy, STICK_RADIUS*2, STICK_RADIUS*2, 0.5f, 0.5f, 0.5f, 0.6f);
    }
    
    // Кнопка прыжка
    float jbx = eng->width - JUMP_BTN_OFFSET;
    float jby = eng->height - JUMP_BTN_OFFSET;
    draw_rect(eng, jbx, jby, JUMP_BTN_SIZE, JUMP_BTN_SIZE, 0, 0, 0, 0.3f);
    draw_rect(eng, jbx, jby, JUMP_BTN_SIZE-4, JUMP_BTN_SIZE-4, 0, 0, 0, 0.1f);
    
    // Кнопка ломания
    float bbx = eng->width - BREAK_BTN_X;
    float bby = BREAK_BTN_Y;
    draw_rect(eng, bbx, bby, ACTION_BTN_SIZE*2, ACTION_BTN_SIZE*2, 0, 0, 0, 0.3f);
    draw_rect(eng, bbx, bby, ACTION_BTN_SIZE*2-4, ACTION_BTN_SIZE*2-4, 0, 0, 0, 0.1f);
    
    // Кнопка ставления
    float pbx = eng->width - PLACE_BTN_X;
    float pby = PLACE_BTN_Y;
    draw_rect(eng, pbx, pby, ACTION_BTN_SIZE*2, ACTION_BTN_SIZE*2, 0, 0, 0, 0.3f);
    draw_rect(eng, pbx, pby, ACTION_BTN_SIZE*2-4, ACTION_BTN_SIZE*2-4, 0, 0, 0, 0.1f);
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

#endif
