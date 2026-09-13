// main/vocab_app.h —— 法律英语背单词玩法入口
#pragma once

#include <stdbool.h>
#include "bsp_button.h"

// 建立主菜单（调用前须持有 LVGL 锁）
void vocab_app_start(void);

// 按键分发（调用前须持有 LVGL 锁）。返回菜单统一由 OK 长按触发。
void vocab_app_key(bsp_btn_t btn, bsp_btn_ev_t ev);
