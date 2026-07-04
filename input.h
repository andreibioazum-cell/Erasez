#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include <math.h>
#include "engine.h"

static int32_t handle_menu_input(struct engine* eng, float x, float y) {
    int sw = eng->width, sh = eng->height;
    float playX = sw/2.0f, playY = sh*0.55f;
    if (x > playX-120 && x < playX+120 && y > playY-45 && y < playY+45) {
        eng->gameState = STATE_PLAYING;
        eng->playerPos[0] = 0; eng->playerPos[1] = 0; eng->playerPos[2] = 0;
        eng->playerRot = 0; eng->velY = 0; eng->camRotY = 0; eng->animTime = 0;
        return 1;
    }
    return 0;
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int pCount = AMotionEvent_getPointerCount(event);

    if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        int pi = (code == AMOTION_EVENT_ACTION_DOWN) ? 0 :
            (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(event, pi);
        float y = AMotionEvent_getY(event, pi);
        int id = AMotionEvent_getPointerId(event, pi);

        if (eng->gameState == STATE_MENU) return handle_menu_input(eng, x, y);

        /* Прыжок */
        float jbX = eng->width-JUMP_BTN_OFFSET, jbY = eng->height-JUMP_BTN_OFFSET;
        float djx = x-jbX, djy = y-jbY;
        if (sqrtf(djx*djx+djy*djy) < JUMP_BTN_SIZE*1.2f) {
            if (eng->onGround) { eng->velY = JUMP_FORCE; eng->onGround = false; }
            return 1;
        }

        /* Джойстик */
        float jx = JOY_X_OFFSET, jy = eng->height-JOY_Y_OFFSET;
        float djx2 = x-jx, djy2 = y-jy;
        float dist = sqrtf(djx2*djx2+djy2*djy2);
        if (dist < JOY_RADIUS*2.0f) {
            eng->joyTouched = true; eng->isMoving = true; eng->movePointerId = id;
            if (dist > 10.0f) {
                float c = dist > JOY_RADIUS ? JOY_RADIUS : dist;
                eng->moveDirX = (djx2/dist)*(c/JOY_RADIUS);
                eng->moveDirZ = (djy2/dist)*(c/JOY_RADIUS);
            } else { eng->moveDirX = 0; eng->moveDirZ = 0; }
            return 1;
        }

        /* Камера */
        eng->lastTouchX = x; eng->lastTouchY = y; eng->lookPointerId = id;
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_MOVE) {
        if (eng->gameState == STATE_MENU) return 0;
        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);

            if (id == eng->movePointerId && eng->isMoving && eng->joyTouched) {
                float dx = x-JOY_X_OFFSET, dy = y-(eng->height-JOY_Y_OFFSET);
                float d = sqrtf(dx*dx+dy*dy);
                if (d > 10.0f) {
                    float c = d > JOY_RADIUS ? JOY_RADIUS : d;
                    eng->moveDirX = (dx/d)*(c/JOY_RADIUS);
                    eng->moveDirZ = (dy/d)*(c/JOY_RADIUS);
                } else { eng->moveDirX = 0; eng->moveDirZ = 0; }
            }
            if (id == eng->lookPointerId) {
                eng->camRotY += (x-eng->lastTouchX)*0.005f;
                eng->lastTouchX = x; eng->lastTouchY = y;
            }
        }
        return 1;
    }

    if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);
        if (id == eng->movePointerId) {
            eng->isMoving = false; eng->moveDirX = 0; eng->moveDirZ = 0;
            eng->movePointerId = -1; eng->joyTouched = false;
        }
        if (id == eng->lookPointerId) eng->lookPointerId = -1;
        return 1;
    }
    return 0;
}

#endif
