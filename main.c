#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "engine.h"
#include "world.h"
#include "math_utils.h"
#include "physics.h"
#include "input.h"
#include "render.h"

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display) return;

    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);

    if (eng->gameState == STATE_MENU) {
        glClearColor(0.15f, 0.15f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_menu(eng);
        glDisable(GL_BLEND);
        eglSwapBuffers(eng->display, eng->surface);
        return;
    }

    update_world(eng);
    apply_physics(eng);

    glClearColor(0.53f, 0.81f, 0.98f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_world(eng);
    draw_ui(eng);

    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;

    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, 0, 0);
        EGLConfig config; EGLint n;
        EGLint att[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_DEPTH_SIZE, 16, EGL_NONE
        };
        eglChooseConfig(eng->display, att, &config, 1, &n);
        eng->surface = eglCreateWindowSurface(eng->display, config,
                                               eng->app->window, NULL);
        EGLint ctxAtt[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAtt);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

        /* Шейдер мира — одна текстура */
        const char* vS =
            "attribute vec3 pos; attribute vec2 uv; attribute vec3 norm;"
            "varying vec2 vUV; varying vec3 vNorm; varying vec3 vWorldPos;"
            "uniform mat4 m;"
            "void main(){"
            "  gl_Position = m * vec4(pos, 1.0);"
            "  vUV = uv; vNorm = norm; vWorldPos = pos;"
            "}";
        const char* fS =
            "precision mediump float;"
            "varying vec2 vUV; varying vec3 vNorm; varying vec3 vWorldPos;"
            "uniform vec3 camPos; uniform sampler2D tex;"
            "void main(){"
            "  vec4 tc = texture2D(tex, vUV);"
            "  vec3 sun = normalize(vec3(0.35, 0.85, 0.4));"
            "  float diff = max(dot(vNorm, sun), 0.0);"
            "  float amb = 0.45;"
            "  float fb = 0.0;"
            "  if(vNorm.y > 0.5) fb = 0.1;"
            "  if(vNorm.y < -0.5) fb = -0.1;"
            "  float light = clamp(amb + diff * 0.55 + fb, 0.2, 1.0);"
            "  vec3 lit = tc.rgb * light;"
            "  float dist = length((vWorldPos - camPos).xz);"
            "  float fog = clamp((dist - 30.0) / 30.0, 0.0, 0.8);"
            "  gl_FragColor = vec4(mix(lit, vec3(0.53, 0.81, 0.98), fog), 1.0);"
            "}";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vS, NULL); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fS, NULL); glCompileShader(fs);

        eng->program = glCreateProgram();
        glBindAttribLocation(eng->program, 0, "pos");
        glBindAttribLocation(eng->program, 1, "uv");
        glBindAttribLocation(eng->program, 2, "norm");
        glAttachShader(eng->program, vs);
        glAttachShader(eng->program, fs);
        glLinkProgram(eng->program);
        glDeleteShader(vs); glDeleteShader(fs);

        /* Кэш uniform */
        eng->uMVP = glGetUniformLocation(eng->program, "m");
        eng->uCamPos = glGetUniformLocation(eng->program, "camPos");
        eng->uTex = glGetUniformLocation(eng->program, "tex");

        init_textures(eng);
        init_ui_shader();

    } else if (cmd == APP_CMD_TERM_WINDOW) {
        if (eng->display != EGL_NO_DISPLAY) {
            eglMakeCurrent(eng->display, EGL_NO_SURFACE,
                           EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eng->context != EGL_NO_CONTEXT)
                eglDestroyContext(eng->display, eng->context);
            if (eng->surface != EGL_NO_SURFACE)
                eglDestroySurface(eng->display, eng->surface);
            eng->display = EGL_NO_DISPLAY;
            eng->surface = EGL_NO_SURFACE;
            eng->context = EGL_NO_CONTEXT;
        }
    }
}

void android_main(struct android_app* state) {
    struct engine eng;
    memset(&eng, 0, sizeof(eng));

    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.worldLoaded = false;
    eng.meshDirty = true;
    eng.gameState = STATE_MENU;
    eng.camPos[0] = (float)(PLATFORM_SIZE / 2);
    eng.camPos[1] = (float)(PLATFORM_Y) + EYE_H + 1.0f;
    eng.camPos[2] = -(float)(PLATFORM_SIZE / 2);

    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;

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
