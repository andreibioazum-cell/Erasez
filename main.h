#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "world.h"
#include "physics.h"
#include "input.h"
#include "render.h"

static const char* vertexShader =
    "attribute vec3 aPos;"
    "attribute vec2 aUV;"
    "attribute vec3 aNormal;"
    "varying vec2 vUV;"
    "varying vec3 vNormal;"
    "varying vec3 vWorldPos;"
    "uniform mat4 uMVP;"
    "uniform vec3 uCamPos;"
    "void main() {"
    "  gl_Position = uMVP * vec4(aPos, 1.0);"
    "  vUV = aUV;"
    "  vNormal = aNormal;"
    "  vWorldPos = aPos - uCamPos;"
    "}";

static const char* fragmentShader =
    "precision highp float;"
    "varying vec2 vUV;"
    "varying vec3 vNormal;"
    "varying vec3 vWorldPos;"
    "uniform sampler2D uTexture;"
    "uniform vec3 uCamPos;"
    "void main() {"
    "  vec4 color = texture2D(uTexture, vUV);"
    "  if (color.a < 0.5) discard;"
    "  "
    "  vec3 lightDir = normalize(vec3(0.5, 0.8, 0.3));"
    "  float diff = max(dot(vNormal, lightDir), 0.0);"
    "  float ambient = 0.35;"
    "  float light = ambient + diff * 0.65;"
    "  "
    "  float fogDist = length(vWorldPos);"
    "  float fog = clamp((fogDist - 10.0) / 30.0, 0.0, 0.85);"
    "  vec3 fogColor = vec3(0.53, 0.81, 0.98);"
    "  "
    "  vec3 finalColor = color.rgb * light;"
    "  finalColor = mix(finalColor, fogColor, fog);"
    "  gl_FragColor = vec4(finalColor, 1.0);"
    "}";

static void init_graphics(struct engine* eng) {
    // Шейдеры
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, NULL);
    glCompileShader(fs);
    
    eng->program = glCreateProgram();
    glAttachShader(eng->program, vs);
    glAttachShader(eng->program, fs);
    glLinkProgram(eng->program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // Текстура
    srand(time(NULL));
    eng->texAtlas = create_texture_atlas(eng);
    
    // UI шейдер
    init_ui_shader(eng);
}

static void engine_draw_frame(struct engine* eng) {
    if (!eng->display || !eng->surface) return;
    
    eglQuerySurface(eng->display, eng->surface, EGL_WIDTH, &eng->width);
    eglQuerySurface(eng->display, eng->surface, EGL_HEIGHT, &eng->height);
    glViewport(0, 0, eng->width, eng->height);
    
    // Физика
    apply_physics(eng);
    
    // Рендеринг
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    render_world(eng);
    draw_ui(eng);
    
    eglSwapBuffers(eng->display, eng->surface);
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eng->display, NULL, NULL);
        
        EGLConfig config;
        EGLint numConfigs;
        EGLint attrs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        eglChooseConfig(eng->display, attrs, &config, 1, &numConfigs);
        
        eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
        
        EGLint ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        eng->context = eglCreateContext(eng->display, config, NULL, ctxAttrs);
        eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);
        
        init_graphics(eng);
        init_world(eng);
        
        // Стартовая позиция
        eng->camPos[0] = WORLD_SIZE_X / 2.0f;
        eng->camPos[1] = 5.0f;
        eng->camPos[2] = WORLD_SIZE_Z / 2.0f;
    }
    
    if (cmd == APP_CMD_TERM_WINDOW) {
        if (eng->display) {
            eglMakeCurrent(eng->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eng->context) eglDestroyContext(eng->display, eng->context);
            if (eng->surface) eglDestroySurface(eng->display, eng->surface);
            eglTerminate(eng->display);
            eng->display = NULL;
            eng->surface = NULL;
            eng->context = NULL;
        }
    }
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    eng.movePointerId = -1;
    eng.lookPointerId = -1;
    eng.meshDirty = true;
    
    state->userData = &eng;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eng.app = state;
    
    while (1) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        engine_draw_frame(&eng);
    }
}
