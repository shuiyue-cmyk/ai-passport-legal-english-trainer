// main/vocab_app.c —— 法律英语背单词玩法
//
// 词库来源：《American Law and Legal Systems》(Calvi & Coleman) 14 章中英对照教材
//   教材已标注术语 1634 条 + 依《元照英美法词典》补注的英美体制术语 45 条 = 1679 条。
//
// 交互（全局约定：OK 长按 = 返回主菜单）
//   主菜单   : ↑↓ 选择, OK 进入
//   卡片页   : 未翻面 OK=显示答案; 已翻面 ↑=不认识 ↓=认识 OK=发音
//              ↑↓ 双击 = 跳 10 条, ↑↓ 长按 = 跳章
//   拼写页   : ↑↓ 选字母, OK 确认, OK 双击 删除, ↑↓ 双击 发音
//   统计页   : OK 长按 返回
//
// 字体分工（三套，见 tools/gen_vocab_data.py 与 main/CMakeLists.txt）：
//   vocab_cjk_16  —— 词库中文与释义（黑体 16px，1116 字）
//   vocab_hint_14 —— 顶栏/底栏界面文案（黑体 14px，105 字）
//   vocab_ipa_16  —— 国际音标（Arial 16px，含 IPA 扩展区）
#include "vocab_app.h"
#include "vocab_data.h"
#include "vocab_model.h"
#include "vocab_audio.h"
#include "bsp_display.h"

#include "lvgl.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LV_FONT_DECLARE(vocab_cjk_16);
LV_FONT_DECLARE(vocab_hint_14);
LV_FONT_DECLARE(vocab_ipa_16);

static const char *TAG = "vocab_app";

// ---------------------------------------------------------------- 常量与配色
#define SCR_W 240
#define SCR_H 320

#define C_BG      0xF4F4EA   // 纸白背景
#define C_INK     0x17202A   // 主文字
#define C_HEADER  0x17202A   // 顶栏
#define C_HDRTXT  0xF4F4EA
#define C_ACCENT  0x1F6FB2   // 英文/强调
#define C_ANS     0xC0392B   // 中文答案
#define C_MUTED   0x5C6B73   // 次要文字
#define C_FOOT    0xE2E2D6   // 底栏
#define C_SEL     0xFFD928   // 选中高亮
#define C_OK      0x2E8B57
#define C_BAD     0xC0392B

#define HDR_H 28
#define FTR_H 30
#define CONT_Y HDR_H
#define CONT_H (SCR_H - HDR_H - FTR_H)

#define ALPHA_LEN 26

// ---------------------------------------------------------------- 视图与模式
typedef enum { VIEW_MENU = 0, VIEW_CARD, VIEW_SPELL, VIEW_STATS } view_t;

// ⚠ 计数枚举必须放在最后一个成员，且成员数要和 MODE_NAME[] 一致。
//   这里 6 个模式（下标 0..5），MODE_COUNT = 6。
typedef enum {
    MODE_EN2ZH = 0,   // 看英文想中文
    MODE_ZH2EN,       // 看中文想英文
    MODE_SPELL,       // 看中文拼英文
    MODE_MIXED,       // 英↔中随机
    MODE_WEAK,        // 错词本
    MODE_STATS,       // 学习统计
    MODE_COUNT
} mode_t;

static const char *MODE_NAME[MODE_COUNT] = {
    "英→中", "中→英", "拼写", "混合练习", "错词本", "学习统计"
};

_Static_assert(sizeof(MODE_NAME) / sizeof(MODE_NAME[0]) == MODE_COUNT,
               "MODE_NAME 与 MODE_COUNT 数量不一致");

// ---------------------------------------------------------------- 全局状态
static view_t  s_view = VIEW_MENU;
static mode_t  s_mode = MODE_EN2ZH;
static int     s_menu_sel = 0;

static uint8_t *s_prog_bytes = NULL;      // 每词 1 字节进度
static int     *s_idx_storage = NULL;     // 出题顺序缓冲
static vocab_prog_t    s_prog;
static vocab_session_t s_sess;

static bool    s_flipped = false;
static uint16_t s_chapter_mask = 0;       // 0 = 不限章节
static uint32_t s_mixed_seed = 1;
static int     s_saved_index = -1;        // 上次学习页里的词条 id（跨会话恢复用）

// 拼写状态
static char    s_typed[64];
static int     s_typed_len = 0;
static int     s_target_len = 0;
static int     s_alpha_pos = 0;
static int     s_spell_result = 0;        // 0=进行中 1=正确 -1=错误

// LVGL 对象
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_hdr_l = NULL, *s_hdr_c = NULL, *s_hdr_r = NULL;
static lv_obj_t *s_ftr = NULL;
static lv_obj_t *s_lbl_prompt = NULL, *s_lbl_sub = NULL;
static lv_obj_t *s_lbl_ans = NULL, *s_lbl_ans2 = NULL, *s_lbl_def = NULL;
static lv_obj_t *s_lbl_input = NULL, *s_lbl_pick = NULL;
static lv_obj_t *s_menu_panels[MODE_COUNT];
static lv_obj_t *s_menu_labels[MODE_COUNT];

// ---------------------------------------------------------------- NVS 持久化
#define NVS_NS    "vocab"
#define NVS_KPROG "prog"
#define NVS_KSTAT "stat"

typedef struct {
    int32_t  index;
    uint8_t  mode;
    uint16_t chapter_mask;
    uint32_t magic;
} vocab_stat_t;
#define STAT_MAGIC 0x4C45564Fu   /* "LEVO" */

static void progress_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = VOCAB_COUNT;
    if (nvs_get_blob(h, NVS_KPROG, s_prog_bytes, &len) != ESP_OK || len != (size_t)VOCAB_COUNT) {
        memset(s_prog_bytes, 0, (size_t)VOCAB_COUNT);
    }
    vocab_stat_t st;
    size_t slen = sizeof(st);
    if (nvs_get_blob(h, NVS_KSTAT, &st, &slen) == ESP_OK && st.magic == STAT_MAGIC) {
        if (st.mode < MODE_COUNT) s_mode = (mode_t)st.mode;
        s_chapter_mask = st.chapter_mask;
        if (st.index >= 0 && st.index < VOCAB_COUNT) s_saved_index = st.index;
    }
    nvs_close(h);
}

static void progress_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KPROG, s_prog_bytes, (size_t)VOCAB_COUNT);
    int cur = vocab_session_current(&s_sess);
    if (cur >= 0) s_saved_index = cur;
    vocab_stat_t st = {
        .index = s_saved_index,
        .mode = (uint8_t)s_mode,
        .chapter_mask = s_chapter_mask,
        .magic = STAT_MAGIC,
    };
    nvs_set_blob(h, NVS_KSTAT, &st, sizeof(st));
    nvs_commit(h);
    nvs_close(h);
}

// ---------------------------------------------------------------- UI 小工具
// 所有自建对象一律先 lv_obj_remove_style_all()，去掉默认主题带来的
// 背景/圆角/边框/阴影，再显式设置需要的样式。否则会凭空出现"白框"。
static lv_obj_t *mk_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

// 纯容器：完全透明、无边框，只用于布局
static lv_obj_t *mk_container(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    return o;
}

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                          lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}

static void scr_begin(void)
{
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);   // 必须显式不透明，否则残留上一屏像素
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_radius(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    mk_rect(s_scr, 0, 0, SCR_W, HDR_H, C_HEADER);
    s_hdr_l = mk_label(s_scr, &vocab_hint_14, C_HDRTXT, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_hdr_l, 96);
    lv_obj_set_pos(s_hdr_l, 8, 7);

    s_hdr_c = mk_label(s_scr, &lv_font_montserrat_14, C_HDRTXT, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_hdr_c, 96);
    lv_obj_set_pos(s_hdr_c, 72, 7);

    s_hdr_r = mk_label(s_scr, &vocab_hint_14, 0x8FD3FF, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(s_hdr_r, 62);
    lv_obj_set_pos(s_hdr_r, 170, 7);

    mk_rect(s_scr, 0, SCR_H - FTR_H, SCR_W, FTR_H, C_FOOT);
    s_ftr = mk_label(s_scr, &vocab_hint_14, C_MUTED, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_ftr, SCR_W - 10);
    lv_obj_set_pos(s_ftr, 5, SCR_H - FTR_H + 8);

    s_lbl_prompt = s_lbl_sub = s_lbl_ans = s_lbl_ans2 = s_lbl_def = NULL;
    s_lbl_input = s_lbl_pick = NULL;
}

static void set_hdr(const char *l, const char *c, const char *r)
{
    if (s_hdr_l) lv_label_set_text(s_hdr_l, l ? l : "");
    if (s_hdr_c) lv_label_set_text(s_hdr_c, c ? c : "");
    if (s_hdr_r) lv_label_set_text(s_hdr_r, r ? r : "");
}

static void set_ftr(const char *t)
{
    if (s_ftr) lv_label_set_text(s_ftr, t ? t : "");
}

// ---------------------------------------------------------------- 章节徽标
static void chapter_badge(const vocab_entry_t *e, char *buf, size_t n)
{
    int first = 0, cnt = 0;
    for (int c = 1; c <= 14; c++) {
        if (e->chapters & (1u << (c - 1))) { if (!first) first = c; cnt++; }
    }
    if (cnt == 0)       snprintf(buf, n, "--");
    else if (cnt == 14) snprintf(buf, n, "全书");
    else if (cnt == 1)  snprintf(buf, n, "Ch.%d", first);
    else                snprintf(buf, n, "Ch.%d+%d", first, cnt - 1);
}

// ---------------------------------------------------------------- 主菜单
static void menu_build(void);

static void menu_refresh(void)
{
    int learned = 0, good = 0, wrong = 0;
    vocab_prog_stats(&s_prog, &learned, &good, &wrong);
    for (int i = 0; i < MODE_COUNT; i++) {
        lv_obj_set_style_bg_color(s_menu_panels[i],
            lv_color_hex(i == s_menu_sel ? C_SEL : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(s_menu_panels[i],
            lv_color_hex(i == s_menu_sel ? C_INK : 0xBFC8CC), 0);
        lv_obj_set_style_text_color(s_menu_labels[i],
            lv_color_hex(i == s_menu_sel ? C_INK : 0x33444C), 0);
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "已学 %d/%d  熟练 %d  错词 %d",
             learned, VOCAB_COUNT, good, wrong);
    set_hdr("法律英语", "", "");
    set_ftr(buf);
}

static void menu_build(void)
{
    scr_begin();
    set_hdr("法律英语", "", "");
    s_view = VIEW_MENU;

    for (int i = 0; i < MODE_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int x = 10 + col * 114;
        int y = CONT_Y + 16 + row * 52;
        lv_obj_t *p = mk_rect(s_scr, x, y, 106, 44, 0xFFFFFF);
        lv_obj_set_style_radius(p, 6, 0);
        lv_obj_set_style_border_width(p, 3, 0);
        lv_obj_set_style_border_color(p, lv_color_hex(0xBFC8CC), 0);
        s_menu_panels[i] = p;

        lv_obj_t *lb = mk_label(p, &vocab_cjk_16, 0x33444C, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(lb, MODE_NAME[i]);
        lv_obj_set_width(lb, 96);
        lv_obj_center(lb);
        s_menu_labels[i] = lb;
    }
    menu_refresh();

    // MODE_NAME 与 MODE_COUNT 的一致性由上方 _Static_assert 保证；
    // 这里不读取对象坐标，因为 LVGL 在 lv_screen_load() 前尚未完成布局计算。

    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------- 卡片视图
static int mixed_dir(int id)
{
    uint32_t x = (uint32_t)id * 2654435761u + s_mixed_seed;
    x ^= x >> 13;
    return (int)(x & 1u);
}

// 返回 true 表示「先显示英文、答案在中文」（英→中）
static bool card_dir_en_first(int id)
{
    if (s_mode == MODE_EN2ZH) return true;
    if (s_mode == MODE_ZH2EN) return false;
    if (s_mode == MODE_MIXED) return mixed_dir(id) == 0;
    return true;
}

static void card_render(void);

static void card_build(void)
{
    scr_begin();
    s_view = VIEW_CARD;
    s_flipped = false;

    lv_obj_t *cont = mk_container(s_scr, 4, CONT_Y + 2, SCR_W - 8, CONT_H - 4);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 6, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    s_lbl_prompt = mk_label(cont, &lv_font_montserrat_20, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_prompt, SCR_W - 24);

    s_lbl_sub = mk_label(cont, &vocab_ipa_16, C_MUTED, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_sub, SCR_W - 24);

    s_lbl_ans = mk_label(cont, &vocab_cjk_16, C_ANS, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_ans, SCR_W - 24);

    s_lbl_ans2 = mk_label(cont, &vocab_ipa_16, C_MUTED, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_ans2, SCR_W - 24);

    s_lbl_def = mk_label(cont, &vocab_cjk_16, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_lbl_def, SCR_W - 24);

    card_render();
    lv_screen_load(s_scr);
}

static void card_render(void)
{
    int id = vocab_session_current(&s_sess);
    if (id < 0) {
        set_hdr(MODE_NAME[s_mode], "", "");
        lv_label_set_text(s_lbl_prompt, "该筛选下没有词条");
        lv_label_set_text(s_lbl_sub, "");
        lv_label_set_text(s_lbl_ans, "");
        lv_label_set_text(s_lbl_ans2, "");
        lv_label_set_text(s_lbl_def, "");
        set_ftr("长按 OK 返回菜单");
        return;
    }
    const vocab_entry_t *e = &VOCAB[id];
    bool en_first = card_dir_en_first(id);

    char hdr_r[24], hdr_c[32], badge[16];
    chapter_badge(e, badge, sizeof(badge));
    snprintf(hdr_r, sizeof(hdr_r), "%s", badge);
    snprintf(hdr_c, sizeof(hdr_c), "%d/%d", s_sess.pos + 1, s_sess.n);
    set_hdr(MODE_NAME[s_mode], hdr_c, hdr_r);

    const char *ipa = (e->ipa && e->ipa[0]) ? e->ipa : "";
    const char *def = (e->def_idx != 0xFFFF) ? vocab_def(e->def_idx) : "";

    if (!s_flipped) {
        if (en_first) {
            lv_obj_set_style_text_font(s_lbl_prompt, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(s_lbl_prompt, lv_color_hex(C_INK), 0);
            lv_label_set_text(s_lbl_prompt, e->en);
            lv_label_set_text(s_lbl_sub, ipa);
        } else {
            lv_obj_set_style_text_font(s_lbl_prompt, &vocab_cjk_16, 0);
            lv_obj_set_style_text_color(s_lbl_prompt, lv_color_hex(C_INK), 0);
            lv_label_set_text(s_lbl_prompt, e->zh);
            lv_label_set_text(s_lbl_sub, "");
        }
        lv_label_set_text(s_lbl_ans, "");
        lv_label_set_text(s_lbl_ans2, "");
        lv_label_set_text(s_lbl_def, "");
        lv_obj_add_flag(s_lbl_ans, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_lbl_ans2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_lbl_def, LV_OBJ_FLAG_HIDDEN);
        set_ftr("OK 看答案   ↑↓ 换词   ↑↓长按 跳章");
    } else {
        if (en_first) {
            // 提示仍是英文，答案是中文
            lv_obj_set_style_text_font(s_lbl_prompt, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(s_lbl_prompt, lv_color_hex(C_ACCENT), 0);
            lv_label_set_text(s_lbl_prompt, e->en);
            lv_label_set_text(s_lbl_sub, ipa);
            lv_obj_set_style_text_font(s_lbl_ans, &vocab_cjk_16, 0);
            lv_obj_set_style_text_color(s_lbl_ans, lv_color_hex(C_ANS), 0);
            lv_label_set_text(s_lbl_ans, e->zh);
            lv_label_set_text(s_lbl_ans2, "");
            lv_obj_add_flag(s_lbl_ans2, LV_OBJ_FLAG_HIDDEN);
        } else {
            // 提示是中文，答案是英文 + 音标
            lv_obj_set_style_text_font(s_lbl_prompt, &vocab_cjk_16, 0);
            lv_obj_set_style_text_color(s_lbl_prompt, lv_color_hex(C_INK), 0);
            lv_label_set_text(s_lbl_prompt, e->zh);
            lv_label_set_text(s_lbl_sub, "");
            lv_obj_set_style_text_font(s_lbl_ans, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(s_lbl_ans, lv_color_hex(C_ACCENT), 0);
            lv_label_set_text(s_lbl_ans, e->en);
            lv_label_set_text(s_lbl_ans2, ipa);
            if (ipa[0]) lv_obj_clear_flag(s_lbl_ans2, LV_OBJ_FLAG_HIDDEN);
            else        lv_obj_add_flag(s_lbl_ans2, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(s_lbl_def, def);
        lv_obj_clear_flag(s_lbl_ans, LV_OBJ_FLAG_HIDDEN);
        if (def[0]) lv_obj_clear_flag(s_lbl_def, LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(s_lbl_def, LV_OBJ_FLAG_HIDDEN);
        set_ftr("↑ 不认识   ↓ 认识   OK 发音");
    }
}

static void card_mark(bool correct)
{
    int id = vocab_session_current(&s_sess);
    if (id >= 0) vocab_prog_answer(&s_prog, id, correct);
    progress_save();
    s_flipped = false;
    vocab_session_next(&s_sess);
    card_render();
}

// ---------------------------------------------------------------- 拼写视图
static void spell_render(void);
static void spell_load(void);

static void spell_build(void)
{
    scr_begin();
    s_view = VIEW_SPELL;

    lv_obj_t *cont = mk_container(s_scr, 4, CONT_Y + 2, SCR_W - 8, CONT_H - 4);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 8, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_lbl_prompt = mk_label(cont, &vocab_cjk_16, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_prompt, SCR_W - 24);

    s_lbl_sub = mk_label(cont, &vocab_hint_14, C_MUTED, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_sub, SCR_W - 24);

    s_lbl_input = mk_label(cont, &lv_font_montserrat_20, C_ACCENT, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_input, SCR_W - 24);

    s_lbl_pick = mk_label(cont, &lv_font_montserrat_20, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_pick, SCR_W - 24);

    s_lbl_ans = mk_label(cont, &vocab_cjk_16, C_OK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_lbl_ans, SCR_W - 24);

    s_lbl_ans2 = NULL;
    s_lbl_def = NULL;

    spell_load();
    lv_screen_load(s_scr);
}

static void spell_load(void)
{
    int id = vocab_session_current(&s_sess);
    s_typed_len = 0;
    s_typed[0] = 0;
    s_alpha_pos = 0;
    s_spell_result = 0;
    if (id >= 0) {
        int n = (int)strlen(VOCAB[id].en);
        s_target_len = (n > 40) ? 40 : n;
    } else {
        s_target_len = 0;
    }
    spell_render();
}

static void spell_render(void)
{
    int id = vocab_session_current(&s_sess);
    if (id < 0) {
        set_hdr("拼写", "", "");
        lv_label_set_text(s_lbl_prompt, "该筛选下没有可拼写的词条");
        lv_label_set_text(s_lbl_sub, "");
        lv_label_set_text(s_lbl_input, "");
        lv_label_set_text(s_lbl_pick, "");
        lv_label_set_text(s_lbl_ans, "");
        set_ftr("长按 OK 返回菜单");
        return;
    }
    const vocab_entry_t *e = &VOCAB[id];
    char hdr_r[24], hdr_c[32], badge[16];
    chapter_badge(e, badge, sizeof(badge));
    snprintf(hdr_r, sizeof(hdr_r), "%s", badge);
    snprintf(hdr_c, sizeof(hdr_c), "%d/%d", s_sess.pos + 1, s_sess.n);
    set_hdr("拼写", hdr_c, hdr_r);

    lv_obj_set_style_text_color(s_lbl_prompt, lv_color_hex(C_INK), 0);
    lv_label_set_text(s_lbl_prompt, e->zh);

    char sub[64];
    snprintf(sub, sizeof(sub), "首字母 %c · 共 %d 个字母", e->en[0], (int)strlen(e->en));
    lv_label_set_text(s_lbl_sub, sub);

    char inbuf[160];
    int p = 0;
    for (int i = 0; i < s_target_len && p < (int)sizeof(inbuf) - 4; i++) {
        inbuf[p++] = (i < s_typed_len) ? s_typed[i] : '_';
        inbuf[p++] = ' ';
    }
    inbuf[p] = 0;
    lv_label_set_text(s_lbl_input, inbuf);

    if (s_spell_result == 0) {
        char pb[16];
        snprintf(pb, sizeof(pb), "▲ %c ▼", (char)('a' + s_alpha_pos));
        lv_label_set_text(s_lbl_pick, pb);
        lv_obj_set_style_text_color(s_lbl_pick, lv_color_hex(C_INK), 0);
        lv_label_set_text(s_lbl_ans, "");
        set_ftr("↑↓ 选字母   OK 确认   双击OK 删除");
    } else if (s_spell_result > 0) {
        lv_label_set_text(s_lbl_pick, "");
        lv_obj_set_style_text_color(s_lbl_ans, lv_color_hex(C_OK), 0);
        lv_label_set_text(s_lbl_ans, "拼写正确 ✓");
        set_ftr("OK 下一词   长按 OK 返回");
    } else {
        char ab[128];
        snprintf(ab, sizeof(ab), "拼错了：%s", e->en);
        lv_label_set_text(s_lbl_pick, "");
        lv_obj_set_style_text_color(s_lbl_ans, lv_color_hex(C_BAD), 0);
        lv_label_set_text(s_lbl_ans, ab);
        set_ftr("OK 下一词   长按 OK 返回");
    }
}

static void spell_check(void)
{
    int id = vocab_session_current(&s_sess);
    if (id < 0) return;
    s_spell_result = (strcmp(s_typed, VOCAB[id].en) == 0) ? 1 : -1;
    vocab_prog_answer(&s_prog, id, s_spell_result > 0);
    progress_save();
    spell_render();
}

// ---------------------------------------------------------------- 统计视图
static void stats_build(void)
{
    scr_begin();
    s_view = VIEW_STATS;
    set_hdr("学习统计", "", "");

    int learned = 0, good = 0, wrong = 0;
    vocab_prog_stats(&s_prog, &learned, &good, &wrong);
    int pct = (int)((long)learned * 100 / VOCAB_COUNT);

    lv_obj_t *cont = mk_container(s_scr, 10, CONT_Y + 10, SCR_W - 20, CONT_H - 20);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_set_style_pad_row(cont, 10, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    struct { const char *k; int v; uint32_t c; } rows[] = {
        { "词库总量", VOCAB_COUNT, C_INK    },
        { "已学词条", learned,     C_ACCENT },
        { "熟练词条", good,        C_OK     },
        { "错词数量", wrong,       C_BAD    },
    };
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        char b[64];
        snprintf(b, sizeof(b), "%s：%d", rows[i].k, rows[i].v);
        lv_obj_t *l = mk_label(cont, &vocab_cjk_16, rows[i].c, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_width(l, SCR_W - 40);
        lv_label_set_text(l, b);
    }

    char pb[64];
    snprintf(pb, sizeof(pb), "学习进度：%d%%", pct);
    lv_obj_t *pl = mk_label(cont, &vocab_cjk_16, C_INK, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(pl, SCR_W - 40);
    lv_label_set_text(pl, pb);

    lv_obj_t *bar = mk_rect(cont, 0, 0, SCR_W - 44, 14, 0xD9DED8);
    lv_obj_set_style_radius(bar, 4, 0);
    int fill = (SCR_W - 44) * pct / 100;
    if (fill > 0) {
        lv_obj_t *f = mk_rect(bar, 0, 0, fill, 14, C_OK);
        lv_obj_set_style_radius(f, 4, 0);
    }

    lv_obj_t *note = mk_label(cont, &vocab_hint_14, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(note, SCR_W - 40);
    lv_label_set_text(note, "进度保存在设备 NVS，断电不丢。");

    set_ftr("长按 OK 返回菜单");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------- 会话启动
static bool is_single_word(const char *s)
{
    for (const char *p = s; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) return false;
    }
    return true;
}

static void show_message(const char *title, const char *msg)
{
    scr_begin();
    s_view = VIEW_STATS;
    set_hdr(title, "", "");
    lv_obj_t *l = mk_label(s_scr, &vocab_cjk_16, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(l, SCR_W - 40);
    lv_obj_set_pos(l, 20, 120);
    lv_label_set_text(l, msg);
    set_ftr("长按 OK 返回菜单");
    lv_screen_load(s_scr);
}

static void start_mode(mode_t m)
{
    s_mode = m;

    if (m == MODE_STATS) { stats_build(); return; }

    vocab_order_t order = VOCAB_ORDER_SEQ;
    if (m == MODE_MIXED) { order = VOCAB_ORDER_SHUFFLE; s_mixed_seed = (uint32_t)lv_tick_get() | 1u; }
    if (m == MODE_WEAK)  order = VOCAB_ORDER_WEAK;

    vocab_session_build(&s_sess, s_idx_storage, &s_prog, s_chapter_mask, order,
                        (uint32_t)lv_tick_get() | 1u);

    if (m == MODE_SPELL) {
        int w = 0;
        for (int i = 0; i < s_sess.n; i++) {
            if (is_single_word(VOCAB[s_sess.idx[i]].en)) s_sess.idx[w++] = s_sess.idx[i];
        }
        s_sess.n = w;
    }

    if (s_sess.n <= 0) {
        if (m == MODE_WEAK) show_message(MODE_NAME[m], "错词本是空的——先去做几组练习吧。");
        else                show_message(MODE_NAME[m], "当前筛选下没有可练习的词条。");
        return;
    }

    // 顺序/拼写模式按词条 id 恢复上次位置；混合随机每轮重新洗牌，不续接。
    if (order == VOCAB_ORDER_SEQ && s_saved_index >= 0 &&
        vocab_session_set_current_id(&s_sess, s_saved_index)) {
        ESP_LOGI(TAG, "断点续背: 从第 %d 条继续", s_saved_index + 1);
    }

    if (m == MODE_SPELL) spell_build();
    else                 card_build();
}

// ---------------------------------------------------------------- 按键分发
static void card_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (btn == BSP_BTN_OK) {
        if (ev == BSP_BTN_CLICK) {
            if (!s_flipped) { s_flipped = true; card_render(); }
            else            { card_mark(true); }
        } else if (ev == BSP_BTN_DOUBLE) {
            int id = vocab_session_current(&s_sess);
            if (id >= 0) vocab_audio_play(id);
        }
        return;
    }
    if (ev == BSP_BTN_CLICK) {
        if (s_flipped) {
            card_mark(btn == BSP_BTN_DOWN);   // ↓ = 认识, ↑ = 不认识
        } else {
            if (btn == BSP_BTN_DOWN) vocab_session_next(&s_sess);
            else                     vocab_session_prev(&s_sess);
            progress_save();
            card_render();
        }
    } else if (ev == BSP_BTN_DOUBLE) {
        vocab_session_jump(&s_sess, btn == BSP_BTN_DOWN ? 10 : -10);
        s_flipped = false;
        progress_save();
        card_render();
    } else if (ev == BSP_BTN_LONG) {
        if (vocab_session_seek_chapter(&s_sess, btn == BSP_BTN_DOWN ? 1 : -1)) {
            s_flipped = false;
            progress_save();
            card_render();
        }
    }
}

static void spell_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    int id = vocab_session_current(&s_sess);
    if (id < 0) return;

    if (s_spell_result != 0) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            s_flipped = false;
            vocab_session_next(&s_sess);
            progress_save();
            spell_load();
        }
        return;
    }

    if (btn == BSP_BTN_OK) {
        if (ev == BSP_BTN_CLICK) {
            if (s_typed_len < s_target_len) {
                s_typed[s_typed_len++] = (char)('a' + s_alpha_pos);
                s_typed[s_typed_len] = 0;
                if (s_typed_len == s_target_len) { spell_check(); return; }
            }
            spell_render();
        } else if (ev == BSP_BTN_DOUBLE) {
            if (s_typed_len > 0) s_typed[--s_typed_len] = 0;
            spell_render();
        }
        return;
    }
    if (ev == BSP_BTN_CLICK) {
        s_alpha_pos = (s_alpha_pos + (btn == BSP_BTN_DOWN ? 1 : ALPHA_LEN - 1)) % ALPHA_LEN;
        spell_render();
    } else if (ev == BSP_BTN_LONG) {
        s_alpha_pos = (btn == BSP_BTN_DOWN) ? ALPHA_LEN - 1 : 0;
        spell_render();
    } else if (ev == BSP_BTN_DOUBLE) {
        vocab_audio_play(id);
    }
}

void vocab_app_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // 全局：OK 长按 = 返回主菜单
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        vocab_audio_stop();
        progress_save();
        if (s_view != VIEW_MENU) menu_build();
        return;
    }

    switch (s_view) {
    case VIEW_MENU:
        if (ev == BSP_BTN_CLICK) {
            if (btn == BSP_BTN_UP)   { s_menu_sel = (s_menu_sel + MODE_COUNT - 1) % MODE_COUNT; menu_refresh(); }
            if (btn == BSP_BTN_DOWN) { s_menu_sel = (s_menu_sel + 1) % MODE_COUNT;              menu_refresh(); }
            if (btn == BSP_BTN_OK)   { start_mode((mode_t)s_menu_sel); }
        }
        break;
    case VIEW_CARD:  card_key(btn, ev);  break;
    case VIEW_SPELL: spell_key(btn, ev); break;
    case VIEW_STATS: break;
    default: break;
    }
}

void vocab_app_start(void)
{
    s_prog_bytes = malloc((size_t)VOCAB_COUNT);
    s_idx_storage = malloc(sizeof(int) * (size_t)VOCAB_COUNT);
    if (!s_prog_bytes || !s_idx_storage) {
        ESP_LOGE(TAG, "内存不足：需要 %d B 进度 + %d B 索引",
                 VOCAB_COUNT, (int)(sizeof(int) * VOCAB_COUNT));
        return;
    }
    memset(s_prog_bytes, 0, (size_t)VOCAB_COUNT);
    vocab_prog_init(&s_prog, s_prog_bytes, VOCAB_COUNT);
    progress_load();
    if (s_saved_index >= 0) {
        ESP_LOGI(TAG, "恢复断点: 上次词条 id=%d", s_saved_index + 1);
    }

    if (vocab_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "音频分区不可用，发音功能将静默跳过");
    }

    s_menu_sel = 0;
    menu_build();
    ESP_LOGI(TAG, "词库就绪: %d 条 (教材标注 1634 + 词典补注 45), 模式 %d 个",
             VOCAB_COUNT, MODE_COUNT);
}
