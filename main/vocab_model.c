// main/vocab_model.c —— 纯逻辑实现，不引用任何 ESP-IDF/LVGL 头文件。
#include "vocab_model.h"
#include "vocab_data.h"
#include <string.h>

// ---------------------------------------------------------------------------
// 进度：每词 1 字节
// ---------------------------------------------------------------------------
void vocab_prog_init(vocab_prog_t *p, uint8_t *storage, int count)
{
    p->bytes = storage;
    p->count = count;
}

uint8_t vocab_prog_level(const vocab_prog_t *p, int id)
{
    if (id < 0 || id >= p->count) return VOCAB_LEVEL_NEW;
    return (uint8_t)(p->bytes[id] & 0x03u);
}

uint8_t vocab_prog_wrong(const vocab_prog_t *p, int id)
{
    if (id < 0 || id >= p->count) return 0;
    return (uint8_t)((p->bytes[id] >> 2) & 0x0Fu);
}

void vocab_prog_answer(vocab_prog_t *p, int id, bool correct)
{
    if (id < 0 || id >= p->count) return;
    uint8_t v = p->bytes[id];
    uint8_t lvl   = (uint8_t)(v & 0x03u);
    uint8_t wrong = (uint8_t)((v >> 2) & 0x0Fu);

    if (correct) {
        if (lvl < VOCAB_LEVEL_GOOD) lvl++;
    } else {
        // 答错：直接降为「生疏」，并累计错误次数
        lvl = VOCAB_LEVEL_WEAK;
        if (wrong < 15) wrong++;
    }
    p->bytes[id] = (uint8_t)((lvl & 0x03u) | ((wrong & 0x0Fu) << 2));
}

void vocab_prog_reset(vocab_prog_t *p)
{
    if (p->bytes && p->count > 0) memset(p->bytes, 0, (size_t)p->count);
}

void vocab_prog_stats(const vocab_prog_t *p, int *learned, int *good, int *wrong)
{
    int l = 0, g = 0, w = 0;
    for (int i = 0; i < p->count; i++) {
        uint8_t v = p->bytes[i];
        uint8_t lvl = (uint8_t)(v & 0x03u);
        if (lvl > VOCAB_LEVEL_NEW) l++;
        if (lvl == VOCAB_LEVEL_GOOD) g++;
        if (((v >> 2) & 0x0Fu) > 0) w++;
    }
    if (learned) *learned = l;
    if (good)    *good = g;
    if (wrong)   *wrong = w;
}

// ---------------------------------------------------------------------------
// 出题顺序
// ---------------------------------------------------------------------------
static uint32_t xs32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x ? x : 0x9E3779B9u;
    return *s;
}

// 词条所属的最小章节号（1..14）；无章节返回 0
static int primary_chapter(int id)
{
    uint16_t m = VOCAB[id].chapters;
    for (int c = 1; c <= 14; c++)
        if (m & (1u << (c - 1))) return c;
    return 0;
}

void vocab_session_build(vocab_session_t *s, int *storage,
                         const vocab_prog_t *p, uint16_t chapters,
                         vocab_order_t order, uint32_t seed)
{
    s->idx = storage;
    s->n = 0;
    s->pos = 0;

    for (int i = 0; i < VOCAB_COUNT; i++) {
        if (chapters && !(VOCAB[i].chapters & chapters)) continue;
        if (order == VOCAB_ORDER_WEAK) {
            // 错词本 = 答错过、且尚未练到熟练的词
            if (vocab_prog_wrong(p, i) == 0) continue;
            if (vocab_prog_level(p, i) >= VOCAB_LEVEL_GOOD) continue;
        }
        storage[s->n++] = i;
    }

    if (order == VOCAB_ORDER_SHUFFLE) {
        uint32_t st = seed ? seed : 0x12345678u;
        for (int i = s->n - 1; i > 0; i--) {
            int j = (int)(xs32(&st) % (uint32_t)(i + 1));
            int t = storage[i]; storage[i] = storage[j]; storage[j] = t;
        }
    }
}

int vocab_session_current(const vocab_session_t *s)
{
    if (!s->idx || s->n <= 0) return -1;
    if (s->pos < 0 || s->pos >= s->n) return -1;
    return s->idx[s->pos];
}

bool vocab_session_set_current_id(vocab_session_t *s, int id)
{
    if (!s->idx || s->n <= 0) return false;
    for (int i = 0; i < s->n; i++) {
        if (s->idx[i] == id) { s->pos = i; return true; }
    }
    return false;
}

void vocab_session_next(vocab_session_t *s)
{
    if (s->n <= 0) return;
    s->pos = (s->pos + 1) % s->n;
}

void vocab_session_prev(vocab_session_t *s)
{
    if (s->n <= 0) return;
    s->pos = (s->pos + s->n - 1) % s->n;
}

void vocab_session_jump(vocab_session_t *s, int delta)
{
    if (s->n <= 0) return;
    int p = (s->pos + delta) % s->n;
    if (p < 0) p += s->n;
    s->pos = p;
}

bool vocab_session_seek_chapter(vocab_session_t *s, int dir)
{
    if (s->n <= 0 || dir == 0) return false;
    int cur = primary_chapter(s->idx[s->pos]);
    int i = s->pos;
    for (int step = 0; step < s->n; step++) {
        i = (i + dir + s->n) % s->n;
        int c = primary_chapter(s->idx[i]);
        if (c != cur && c != 0) {
            s->pos = i;
            return true;
        }
    }
    return false;
}
