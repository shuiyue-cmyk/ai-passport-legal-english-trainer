// tests/test_resume_logic.c —— 验证「按词条 id 恢复位置」的会话逻辑
#include "vocab_model.h"
#include "vocab_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CAP 4096
static int fails = 0;
static void check(int cond, const char *msg) {
    if (!cond) { fails++; printf("FAIL: %s\n", msg); }
}

int main(void) {
    static int idx[TEST_CAP];
    static uint8_t prog[TEST_CAP];
    vocab_prog_t p; vocab_prog_init(&p, prog, TEST_CAP);
    memset(prog, 0, sizeof(prog));
    vocab_session_t s;

    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_SEQ, 7);
    check(s.n == VOCAB_COUNT, "session should contain all entries");
    check(s.idx[0] == 0 && s.idx[s.n-1] == VOCAB_COUNT-1, "seq ordering");

    int target = 99;
    check(vocab_session_set_current_id(&s, target) == true, "resume by id 99");
    check(vocab_session_current(&s) == target, "current should be 99");

    vocab_session_next(&s);
    check(vocab_session_current(&s) == target + 1, "next after resume is 100");
    vocab_session_prev(&s);
    check(vocab_session_current(&s) == target, "back to resumed 99");

    vocab_session_prev(&s);
    check(vocab_session_current(&s) == target - 1, "prev wraps relative to resumed 99");

    check(vocab_session_set_current_id(&s, VOCAB_COUNT) == false, "invalid id rejected");
    check(vocab_session_current(&s) == target - 1, "invalid set does not disturb pos");

    vocab_session_build(&s, idx, &p, 0, VOCAB_ORDER_WEAK, 7);
    check(s.n == 0, "weak session empty without wrong answers");

    printf("resume_logic tests %s\n", fails ? "FAILED" : "PASS");
    return fails ? 1 : 0;
}
