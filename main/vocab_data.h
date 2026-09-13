// main/vocab_data.h —— 词库数据结构（数据由 tools/gen_vocab_data.py 生成）
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *en;        // 英文术语
    const char *zh;        // 中文译名；多个义项以「；」分隔
    const char *ipa;       // 国际音标（可能为空串）
    uint16_t    chapters;  // 出现章节位掩码：bit0 = 第 1 章 … bit13 = 第 14 章
    uint16_t    def_idx;   // 释义索引；0xFFFF 表示该词无补充释义
    uint16_t    freq;      // 在教材中出现的次数
} vocab_entry_t;

extern const vocab_entry_t VOCAB[];
extern const int VOCAB_COUNT;
extern const int VOCAB_DEF_COUNT;

// 取第 i 条释义；越界返回空串。
const char *vocab_def(int i);

// 取第 i 条的教材例句（英文/中文）；无例句返回空串。
// 例句与 VOCAB 同下标平行存放（S_EX_EN / S_EX_ZH）。
extern const int VOCAB_EX_COUNT;
const char *vocab_ex_en(int i);
const char *vocab_ex_zh(int i);

// 章节位掩码辅助
static inline bool vocab_in_chapter(const vocab_entry_t *e, int ch)
{
    return (ch >= 1 && ch <= 14) && ((e->chapters >> (ch - 1)) & 1u);
}
