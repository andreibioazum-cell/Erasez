#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#define PI 3.14159265f

/* UI */
#define JOY_RADIUS      80.0f
#define JOY_X_OFFSET   130.0f
#define JOY_Y_OFFSET   130.0f
#define STICK_RADIUS    32.0f
#define JUMP_BTN_SIZE   80.0f
#define JUMP_BTN_OFFSET 130.0f

/* Игрок */
#define PLAYER_W    0.4f
#define EYE_H       1.65f
#define HEAD_MARGIN 0.15f
#define GAME_FOV    1.4915f

/* Физика */
#define GRAVITY    0.018f
#define JUMP_FORCE 0.12f
#define TERM_VEL  -0.35f

/* Платформа */
#define PLATFORM_SIZE    50
#define PLATFORM_Y       0
#define CHUNK_H          4
#define WORLD_BUF        (PLATFORM_SIZE)

/* Грани */
#define FACE_XP 0x01
#define FACE_XN 0x02
#define FACE_YP 0x04
#define FACE_YN 0x08
#define FACE_ZP 0x10
#define FACE_ZN 0x20

#define BLOCK_AIR   0
#define BLOCK_SOLID 1

/* Модели */
#define MAX_MODELS 64

typedef enum {
    STATE_MENU    = 0,
    STATE_PLAYING = 1
} GameState;

typedef enum {
    MODEL_NONE = 0,
    MODEL_TREE,
    MODEL_ROCK,
    MODEL_SNOWMAN,
    MODEL_HOUSE
} ModelType;

struct model_instance {
    ModelType type;
    float x, y, z;
    float rotY;
    float scale;
};

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;

    GLuint program;
    GLuint modelProgram;

    /* Кэш uniform — мир */
    GLint uMVP;
    GLint uCamPos;
    GLint uTex;

    /* Кэш uniform — модели */
    GLint umMVP;
    GLint umColor;

    GLuint texSnow;

    float camPos[3];
    float camRot[2]; /* [0]=pitch, [1]=yaw */
    float velY;

    float joyX, joyY;
    float moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;

    bool isMoving;
    int movePointerId;
    int lookPointerId;
    bool onGround;

    GLuint vbo;
    int visibleFaceCount;
    bool meshDirty;
    bool worldLoaded;

    unsigned char blocks[WORLD_BUF][CHUNK_H][WORLD_BUF];
    unsigned char faces[WORLD_BUF][CHUNK_H][WORLD_BUF];

    /* Модели на сцене */
    struct model_instance models[MAX_MODELS];
    int modelCount;

    GameState gameState;
};

#endif
