#ifndef INPUT_H
#define INPUT_H

#include <android_native_app_glue.h>
#include "engine.h"
#include "world.h"

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
        
        // Джойстик (левая половина)
        if (x < eng->width / 2.0f && eng->movePointerId == -1) {
            eng->isMoving = true;
            eng->movePointerId = id;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
            return 1;
        }
        
        // Взгляд (правая половина)
        if (x >= eng->width / 2.0f && eng->lookPointerId == -1) {
            eng->lastTouchX = x;
            eng->lastTouchY = y;
            eng->lookPointerId = id;
            return 1;
        }
        
        // Кнопка прыжка
        float jbx = eng->width - JUMP_BTN_OFFSET;
        float jby = eng->height - JUMP_BTN_OFFSET;
        float dx = x - jbx, dy = y - jby;
        if (dx*dx + dy*dy < (JUMP_BTN_SIZE*1.2f)*(JUMP_BTN_SIZE*1.2f)) {
            if (eng->onGround) {
                eng->velY = JUMP_FORCE;
                eng->onGround = false;
            }
            return 1;
        }
        
        // Кнопка ломания
        float bbx = eng->width - BREAK_BTN_X;
        float bby = BREAK_BTN_Y;
        dx = x - bbx; dy = y - bby;
        if (dx*dx + dy*dy < (ACTION_BTN_SIZE*1.3f)*(ACTION_BTN_SIZE*1.3f)) {
            int hx, hy, hz, px, py, pz;
            if (raycast(eng, &hx, &hy, &hz, &px, &py, &pz)) {
                if (hy > 0) {
                    world_set_block(eng, hx, hy, hz, BLOCK_AIR);
                }
            }
            return 1;
        }
        
        // Кнопка ставления
        float pbx = eng->width - PLACE_BTN_X;
        float pby = PLACE_BTN_Y;
        dx = x - pbx; dy = y - pby;
        if (dx*dx + dy*dy < (ACTION_BTN_SIZE*1.3f)*(ACTION_BTN_SIZE*1.3f)) {
            int hx, hy, hz, px, py, pz;
            if (raycast(eng, &hx, &hy, &hz, &px, &py, &pz)) {
                if (px >= 0 && px < WORLD_SIZE_X && pz >= 0 && pz < WORLD_SIZE_Z) {
                    // Проверка на занятость
                    if (world_block_at(eng, px, py, pz) == BLOCK_AIR) {
                        world_set_block(eng, px, py, pz, BLOCK_GRASS);
                    }
                }
            }
            return 1;
        }
        
        return 1;
    }
    
    if (code == AMOTION_EVENT_ACTION_MOVE) {
        for (int i = 0; i < pCount; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
            int id = AMotionEvent_getPointerId(event, i);
            
            if (id == eng->movePointerId && eng->isMoving) {
                float dx = x - JOY_X_OFFSET;
                float dy = y - (eng->height - JOY_Y_OFFSET);
                float d = sqrtf(dx*dx + dy*dy);
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
                float dx = x - eng->lastTouchX;
                float dy = y - eng->lastTouchY;
                eng->camRot[1] += dx * 0.004f;
                eng->camRot[0] += dy * 0.004f;
                if (eng->camRot[0] > 1.4f) eng->camRot[0] = 1.4f;
                if (eng->camRot[0] < -1.4f) eng->camRot[0] = -1.4f;
                eng->lastTouchX = x;
                eng->lastTouchY = y;
            }
        }
        return 1;
    }
    
    if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
        int pi = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int id = AMotionEvent_getPointerId(event, pi);
        
        if (id == eng->movePointerId) {
            eng->isMoving = false;
            eng->movePointerId = -1;
            eng->moveDirX = 0;
            eng->moveDirZ = 0;
        }
        if (id == eng->lookPointerId) {
            eng->lookPointerId = -1;
        }
        return 1;
    }
    
    return 0;
}

#endif
