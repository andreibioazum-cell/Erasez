#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include <math.h>
#include "engine.h"

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);

    if (code == AMOTION_EVENT_ACTION_DOWN ||
        code == AMOTION_EVENT_ACTION_POINTER_DOWN) {

        int pi = (code == AMOTION_EVENT_ACTION_DOWN) ? 0 :
            (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        float x = AMotionEvent_getX(event, pi);
        float y = AMotionEvent_getY(event, pi);
        int id = AMotionEvent_getPointerId(event, pi);

        /* Меню — тап начинает игру */
        if (eng->gameState == STATE_MENU) {
            eng->gameState = STATE_PLAYING;
            eng->worldLoaded = false;
            eng->camPos[0] = (float)(PLATFORM_SIZE / 2);
            eng->camPos[1] = (float)(PLATFORM_Y) + EYE_H + 1.0f;
            eng->camPos[2] = -(float)(PLATFORM_SIZE / 2);
            eng->velY = 0;
            return 1;
        }

        /* Прыжок — правый нижний угол */
        float jbX = eng->width - JUMP_BTN_OFFSET;
        float jbY = eng->height - JUMP_BTN_OFFSET;
        float djx = x - jbX, djy = y - jbY;
        if (sqrtf(djx * djx + djy * djy) < JUMP_BTN_SIZE * 1.2f) {
            if (eng->onGround) {
                eng->velY = JUMP_FORCE;
                eng->onGround = false;
            }
            return 1;
        }

        /* Джойстик — левая половина */
        if (x < eng->width / 2) {
            eng->joyX = JOY_X_OFFSET;
            eng->joyY = eng->height - JOY_Y_OFFSET;
            eng->isMoving = true;
            eng->movePointerId = id;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
        } else {
            /* Камера — правая половина */
            eng->lastTouchX = x;
            eng->lastTouchY = y;
            eng->lookPointerId = id;
        }
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_MOVE) {
        if (eng->gameState == STATE_MENU) return 0;

        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);

            if (id == eng->movePointerId && eng->isMoving) {
                float dx = x - eng->joyX, dy = y - eng->joyY;
                float d = sqrtf(dx * dx + dy * dy);
                if (d > 10.0f) {
                    float c = d > JOY_RADIUS ? JOY_RADIUS : d;
                    eng->moveDirX = (dx / d) * (c / JOY_RADIUS);
                    eng->moveDirZ = (dy / d) * (c / JOY_RADIUS);
                } else {
                    eng->moveDirX = 0;
                    eng->moveDirZ = 0;
                }
            }

            if (id == eng->lookPointerId) {
                eng->camRot[1] += (x - eng->lastTouchX) * 0.004f;
                eng->camRot[0] += (y - eng->lastTouchY) * 0.004f;
                if (eng->camRot[0] > 1.4f) eng->camRot[0] = 1.4f;
                if (eng->camRot[0] < -1.4f) eng->camRot[0] = -1.4f;
                eng->lastTouchX = x;
                eng->lastTouchY = y;
            }
        }
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_UP ||
        code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                 >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);

        if (id == eng->movePointerId) {
            eng->isMoving = false;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
            eng->movePointerId = -1;
        }
        if (id == eng->lookPointerId)
            eng->lookPointerId = -1;
        return 1;
    }

    return 0;
}

#endif
