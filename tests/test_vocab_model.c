// tests/test_vocab_model.c —— vocab_model 的 host 测试（无需硬件）
// 编译: cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_vocab_model.c \
//           main/vocab_model.c main/vocab_data.c -o test_vocab_model
#include "vocab_model.h"
#include "vocab_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C 语言里 const int 不是常量表达式，数组用固定上限
#define TEST_CAP 4096

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); }\
    } while (0)

static void test_progress(void)
{
    static uint8_t store[64];
    vocab_prog_t p;
    vocab_prog_init(&p, store, 64);
    memset(store, 0, sizeof(store));

    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_NEW, "新词等级应为 NEW");
    CHECK(vocab_prog_wrong(&p, 0) == 0, "新词错次应为 0");

    vocab_prog_answer(&p, 0, true);
    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_WEAK, "答对一次 -> 初识");

    vocab_prog_answer(&p, 0, true);
    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_FAIR, "答对两次 -> 熟悉");

    vocab_prog_answer(&p, 0, true);
    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_GOOD, "答对三次 -> 熟练");

    vocab_prog_answer(&p, 0, true);
    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_GOOD, "熟练后不再上升");

    vocab_prog_answer(&p, 0, false);
    CHECK(vocab_prog_level(&p, 0) == VOCAB_LEVEL_WEAK, "答错 -> 回落初识");
    CHECK(vocab_prog_wrong(&p, 0) == 1, "错次应为 1");

    for (int i = 0; i < 40; i++) vocab_prog_answer(&p, 0, false);
    CHECK(vocab_prog_wrong(&p, 0) == 15, "错次上限为 15");

    // 越界安全
    vocab_prog_answer(&p, -1, true);
    vocab_prog_answer(&p, 9999, true);
    CHECK(vocab_prog_level(&p, -1) == VOCAB_LEVEL_NEW, "越界读应返回 NEW");
    CHECK(vocab_prog_level(&p, 9999) == VOCAB_LEVEL_NEW, "越界读应返回 NEW");

    // 统计
    int learned = 0, good = 0, wrong = 0;
    vocab_prog_stats(&p, &learned, &good, &wrong);
    CHECK(learned == 1, "已学 1 条");
    CHECK(good == 0, "熟练 0 条");
    CHECK(wrong == 1, "错词 1 条");

    vocab_prog_reset(&p);
    vocab_prog_stats(&p, &learned, &good, &wrong);
    CHECK(learned == 0 && good == 0 && wrong == 0, "重置后统计归零");
}

static void test_session_seq(void)
{
    static int idx[TEST_CAP];
    static uint8_t store[TEST_CAP];
    vocab_prog_t p;
    vocab_prog_init(&p, store, VOCAB_COUNT);
    memset(store, 0, sizeof(store));

    vocab_session_t s;
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_SEQ, 1);
    CHECK(s.n == VOCAB_COUNT, "不限章节时应包含全部词条");

    s.pos = 0;
    int first = vocab_session_current(&s);
    CHECK(first == 0, "顺序模式首条为 0");

    vocab_session_next(&s);
    CHECK(vocab_session_current(&s) == 1, "next 前进一条");

    vocab_session_prev(&s);
    CHECK(vocab_session_current(&s) == 0, "prev 后退一条");

    vocab_session_prev(&s);
    CHECK(vocab_session_current(&s) == VOCAB_COUNT - 1, "prev 从头回绕到末尾");

    vocab_session_next(&s);
    CHECK(vocab_session_current(&s) == 0, "next 从末尾回绕到开头");

    vocab_session_jump(&s, 10);
    CHECK(vocab_session_current(&s) == 10, "jump +10");

    vocab_session_jump(&s, -20);
    CHECK(vocab_session_current(&s) == VOCAB_COUNT - 10, "jump -20 负向回绕");
}

static void test_session_chapter(void)
{
    static int idx[TEST_CAP];
    static uint8_t store[TEST_CAP];
    vocab_prog_t p;
    vocab_prog_init(&p, store, VOCAB_COUNT);
    memset(store, 0, sizeof(store));

    vocab_session_t s;
    // 只要第 1 章
    vocab_session_build(&s, idx, &p, 1u << 0, VOCAB_ORDER_SEQ, 1);
    CHECK(s.n > 0, "第 1 章应有词条");
    int all_ch1 = 1;
    for (int i = 0; i < s.n; i++)
        if (!vocab_in_chapter(&VOCAB[s.idx[i]], 1)) all_ch1 = 0;
    CHECK(all_ch1, "章节筛选结果应全部属于第 1 章");

    // 只取第 3 章，逐条前进应能命中多个不同章节（通过不限章节的会话验证 seek）
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_SEQ, 1);
    s.pos = 0;
    int c0 = 0;
    for (int c = 1; c <= 14; c++) if (VOCAB[s.idx[0]].chapters & (1u << (c - 1))) { c0 = c; break; }
    CHECK(vocab_session_seek_chapter(&s, 1), "seek_chapter 正向应能移动");
    int c1 = 0;
    for (int c = 1; c <= 14; c++) if (VOCAB[s.idx[s.pos]].chapters & (1u << (c - 1))) { c1 = c; break; }
    CHECK(c1 != c0, "seek_chapter 后应换到不同章节");
}

static void test_session_weak(void)
{
    static int idx[TEST_CAP];
    static uint8_t store[TEST_CAP];
    vocab_prog_t p;
    vocab_prog_init(&p, store, VOCAB_COUNT);
    memset(store, 0, sizeof(store));

    vocab_session_t s;
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_WEAK, 1);
    CHECK(s.n == 0, "没有任何答题记录时错词本应为空");

    vocab_prog_answer(&p, 5, false);
    vocab_prog_answer(&p, 9, false);
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_WEAK, 1);
    CHECK(s.n == 2, "两条错词应进入错词本");
    int has5 = 0, has9 = 0;
    for (int i = 0; i < s.n; i++) { if (s.idx[i] == 5) has5 = 1; if (s.idx[i] == 9) has9 = 1; }
    CHECK(has5 && has9, "错词本应含 5 与 9");

    // 答对 3 次后脱离错词本
    vocab_prog_answer(&p, 5, true);
    vocab_prog_answer(&p, 5, true);
    vocab_prog_answer(&p, 5, true);
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_WEAK, 1);
    has5 = 0;
    for (int i = 0; i < s.n; i++) if (s.idx[i] == 5) has5 = 1;
    CHECK(!has5, "熟练后应移出错词本");
}

static void test_session_shuffle(void)
{
    static int idx[TEST_CAP];
    static int idx2[TEST_CAP];
    static uint8_t store[TEST_CAP];
    vocab_prog_t p;
    vocab_prog_init(&p, store, VOCAB_COUNT);
    memset(store, 0, sizeof(store));

    vocab_session_t s, s2;
    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_SHUFFLE, 12345);
    vocab_session_build(&s2, idx2, &p, 0, VOCAB_ORDER_SHUFFLE, 12345);
    CHECK(s.n == VOCAB_COUNT, "洗牌不改变条目数");
    CHECK(memcmp(idx, idx2, sizeof(int) * (size_t)s.n) == 0, "同种子洗牌结果应一致");

    vocab_session_build(&s2, idx2, &p, 0, VOCAB_ORDER_SHUFFLE, 999);
    CHECK(memcmp(idx, idx2, sizeof(int) * (size_t)s.n) != 0, "不同种子洗牌结果应不同");

    // 洗牌后仍是 0..N-1 的排列
    static uint8_t seen[TEST_CAP];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < s.n; i++) {
        CHECK(s.idx[i] >= 0 && s.idx[i] < VOCAB_COUNT, "洗牌索引在范围内");
        seen[s.idx[i]] = 1;
    }
    int total = 0;
    for (int i = 0; i < VOCAB_COUNT; i++) total += seen[i];
    CHECK(total == VOCAB_COUNT, "洗牌是完整排列");
}

static void test_data_integrity(void)
{
    CHECK(VOCAB_COUNT > 1500, "词库应有 1500 条以上");
    int empty_en = 0, empty_zh = 0, bad_ch = 0, with_def = 0, with_ipa = 0;
    for (int i = 0; i < VOCAB_COUNT; i++) {
        if (!VOCAB[i].en || !VOCAB[i].en[0]) empty_en++;
        if (!VOCAB[i].zh || !VOCAB[i].zh[0]) empty_zh++;
        if (VOCAB[i].chapters == 0) bad_ch++;
        if (VOCAB[i].def_idx != 0xFFFF) with_def++;
        if (VOCAB[i].ipa && VOCAB[i].ipa[0]) with_ipa++;
    }
    CHECK(empty_en == 0, "不应有空的英文术语");
    CHECK(empty_zh == 0, "不应有空的英文译名");
    CHECK(bad_ch == 0, "每条都应至少归属一个章节");
    CHECK(with_def > 0, "应有补充释义条目");
    CHECK(with_ipa > 1000, "应有 1000 条以上带音标");
    CHECK(VOCAB_DEF_COUNT > 0, "释义表非空");
    CHECK(vocab_def(0)[0] != 0, "释义 0 非空");
    CHECK(vocab_def(-1)[0] == 0, "越界释义返回空串");
}

int main(void)
{
    test_progress();
    test_session_seq();
    test_session_chapter();
    test_session_weak();
    test_session_shuffle();
    test_data_integrity();

    printf("vocab_model host tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
