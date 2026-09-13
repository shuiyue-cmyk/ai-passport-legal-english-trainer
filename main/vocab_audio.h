// main/vocab_audio.h —— 从 vocabfs 数据分区读取 Opus 裸包流并播放
#pragma once

#include <stdbool.h>
#include "esp_err.h"

// 挂载/校验数据分区并启动常驻播放任务。可重复调用（幂等）。
esp_err_t vocab_audio_init(void);

// 数据分区是否可用
bool vocab_audio_ready(void);

// 请求播放第 id 条音频（会打断正在播放的）。越界或未初始化则忽略。
void vocab_audio_play(int id);

// 停止当前播放
void vocab_audio_stop(void);

// 是否正在播放
bool vocab_audio_playing(void);

// 音量 0..100
void vocab_audio_set_volume(uint8_t percent);
