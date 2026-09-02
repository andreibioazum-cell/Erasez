#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include <math.h>
#include "engine.h"

/* Старт новой игры / возрождение у точки спавна. */
static void start_game(struct engine* e) {
    e->gameState = STATE_PLAYING;
    e->px = col_cx(WORLD_X / 2);
    e->pz = col_cz(WORLD_Z / 2);
    e->py = e->spawnY;
    e->yaw = -0.8f;              /* красивый ракурс на поляну */
    e->pitch = -0.04f;
    e->velY = 0.0f;
    e->onGround = false;
    e->flying = false;
    e->jumpHeld = false;
    e->downHeld = false;
    e->isMoving = false;
    e->joyTouched = false;
    e->moveDirX = 0.0f;
    e->moveDirZ = 0.0f;
    e->movePointerId = -1;
    e->lookPointerId = -1;
    e->jumpPointerId = -1;
    e->downPointerId = -1;
    e->walkPhase = 0.0f;
}

static int handle_menu_input(struct engine* e, float x, float y) {
    int sw = e->width, sh = e->height;
    float px = sw * 0.5f, py = sh * 0.58f;
    float dx = x - px, dy = y - py;
    if (dx * dx + dy * dy < 76.0f * 76.0f) {
        start_game(e);
        return 1;
    }
    return 0;
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* e = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);
    int sw = e->width, sh = e->height;

    if (code == AMOTION_EVENT_ACTION_DOWN ||
        code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        int pi = (code == AMOTION_EVENT_ACTION_DOWN) ? 0 :
                 (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                 AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(event, pi);
        float y = AMotionEvent_getY(event, pi);
        int id = AMotionEvent_getPointerId(event, pi);

        if (e->gameState == STATE_MENU) return handle_menu_input(e, x, y);

        /* Кнопки на правой стороне */
        float bx = sw - BTN_OFFSET, by = sh - BTN_OFFSET;
        float dx = x - bx, dy = y - by;

        /* Прыжок (или вверх в полёте) */
        float rJump = BTN_RADIUS + 12.0f;
        if (dx * dx + dy * dy < rJump * rJump) {
            if (e->flying) {
                e->jumpPointerId = id;
                e->jumpHeld = true;
            } else if (e->onGround) {
                e->velY = JUMP_V;
                e->onGround = false;
            }
            return 1;
        }

        /* Спуск (виден в полёте) */
        if (e->flying) {
            float dxd = x - (bx - BTN_GAP), dyd = y - by;
            float rd = BTN_RADIUS * 0.9f + 12.0f;
            if (dxd * dxd + dyd * dyd < rd * rd) {
                e->downPointerId = id;
                e->downHeld = true;
                return 1;
            }
        }

        /* Полёт вкл/выкл */
        float dxf = x - bx, dyf = y - (by - BTN_GAP);
        float rf = FLY_RADIUS + 14.0f;
        if (dxf * dxf + dyf * dyf < rf * rf) {
            e->flying = !e->flying;
            e->jumpHeld = false;
            e->downHeld = false;
            return 1;
        }

        /* Джойстик слева */
        float jx = JOY_OFFSET, jy = sh - JOY_OFFSET;
        float djx = x - jx, djy = y - jy;
        float dist = sqrtf(djx * djx + djy * djy);
        if (dist < JOY_RADIUS * 1.9f && x < sw * 0.5f) {
            e->joyTouched = true;
            e->isMoving = true;
            e->movePointerId = id;
            if (dist > 6.0f) {
                float c = dist > JOY_RADIUS ? JOY_RADIUS : dist;
                e->moveDirX = (djx / dist) * (c / JOY_RADIUS);
                e->moveDirZ = (djy / dist) * (c / JOY_RADIUS);
            } else {
                e->moveDirX = 0.0f;
                e->moveDirZ = 0.0f;
            }
            return 1;
        }

        /* Поворот камеры */
        e->lastLookX = x;
        e->lastLookY = y;
        e->lookPointerId = id;
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_MOVE) {
        if (e->gameState == STATE_MENU) return 0;
        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);

            if (id == e->movePointerId && e->joyTouched) {
                float dx = x - JOY_OFFSET;
                float dy = y - (sh - JOY_OFFSET);
                float d = sqrtf(dx * dx + dy * dy);
                if (d > 6.0f) {
                    float c = d > JOY_RADIUS ? JOY_RADIUS : d;
                    e->moveDirX = (dx / d) * (c / JOY_RADIUS);
                    e->moveDirZ = (dy / d) * (c / JOY_RADIUS);
                } else {
                    e->moveDirX = 0.0f;
                    e->moveDirZ = 0.0f;
                }
            }
            if (id == e->lookPointerId) {
                float dxx = x - e->lastLookX;
                float dyy = y - e->lastLookY;
                e->lastLookX = x;
                e->lastLookY = y;
                e->yaw += dxx * 0.0065f;
                e->pitch -= dyy * 0.0065f;   /* вверх пальцем = смотрим вверх */
                if (e->pitch > 1.5f) e->pitch = 1.5f;
                if (e->pitch < -1.5f) e->pitch = -1.5f;
            }
        }
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_UP ||
        code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                 AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);
        if (id == e->movePointerId) {
            e->isMoving = false;
            e->moveDirX = 0.0f;
            e->moveDirZ = 0.0f;
            e->movePointerId = -1;
            e->joyTouched = false;
        }
        if (id == e->lookPointerId) e->lookPointerId = -1;
        if (id == e->jumpPointerId) {
            e->jumpPointerId = -1;
            e->jumpHeld = false;
        }
        if (id == e->downPointerId) {
            e->downPointerId = -1;
            e->downHeld = false;
        }
        return 1;
    }
    return 0;
}

#endif
