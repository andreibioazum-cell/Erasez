#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <math.h>

#define JOY_RADIUS      80.0f
#define JOY_X_OFFSET    130.0f
#define JOY_Y_OFFSET    130.0f
#define STICK_RADIUS    32.0f

#define JUMP_BTN_SIZE   80.0f
#define JUMP_BTN_OFFSET 130.0f

#define ACTION_BTN_SIZE 45.0f
#define BREAK_BTN_X     80.0f
#define BREAK_BTN_Y     80.0f
#define PLACE_BTN_X     80.0f
#define PLACE_BTN_Y     190.0f

#define PI              3.14159265f

#define PLAYER_W        0.3f
#define EYE_H           1.6f
#define HEAD_MARGIN     0.2f
#define GAME_FOV        1.4915f

#define GRAVITY         0.008f
#define JUMP_FORCE      0.15f
#define TERM_VEL       -0.35f
#define PLAYER_SPEED    0.09f

#define WORLD_SIZE_X    32
#define WORLD_SIZE_Y    16
#define WORLD_SIZE_Z    32

#define BLOCK_AIR       0
#define BLOCK_GRASS     1
#define BLOCK_DIRT      2
#define BLOCK_STONE     3
#define BLOCK_WOOD      4
#define BLOCK_LEAVES    5
#define BLOCK_SAND      6

#define RAY_DIST        8.0f
#define RAY_STEP        0.02f
#define MAX_EDITS       1024

#define FACE_XP 0x01
#define FACE_XN 0x02
#define FACE_YP 0x04
#define FACE_YN 0x08
#define FACE_ZP 0x10
#define FACE_ZN 0x20

struct block_edit {
    int wx, wy, wz;
    unsigned char val;
};

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;
    
    GLuint program;
    GLuint uiProgram;
    GLuint texAtlas;
    
    float camPos[3];
    float camRot[2];
    float velY;
    bool
