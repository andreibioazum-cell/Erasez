#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>

/* ================= ВОКСЕЛЬНЫЙ МИР ================= */
#define WORLD_X  96            /* блоков по X            */
#define WORLD_Y  44            /* блоков по Y (высота)   */
#define WORLD_Z  96            /* блоков по Z            */

enum {
    BLOCK_AIR    = 0,
    BLOCK_GRASS  = 1,
    BLOCK_DIRT   = 2,
    BLOCK_STONE  = 3,
    BLOCK_SAND   = 4,
    BLOCK_SNOW   = 5,
    BLOCK_WOOD   = 6,
    BLOCK_LEAVES = 7
};

#define PI 3.14159265f

/* ================= РАСКЛАДКА УПРАВЛЕНИЯ ================= */
/* (всё в пикселях экрана; y отсчитывается сверху) */
#define JOY_OFFSET     150.0f  /* база джойстика: центр (JOY_OFFSET, H-JOY_OFFSET) */
#define JOY_RADIUS     88.0f
#define STICK_RADIUS   32.0f
#define BTN_RADIUS     46.0f   /* прыжок/спуск */
#define BTN_OFFSET     150.0f  /* правый нижний угол (прыжок) */
#define BTN_GAP        112.0f  /* шаг между кнопками по вертикали/горизонтали */
#define FLY_RADIUS     34.0f

#define STATE_MENU     0
#define STATE_PLAYING  1

/* ================= ИГРОК (мир = блоки по 1.0) ================= */
#define P_HALF_W    0.30f      /* половина ширины коллизии (XZ) */
#define P_HEIGHT    1.80f      /* рост (ноги-голова) */
#define EYE_H       1.62f      /* высота глаз */
#define WALK_SPEED  5.4f       /* м/с */
#define FLY_SPEED   7.0f       /* м/с в полёте */
#define FLY_VERT    6.5f       /* м/с вверх/вниз в полёте */
#define GRAVITY     24.0f      /* м/с^2 */
#define JUMP_V      8.6f       /* начальная скорость прыжка (~1.5 блока) */
#define TERMINAL_V -18.0f
#define MAX_STEP    1.05f      /* автоподъём ступеньки */
#define GAME_FOV    1.15f      /* радианы */

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig eglConfig;
    int32_t width, height;

    GLuint worldProgram, skyProgram, uiProgram;
    GLuint worldVBO, skyVBO;
    int worldVerts;

    unsigned char* blocks;      /* [y][z][x] -> y*WORLD_Z*WORLD_X + z*WORLD_X + x */

    float px, py, pz;           /* позиция ног игрока */
    float yaw, pitch;           /* взгляд */
    float velY;
    float spawnY;
    float moveDirX, moveDirZ;   /* джойстик: -1..1 */
    float lastLookX, lastLookY;
    float walkPhase;

    bool onGround;
    bool flying;
    bool jumpHeld;
    bool downHeld;
    bool joyTouched;
    bool isMoving;
    int movePointerId;
    int lookPointerId;
    int jumpPointerId;
    int downPointerId;
    int gameState;
    bool worldReady;

    int64_t lastTickMs;
};

#endif
