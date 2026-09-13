// main/vocab_model.h —— 背诵进度与出题顺序（纯逻辑，不依赖 ESP-IDF/LVGL，可做 host 测试）
#pragma once

#include <stdint.h>
#include <stdbool.h>

// 掌握度等级
#define VOCAB_LEVEL_NEW   0   // 未学
#define VOCAB_LEVEL_WEAK  1   // 初识（答对 1 次，或答错后回落）
#define VOCAB_LEVEL_FAIR  2   // 熟悉（累计答对 2 次）
#define VOCAB_LEVEL_GOOD  3   // 熟练（累计答对 3 次及以上）

// 每词 1 字节：bit0-1 = 掌握度(0..3)，bit2-5 = 答错次数(0..15)
// 规则：答对则掌握度 +1（上限 3）；答错则掌握度回落为 1，且答错次数 +1（上限 15）。
typedef struct {
    uint8_t *bytes;
    int      count;
} vocab_prog_t;

void    vocab_prog_init(vocab_prog_t *p, uint8_t *storage, int count);
uint8_t vocab_prog_level(const vocab_prog_t *p, int id);
uint8_t vocab_prog_wrong(const vocab_prog_t *p, int id);
void    vocab_prog_answer(vocab_prog_t *p, int id, bool correct);
void    vocab_prog_reset(vocab_prog_t *p);
// 统计：已学(level>0)、熟练(level==3)、错词(wrong>0)
void    vocab_prog_stats(const vocab_prog_t *p, int *learned, int *good, int *wrong);

// ---- 出题顺序 ----
typedef enum {
    VOCAB_ORDER_SEQ = 0,   // 按词库顺序
    VOCAB_ORDER_SHUFFLE,   // 随机
    VOCAB_ORDER_WEAK,      // 只出错词/生疏词
} vocab_order_t;

typedef struct {
    int *idx;   // 词条索引数组，长度 = count
    int  n;
    int  pos;
} vocab_session_t;

// chapters: 允许的章节位掩码；0 表示不限章节
void vocab_session_build(vocab_session_t *s, int *storage,
                         const vocab_prog_t *p, uint16_t chapters,
                         vocab_order_t order, uint32_t seed);
int  vocab_session_current(const vocab_session_t *s);   // 返回词条 id，-1 表示空
// 直接定位到指定词条 id；不存在返回 false。
bool vocab_session_set_current_id(vocab_session_t *s, int id);
void vocab_session_next(vocab_session_t *s);
void vocab_session_prev(vocab_session_t *s);
void vocab_session_jump(vocab_session_t *s, int delta);
// 跳到相邻章节的第一条；dir>0 向后，dir<0 向前。返回是否移动成功。
bool vocab_session_seek_chapter(vocab_session_t *s, int dir);
