#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>
#include <string.h>

static void mat4_identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_perspective(float* m, float fov, float aspect,
                             float n, float f) {
    float S = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 64);
    m[0]  = S / aspect;
    m[5]  = S;
    m[10] = (f + n) / (n - f);
    m[11] = -1.0f;
    m[14] = (2.0f * f * n) / (n - f);
}

static void mat4_rotate_y(float* m, float a) {
    mat4_identity(m);
    m[0]  =  cosf(a); m[2]  = -sinf(a);
    m[8]  =  sinf(a); m[10] =  cosf(a);
}

static void mat4_rotate_x(float* m, float a) {
    mat4_identity(m);
    m[5]  =  cosf(a); m[6]  =  sinf(a);
    m[9]  = -sinf(a); m[10] =  cosf(a);
}

static void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_mul(float* out, const float* a, const float* b) {
    float res[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            res[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
                            + a[1 * 4 + r] * b[c * 4 + 1]
                            + a[2 * 4 + r] * b[c * 4 + 2]
                            + a[3 * 4 + r] * b[c * 4 + 3];
    memcpy(out, res, 64);
}

static void mat4_lookat(float* m, const float* pos, float pitch, float yaw) {
    float rotX[16], rotY[16], trans[16], tmp[16];
    mat4_translate(trans, -pos[0], -pos[1], -pos[2]);
    mat4_rotate_y(rotY, yaw);
    mat4_rotate_x(rotX, pitch);
    mat4_mul(tmp, rotY, trans);
    mat4_mul(m, rotX, tmp);
}

#endif
