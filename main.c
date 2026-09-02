#include <android_native_app_glue.h>
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
    if (!eng->app->window) return;

    /* dt (секунды) */
    int64_t now = now_ms();
    float dt = eng->lastTickMs ? (float)(now - eng->lastTickMs) / 1000.0f : 0.016f;
    eng->lastTickMs = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    if (eng->gameState == STATE_PLAYING) apply_physics(eng, dt);
    renderer_frame(eng);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (!eng->worldReady) {
                w_gen(eng);
                LOGI("world generated: %dx%dx%d", WORLD_X, WORLD_Y, WORLD_Z);
            }
            eng->lastTickMs = now_ms();
            renderer_init(eng);
            break;
        case APP_CMD_TERM_WINDOW:
            renderer_destroy(eng);
            eng->lastTickMs = 0;
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
