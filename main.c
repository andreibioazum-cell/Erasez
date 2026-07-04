#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <stdlib.h>

#include "engine.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

#define LOG_TAG "Game"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;
    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);

    if (eng->gameState == STATE_MENU) {
        glClearColor(0.15f,0.15f,0.2f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        draw_menu(eng); glDisable(GL_BLEND);
        eglSwapBuffers(eng->display,eng->surface);
        return;
    }

    apply_physics(eng);
    glClearColor(0.53f,0.81f,0.98f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    render_world(eng);
    draw_ui(eng);
    eglSwapBuffers(eng->display,eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eng->display==EGL_NO_DISPLAY){LOGE("eglGetDisplay failed");return;}
        if (!eglInitialize(eng->display,NULL,NULL)){LOGE("eglInit failed");return;}
        EGLConfig config; EGLint n;
        EGLint att[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_DEPTH_SIZE,16,EGL_NONE};
        if (!eglChooseConfig(eng->display,att,&config,1,&n)||n==0){LOGE("eglConfig failed");return;}
        eng->surface=eglCreateWindowSurface(eng->display,config,eng->app->window,NULL);
        if (eng->surface==EGL_NO_SURFACE){LOGE("eglSurface failed");return;}
        EGLint ctxAtt[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        eng->context=eglCreateContext(eng->display,config,NULL,ctxAtt);
        if (eng->context==EGL_NO_CONTEXT){LOGE("eglContext failed");return;}
        if (!eglMakeCurrent(eng->display,eng->surface,eng->surface,eng->context)){LOGE("eglMakeCurrent failed");return;}

        init_color_shader(eng);
        init_tex_shader(eng);
        init_ui_shader();
        init_cube_vbo();
        init_platform_vbo();
        init_textures(eng);
        init_world_blocks(eng);
        LOGI("Engine initialized");
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.gameState = STATE_MENU;
    eng.joyTouched = false;
    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

    while (1) {
        int ev; struct android_poll_source* s;
        while (ALooper_pollOnce(0,NULL,&ev,(void**)&s) >= 0) {
            if (s) s->process(state,s);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
