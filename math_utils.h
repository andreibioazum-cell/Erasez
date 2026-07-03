#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>
#include <string.h>

static void mat4_identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_perspective(float* m, float fov, float aspect, float n, float f) {
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
    m[0]  =  cosf(a);
    m[2]  = -sinf(a);
    m[8]  =  sinf(a);
    m[10] =  cosf(a);
}

static void mat4_rotate_x(float* m, float a) {
    mat4_identity(m);
    m[5]  =  cosf(a);
    m[6]  =  sinf(a);
    m[9]  = -sinf(a);
    m[10] =  cosf(a);
}

static void mat4_rotate_z(float* m, float a) {
    mat4_identity(m);
    m[0] =  cosf(a);
    m[1] =  sinf(a);
    m[4] = -sinf(a);
    m[5] =  cosf(a);
}

static void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_scale(float* m, float sx, float sy, float sz) {
    mat4_identity(m);
    m[0] = sx; m[5] = sy; m[10] = sz;
}

static void mat4_mul(float* out, const float* a, const float* b) {
    float res[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            res[c*4+r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1]
                       + a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
    memcpy(out, res, 64);
}

static void mat4_lookat_pos(float* m, float eyeX, float eyeY, float eyeZ,
                             float tgtX, float tgtY, float tgtZ) {
    float fx = tgtX - eyeX, fy = tgtY - eyeY, fz = tgtZ - eyeZ;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    if (fl < 0.001f) fl = 0.001f;
    fx /= fl; fy /= fl; fz /= fl;

    float ux = 0, uy = 1, uz = 0;
    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    if (sl < 0.001f) sl = 0.001f;
    sx /= sl; sy /= sl; sz /= sl;

    float ux2 = sy*fz - sz*fy, uy2 = sz*fx - sx*fz, uz2 = sx*fy - sy*fx;

    mat4_identity(m);
    m[0] = sx;  m[4] = sy;  m[8]  = sz;
    m[1] = ux2; m[5] = uy2; m[9]  = uz2;
    m[2] = -fx; m[6] = -fy; m[10] = -fz;
    m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    m[13] = -(ux2*eyeX + uy2*eyeY + uz2*eyeZ);
    m[14] = -(-fx*eyeX + -fy*eyeY + -fz*eyeZ);
}

#endif
