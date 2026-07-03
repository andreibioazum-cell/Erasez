#ifndef ENGINE_H
#define ENGINE_H

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#define JOY_RADIUS      80.0f
#define JOY_X_OFFSET    130.0f
#define JOY_Y_OFFSET    130.0f
#define STICK_RADIUS    32.0f

#define JUMP_BTN_SIZE   80.0f
#define JUMP_BTN_OFFSET 130.0f

#define PI              3.14159265f

#define PLAYER_W        0.3f
#define PLAYER_H        1.8f
#define EYE_H           1.6f
#define HEAD_MARGIN     0.1f
#define GAME_FOV        1.2f

#define GRAVITY         0.006f
#define JUMP_FORCE      0.13f
#define TERM_VEL       -0.3f

#define CAM_DIST        5.0f
#define CAM_HEIGHT      2.5f
#define CAM_PITCH       0.35f

#define PLATFORM_SIZE   50.0f
#define PLATFORM_Y      0.0f

#define STATE_MENU      0
#define STATE_PLAYING   1

#define ANIM_SPEED      0.12f

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width, height;

    GLuint colorProgram;
    GLuint texProgram;
    GLuint texPlatform;

    float playerPos[3];
    float playerRot;
    float velY;

    float moveDirX, moveDirZ;
    float lastTouchX, lastTouchY;

    bool isMoving;
    int movePointerId;
    int lookPointerId;
    bool onGround;
    bool joyTouched;

    float camRotY;

    float animTime;
    float animLegAngle;
    float animArmAngle;

    int gameState;
};

#endif
