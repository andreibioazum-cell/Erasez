#ifndef RENDER_H
#define RENDER_H

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <android/log.h>
#include "engine.h"
#include "world.h"
#include "math_utils.h"

#define LOG_TAG "Render"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* ============================================================
   WebGPU-рендер (wgpu-native, C API webgpu.h + WGSL).
   Один рендер-пасс за кадр: небо -> мир -> UI.
   Мир и небо рисуются через общий uniform (виртуальная камера),
   небо дополнительно через второй uniform с базисом камеры.
   ============================================================ */

/* ---------------- WGSL ---------------- */

static const char* WGSL_WORLD_VS =
    "struct Uniforms { mvp: mat4x4<f32>, eye: vec3<f32>, viewport: vec2<f32> };\n"
    "@group(0) @binding(0) var<uniform> ub: Uniforms;\n"
    "struct VOut { @builtin(position) pos: vec4<f32>,"
    " @location(0) world: vec3<f32>, @location(1) nrm: vec3<f32>,"
    " @location(2) col: vec3<f32> };\n"
    "@vertex fn vs_main(@location(0) aPos: vec3<f32>,"
    " @location(1) aNorm: vec4<f32>, @location(2) aCol: vec4<f32>) -> VOut {\n"
    "  var out: VOut;\n"
    "  out.pos = ub.mvp * vec4<f32>(aPos, 1.0);\n"
    "  out.world = aPos;\n"
    "  out.nrm = aNorm.xyz;\n"
    "  out.col = aCol.rgb;\n"
    "  return out;\n"
    "}\n";

static const char* WGSL_WORLD_FS =
    "struct FIn { @location(0) world: vec3<f32>,"
    " @location(1) nrm: vec3<f32>, @location(2) col: vec3<f32> };\n"
    "struct Uniforms { mvp: mat4x4<f32>, eye: vec3<f32>, viewport: vec2<f32> };\n"
    "@group(0) @binding(0) var<uniform> ub: Uniforms;\n"
    "@fragment fn fs_main(in: FIn) -> @location(0) vec4<f32> {\n"
    "  let n = normalize(in.nrm);\n"
    "  let sun = normalize(vec3<f32>(0.45, 0.68, 0.58));\n"
    "  let dif = max(dot(n, sun), 0.0);\n"
    "  let light = clamp(0.56 + dif * 0.62, 0.34, 1.2);\n"
    "  var col = in.col * light;\n"
    "  let d = distance(in.world, ub.eye);\n"
    "  let fog = clamp((d - 24.0) / 42.0, 0.0, 0.96);\n"
    "  let sh = vec3<f32>(0.72, 0.83, 0.94);\n"
    "  return vec4<f32>(mix(col, sh, fog), 1.0);\n"
    "}\n";

static const char* WGSL_SKY_VS =
    "@vertex fn vs_main(@builtin(vertex_index) vi: u32)"
    " -> @builtin(position) vec4<f32> {\n"
    "  var p = vec2<f32>(-1.0, -1.0);\n"
    "  if (vi == 1u) { p = vec2<f32>(3.0, -1.0); }\n"
    "  if (vi == 2u) { p = vec2<f32>(-1.0, 3.0); }\n"
    "  return vec4<f32>(p, 0.5, 1.0);\n"
    "}\n";

static const char* WGSL_SKY_FS =
    "struct SkyUB { fov: vec2<f32>, pad0: vec2<f32>, fwd: vec4<f32>,"
    " right: vec4<f32>, up: vec4<f32>, sun: vec4<f32>,"
    " viewport: vec4<f32> };\n"
    "@group(0) @binding(0) var<uniform> ub: SkyUB;\n"
    "@fragment fn fs_main(@builtin(position) frag: vec4<f32>)"
    " -> @location(0) vec4<f32> {\n"
    "  let vp = ub.viewport.xy;\n"
    "  let ndc = vec2<f32>(frag.x / vp.x * 2.0 - 1.0,"
    "                      1.0 - frag.y / vp.y * 2.0);\n"
    "  let d = normalize(ub.fwd.xyz + ub.right.xyz * ndc.x * ub.fov.y\n"
    "                    + ub.up.xyz * ndc.y * ub.fov.x);\n"
    "  let horizon = vec3<f32>(0.72, 0.83, 0.94);\n"
    "  let zenith = vec3<f32>(0.28, 0.52, 0.92);\n"
    "  let h = clamp(d.y * 1.7 + 0.18, 0.0, 1.0);\n"
    "  var col = mix(horizon, zenith, h * h);\n"
    "  if (d.y < 0.0) {\n"
    "    let gnd = vec3<f32>(0.46, 0.56, 0.64);\n"
    "    col = mix(col, gnd, clamp(-d.y * 2.2, 0.0, 1.0));\n"
    "  }\n"
    "  let s = max(dot(d, normalize(ub.sun.xyz)), 0.0);\n"
    "  col += vec3<f32>(1.0, 0.95, 0.8) * pow(s, 140.0) * 2.2;\n"
    "  col += vec3<f32>(1.0, 0.97, 0.88) * pow(s, 10.0) * 0.22;\n"
    "  return vec4<f32>(col, 1.0);\n"
    "}\n";

static const char* WGSL_UI_VS =
    "struct VOut { @builtin(position) pos: vec4<f32>,"
    " @location(0) col: vec4<f32> };\n"
    "@vertex fn vs_main(@location(0) aPos: vec2<f32>,"
    " @location(1) aCol: vec4<f32>) -> VOut {\n"
    "  var out: VOut;\n"
    "  out.pos = vec4<f32>(aPos, 0.5, 1.0);\n"
    "  out.col = aCol;\n"
    "  return out;\n"
    "}\n";

static const char* WGSL_UI_FS =
    "struct FIn { @location(0) col: vec4<f32> };\n"
    "@fragment fn fs_main(in: FIn) -> @location(0) vec4<f32> {\n"
    "  return in.col;\n"
    "}\n";

/* ---------------- шейдер-модуль ---------------- */

static WGPUShaderModule wg_create_shader(WGPUDevice d, const char* src,
                                         const char* name) {
    WGPUShaderSourceWGSL wgsl = {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = { (char*)src, WGPU_STRLEN },
    };
    return wgpuDeviceCreateShaderModule(
        d, &(const WGPUShaderModuleDescriptor){
               .label = (WGPUStringView){ (char*)name, WGPU_STRLEN },
               .nextInChain = (WGPUChainedStruct*)&wgsl,
           });
}

/* ---------------- uniform-буферы ---------------- */

typedef struct {              /* std140, 96 байт */
    float mvp[16];            /*   0 */
    float eye[4];             /*  64 (x,y,z,pad) */
    float viewport[4];        /*  80 (w,h,pad,pad) */
} CamUB;

typedef struct {              /* std140, 96 байт */
    float fov[4];             /*   0 (tanY, tanY*aspect, pad, pad) */
    float fwd[4];             /*  16 */
    float right[4];           /*  32 */
    float up[4];              /*  48 */
    float sun[4];             /*  64 */
    float viewport[4];        /*  80 (w,h,pad,pad) */
} SkyUB;

static void fill_uniforms(struct engine* e) {
    float w = (float)e->width, h = (float)e->height;
    if (w < 1.0f || h < 1.0f) return;
    float aspect = w / h;

    float cp = cosf(e->pitch), sp = sinf(e->pitch);
    float fx = sinf(e->yaw), fz = -cosf(e->yaw);
    float dir[3] = { fx * cp, sp, fz * cp };

    float bob = 0.0f;
    if (e->onGround && e->isMoving) bob = 0.05f * sinf(e->walkPhase * 2.0f);
    float ex = e->px, ey = e->py + EYE_H + bob, ez = e->pz;

    float right[3] = { cosf(e->yaw), 0.0f, sinf(e->yaw) };
    float up[3];
    up[0] = -right[2] * dir[1];
    up[1] = right[2] * dir[0] - right[0] * dir[2];
    up[2] = right[0] * dir[1];
    float ul = sqrtf(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    if (ul < 0.0001f) { up[0] = 0; up[1] = 1; up[2] = 0; ul = 1; }
    up[0] /= ul; up[1] /= ul; up[2] /= ul;

    float proj[16], view[16], vp[16];
    mat4_perspective(proj, GAME_FOV, aspect, 0.1f, 320.0f);
    float tx = ex + dir[0], ty = ey + dir[1], tz = ez + dir[2];
    mat4_lookat_pos(view, ex, ey, ez, tx, ty, tz);
    mat4_mul(vp, proj, view);

    CamUB cam;
    memset(&cam, 0, sizeof(cam));
    memcpy(cam.mvp, vp, 64);
    cam.eye[0] = ex; cam.eye[1] = ey; cam.eye[2] = ez;
    cam.viewport[0] = w; cam.viewport[1] = h;
    wgpuQueueWriteBuffer(e->wg.queue, e->wg.ubCam, 0, &cam, sizeof(cam));

    SkyUB sky;
    memset(&sky, 0, sizeof(sky));
    float tanY = tanf(GAME_FOV * 0.5f);
    sky.fov[0] = tanY;
    sky.fov[1] = tanY * aspect;
    memcpy(sky.fwd, dir, 12);
    memcpy(sky.right, right, 12);
    memcpy(sky.up, up, 12);
    sky.sun[0] = 0.45f; sky.sun[1] = 0.68f; sky.sun[2] = 0.58f;
    sky.viewport[0] = w; sky.viewport[1] = h;
    wgpuQueueWriteBuffer(e->wg.queue, e->wg.ubSky, 0, &sky, sizeof(sky));
}

/* ---------------- меш мира ----------------
   Формат вершины (20 байт):
   pos float32x3 @0, normal snorm8x4 @12, color unorm8x4 @16.     */

typedef struct { uint8_t* d; size_t n; size_t cap; } MBuf;

static void mb_need(MBuf* m, size_t add) {
    if (m->n + add <= m->cap) return;
    size_t c = m->cap ? m->cap : (1 << 20);
    while (c < m->n + add) c *= 2;
    m->d = (uint8_t*)realloc(m->d, c);
    m->cap = c;
}

static void mb_push(MBuf* m, const void* p, size_t s) {
    mb_need(m, s);
    memcpy(m->d + m->n, p, s);
    m->n += s;
}

static void mb_vert(MBuf* m, float x, float y, float z,
                    float nx, float ny, float nz,
                    float r, float g, float b) {
    float f[3] = { x, y, z };
    int8_t norm[4] = { (int8_t)(nx * 127), (int8_t)(ny * 127),
                       (int8_t)(nz * 127), 0 };
    uint8_t col[4] = { (uint8_t)(r * 255 + 0.5f),
                       (uint8_t)(g * 255 + 0.5f),
                       (uint8_t)(b * 255 + 0.5f), 255 };
    mb_push(m, f, 12);
    mb_push(m, norm, 4);
    mb_push(m, col, 4);
}

static void block_color(int type, int axis, int sign,
                        float* r, float* g, float* b) {
    float top = (axis == 1 && sign > 0) ? 1.0f : 0.0f;
    switch (type) {
        case BLOCK_GRASS:
            if (top) { *r = 0.40f; *g = 0.70f; *b = 0.30f; }
            else     { *r = 0.58f; *g = 0.44f; *b = 0.29f; }
            break;
        case BLOCK_DIRT:  *r = 0.60f; *g = 0.45f; *b = 0.30f; break;
        case BLOCK_STONE: *r = 0.56f; *g = 0.57f; *b = 0.61f; break;
        case BLOCK_SAND:  *r = 0.93f; *g = 0.86f; *b = 0.62f; break;
        case BLOCK_SNOW:
            *r = 0.93f; *g = 0.95f; *b = 1.0f;
            if (!top) { *r *= 0.94f; *g *= 0.96f; }
            break;
        case BLOCK_WOOD:
            if (axis == 1) { *r = 0.62f; *g = 0.48f; *b = 0.28f; }
            else           { *r = 0.48f; *g = 0.34f; *b = 0.20f; }
            break;
        case BLOCK_LEAVES:
            *r = 0.21f; *g = 0.50f; *b = 0.16f;
            if (top) { *r *= 1.15f; *g *= 1.15f; }
            break;
        default: *r = 0.9f; *g = 0.2f; *b = 0.9f; break;
    }
}

/* Одна грань куба. a - ось (0=X,1=Y,2=Z), s - сторона (+-1). */
static void mb_face(MBuf* m, int bx, int by, int bz, int a, int s,
                    float tint, int type) {
    static const int UAX[3] = { 1, 2, 0 };
    static const int VAX[3] = { 2, 0, 1 };
    float uu[3] = { 0, 0, 0 }, vv[3] = { 0, 0, 0 };
    uu[UAX[a]] = 1.0f;
    vv[VAX[a]] = 1.0f;

    float cx = (float)bx + 0.5f - WORLD_X * 0.5f;
    float cy = (float)by + 0.5f;
    float cz = (float)bz + 0.5f - WORLD_Z * 0.5f;

    float nrm[3] = { 0, 0, 0 };
    nrm[a] = (float)s;

    float r, g, b;
    block_color(type, a, s, &r, &g, &b);
    r *= tint; g *= tint; b *= tint;
    if (s < 0) { r *= 0.72f; g *= 0.72f; b *= 0.72f; }

    /* Углы грани в порядке, дающем CCW наружу при y-up NDC.
       (проекция в рендере переворачивает Y для WebGPU). */
    int su[4] = { -1, 1, 1, -1 };
    int sv[4] = { -1, -1, 1, 1 };
    float c[4][3];
    for (int i = 0; i < 4; i++) {
        c[i][0] = cx + 0.5f * ((float)su[i] * uu[0] + (float)sv[i] * vv[0]);
        c[i][1] = cy + 0.5f * ((float)su[i] * uu[1] + (float)sv[i] * vv[1]);
        c[i][2] = cz + 0.5f * ((float)su[i] * uu[2] + (float)sv[i] * vv[2]);
    }

    int order[6];
    if (s > 0) { int o[6] = { 0, 1, 2, 0, 2, 3 }; memcpy(order, o, 24); }
    else       { int o[6] = { 0, 3, 2, 0, 2, 1 }; memcpy(order, o, 24); }
    for (int i = 0; i < 6; i++) {
        int v = order[i];
        mb_vert(m, c[v][0], c[v][1], c[v][2], nrm[0], nrm[1], nrm[2], r, g, b);
    }
}

/* Полная сборка статичного меша (после генерации мира). */
static void build_world_mesh(struct engine* e) {
    MBuf m = { 0 };
    for (int y = 0; y < WORLD_Y; y++)
        for (int z = 0; z < WORLD_Z; z++)
            for (int x = 0; x < WORLD_X; x++) {
                int b = world_get(e, x, y, z);
                if (b == BLOCK_AIR) continue;
                float tint = 0.90f + 0.20f * w_rand01(x, z);
                for (int a = 0; a < 3; a++)
                    for (int s = -1; s <= 1; s += 2) {
                        int dx = 0, dy = 0, dz = 0;
                        if (a == 0) dx = s; else if (a == 1) dy = s; else dz = s;
                        if (world_get(e, x + dx, y + dy, z + dz) != BLOCK_AIR)
                            continue;
                        mb_face(&m, x, y, z, a, s, tint, b);
                    }
            }
    e->wg.worldVerts = (int)(m.n / 20);
    LOGI("world verts: %d", e->wg.worldVerts);

    if (e->wg.bufWorld) { wgpuBufferRelease(e->wg.bufWorld); e->wg.bufWorld = NULL; }
    if (m.n > 0 && e->wg.device) {
        e->wg.bufWorld = wgpuDeviceCreateBuffer(
            e->wg.device, &(const WGPUBufferDescriptor){
                              .usage = WGPUBufferUsage_Vertex |
                                       WGPUBufferUsage_CopyDst,
                              .size = (uint64_t)m.n,
                          });
        if (e->wg.bufWorld)
            wgpuQueueWriteBuffer(e->wg.queue, e->wg.bufWorld, 0, m.d, m.n);
    }
    free(m.d);
}

/* ---------------- UI (2D, NDC с y вверх; ui_vert перевернёт Y) ---------------- */

typedef struct { uint8_t* d; size_t n; size_t cap; } UBuf;

static void ub_need(UBuf* b, size_t add) {
    if (b->n + add <= b->cap) return;
    size_t c = b->cap ? b->cap : (1 << 16);
    while (c < b->n + add) c *= 2;
    b->d = (uint8_t*)realloc(b->d, c);
    b->cap = c;
}

static void ui_vert(UBuf* b, float x, float y,
                    float r, float g, float bl, float a) {
    uint8_t v[12];
    float f[2] = { x, -y };    /* WebGPU: NDC y вниз */
    memcpy(v, f, 8);
    v[8] = (uint8_t)(r * 255 + 0.5f);
    v[9] = (uint8_t)(g * 255 + 0.5f);
    v[10] = (uint8_t)(bl * 255 + 0.5f);
    v[11] = (uint8_t)(a * 255 + 0.5f);
    ub_need(b, 12);
    memcpy(b->d + b->n, v, 12);
    b->n += 12;
}

static void ui_rect(UBuf* b, float cx, float cy, float hw, float hh,
                    int sw, int sh, float r, float g, float bl, float a) {
    float nx = (cx / sw) * 2.0f - 1.0f, ny = 1.0f - (cy / sh) * 2.0f;
    float rx = (hw / sw) * 2.0f, ry = (hh / sh) * 2.0f;
    ui_vert(b, nx - rx, ny - ry, r, g, bl, a);
    ui_vert(b, nx + rx, ny - ry, r, g, bl, a);
    ui_vert(b, nx + rx, ny + ry, r, g, bl, a);
    ui_vert(b, nx - rx, ny - ry, r, g, bl, a);
    ui_vert(b, nx + rx, ny + ry, r, g, bl, a);
    ui_vert(b, nx - rx, ny + ry, r, g, bl, a);
}

static void ui_tri(UBuf* b, float cx, float cy, float sx, float sy, int dir,
                   int sw, int sh, float r, float g, float bl, float a) {
    float nx = (cx / sw) * 2 - 1, ny = 1 - (cy / sh) * 2;
    float ax = (sx / sw) * 2, ay = (sy / sh) * 2;
    if (dir == 1) {   /* стрелка вверх (в GL-NDC y вверх) */
        ui_vert(b, nx, ny + ay, r, g, bl, a);
        ui_vert(b, nx - ax, ny - ay * 0.6f, r, g, bl, a);
        ui_vert(b, nx + ax, ny - ay * 0.6f, r, g, bl, a);
    } else {
        ui_vert(b, nx, ny - ay, r, g, bl, a);
        ui_vert(b, nx - ax, ny + ay * 0.6f, r, g, bl, a);
        ui_vert(b, nx + ax, ny + ay * 0.6f, r, g, bl, a);
    }
}

static void ui_circle(UBuf* b, float cx, float cy, float r, int sw, int sh,
                      float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2 - 1, ny = 1 - (cy / sh) * 2;
    float rx = (r / sw) * 2, ry = (r / sh) * 2;
    int segs = 26;
    float prevX = nx + rx, prevY = ny;
    for (int i = 1; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        float px = nx + cosf(a) * rx, py = ny + sinf(a) * ry;
        ui_vert(b, nx, ny, cr, cg, cb, ca);
        ui_vert(b, prevX, prevY, cr, cg, cb, ca);
        ui_vert(b, px, py, cr, cg, cb, ca);
        prevX = px; prevY = py;
    }
}

static void ui_ring(UBuf* b, float cx, float cy, float r, float thick,
                    int sw, int sh, float cr, float cg, float cb, float ca) {
    float nx = (cx / sw) * 2 - 1, ny = 1 - (cy / sh) * 2;
    float ro = (r / sw) * 2, ri = ((r - thick) / sw) * 2;
    float ro2 = (r / sh) * 2, ri2 = ((r - thick) / sh) * 2;
    int segs = 30;
    float o0x = nx + ro, o0y = ny, i0x = nx + ri, i0y = ny;
    for (int i = 1; i <= segs; i++) {
        float a = (float)i / segs * 2.0f * PI;
        float c = cosf(a), s = sinf(a);
        float o1x = nx + c * ro, o1y = ny + s * ro2;
        float i1x = nx + c * ri, i1y = ny + s * ri2;
        ui_vert(b, o0x, o0y, cr, cg, cb, ca);
        ui_vert(b, i0x, i0y, cr, cg, cb, ca);
        ui_vert(b, o1x, o1y, cr, cg, cb, ca);
        ui_vert(b, o1x, o1y, cr, cg, cb, ca);
        ui_vert(b, i0x, i0y, cr, cg, cb, ca);
        ui_vert(b, i1x, i1y, cr, cg, cb, ca);
        o0x = o1x; o0y = o1y; i0x = i1x; i0y = i1y;
    }
}

/* ---------------- UI-контент ---------------- */

static void ui_menu(struct engine* e, UBuf* b) {
    int sw = e->width, sh = e->height;
    ui_rect(b, sw / 2.0f, sh / 2.0f, sw / 2.0f, sh / 2.0f, sw, sh,
            0.10f, 0.13f, 0.18f, 0.92f);

    float ly = sh * 0.30f;
    float bx = sw / 2.0f;
    ui_rect(b, bx, ly + 24, 150, 34, sw, sh, 0.16f, 0.24f, 0.34f, 1.0f);
    for (int i = -3; i <= 3; i++) {
        float hgt = 8.0f + (i < 0 ? -i * 4.0f : i * 4.0f) * 0.5f;
        ui_rect(b, bx + i * 20, ly + 24 - hgt * 0.5f - 17, 10, hgt, sw, sh,
                0.42f, 0.72f, 0.32f, 1.0f);
    }

    float playX = bx, playY = sh * 0.58f;
    ui_circle(b, playX, playY, 56, sw, sh, 0.13f, 0.55f, 0.22f, 1.0f);
    ui_ring(b, playX, playY, 56, 4, sw, sh, 0.45f, 1.0f, 0.5f, 1.0f);
    ui_tri(b, playX, playY, 18, 20, 1, sw, sh, 1, 1, 1, 1.0f);
}

static void ui_controls(struct engine* e, UBuf* b) {
    int sw = e->width, sh = e->height;

    float cx = sw * 0.5f, cy = sh * 0.5f;
    ui_rect(b, cx - 8, cy, 2.5f, 1.2f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(b, cx + 6, cy, 2.5f, 1.2f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(b, cx, cy - 8, 1.2f, 2.5f, sw, sh, 1, 1, 1, 0.85f);
    ui_rect(b, cx, cy + 6, 1.2f, 2.5f, sw, sh, 1, 1, 1, 0.85f);

    float jx = JOY_OFFSET, jy = sh - JOY_OFFSET;
    ui_circle(b, jx, jy, JOY_RADIUS, sw, sh, 0.08f, 0.10f, 0.14f, 0.30f);
    ui_ring(b, jx, jy, JOY_RADIUS, 3, sw, sh, 1, 1, 1, 0.35f);
    ui_circle(b, jx + e->moveDirX * JOY_RADIUS * 0.6f,
              jy + e->moveDirZ * JOY_RADIUS * 0.6f,
              STICK_RADIUS, sw, sh, 1, 1, 1, 0.55f);

    float bx = sw - BTN_OFFSET, by = sh - BTN_OFFSET;
    ui_circle(b, bx, by, BTN_RADIUS, sw, sh, 0.08f, 0.10f, 0.14f, 0.32f);
    ui_ring(b, bx, by, BTN_RADIUS, 3, sw, sh, 1, 1, 1, 0.4f);
    ui_tri(b, bx, by, 16, 18, 1, sw, sh, 1, 1, 1, 0.95f);

    if (e->flying) {
        float dx2 = bx - BTN_GAP, dy2 = by;
        ui_circle(b, dx2, dy2, BTN_RADIUS * 0.9f, sw, sh, 0.08f, 0.10f, 0.14f, 0.32f);
        ui_ring(b, dx2, dy2, BTN_RADIUS * 0.9f, 3, sw, sh, 1, 1, 1, 0.4f);
        ui_tri(b, dx2, dy2, 15, 17, -1, sw, sh, 1, 1, 1, 0.95f);
    }

    float fx2 = bx, fy2 = by - BTN_GAP;
    ui_circle(b, fx2, fy2, FLY_RADIUS, sw, sh, 0.10f, 0.18f, 0.30f,
              e->flying ? 0.75f : 0.35f);
    ui_ring(b, fx2, fy2, FLY_RADIUS, 3, sw, sh, 1, 1, 1, 0.55f);
    ui_tri(b, fx2, fy2 - 4, 9, 9, 1, sw, sh, 1, 1, 1, 0.95f);
    ui_tri(b, fx2, fy2 + 4, 9, 9, -1, sw, sh, 1, 1, 1, 0.95f);
}

/* ============================================================
   Инициализация устройств/пайплайнов
   ============================================================ */

static WGPUAdapter s_adapter;
static WGPUDevice s_device;

static void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                       WGPUStringView message, void* userdata1,
                       void* userdata2) {
    (void)userdata1; (void)userdata2;
    if (status == WGPURequestAdapterStatus_Success) s_adapter = adapter;
    else LOGE("requestAdapter failed: %.*s", (int)message.length,
              message.data ? message.data : "");
}

static void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
                      WGPUStringView message, void* userdata1,
                      void* userdata2) {
    (void)userdata1; (void)userdata2;
    if (status == WGPURequestDeviceStatus_Success) s_device = device;
    else LOGE("requestDevice failed: %.*s", (int)message.length,
              message.data ? message.data : "");
}

static WGPUDepthStencilState depth_state(bool write, WGPUCompareFunction cmp) {
    WGPUDepthStencilState s;
    memset(&s, 0, sizeof(s));
    s.format = WGPUTextureFormat_Depth24Plus;
    s.depthWriteEnabled = write ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    s.depthCompare = cmp;
    s.stencilFront.compare = WGPUCompareFunction_Always;
    s.stencilBack.compare = WGPUCompareFunction_Always;
    s.stencilReadMask = 0xFF;
    s.stencilWriteMask = 0xFF;
    return s;
}

static void recreate_depth(struct engine* e, uint32_t w, uint32_t h) {
    if (e->wg.viewDepth) { wgpuTextureViewRelease(e->wg.viewDepth); e->wg.viewDepth = NULL; }
    if (e->wg.texDepth) { wgpuTextureRelease(e->wg.texDepth); e->wg.texDepth = NULL; }
    if (w == 0 || h == 0) return;
    e->wg.texDepth = wgpuDeviceCreateTexture(
        e->wg.device, &(const WGPUTextureDescriptor){
                          .usage = WGPUTextureUsage_RenderAttachment,
                          .dimension = WGPUTextureDimension_2D,
                          .size = (WGPUExtent3D){ w, h, 1 },
                          .format = WGPUTextureFormat_Depth24Plus,
                          .mipLevelCount = 1,
                          .sampleCount = 1,
                      });
    if (e->wg.texDepth)
        e->wg.viewDepth = wgpuTextureCreateView(e->wg.texDepth, NULL);
}

static WGPUBindGroup make_bg(struct engine* e, WGPUBuffer ub) {
    return wgpuDeviceCreateBindGroup(
        e->wg.device, &(const WGPUBindGroupDescriptor){
                          .layout = e->wg.bgl,
                          .entryCount = 1,
                          .entries = (const WGPUBindGroupEntry[]){
                              (const WGPUBindGroupEntry){
                                  .binding = 0,
                                  .buffer = ub,
                                  .offset = 0,
                                  .size = 0,
                              },
                          },
                      });
}

void renderer_destroy(struct engine* e); /* fwd: используется ниже */

void renderer_init(struct engine* e) {
    if (e->wg.active) return;
    s_adapter = NULL;
    s_device = NULL;

    e->wg.instance = wgpuCreateInstance(NULL);
    if (!e->wg.instance) { LOGE("wgpuCreateInstance failed"); return; }

    ANativeWindow* win = e->app->window;
    if (win) {
        WGPUSurfaceSourceAndroidNativeWindow src = {
            .chain = { .sType = WGPUSType_SurfaceSourceAndroidNativeWindow },
            .window = (void*)win,
        };
        e->wg.surface = wgpuInstanceCreateSurface(
            e->wg.instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain = (WGPUChainedStruct*)&src,
            });
    }
    if (!e->wg.surface) { LOGE("wgpuInstanceCreateSurface failed"); renderer_destroy(e); return; }

    wgpuInstanceRequestAdapter(
        e->wg.instance,
        &(const WGPURequestAdapterOptions){
            .compatibleSurface = e->wg.surface,
        },
        (const WGPURequestAdapterCallbackInfo){
            .callback = on_adapter,
        });
    if (!s_adapter) { LOGE("no adapter (нужен Vulkan)"); renderer_destroy(e); return; }
    e->wg.adapter = s_adapter;

    wgpuAdapterRequestDevice(e->wg.adapter, NULL,
                             (const WGPURequestDeviceCallbackInfo){
                                 .callback = on_device,
                             });
    if (!s_device) { LOGE("no device"); renderer_destroy(e); return; }
    e->wg.device = s_device;
    e->wg.queue = wgpuDeviceGetQueue(e->wg.device);

    WGPUSurfaceCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    if (wgpuSurfaceGetCapabilities(e->wg.surface, e->wg.adapter, &caps) !=
        WGPUStatus_Success) {
        LOGE("surface capabilities failed");
        renderer_destroy(e);
        return;
    }
    if (caps.formatCount == 0) { LOGE("no surface formats"); renderer_destroy(e); return; }
    e->wg.fmt = caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    /* Шейдеры */
    e->wg.shWorld = wg_create_shader(e->wg.device, WGSL_WORLD_VS, "world_vs");
    e->wg.shSky = wg_create_shader(e->wg.device, WGSL_SKY_VS, "sky_vs");
    e->wg.shUi = wg_create_shader(e->wg.device, WGSL_UI_VS, "ui_vs");

    /* Bind group layout: uniform @binding0, vertex+fragment */
    e->wg.bgl = wgpuDeviceCreateBindGroupLayout(
        e->wg.device, &(const WGPUBindGroupLayoutDescriptor){
                          .label = (WGPUStringView){ "bgl", WGPU_STRLEN },
                          .entryCount = 1,
                          .entries = (const WGPUBindGroupLayoutEntry[]){
                              (const WGPUBindGroupLayoutEntry){
                                  .binding = 0,
                                  .visibility = WGPUShaderStage_Vertex |
                                                WGPUShaderStage_Fragment,
                                  .buffer = (const WGPUBufferBindingLayout){
                                      .type = WGPUBufferBindingType_Uniform,
                                  },
                              },
                          },
                      });
    e->wg.layWorld = wgpuDeviceCreatePipelineLayout(
        e->wg.device, &(const WGPUPipelineLayoutDescriptor){
                          .bindGroupLayoutCount = 1,
                          .bindGroupLayouts = &e->wg.bgl,
                      });
    e->wg.laySky = e->wg.layWorld;   /* тот же объект, без AddRef */
    e->wg.layUi = e->wg.layWorld;

    e->wg.ubCam = wgpuDeviceCreateBuffer(
        e->wg.device, &(const WGPUBufferDescriptor){
                          .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                          .size = 256,
                      });
    e->wg.ubSky = wgpuDeviceCreateBuffer(
        e->wg.device, &(const WGPUBufferDescriptor){
                          .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                          .size = 256,
                      });
    e->wg.bgWorld = make_bg(e, e->wg.ubCam);
    e->wg.bgSky = make_bg(e, e->wg.ubSky);
    e->wg.bgUi = make_bg(e, e->wg.ubCam);

    e->wg.bufUi = wgpuDeviceCreateBuffer(
        e->wg.device, &(const WGPUBufferDescriptor){
                          .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                          .size = 1 << 20,
                      });
    e->wg.uiVerts = 0;

    build_world_mesh(e);
    e->wg.active = true;
    LOGI("wgpu initialized, fmt=0x%x", (unsigned)e->wg.fmt);
}

/* Конфигурация поверхности + глубина под новый размер */
void renderer_resize(struct engine* e, uint32_t w, uint32_t h) {
    if (!e->wg.active || !e->wg.surface || !e->wg.device) return;
    if (w == 0 || h == 0) return;
    e->width = w;
    e->height = h;
    wgpuSurfaceConfigure(
        e->wg.surface,
        &(const WGPUSurfaceConfiguration){
            .device = e->wg.device,
            .format = e->wg.fmt,
            .usage = WGPUTextureUsage_RenderAttachment,
            .width = w,
            .height = h,
            .presentMode = WGPUPresentMode_Fifo,
            .alphaMode = WGPUCompositeAlphaMode_Auto,
        });
    recreate_depth(e, w, h);
}

/* Пайплайны создаём один раз (нужны device + формат поверхности) */
static void make_pipelines(struct engine* e) {
    if (e->wg.pipeWorld) return;

    /* --- мир --- */
    {
        WGPUShaderModule fs = wg_create_shader(e->wg.device, WGSL_WORLD_FS, "world_fs");
        const WGPUVertexAttribute attrs[3] = {
            { .format = WGPUVertexFormat_Float32x3, .offset = 0,  .shaderLocation = 0 },
            { .format = WGPUVertexFormat_Snorm8x4,  .offset = 12, .shaderLocation = 1 },
            { .format = WGPUVertexFormat_Unorm8x4,  .offset = 16, .shaderLocation = 2 },
        };
        const WGPUVertexBufferLayout vb = {
            .arrayStride = 20,
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 3,
            .attributes = attrs,
        };
        WGPUDepthStencilState ds = depth_state(true, WGPUCompareFunction_Less);
        e->wg.pipeWorld = wgpuDeviceCreateRenderPipeline(
            e->wg.device, &(const WGPURenderPipelineDescriptor){
                              .label = (WGPUStringView){ "world", WGPU_STRLEN },
                              .layout = e->wg.layWorld,
                              .vertex = (const WGPUVertexState){
                                  .module = e->wg.shWorld,
                                  .entryPoint = (WGPUStringView){ "vs_main", WGPU_STRLEN },
                                  .bufferCount = 1,
                                  .buffers = &vb,
                              },
                              .primitive = (const WGPUPrimitiveState){
                                  .topology = WGPUPrimitiveTopology_TriangleList,
                                  .frontFace = WGPUFrontFace_CCW,
                                  .cullMode = WGPUCullMode_Back,
                              },
                              .depthStencil = &ds,
                              .multisample = (const WGPUMultisampleState){
                                  .count = 1, .mask = 0xFFFFFFFF,
                              },
                              .fragment = &(const WGPUFragmentState){
                                  .module = fs,
                                  .entryPoint = (WGPUStringView){ "fs_main", WGPU_STRLEN },
                                  .targetCount = 1,
                                  .targets = &(const WGPUColorTargetState){
                                      .format = e->wg.fmt,
                                      .writeMask = WGPUColorWriteMask_All,
                                  },
                              },
                          });
        if (fs) wgpuShaderModuleRelease(fs);
        if (!e->wg.pipeWorld) LOGE("world pipeline failed");
    }

    /* --- небо --- */
    {
        WGPUShaderModule fs = wg_create_shader(e->wg.device, WGSL_SKY_FS, "sky_fs");
        WGPUDepthStencilState ds = depth_state(false, WGPUCompareFunction_Always);
        e->wg.pipeSky = wgpuDeviceCreateRenderPipeline(
            e->wg.device, &(const WGPURenderPipelineDescriptor){
                              .label = (WGPUStringView){ "sky", WGPU_STRLEN },
                              .layout = e->wg.laySky,
                              .vertex = (const WGPUVertexState){
                                  .module = e->wg.shSky,
                                  .entryPoint = (WGPUStringView){ "vs_main", WGPU_STRLEN },
                              },
                              .primitive = (const WGPUPrimitiveState){
                                  .topology = WGPUPrimitiveTopology_TriangleList,
                              },
                              .depthStencil = &ds,
                              .multisample = (const WGPUMultisampleState){
                                  .count = 1, .mask = 0xFFFFFFFF,
                              },
                              .fragment = &(const WGPUFragmentState){
                                  .module = fs,
                                  .entryPoint = (WGPUStringView){ "fs_main", WGPU_STRLEN },
                                  .targetCount = 1,
                                  .targets = &(const WGPUColorTargetState){
                                      .format = e->wg.fmt,
                                      .writeMask = WGPUColorWriteMask_All,
                                  },
                              },
                          });
        if (fs) wgpuShaderModuleRelease(fs);
        if (!e->wg.pipeSky) LOGE("sky pipeline failed");
    }

    /* --- UI --- */
    {
        WGPUShaderModule fs = wg_create_shader(e->wg.device, WGSL_UI_FS, "ui_fs");
        const WGPUVertexAttribute attrs[2] = {
            { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0 },
            { .format = WGPUVertexFormat_Unorm8x4,  .offset = 8, .shaderLocation = 1 },
        };
        const WGPUVertexBufferLayout vb = {
            .arrayStride = 12,
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 2,
            .attributes = attrs,
        };
        const WGPUBlendState blend = {
            .color = (const WGPUBlendComponent){
                .operation = WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor_SrcAlpha,
                .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            },
            .alpha = (const WGPUBlendComponent){
                .operation = WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor_One,
                .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            },
        };
        WGPUDepthStencilState ds = depth_state(false, WGPUCompareFunction_Always);
        e->wg.pipeUi = wgpuDeviceCreateRenderPipeline(
            e->wg.device, &(const WGPURenderPipelineDescriptor){
                              .label = (WGPUStringView){ "ui", WGPU_STRLEN },
                              .layout = e->wg.layUi,
                              .vertex = (const WGPUVertexState){
                                  .module = e->wg.shUi,
                                  .entryPoint = (WGPUStringView){ "vs_main", WGPU_STRLEN },
                                  .bufferCount = 1,
                                  .buffers = &vb,
                              },
                              .primitive = (const WGPUPrimitiveState){
                                  .topology = WGPUPrimitiveTopology_TriangleList,
                              },
                              .depthStencil = &ds,
                              .multisample = (const WGPUMultisampleState){
                                  .count = 1, .mask = 0xFFFFFFFF,
                              },
                              .fragment = &(const WGPUFragmentState){
                                  .module = fs,
                                  .entryPoint = (WGPUStringView){ "fs_main", WGPU_STRLEN },
                                  .targetCount = 1,
                                  .targets = &(const WGPUColorTargetState){
                                      .format = e->wg.fmt,
                                      .writeMask = WGPUColorWriteMask_All,
                                      .blend = &blend,
                                  },
                              },
                          });
        if (fs) wgpuShaderModuleRelease(fs);
        if (!e->wg.pipeUi) LOGE("ui pipeline failed");
    }
}

/* ============================================================
   Кадр
   ============================================================ */

void renderer_frame(struct engine* e) {
    if (!e->wg.active || !e->wg.surface || !e->wg.device) return;
    make_pipelines(e);

    ANativeWindow* win = e->app->window;
    if (win) {
        int32_t w = ANativeWindow_getWidth(win);
        int32_t h = ANativeWindow_getHeight(win);
        if (w > 0 && h > 0 &&
            ((uint32_t)w != e->width || (uint32_t)h != e->height))
            renderer_resize(e, (uint32_t)w, (uint32_t)h);
    }
    if (!e->wg.viewDepth) return;

    fill_uniforms(e);

    WGPUSurfaceTexture st;
    memset(&st, 0, sizeof(st));
    wgpuSurfaceGetCurrentTexture(e->wg.surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (st.texture) wgpuTextureRelease(st.texture);
        return;
    }
    if (!st.texture) return;

    WGPUTextureView frame = wgpuTextureCreateView(st.texture, NULL);
    if (!frame) { wgpuTextureRelease(st.texture); return; }

    /* Геометрия UI кадра */
    UBuf ub = { 0 };
    if (e->gameState == STATE_MENU) ui_menu(e, &ub);
    else ui_controls(e, &ub);
    e->wg.uiVerts = (uint32_t)(ub.n / 12);
    if (ub.n > 0)
        wgpuQueueWriteBuffer(e->wg.queue, e->wg.bufUi, 0, ub.d, ub.n);
    free(ub.d);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(
        e->wg.device, &(const WGPUCommandEncoderDescriptor){
                          .label = (WGPUStringView){ "frame", WGPU_STRLEN },
                      });

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(
        enc, &(const WGPURenderPassDescriptor){
                 .colorAttachmentCount = 1,
                 .colorAttachments = &(const WGPURenderPassColorAttachment){
                     .view = frame,
                     .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                     .loadOp = WGPULoadOp_Clear,
                     .storeOp = WGPUStoreOp_Store,
                     .clearValue = (const WGPUColor){ 0.53, 0.76, 0.92, 1.0 },
                 },
                 .depthStencilAttachment =
                     &(const WGPURenderPassDepthStencilAttachment){
                         .view = e->wg.viewDepth,
                         .depthLoadOp = WGPULoadOp_Clear,
                         .depthStoreOp = WGPUStoreOp_Store,
                         .depthClearValue = 1.0f,
                     },
             });

    /* Небо */
    if (e->wg.pipeSky) {
        wgpuRenderPassEncoderSetPipeline(pass, e->wg.pipeSky);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, e->wg.bgSky, 0, NULL);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    }

    /* Мир */
    if (e->wg.pipeWorld && e->wg.worldVerts > 0 && e->wg.bufWorld) {
        wgpuRenderPassEncoderSetPipeline(pass, e->wg.pipeWorld);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, e->wg.bgWorld, 0, NULL);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, e->wg.bufWorld, 0,
                                             (uint64_t)e->wg.worldVerts * 20);
        wgpuRenderPassEncoderDraw(pass, (uint32_t)e->wg.worldVerts, 1, 0, 0);
    }

    /* UI */
    if (e->wg.pipeUi && e->wg.uiVerts > 0) {
        wgpuRenderPassEncoderSetPipeline(pass, e->wg.pipeUi);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, e->wg.bgUi, 0, NULL);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, e->wg.bufUi, 0,
                                             (uint64_t)e->wg.uiVerts * 12);
        wgpuRenderPassEncoderDraw(pass, e->wg.uiVerts, 1, 0, 0);
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(
        enc, &(const WGPUCommandBufferDescriptor){
                 .label = (WGPUStringView){ "cb", WGPU_STRLEN },
             });
    wgpuCommandEncoderRelease(enc);
    if (cb) {
        wgpuQueueSubmit(e->wg.queue, 1, &cb);
        wgpuCommandBufferRelease(cb);
    }
    wgpuSurfacePresent(e->wg.surface);

    wgpuTextureViewRelease(frame);
    wgpuTextureRelease(st.texture);
}

/* ============================================================
   Уничтожение
   ============================================================ */

void renderer_destroy(struct engine* e) {
    struct wgpu_ctx* w = &e->wg;
    if (w->viewDepth) { wgpuTextureViewRelease(w->viewDepth); w->viewDepth = NULL; }
    if (w->texDepth) { wgpuTextureRelease(w->texDepth); w->texDepth = NULL; }
    if (w->bgUi) { wgpuBindGroupRelease(w->bgUi); w->bgUi = NULL; }
    if (w->bgSky) { wgpuBindGroupRelease(w->bgSky); w->bgSky = NULL; }
    if (w->bgWorld) { wgpuBindGroupRelease(w->bgWorld); w->bgWorld = NULL; }
    if (w->pipeUi) { wgpuRenderPipelineRelease(w->pipeUi); w->pipeUi = NULL; }
    if (w->pipeSky) { wgpuRenderPipelineRelease(w->pipeSky); w->pipeSky = NULL; }
    if (w->pipeWorld) { wgpuRenderPipelineRelease(w->pipeWorld); w->pipeWorld = NULL; }
    if (w->bufUi) { wgpuBufferRelease(w->bufUi); w->bufUi = NULL; }
    if (w->bufWorld) { wgpuBufferRelease(w->bufWorld); w->bufWorld = NULL; }
    if (w->ubSky) { wgpuBufferRelease(w->ubSky); w->ubSky = NULL; }
    if (w->ubCam) { wgpuBufferRelease(w->ubCam); w->ubCam = NULL; }
    /* laySky/layUi указывают на тот же объект, что layWorld */
    w->layUi = NULL;
    w->laySky = NULL;
    if (w->layWorld) { wgpuPipelineLayoutRelease(w->layWorld); w->layWorld = NULL; }
    if (w->bgl) { wgpuBindGroupLayoutRelease(w->bgl); w->bgl = NULL; }
    if (w->shUi) { wgpuShaderModuleRelease(w->shUi); w->shUi = NULL; }
    if (w->shSky) { wgpuShaderModuleRelease(w->shSky); w->shSky = NULL; }
    if (w->shWorld) { wgpuShaderModuleRelease(w->shWorld); w->shWorld = NULL; }
    if (w->queue) { wgpuQueueRelease(w->queue); w->queue = NULL; }
    if (w->device) { wgpuDeviceRelease(w->device); w->device = NULL; }
    if (w->adapter) { wgpuAdapterRelease(w->adapter); w->adapter = NULL; }
    if (w->surface) { wgpuSurfaceRelease(w->surface); w->surface = NULL; }
    if (w->instance) { wgpuInstanceRelease(w->instance); w->instance = NULL; }
    w->active = false;
    e->width = 0;
    e->height = 0;
    w->fmt = WGPUTextureFormat_Undefined;
    w->worldVerts = 0;
    s_adapter = NULL;
    s_device = NULL;
}

#endif
