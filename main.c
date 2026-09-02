#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine.h"
#include "math_utils.h"
#include "world.h"
#include "physics.h"
#include "input.h"
#include "render.h"

#define LOG_TAG "Erasez"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display || !eng->surface) return;

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    if (eng->width <= 0 || eng->height <= 0) return;
    glViewport(0, 0, eng->width, eng->height);

    /* dt (секунды) */
    int64_t now = now_ms();
    float dt = eng->lastTickMs ? (float)(now - eng->lastTickMs) / 1000.0f : 0.016f;
    eng->lastTickMs = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    if (eng->gameState == STATE_MENU) {
        glClearColor(0.16f, 0.20f, 0.26f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw_ui(eng);
        eglSwapBuffers(eng->display, eng->surface);
        return;
    }

    apply_physics(eng, dt);
    glClear(GL_DEPTH_BUFFER_BIT);
    render_scene(eng);
    draw_ui(eng);
    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_init_gl(struct engine* eng) {
    if (eng->context == EGL_NO_CONTEXT) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eng->display == EGL_NO_DISPLAY) { LOGE("eglGetDisplay failed"); return; }
        if (!eglInitialize(eng->display, NULL, NULL)) { LOGE("eglInit failed"); return; }
        EGLConfig config;
        EGLint n;
        EGLint att[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_DEPTH_SIZE, 16, EGL_NONE };
        if (!eglChooseConfig(eng->display, att, &config, 1, &n) || n == 0) {
            LOGE("eglConfig failed"); return;
        }
        eng->eglConfig = config;
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        if (eng->context == EGL_NO_CONTEXT) { LOGE("eglContext failed"); return; }
    }

    if (eng->surface == EGL_NO_SURFACE) {
        eng->surface = eglCreateWindowSurface(eng->display, eng->eglConfig,
                                              eng->app->window, NULL);
        if (eng->surface == EGL_NO_SURFACE) { LOGE("eglSurface failed"); return; }
    }
    if (!eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context)) {
        LOGE("eglMakeCurrent failed"); return;
    }

    if (!eng->worldProgram) {
        init_game_programs(eng);
        if (!eng->worldReady) {
            w_gen(eng);
            LOGI("world generated: %dx%dx%d", WORLD_X, WORLD_Y, WORLD_Z);
        }
        build_world_mesh(eng);
    }
    LOGI("engine initialized");
}

static void engine_term_gl(struct engine* eng) {
    if (eng->display != EGL_NO_DISPLAY) {
        if (eng->context != EGL_NO_CONTEXT) {
            eglMakeCurrent(eng->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
        }
        if (eng->surface != EGL_NO_SURFACE) {
            eglDestroySurface(eng->display, eng->surface);
            eng->surface = EGL_NO_SURFACE;
        }
        if (eng->context != EGL_NO_CONTEXT) {
            eglDestroyContext(eng->display, eng->context);
            eng->context = EGL_NO_CONTEXT;
        }
        eglTerminate(eng->display);
        eng->display = EGL_NO_DISPLAY;
    }
    eng->worldProgram = 0;
    eng->skyProgram = 0;
    eng->uiProgram = 0;
    eng->worldVBO = 0;
    eng->skyVBO = 0;
    eng->worldVerts = 0;
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            engine_init_gl(eng);
            break;
        case APP_CMD_TERM_WINDOW:
            engine_term_gl(eng);
            break;
        default:
            break;
    }
}

void android_main(struct android_app* state) {
    struct engine eng;
    memset(&eng, 0, sizeof(eng));
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.jumpPointerId = -1;
    eng.downPointerId = -1;
    eng.gameState = STATE_MENU;
    eng.app = state;
    eng.lastTickMs = 0;
    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;

    while (1) {
        int ev;
        struct android_poll_source* s;
        while (ALooper_pollOnce(0, NULL, &ev, (void**)&s) >= 0) {
            if (s) s->process(state, s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
