// main/vocab_app.c —— 法律英语背单词玩法
//
// 词库来源：《American Law and Legal Systems》(Calvi & Coleman)中英对照教材
//   术语经去重、词典补注、非法律词削减与翻译复核，当前共 1389 条（见 tools/vocab_master.json）。
//
// 交互（全局约定：OK 长按 = 返回主菜单）
//   主菜单   : ↑↓ 选择, OK 进入（含底部“重置进度”，删除前二次确认）
//   卡片页   : 未翻面 OK=显示答案; 已翻面 ↑=不认识 ↓=认识 OK=发音
//              ↑↓ 双击 = 跳 10 条, ↑↓ 长按 = 跳章
//   通用     : 新词条出现自动播放一遍发音（翻面/判分/切阶段不重播）
//   选择题   : ↑↓ 选行, OK 确认；A-D 作答，末行“不认识”（记错亮答案）
//              与“播放音频”（只发音）左右各一；答错标错、隔 3 词重练，直到答对过关
//   复习页   : 英文认不认识；认识亮中文，OK 进四选一；答错回错词本，
//              答对按 1/4 概率 30 分钟后再见（未抽中即毕业）；不认识直接往后放一轮四选一
//   长文本   : 释义与例句首停 3 秒后单行来回弹滚动（短文本静止），无需按键翻页
//   确认页   : ↑↓ 换选项, OK 确认
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
#include "bsp_battery.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

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

// 主菜单焦点：0..MODE_COUNT-1 是模式，MODE_COUNT 是底部“重置进度”按钮。
#define MENU_FOCUS_COUNT (MODE_COUNT + 1)
#define FOCUS_RESET MODE_COUNT

// ---------------------------------------------------------------- 视图与模式
typedef enum { VIEW_MENU = 0, VIEW_CARD, VIEW_CONFIRM, VIEW_QUIZ, VIEW_REVIEW, VIEW_STATS } view_t;

// ⚠ 计数枚举必须放在最后一个成员，且成员数要和 MODE_NAME[] 一致。
//   这里 5 个模式（下标 0..4），MODE_COUNT = 5。
//   NVS 里存的老模式号（带拼写时代）由 progress_load() 迁移，不要直接复用旧号。
typedef enum {
    MODE_EN2ZH = 0,   // 看英文想中文
    MODE_ZH2EN,       // 看中文想英文
    MODE_MIXED,       // 学习单词（4 选 1 闯关）
    MODE_WEAK,        // 错词本
    MODE_STATS,       // 学习统计
    MODE_REVIEW,      // 复习（到期重现，过关流）
    MODE_COUNT
} mode_t;

static const char *MODE_NAME[MODE_COUNT] = {
    "英→中", "中→英", "学习单词", "错词本", "学习统计", "复习"
};

// 主菜单显示顺序：学习单词置顶独占一行，复习跟在错词本后面。
// s_menu_sel 是显示序号（与枚举无关，老 NVS 存的模式号不受影响）。
static const uint8_t MENU_ORDER[MODE_COUNT] = {
    MODE_MIXED, MODE_EN2ZH, MODE_ZH2EN, MODE_WEAK, MODE_REVIEW, MODE_STATS
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
static int     s_saved_index = -1;        // 上次学习页里的词条 id（跨会话恢复用）
static int32_t s_batch_mixed = 0;         // 混合练习已完成的 20 词批次数
static int32_t s_batch_weak = 0;          // 错词本已完成的 20 词组数
static uint32_t s_study_sec = 0;          // 累计学习秒数（NVS 持久化）
static uint32_t s_study_start = 0;        // 本次进入学习页的 tick，0=不在学习中

// 复习到期表：词条 id → 到期时的累计学习分钟；0xFFFF=未安排。
// 学习单词答对、错词本答对、复习答对都会把到期推后 30 学习分钟。
static uint16_t *s_due = NULL;
#define DUE_SPLIT 700
#define DUE_NEVER 0xFFFF
#define NVS_KDUEA "duea"
#define NVS_KDUEB "dueb"
// 到期表读写函数定义在后（需 NVS 命名空间宏），此处仅声明。
static void due_load(void);
static void due_save(void);
static void due_schedule(int id, bool from_review);

// 选择题状态（混合练习 / 错词本共用）
#define QUIZ_BATCH 20     // 每批词数
#define QUIZ_OPTS  4      // 每题中文备选数
#define QUIZ_EXTRA 2      // 动作项：不认识 / 播放音频（同行显示，左右各一）
#define QUIZ_ROWS  (QUIZ_OPTS + QUIZ_EXTRA)   // 光标位置数
#define QUIZ_DELAY 3      // 答错后隔几词重现
#define QUIZ_QMAX  (QUIZ_BATCH + 24)   // 出题队列上限（含重练插入）
static int      s_q_ids[QUIZ_BATCH];   // 本批词条 id
static int      s_q_n = 0;             // 本批词数（≤20）
static int      s_q_queue[QUIZ_QMAX];  // 出题队列（词条 id，答错插回）
static int      s_q_qlen = 0, s_q_qpos = 0;
static int      s_q_opt[QUIZ_OPTS];    // 当前题 4 个选项的词条 id
static int      s_q_answer = 0;        // 正确选项下标
static int      s_q_sel = 0;           // 光标
static int      s_q_judged = 0;        // 0=未判 1=答对 -1=答错
static int      s_q_done = 0;          // 本批已过关数
static bool     s_q_ok[QUIZ_BATCH];    // 本批过关标记
static uint32_t s_q_rng = 0x12345678u;
static void quiz_clear_state(void);   // 定义在后：重置进度时清批内续背

// 学习锁死批次：按主章节（最小章节号）+ 词库顺序排好，每 20 个一批
// （末批 9 个，不足顺延下章补齐）。批次成员固定，不随掌握度漂移。
// s_batch_mixed 存批号（NVS），做完一批 +1。
static int *s_lock = NULL;      // 锁死顺序表（词条 id）
static int  s_lock_built = 0;
static int  s_q_batch_cur = 0;  // 本次运行的批次

// 批内续背 blob（与 prog/stat 同一次 NVS 事务写入，见 progress_save）
#define NVS_KQUIZ "qstat"
typedef struct {
    uint8_t  mode;
    int32_t  batch;
    uint8_t  n;
    uint16_t ids[QUIZ_BATCH];
} quiz_stat_t;

// 拼写状态（已删除拼写模式，此处不再保留输入缓冲）

// 二次确认状态（重置进度用）
static int     s_confirm_sel = 0;

// LVGL 对象
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_hdr_l = NULL, *s_hdr_c = NULL, *s_hdr_r = NULL;
static lv_obj_t *s_ftr = NULL;
static lv_obj_t *s_lbl_prompt = NULL, *s_lbl_sub = NULL;
static lv_obj_t *s_lbl_ans = NULL, *s_lbl_ans2 = NULL, *s_lbl_def = NULL;
static lv_obj_t *s_lbl_ex_en = NULL, *s_lbl_ex_zh = NULL;   // 教材例句（英/中）
static lv_obj_t *s_lbl_input = NULL, *s_lbl_pick = NULL;
static lv_obj_t *s_q_prompt = NULL, *s_q_opts[QUIZ_OPTS + 1];
static lv_obj_t *s_menu_panels[MODE_COUNT];
static lv_obj_t *s_menu_labels[MODE_COUNT];
static lv_obj_t *s_reset_panel = NULL, *s_reset_label = NULL;
static lv_obj_t *s_confirm_panels[2], *s_confirm_labels[2];

// ---------------------------------------------------------------- NVS 持久化
#define NVS_NS    "vocab"
#define NVS_KPROG "prog"
#define NVS_KSTAT "stat"

typedef struct {
    int32_t  index;
    uint8_t  mode;
    uint16_t chapter_mask;
    uint32_t magic;
    int32_t  batch_mixed;   // 后加字段：老 NVS 读不到时保持 0
    int32_t  batch_weak;
    uint32_t study_sec;     // 累计学习秒数（卡片/选择题页面内的时间）
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
    memset(&st, 0, sizeof(st));
    size_t slen = sizeof(st);
    if (nvs_get_blob(h, NVS_KSTAT, &st, &slen) == ESP_OK && st.magic == STAT_MAGIC) {
        // 老 NVS 迁移：拼写模式（旧 2）已删除。旧号 2→英→中，旧 3..5（混合/错词本/统计）
        // 依次前移一位；越界则回落到英→中。
        uint8_t m = st.mode;
        if (m == 2) m = (uint8_t)MODE_EN2ZH;
        else if (m >= 3 && m <= 5) m = (uint8_t)(m - 1);
        if (m < MODE_COUNT) s_mode = (mode_t)m;
        else s_mode = MODE_EN2ZH;
        s_chapter_mask = st.chapter_mask;
        if (st.index >= 0 && st.index < VOCAB_COUNT) s_saved_index = st.index;
        // 批次/时长字段是后加的：按偏移逐个判断，老 blob 短读不到时保持 0。
        if (slen >= offsetof(vocab_stat_t, batch_mixed) + sizeof(int32_t)) {
            if (st.batch_mixed >= 0) s_batch_mixed = st.batch_mixed;
            if (st.batch_weak >= 0) s_batch_weak = st.batch_weak;
        }
        if (slen >= offsetof(vocab_stat_t, study_sec) + sizeof(uint32_t)) {
            s_study_sec = st.study_sec;
        }
    }
    nvs_close(h);
}

static void progress_save(void)
{
    // 选择题批内进度并入同一事务（仅 quiz 进行中才写；他处调用不动旧 blob，
    // 这样中途去卡片逛一圈回来还能续接）。
    quiz_stat_t q;
    bool save_q = (s_view == VIEW_QUIZ);
    if (save_q) {
        memset(&q, 0, sizeof(q));
        q.mode = (uint8_t)s_mode;
        q.batch = (s_mode == MODE_MIXED) ? s_batch_mixed : s_batch_weak;
        for (int k = 0; k < s_q_n && q.n < QUIZ_BATCH; k++) {
            if (s_q_ok[k]) q.ids[q.n++] = (uint16_t)s_q_ids[k];
        }
    }
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
        .batch_mixed = s_batch_mixed,
        .batch_weak = s_batch_weak,
        .study_sec = s_study_sec,
    };
    nvs_set_blob(h, NVS_KSTAT, &st, sizeof(st));
    if (save_q) nvs_set_blob(h, NVS_KQUIZ, &q, sizeof(q));
    nvs_commit(h);
    nvs_close(h);
}

// 复习到期表读写（到期=累计学习分钟；0xFFFF=未安排）。
static void due_load(void)
{
    memset(s_due, 0xFF, sizeof(uint16_t) * (size_t)VOCAB_COUNT);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t la = sizeof(uint16_t) * DUE_SPLIT;
    size_t lb = sizeof(uint16_t) * ((size_t)VOCAB_COUNT - DUE_SPLIT);
    nvs_get_blob(h, NVS_KDUEA, s_due, &la);
    nvs_get_blob(h, NVS_KDUEB, s_due + DUE_SPLIT, &lb);
    nvs_close(h);
}

static void due_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KDUEA, s_due, sizeof(uint16_t) * DUE_SPLIT);
    nvs_set_blob(h, NVS_KDUEB, s_due + DUE_SPLIT,
                 sizeof(uint16_t) * ((size_t)VOCAB_COUNT - DUE_SPLIT));
    nvs_commit(h);
    nvs_close(h);
}

#define RV_DUE_MIN 30   // 准入后多少学习分钟到期
#define RV_ADMIT_PCT 25   // 复习准入概率(%)：答对后按此概率进/续复习池；
                          // 未抽中时学习/错词保留原到期，复习中则毕业离池。
static uint32_t q_rnd(void);   // 定义在后（选择题随机数），此处先声明
static void due_schedule(int id, bool from_review)
{
    if (id < 0 || id >= VOCAB_COUNT) return;
    if ((int)(q_rnd() % 100) >= RV_ADMIT_PCT) {
        if (from_review) {   // 复习中答对但未抽中 = 毕业离池
            s_due[id] = DUE_NEVER;
            due_save();
        }
        return;
    }
    uint32_t m = s_study_sec / 60;
    s_due[id] = (m + RV_DUE_MIN >= DUE_NEVER) ? DUE_NEVER : (uint16_t)(m + RV_DUE_MIN);
    due_save();
}

// 学习计时：进入卡片/选择题开始，回到主菜单结算。 s_study_start==0 表示不在学习中。
static void study_begin(void)
{
    s_study_start = lv_tick_get();
}

static void study_accrue(void)
{
    if (!s_study_start) return;
    uint32_t now = lv_tick_get();
    if (now > s_study_start) s_study_sec += (now - s_study_start) / 1000;
    s_study_start = 0;
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

static lv_timer_t *s_mq_timer = NULL;   // 跑马灯首停定时器

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
    s_lbl_ex_en = s_lbl_ex_zh = NULL;
    s_lbl_input = s_lbl_pick = NULL;
    if (s_mq_timer) { lv_timer_delete(s_mq_timer); s_mq_timer = NULL; }
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

// 顶栏电量：读 CW2017 SOC，5 秒缓存；无电量计返回空串（顶栏不显示）。
static int      s_bat_soc = -1;
static uint32_t s_bat_tick = 0;
static const char *battery_str(void)
{
    uint32_t now = lv_tick_get();
    if (s_bat_soc < 0 || now - s_bat_tick > 5000) {
        int v = bsp_battery_soc();
        if (v >= 0) {
            s_bat_soc = (v > 100) ? 100 : v;
            s_bat_tick = now;
        } else if (s_bat_soc < 0) {
            return "";
        }
    }
    static char s_bat_buf[16];
    snprintf(s_bat_buf, sizeof(s_bat_buf), "电%d%%", s_bat_soc);
    return s_bat_buf;
}

// ---------------------------------------------------------------- 息屏
// 5 分钟无按键自动息屏（关背光+面板休眠，CPU/音频不停）；主菜单长按↑↓立即息屏。
// 息屏后第一下按键只唤醒不动作（防误触），抬起后恢复正常。
#define IDLE_SLEEP_MS (5u * 60u * 1000u)
#define OFF_AFTER_SLEEP_MS (10u * 60u * 1000u)   // 息屏后再闲置这么久自动关机
static bool     s_screen_off = false;
static bool     s_swallow_until_release = false;
static uint32_t s_last_key_tick = 0;
static uint32_t s_sleep_tick = 0;   // 本次息屏的 tick

static void menu_refresh(void);
static void card_render(void);
static void quiz_render(void);
static void screen_wake_refresh(void)
{
    switch (s_view) {
    case VIEW_MENU: menu_refresh(); break;
    case VIEW_CARD: card_render();  break;
    case VIEW_QUIZ: quiz_render();  break;
    default: break;
    }
}

static void screen_sleep(void)
{
    if (s_screen_off) return;
    s_screen_off = true;
    s_sleep_tick = lv_tick_get();
    bsp_display_sleep(true);
}

static void screen_wake(void)
{
    if (!s_screen_off) return;
    s_screen_off = false;
    bsp_display_sleep(false);
    screen_wake_refresh();
}

// 深度睡眠自动关机：落盘全部状态后整机睡眠，任意键唤醒（=重启，从 NVS 恢复）。
// 三键共用 ADC0(GPIO0/RTC IO)，平时上拉为高，任一键按下都拉低，
// C3 无 EXT0，用 GPIO 唤醒（RTC 引脚支持深度睡眠唤醒）。
static void deep_shutdown(void)
{
    ESP_LOGI(TAG, "闲置关机：落盘后进深度睡眠");
    vocab_audio_stop();
    study_accrue();
    progress_save();
    due_save();
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_deep_sleep_start();
}

static void sleep_check_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t now = lv_tick_get();
    if (!s_screen_off) {
        if (now - s_last_key_tick > IDLE_SLEEP_MS) screen_sleep();
    } else if (now - s_sleep_tick > OFF_AFTER_SLEEP_MS) {
        deep_shutdown();
    }
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
    for (int d = 0; d < MODE_COUNT; d++) {
        lv_obj_set_style_bg_color(s_menu_panels[d],
            lv_color_hex(d == s_menu_sel ? C_SEL : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(s_menu_panels[d],
            lv_color_hex(d == s_menu_sel ? C_INK : 0xBFC8CC), 0);
        lv_obj_set_style_text_color(s_menu_labels[d],
            lv_color_hex(d == s_menu_sel ? C_INK : 0x33444C), 0);
    }
    if (s_reset_panel) {
        bool sel = (s_menu_sel == FOCUS_RESET);
        lv_obj_set_style_bg_color(s_reset_panel,
            lv_color_hex(sel ? C_SEL : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(s_reset_panel,
            lv_color_hex(sel ? C_BAD : 0xBFC8CC), 0);
        lv_obj_set_style_text_color(s_reset_label,
            lv_color_hex(sel ? C_BAD : 0x33444C), 0);
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "已学 %d/%d  熟练 %d  错词 %d",
             learned, VOCAB_COUNT, good, wrong);
    set_hdr("法律英语", "", battery_str());
    set_ftr(buf);
}

static void menu_build(void)
{
    scr_begin();
    set_hdr("法律英语", "", battery_str());
    s_view = VIEW_MENU;
    if (s_menu_sel < 0 || s_menu_sel >= MENU_FOCUS_COUNT) s_menu_sel = 0;

    for (int d = 0; d < MODE_COUNT; d++) {
        uint8_t m = MENU_ORDER[d];
        lv_obj_t *p;
        int lw;
        if (d == 0) {
            // 学习单词置顶独占一行
            p = mk_rect(s_scr, 10, CONT_Y + 12, SCR_W - 20, 40, 0xFFFFFF);
            lw = SCR_W - 40;
        } else if (d < MODE_COUNT - 1) {
            int j = d - 1, col = j % 2, row = j / 2;
            p = mk_rect(s_scr, 10 + col * 114, CONT_Y + 60 + row * 50,
                        106, 42, 0xFFFFFF);
            lw = 96;
        } else {
            // 学习统计坐底行左半，与重置按钮并排
            p = mk_rect(s_scr, 10, CONT_Y + 160, 106, 36, 0xFFFFFF);
            lw = 96;
        }
        lv_obj_set_style_radius(p, 6, 0);
        lv_obj_set_style_border_width(p, 3, 0);
        lv_obj_set_style_border_color(p, lv_color_hex(0xBFC8CC), 0);
        s_menu_panels[d] = p;

        lv_obj_t *lb = mk_label(p, &vocab_cjk_16, 0x33444C, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(lb, MODE_NAME[m]);
        lv_obj_set_width(lb, lw);
        lv_obj_center(lb);
        s_menu_labels[d] = lb;
    }

    // 底部右半按钮：重置进度（选中时红字提醒危险操作）
    s_reset_panel = mk_rect(s_scr, 10 + 114, CONT_Y + 160, SCR_W - 20 - 114, 36, 0xFFFFFF);
    lv_obj_set_style_radius(s_reset_panel, 6, 0);
    lv_obj_set_style_border_width(s_reset_panel, 3, 0);
    lv_obj_set_style_border_color(s_reset_panel, lv_color_hex(0xBFC8CC), 0);
    s_reset_label = mk_label(s_reset_panel, &vocab_cjk_16, 0x33444C, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text(s_reset_label, "重置进度");
    lv_obj_set_width(s_reset_label, SCR_W - 40);
    lv_obj_center(s_reset_label);

    menu_refresh();

    // MODE_NAME 与 MODE_COUNT 的一致性由上方 _Static_assert 保证；
    // 这里不读取对象坐标，因为 LVGL 在 lv_screen_load() 前尚未完成布局计算。

    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------- 重置进度二次确认
static void confirm_refresh(void)
{
    static const char *opts[2] = { "确认删除", "返回菜单" };
    for (int i = 0; i < 2; i++) {
        bool sel = (i == s_confirm_sel);
        lv_obj_set_style_bg_color(s_confirm_panels[i],
            lv_color_hex(sel ? C_SEL : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(s_confirm_panels[i],
            lv_color_hex(sel ? (i == 0 ? C_BAD : C_INK) : 0xBFC8CC), 0);
        lv_obj_set_style_text_color(s_confirm_labels[i],
            lv_color_hex(sel ? (i == 0 ? C_BAD : C_INK) : 0x33444C), 0);
        (void)opts;
    }
}

static void confirm_build(void)
{
    scr_begin();
    s_view = VIEW_CONFIRM;
    s_confirm_sel = 1;   // 默认停在“返回菜单”，防止误触清空
    set_hdr("确认删除", "", battery_str());

    lv_obj_t *tip = mk_label(s_scr, &vocab_cjk_16, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(tip, SCR_W - 40);
    lv_obj_set_pos(tip, 20, CONT_Y + 16);
    lv_label_set_text(tip, "确定清空全部进度？");

    lv_obj_t *tip2 = mk_label(s_scr, &vocab_cjk_16, C_MUTED, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(tip2, SCR_W - 40);
    lv_obj_set_pos(tip2, 20, CONT_Y + 44);
    lv_label_set_text(tip2, "删除后不能找回");

    static const char *opts[2] = { "确认删除", "返回菜单" };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *p = mk_rect(s_scr, 30, CONT_Y + 84 + i * 52, SCR_W - 60, 42, 0xFFFFFF);
        lv_obj_set_style_radius(p, 6, 0);
        lv_obj_set_style_border_width(p, 3, 0);
        lv_obj_set_style_border_color(p, lv_color_hex(0xBFC8CC), 0);
        s_confirm_panels[i] = p;
        lv_obj_t *lb = mk_label(p, &vocab_cjk_16, 0x33444C, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(lb, opts[i]);
        lv_obj_set_width(lb, SCR_W - 80);
        lv_obj_center(lb);
        s_confirm_labels[i] = lb;
    }
    confirm_refresh();
    set_ftr("↑↓ 换 OK 确认 长按OK返回");
    lv_screen_load(s_scr);
}

static void do_reset_progress(void)
{
    vocab_prog_reset(&s_prog);
    // 清掉当前会话再存，否则 progress_save 会把旧会话位置写回 NVS。
    s_sess.n = 0;
    s_sess.pos = 0;
    s_saved_index = -1;
    s_batch_mixed = 0;
    s_batch_weak = 0;
    s_study_sec = 0;
    s_study_start = 0;
    progress_save();
    quiz_clear_state();
    ESP_LOGI(TAG, "进度已重置");
}

// ---------------------------------------------------------------- 批内续背
// 写入已并入 progress_save；这里只保留清除与重进接续。
// 本批组成可能因掌握度变化而漂移，重进时只认仍在批内的已过关 id。
static void quiz_clear_state(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, NVS_KQUIZ);
    nvs_commit(h);
    nvs_close(h);
}

// 重进同批次时跳过已过关词；若已全过返回 true，由调用方直接推进下一批。
static bool quiz_resume_batch(void)
{
    quiz_stat_t q;
    memset(&q, 0, sizeof(q));
    size_t len = sizeof(q);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t e = nvs_get_blob(h, NVS_KQUIZ, &q, &len);
    nvs_close(h);
    if (e != ESP_OK || len != sizeof(q) || q.n == 0) return false;
    int32_t cur = (s_mode == MODE_MIXED) ? s_batch_mixed : s_batch_weak;
    if (q.mode != (uint8_t)s_mode || q.batch != cur) return false;
    for (int d = 0; d < q.n; d++) {
        for (int k = 0; k < s_q_n; k++) {
            if (s_q_ids[k] == q.ids[d] && !s_q_ok[k]) { s_q_ok[k] = true; s_q_done++; }
        }
    }
    if (s_q_done >= s_q_n) return true;
    int w = 0;
    for (int k = 0; k < s_q_n; k++) {
        if (!s_q_ok[k]) s_q_queue[w++] = s_q_ids[k];
    }
    s_q_qlen = w;
    s_q_qpos = 0;
    return true;
}

// ---------------------------------------------------------------- 教材例句
// 英文例句常态显示；中文例句是“答案”的一部分，仅查看答案时显示
// （卡片翻面后、选择题判分后）。无例句的词条自动隐藏。
// 长文本（释义、例句）首停 3 秒后单行来回弹滚动，短文本静止不动，无需按键翻页。
static void ex_marquee(lv_obj_t *l)
{
    // 先设时长再开滚动：滚动动画创建瞬间按当时样式定时长，事后改追认不上。
    // 用来回弹（SCROLL）而不用循环（CIRCULAR）：循环到头跳回起点会有“拉回”
    // 一截的断点，文本只超出一截时最明显；来回弹全程连续无断点。
    // 时长定死每程 20000ms（不用速度 API：LVGL 单圈 10 秒钳制会把长句压成同速）。
    lv_obj_set_style_anim_duration(l, 20000, 0);
}

// 跑马灯首停 3 秒：建屏时标签静止展示开头，一次性定时器到点才开滚动。
// 切屏时旧定时器在 scr_begin 里丢弃；回调内校验控件有效。
// s_mq_timer 声明在前，此处是回调与布防实现。
// 首停期保持单行截断（DOT）而非换行：长句静止时不会把版面撑高。
#define MARQUEE_STILL_MODE LV_LABEL_LONG_DOT

static void marquee_start_cb(lv_timer_t *t)
{
    (void)t;
    s_mq_timer = NULL;   // repeat_count=1，跑完 LVGL 自删
    if (s_lbl_def && lv_obj_is_valid(s_lbl_def))
        lv_label_set_long_mode(s_lbl_def, LV_LABEL_LONG_SCROLL);
    if (s_lbl_ex_en && lv_obj_is_valid(s_lbl_ex_en))
        lv_label_set_long_mode(s_lbl_ex_en, LV_LABEL_LONG_SCROLL);
    if (s_lbl_ex_zh && lv_obj_is_valid(s_lbl_ex_zh))
        lv_label_set_long_mode(s_lbl_ex_zh, LV_LABEL_LONG_SCROLL);
}

// 每次换词/换题都要重新布防：长模式一旦开着，set_text 会立刻续滚，
// 只布防一次会让首停仅在第一题出现。
static void marquee_arm(void)
{
    if (s_mq_timer) { lv_timer_delete(s_mq_timer); s_mq_timer = NULL; }
    if (s_lbl_def)   lv_label_set_long_mode(s_lbl_def,   MARQUEE_STILL_MODE);
    if (s_lbl_ex_en) lv_label_set_long_mode(s_lbl_ex_en, MARQUEE_STILL_MODE);
    if (s_lbl_ex_zh) lv_label_set_long_mode(s_lbl_ex_zh, MARQUEE_STILL_MODE);
    s_mq_timer = lv_timer_create(marquee_start_cb, 3000, NULL);
    lv_timer_set_repeat_count(s_mq_timer, 1);
}

static void ex_render(int id, bool show_en, bool show_zh)
{
    const char *ee = (id >= 0) ? vocab_ex_en(id) : "";
    const char *ez = (id >= 0) ? vocab_ex_zh(id) : "";
    if (!show_en || !ee[0]) {
        lv_obj_add_flag(s_lbl_ex_en, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_lbl_ex_en, ee);
        lv_obj_clear_flag(s_lbl_ex_en, LV_OBJ_FLAG_HIDDEN);
    }
    if (!show_zh || !ez[0]) {
        lv_obj_add_flag(s_lbl_ex_zh, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_lbl_ex_zh, ez);
        lv_obj_clear_flag(s_lbl_ex_zh, LV_OBJ_FLAG_HIDDEN);
    }
}

// 出题自动发音：新词条第一次出现播一次，同词翻面/判分/切阶段不重播。
static int s_audio_last_id = -1;
static void audio_autoplay(int id)
{
    if (id < 0 || id == s_audio_last_id) return;
    s_audio_last_id = id;
    vocab_audio_play(id);
}

// ---------------------------------------------------------------- 卡片视图
// 卡片只剩英→中 / 中→英两种，方向直接由模式决定。
static bool card_dir_en_first(int id)
{
    (void)id;
    return s_mode == MODE_EN2ZH;
}

static void card_render(void);

static void card_build(void)
{
    scr_begin();
    s_view = VIEW_CARD;
    s_flipped = false;
    study_accrue();   // 自动续批场景先把上一批结算（新进入时为空操作）
    study_begin();

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
    ex_marquee(s_lbl_def);

    s_lbl_ex_en = mk_label(cont, &lv_font_montserrat_14, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_lbl_ex_en, SCR_W - 24);
    ex_marquee(s_lbl_ex_en);

    s_lbl_ex_zh = mk_label(cont, &vocab_cjk_16, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_lbl_ex_zh, SCR_W - 24);
    ex_marquee(s_lbl_ex_zh);

    marquee_arm();   // 3 秒首停后开滚动
    card_render();
    lv_screen_load(s_scr);
}

static void card_render(void)
{
    marquee_arm();   // 换词即重置 3 秒首停
    int id = vocab_session_current(&s_sess);
    if (id < 0) {
        set_hdr(MODE_NAME[s_mode], "", "");
        lv_label_set_text(s_lbl_prompt, "该筛选下没有词条");
        lv_label_set_text(s_lbl_sub, "");
        lv_label_set_text(s_lbl_ans, "");
        lv_label_set_text(s_lbl_ans2, "");
        lv_label_set_text(s_lbl_def, "");
        ex_render(-1, false, false);
        set_ftr("长按 OK 返回菜单");
        return;
    }
    audio_autoplay(id);   // 新词露面读一遍，翻面不重播
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
        // 未翻面：英文例句常态显示，中文例句等看到答案再说。
        ex_render(id, en_first, false);
        set_ftr("OK看答案 ↑↓换词 长按跳章");
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
        // 翻面即看到答案：中→英此时才显示例句，英→中补上中文例句。
        ex_render(id, true, true);
        set_ftr("↑不认识 ↓认识 OK发音");
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

// ---------------------------------------------------------------- 统计视图
static void stats_build(void)
{
    scr_begin();
    s_view = VIEW_STATS;
    set_hdr("学习统计", "", battery_str());

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

    char tb[64];
    if (s_study_sec >= 3600)
        snprintf(tb, sizeof(tb), "学习时长：%u小时%u分",
                 (unsigned)(s_study_sec / 3600), (unsigned)((s_study_sec % 3600) / 60));
    else
        snprintf(tb, sizeof(tb), "学习时长：%u分", (unsigned)(s_study_sec / 60));
    lv_obj_t *tl = mk_label(cont, &vocab_cjk_16, C_INK, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(tl, SCR_W - 40);
    lv_label_set_text(tl, tb);

    lv_obj_t *note = mk_label(cont, &vocab_hint_14, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(note, SCR_W - 40);
    lv_label_set_text(note, "进度保存在设备 NVS，断电不丢。");

    set_ftr("长按 OK 返回菜单");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------- 选择题（混合练习 / 错词本）
// 规则：每批 20 词（错词不足 20 也成一组），英文出题、4 个中文备选。
//   混合练习按“未学优先、词库顺序”分批，做完自动进下一批，批号存 NVS。
//   答对掌握度 +1 过关；答错记错并在 3 词后重现，直到答对才算过本批。
static uint32_t q_rnd(void)
{
    uint32_t x = s_q_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s_q_rng = x ? x : 0x9E3779B9u;
    return s_q_rng;
}

// 取中文第一义项作选项文字（义项间以全角 ；分隔）
static void q_short_zh(int id, char *buf, size_t n)
{
    const char *s = VOCAB[id].zh;
    size_t i = 0;
    while (i + 1 < n && s[i]) {
        if ((unsigned char)s[i] == 0xEF && (unsigned char)s[i + 1] == 0x80 &&
            (unsigned char)s[i + 2] == 0xBB) break;
        buf[i] = s[i];
        i++;
    }
    buf[i] = 0;
}

// 混合：锁死章节批次 + 批内打乱。入口从存档批开始找首个还有生词的批
// （绕一圈）；全学会则回存档批复习。s_q_batch_cur 记录本次运行批号。
static int q_primary_ch(int id)
{
    uint16_t m = VOCAB[id].chapters;
    for (int c = 1; c <= 14; c++) {
        if (m & (1u << (c - 1))) return c;
    }
    return 15;
}

static void q_lock_build(void)
{
    if (s_lock_built) return;
    // s_idx_storage 暂借为排序键 scratch（本函数返回前即用完；
    // 调用时若有卡片会话，其索引随后由选择题接管并清零，重进卡片按 id 恢复）。
    for (int i = 0; i < VOCAB_COUNT; i++) {
        s_lock[i] = i;
        s_idx_storage[i] = q_primary_ch(i) * 2048 + i;
    }
    for (int i = 1; i < VOCAB_COUNT; i++) {
        int t = s_lock[i], k = s_idx_storage[i], j = i - 1;
        while (j >= 0 && s_idx_storage[j] > k) {
            s_lock[j + 1] = s_lock[j];
            s_idx_storage[j + 1] = s_idx_storage[j];
            j--;
        }
        s_lock[j + 1] = t;
        s_idx_storage[j + 1] = k;
    }
    s_lock_built = 1;
}

static int q_lock_batches(void)
{
    return (VOCAB_COUNT + QUIZ_BATCH - 1) / QUIZ_BATCH;
}

static int q_build_mixed(void)
{
    q_lock_build();
    int nb = q_lock_batches();
    int b = (int)(s_batch_mixed % nb), start = b;
    do {
        bool has_new = false;
        for (int k = 0; k < QUIZ_BATCH && b * QUIZ_BATCH + k < VOCAB_COUNT; k++) {
            if (vocab_prog_level(&s_prog, s_lock[b * QUIZ_BATCH + k]) == VOCAB_LEVEL_NEW) {
                has_new = true;
                break;
            }
        }
        if (has_new) break;
        b = (b + 1) % nb;
    } while (b != start);
    s_q_batch_cur = b;
    s_q_n = 0;
    for (int k = 0; k < QUIZ_BATCH && b * QUIZ_BATCH + k < VOCAB_COUNT; k++)
        s_q_ids[s_q_n++] = s_lock[b * QUIZ_BATCH + k];
    return s_q_n;
}

// 错词：答错过且未熟练的词，20 个一组，不满也成一组。
static int q_build_weak(void)
{
    int w = 0;
    for (int i = 0; i < VOCAB_COUNT; i++) {
        if (vocab_prog_wrong(&s_prog, i) == 0) continue;
        if (vocab_prog_level(&s_prog, i) >= VOCAB_LEVEL_GOOD) continue;
        s_idx_storage[w++] = i;
    }
    int groups = (w + QUIZ_BATCH - 1) / QUIZ_BATCH;
    if (groups <= 0) return 0;
    int g = (int)(s_batch_weak % groups);
    s_q_n = 0;
    for (int k = 0; k < QUIZ_BATCH && g * QUIZ_BATCH + k < w; k++)
        s_q_ids[s_q_n++] = s_idx_storage[g * QUIZ_BATCH + k];
    return s_q_n;
}

static void quiz_render(void);
static void quiz_screen_create(void);
static void show_message(const char *title, const char *msg);

// 为 id 组装 4 选项（正确项 + 全库随机干扰项，显示文字互不相同），并洗牌。
// 供选择题与复习共用。
static void q_build_options(int id)
{
    s_q_opt[0] = id;
    char seen[QUIZ_OPTS][96];
    q_short_zh(id, seen[0], sizeof(seen[0]));
    int n = 1, guard = 0;
    while (n < QUIZ_OPTS && guard++ < 400) {
        int c = (int)(q_rnd() % (uint32_t)VOCAB_COUNT);
        if (c == id) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) {
            if (s_q_opt[k] == c) { dup = true; break; }
        }
        char zb[96];
        q_short_zh(c, zb, sizeof(zb));
        for (int k = 0; k < n && !dup; k++) {
            if (strcmp(zb, seen[k]) == 0) dup = true;
        }
        if (dup) continue;
        s_q_opt[n] = c;
        snprintf(seen[n], sizeof(seen[n]), "%s", zb);
        n++;
    }
    while (n < QUIZ_OPTS) { s_q_opt[n] = id; n++; }   // 词库极小才走得到
    for (int i = QUIZ_OPTS - 1; i > 0; i--) {         // 选项洗牌
        int j = (int)(q_rnd() % (uint32_t)(i + 1));
        int t = s_q_opt[i]; s_q_opt[i] = s_q_opt[j]; s_q_opt[j] = t;
    }
    for (int i = 0; i < QUIZ_OPTS; i++) {
        if (s_q_opt[i] == id) s_q_answer = i;
    }
}

static void quiz_next_q(void)
{
    marquee_arm();   // 换题即重置 3 秒首停
    q_build_options(s_q_queue[s_q_qpos]);
    audio_autoplay(s_q_queue[s_q_qpos]);   // 新题露面读一遍
    s_q_sel = 0;
    s_q_judged = 0;
    quiz_render();
}

// title/done/n/id 参数化，供复习复用同一版面（复习传自己的计数与词条）。
static void quiz_render_custom(const char *title, int done, int n, int id)
{
    char hc[32];
    snprintf(hc, sizeof(hc), "%d/%d", done, n);
    set_hdr(title, hc, battery_str());

    lv_obj_set_style_text_font(s_q_prompt, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_q_prompt, lv_color_hex(C_INK), 0);
    lv_label_set_text(s_q_prompt, VOCAB[id].en);

    for (int i = 0; i < QUIZ_OPTS; i++) {
        char zb[96], txt[112];
        q_short_zh(s_q_opt[i], zb, sizeof(zb));
        uint32_t col;
        if (s_q_judged == 0) {
            col = (i == s_q_sel) ? C_INK : 0x33444C;
            snprintf(txt, sizeof(txt), "%s %c. %s",
                     (i == s_q_sel) ? "→" : "·", (char)('A' + i), zb);
        } else if (i == s_q_answer) {
            col = C_OK;
            snprintf(txt, sizeof(txt), "对 %c. %s", (char)('A' + i), zb);
        } else if (i == s_q_sel) {
            col = C_BAD;
            snprintf(txt, sizeof(txt), "错 %c. %s", (char)('A' + i), zb);
        } else {
            col = 0x33444C;
            snprintf(txt, sizeof(txt), "· %c. %s", (char)('A' + i), zb);
        }
        lv_obj_set_style_text_color(s_q_opts[i], lv_color_hex(col), 0);
        lv_label_set_text(s_q_opts[i], txt);
    }

    // 动作行：不认识（记错+亮答案+重练）与播放音频（只发音不判分）同行，
    // 光标左右各一（s_q_sel==4/5），判分后变灰。
    {
        char txt[112];
        uint32_t col;
        if (s_q_judged == 0 && (s_q_sel == QUIZ_OPTS || s_q_sel == QUIZ_OPTS + 1)) {
            col = C_INK;
            snprintf(txt, sizeof(txt), "%s 不认识   %s 播放音频",
                     (s_q_sel == QUIZ_OPTS) ? "→" : "·",
                     (s_q_sel == QUIZ_OPTS + 1) ? "→" : "·");
        } else {
            col = 0x33444C;
            snprintf(txt, sizeof(txt), "· 不认识   · 播放音频");
        }
        lv_obj_set_style_text_color(s_q_opts[QUIZ_OPTS], lv_color_hex(col), 0);
        lv_label_set_text(s_q_opts[QUIZ_OPTS], txt);
    }

    if (s_q_judged == 0)      set_ftr("↑↓ 选 OK 确认");
    else if (s_q_judged > 0)  set_ftr("回答正确 OK 下一词");
    else                      set_ftr("答错了 OK 下一词");
    // 例句：英文常态显示，中文仅判分后显示（判分即看到答案）。
    ex_render(id, true, s_q_judged != 0);
}

static void quiz_render(void)
{
    quiz_render_custom(MODE_NAME[s_mode], s_q_done, s_q_n, s_q_queue[s_q_qpos]);
}

static void quiz_build(void);

static void quiz_batch_done(void)
{
    if (s_mode == MODE_MIXED) s_batch_mixed = s_q_batch_cur + 1;
    else                      s_batch_weak++;
    progress_save();
    quiz_clear_state();
    quiz_build();   // 自动进下一批 / 下一组；无词可练则显示空提示
}

static void quiz_advance(void)
{
    s_q_qpos++;
    if (s_q_done >= s_q_n || s_q_qpos >= s_q_qlen) { quiz_batch_done(); return; }
    quiz_next_q();
}

static void quiz_mark_wrong(int id)
{
    vocab_prog_answer(&s_prog, id, false);
    int at = s_q_qpos + QUIZ_DELAY;   // 答错/不认识都隔 3 词后重练
    if (at > s_q_qlen) at = s_q_qlen;
    if (s_q_qlen < QUIZ_QMAX) {
        memmove(&s_q_queue[at + 1], &s_q_queue[at],
                (size_t)(s_q_qlen - at) * sizeof(int));
        s_q_queue[at] = id;
        s_q_qlen++;
    }
    s_q_judged = -1;
}

static void quiz_answer_cur(void)
{
    if (s_q_judged) { quiz_advance(); return; }
    int id = s_q_queue[s_q_qpos];
    if (s_q_sel == QUIZ_OPTS) {
        // 不认识：记错并亮出正确答案，稍后重练（与答错同规则）。
        quiz_mark_wrong(id);
    } else if (s_q_sel == QUIZ_OPTS + 1) {
        // 播放音频：只发音，不判分不翻页。
        if (id >= 0) vocab_audio_play(id);
        return;
    } else if (s_q_opt[s_q_sel] == id) {
        vocab_prog_answer(&s_prog, id, true);
        due_schedule(id, false);   // 学习/错词答对 → 30 学习分钟后进复习（1/4 概率）
        for (int k = 0; k < s_q_n; k++) {
            if (s_q_ids[k] == id && !s_q_ok[k]) { s_q_ok[k] = true; s_q_done++; }
        }
        s_q_judged = 1;
    } else {
        quiz_mark_wrong(id);
    }
    progress_save();   // 进度 + 批内续背同一次 NVS 事务落盘
    quiz_render();
}

static void quiz_build(void)
{
    int n = (s_mode == MODE_MIXED) ? q_build_mixed() : q_build_weak();
    if (n <= 0) {
        if (s_mode == MODE_WEAK) show_message(MODE_NAME[s_mode], "错词本是空的——先去做几组练习吧。");
        else                     show_message(MODE_NAME[s_mode], "当前筛选下没有可练习的词条。");
        return;
    }
    memset(s_q_ok, 0, sizeof(s_q_ok));
    s_q_done = 0;
    for (int k = 0; k < s_q_n; k++) s_q_queue[k] = s_q_ids[k];
    s_q_qlen = s_q_n;
    s_q_qpos = 0;
    // 批内续背：同批次重进时跳过已过关部分；若已全过直接进下一批。
    if (quiz_resume_batch() && s_q_done >= s_q_n) { quiz_batch_done(); return; }
    s_sess.n = 0;   // 选择题不用卡片会话，清掉避免 progress_save 写回旧位置
    s_sess.pos = 0;
    s_q_rng = (uint32_t)lv_tick_get() | 1u;
    for (int i = s_q_qlen - 1; i > 0; i--) {   // 批内打乱出题顺序
        int j = (int)(q_rnd() % (uint32_t)(i + 1));
        int t = s_q_queue[i]; s_q_queue[i] = s_q_queue[j]; s_q_queue[j] = t;
    }
    quiz_screen_create();
    s_view = VIEW_QUIZ;
    set_hdr(MODE_NAME[s_mode], "", "");
    study_accrue();   // 自动进下一批先把上一批结算（新进入时为空操作）
    study_begin();
    quiz_next_q();
    lv_screen_load(s_scr);
}

// 选择题版面控件（提示 + A-D/动作行 + 例句中英），供选择题与复习共用。
// 调用方负责置 s_view、走各自的下一题并 lv_screen_load。
static void quiz_screen_create(void)
{
    scr_begin();

    lv_obj_t *cont = mk_container(s_scr, 4, CONT_Y + 2, SCR_W - 8, CONT_H - 4);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 4, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_q_prompt = mk_label(cont, &lv_font_montserrat_20, C_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_q_prompt, SCR_W - 24);

    for (int i = 0; i < QUIZ_OPTS + 1; i++) {
        s_q_opts[i] = mk_label(cont, &vocab_cjk_16, 0x33444C, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_width(s_q_opts[i], SCR_W - 24);
    }

    s_lbl_ex_en = mk_label(cont, &lv_font_montserrat_14, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_lbl_ex_en, SCR_W - 24);
    ex_marquee(s_lbl_ex_en);

    s_lbl_ex_zh = mk_label(cont, &vocab_cjk_16, C_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(s_lbl_ex_zh, SCR_W - 24);
    ex_marquee(s_lbl_ex_zh);

    marquee_arm();   // 3 秒首停后开滚动
}

static void quiz_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // 选择题无双击/长按语义（OK 长按由全局接管回菜单），抬起即响应；
    // 180ms 后到达的 CLICK 是同一手势的回声，直接忽略。
    if (ev != BSP_BTN_RELEASE) return;
    if (s_q_judged) { quiz_advance(); return; }   // 判分后任意抬起进下一词
    if (btn == BSP_BTN_UP)        { s_q_sel = (s_q_sel + QUIZ_ROWS - 1) % QUIZ_ROWS; quiz_render(); }
    else if (btn == BSP_BTN_DOWN) { s_q_sel = (s_q_sel + 1) % QUIZ_ROWS;             quiz_render(); }
    else if (btn == BSP_BTN_OK)   { quiz_answer_cur(); }
}

// ---------------------------------------------------------------- 复习
// 入口：学习答对、错词答对的词，30 学习分钟后到期，每次取到期最早的 20 个。
// 过关流：英文认不认识 → 认识亮中文 → OK 进四选一；答对到期顺延，答错扔回错词本。
// 不认识不亮答案，直接往后放一轮四选一。批内退出重进靠到期表自然续接。
#define RV_ASK 0
#define RV_QUIZ 1
static int     s_rv_ids[QUIZ_BATCH];
static int     s_rv_n = 0;
static int     s_rv_qid[QUIZ_QMAX * 2];
static uint8_t s_rv_qph[QUIZ_QMAX * 2];
static int     s_rv_qlen = 0, s_rv_qpos = 0;
static int     s_rv_done = 0;
static bool    s_rv_ok[QUIZ_BATCH];
static bool    s_rv_revealed = false;

static void review_next(void);

static void review_build(void)
{
    uint16_t now_m = (uint16_t)(s_study_sec / 60);
    int w = 0;
    for (int i = 0; i < VOCAB_COUNT; i++) {
        if (s_due[i] != DUE_NEVER && s_due[i] <= now_m) s_idx_storage[w++] = i;
    }
    for (int i = 1; i < w; i++) {   // 按到期从早到晚
        int t = s_idx_storage[i], d = s_due[t], j = i - 1;
        while (j >= 0 && s_due[s_idx_storage[j]] > d) {
            s_idx_storage[j + 1] = s_idx_storage[j];
            j--;
        }
        s_idx_storage[j + 1] = t;
    }
    s_rv_n = 0;
    for (int k = 0; k < QUIZ_BATCH && k < w; k++) s_rv_ids[s_rv_n++] = s_idx_storage[k];
    if (s_rv_n <= 0) {
        uint16_t best = DUE_NEVER;
        for (int i = 0; i < VOCAB_COUNT; i++) {
            if (s_due[i] != DUE_NEVER && s_due[i] > now_m && s_due[i] < best)
                best = s_due[i];
        }
        if (best == DUE_NEVER) {
            show_message("复习", "复习是空的——先去学习吧。");
        } else {
            char m[96];
            unsigned wait = (unsigned)(best - now_m);
            if (wait >= 60)
                snprintf(m, sizeof(m), "复习是空的，最早%u小时后到期。", wait / 60);
            else
                snprintf(m, sizeof(m), "复习是空的，最早%u分钟后到期。", wait);
            show_message("复习", m);
        }
        return;
    }
    memset(s_rv_ok, 0, sizeof(s_rv_ok));
    s_rv_done = 0;
    for (int k = 0; k < s_rv_n; k++) { s_rv_qid[k] = s_rv_ids[k]; s_rv_qph[k] = RV_ASK; }
    s_rv_qlen = s_rv_n;
    s_rv_qpos = 0;
    s_sess.n = 0;
    s_sess.pos = 0;
    s_q_rng = (uint32_t)lv_tick_get() | 1u;
    quiz_screen_create();
    s_view = VIEW_REVIEW;
    set_hdr("复习", "", "");
    study_accrue();   // 自动进下一组先把上一组结算（新进入时为空操作）
    study_begin();
    review_next();
    lv_screen_load(s_scr);
}

static void review_render_ask(int id)
{
    char hc[32];
    snprintf(hc, sizeof(hc), "%d/%d", s_rv_done, s_rv_n);
    set_hdr("复习", hc, battery_str());
    lv_obj_set_style_text_font(s_q_prompt, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_q_prompt, lv_color_hex(C_INK), 0);
    lv_label_set_text(s_q_prompt, VOCAB[id].en);
    if (!s_rv_revealed) {
        static const char *rows[2] = { "认识", "不认识" };
        for (int i = 0; i < 2; i++) {
            char t[32];
            snprintf(t, sizeof(t), "%s %s", (i == s_q_sel) ? "→" : "·", rows[i]);
            lv_obj_set_style_text_color(s_q_opts[i],
                lv_color_hex((i == s_q_sel) ? C_INK : 0x33444C), 0);
            lv_label_set_text(s_q_opts[i], t);
            lv_obj_clear_flag(s_q_opts[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int i = 2; i < QUIZ_OPTS + 1; i++)
            lv_obj_add_flag(s_q_opts[i], LV_OBJ_FLAG_HIDDEN);
        // 只给英文例句（中文含答案先藏起）
        ex_render(id, true, false);
        set_ftr("↑↓ 选 OK 确认");
    } else {
        // 亮中文，OK 进四选一
        lv_obj_set_style_text_color(s_q_opts[0], lv_color_hex(C_ANS), 0);
        lv_label_set_text(s_q_opts[0], VOCAB[id].zh);
        lv_obj_clear_flag(s_q_opts[0], LV_OBJ_FLAG_HIDDEN);
        for (int i = 1; i < QUIZ_OPTS + 1; i++)
            lv_obj_add_flag(s_q_opts[i], LV_OBJ_FLAG_HIDDEN);
        ex_render(id, true, true);
        set_ftr("OK 确认");
    }
}

static void review_render_quiz(void)
{
    quiz_render_custom("复习", s_rv_done, s_rv_n, s_rv_qid[s_rv_qpos]);
}

static void review_next(void)
{
    marquee_arm();   // 换词即重置 3 秒首停
    int id = s_rv_qid[s_rv_qpos];
    audio_autoplay(id);   // 新词露面读一遍（同词切阶段不重播）
    if (s_rv_qph[s_rv_qpos] == RV_ASK) {
        s_rv_revealed = false;
        s_q_sel = 0;
        s_q_judged = 0;
        review_render_ask(id);
    } else {
        q_build_options(id);
        s_q_sel = 0;
        s_q_judged = 0;
        review_render_quiz();
    }
}

static void review_batch_done(void)
{
    progress_save();
    review_build();   // 自动下一组；无到期则空提示
}

static void review_advance(void)
{
    s_rv_qpos++;
    if (s_rv_done >= s_rv_n || s_rv_qpos >= s_rv_qlen) { review_batch_done(); return; }
    review_next();
}

static void review_mark_done(int id)
{
    for (int k = 0; k < s_rv_n; k++) {
        if (s_rv_ids[k] == id && !s_rv_ok[k]) { s_rv_ok[k] = true; s_rv_done++; }
    }
}

static void review_quiz_answer(void)
{
    int id = s_rv_qid[s_rv_qpos];
    if (s_q_sel == QUIZ_OPTS + 1) {
        if (id >= 0) vocab_audio_play(id);   // 播放：不判分
        return;
    }
    if (s_q_sel == QUIZ_OPTS || s_q_opt[s_q_sel] != id) {
        // 答错/不认识：扔回错词本（错次+1 自然落入错词池），本轮结束。
        vocab_prog_answer(&s_prog, id, false);
        s_due[id] = DUE_NEVER;
        due_save();
        review_mark_done(id);
        s_q_judged = -1;
    } else {
        vocab_prog_answer(&s_prog, id, true);
        due_schedule(id, true);   // 答对：抽中则 30 分钟后再见，未抽中毕业
        review_mark_done(id);
        s_q_judged = 1;
    }
    progress_save();
    review_render_quiz();
}

static void review_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_RELEASE) return;
    int id = s_rv_qid[s_rv_qpos];
    if (s_rv_qph[s_rv_qpos] == RV_ASK && !s_rv_revealed) {
        if (btn == BSP_BTN_UP)        { s_q_sel = 0; review_render_ask(id); }
        else if (btn == BSP_BTN_DOWN) { s_q_sel = 1; review_render_ask(id); }
        else if (btn == BSP_BTN_OK) {
            if (s_q_sel == 0) {
                s_rv_revealed = true;   // 认识：亮中文，OK 进四选一
                marquee_arm();          // 新内容（中文例句）重新计首停
                review_render_ask(id);
            } else {
                // 不认识：不亮答案，直接往后放一轮四选一
                int at = s_rv_qpos + QUIZ_DELAY;
                if (at > s_rv_qlen) at = s_rv_qlen;
                if (s_rv_qlen < QUIZ_QMAX * 2) {
                    memmove(&s_rv_qid[at + 1], &s_rv_qid[at],
                            (size_t)(s_rv_qlen - at) * sizeof(int));
                    memmove(&s_rv_qph[at + 1], &s_rv_qph[at],
                            (size_t)(s_rv_qlen - at));
                    s_rv_qid[at] = id;
                    s_rv_qph[at] = RV_QUIZ;
                    s_rv_qlen++;
                }
                review_advance();
            }
        }
        return;
    }
    if (s_rv_revealed) {
        // 亮答案后任意抬起进四选一
        s_rv_qph[s_rv_qpos] = RV_QUIZ;
        review_next();
        return;
    }
    // 四选一阶段（动作行含义与选择题一致）
    if (s_q_judged) { review_advance(); return; }
    if (btn == BSP_BTN_UP)        { s_q_sel = (s_q_sel + QUIZ_ROWS - 1) % QUIZ_ROWS; review_render_quiz(); }
    else if (btn == BSP_BTN_DOWN) { s_q_sel = (s_q_sel + 1) % QUIZ_ROWS;             review_render_quiz(); }
    else if (btn == BSP_BTN_OK)   { review_quiz_answer(); }
}

// ---------------------------------------------------------------- 会话启动
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
    if (m == MODE_MIXED || m == MODE_WEAK) { quiz_build(); return; }
    if (m == MODE_REVIEW) { review_build(); return; }

    vocab_session_build(&s_sess, s_idx_storage, &s_prog, s_chapter_mask,
                        VOCAB_ORDER_SEQ, (uint32_t)lv_tick_get() | 1u);

    if (s_sess.n <= 0) {
        show_message(MODE_NAME[m], "当前筛选下没有可练习的词条。");
        return;
    }

    // 顺序模式按词条 id 恢复上次位置。
    if (s_saved_index >= 0 &&
        vocab_session_set_current_id(&s_sess, s_saved_index)) {
        ESP_LOGI(TAG, "断点续背: 从第 %d 条继续", s_saved_index + 1);
    }

    card_build();
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

static void confirm_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // 确认页同样无双击语义，抬起即响应（OK 长按仍由全局接管回菜单）。
    if (ev != BSP_BTN_RELEASE) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_confirm_sel = 1 - s_confirm_sel;
        confirm_refresh();
    } else if (btn == BSP_BTN_OK) {
        if (s_confirm_sel == 0) {
            do_reset_progress();
            show_message("已删除", "进度已重置");
        } else {
            menu_build();
        }
    }
}

void vocab_app_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    s_last_key_tick = lv_tick_get();
    // 息屏中第一下按键只唤醒不动作，抬起后恢复正常（防误触）。
    // 例外：手动长按息屏的那一下抬起直接吞掉，否则“按住息屏、松手亮屏”。
    if (s_screen_off) {
        if (s_swallow_until_release) {
            s_swallow_until_release = false;
            if (ev == BSP_BTN_RELEASE) return;
        }
        screen_wake();
        s_swallow_until_release = true;
        return;
    }
    if (s_swallow_until_release) {
        if (ev == BSP_BTN_RELEASE) s_swallow_until_release = false;
        return;
    }

    // 全局：OK 长按 = 返回主菜单
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        vocab_audio_stop();
        study_accrue();
        progress_save();
        if (s_view != VIEW_MENU) menu_build();
        return;
    }

    switch (s_view) {
    case VIEW_MENU:
        // 主菜单长按↑↓ = 立即息屏（该手势在菜单无其他语义）。
        // 同时挂吞键：触发息屏的这一下抬起直接吞掉，否则松手即唤醒。
        if ((btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) && ev == BSP_BTN_LONG) {
            screen_sleep();
            s_swallow_until_release = true;
            break;
        }
        // ↑↓ 在菜单无双击/长按语义，抬起即移动光标；OK 进模式仍走 CLICK，
        // 避免长按 OK（回菜单）松手时误进模式。
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            if (ev == BSP_BTN_RELEASE) {
                s_menu_sel = (s_menu_sel + MENU_FOCUS_COUNT +
                              (btn == BSP_BTN_DOWN ? 1 : -1)) % MENU_FOCUS_COUNT;
                menu_refresh();
            }
        } else if (btn == BSP_BTN_OK) {
            if (ev == BSP_BTN_CLICK) {
                if (s_menu_sel == FOCUS_RESET) confirm_build();
                else                           start_mode((mode_t)MENU_ORDER[s_menu_sel]);
            }
        }
        break;
    case VIEW_CARD:    card_key(btn, ev);     break;
    case VIEW_CONFIRM: confirm_key(btn, ev);  break;
    case VIEW_QUIZ:    quiz_key(btn, ev);     break;
    case VIEW_REVIEW:  review_key(btn, ev);   break;
    case VIEW_STATS: break;
    default: break;
    }
}

void vocab_app_start(void)
{
    s_prog_bytes = malloc((size_t)VOCAB_COUNT);
    s_idx_storage = malloc(sizeof(int) * (size_t)VOCAB_COUNT);
    s_due = malloc(sizeof(uint16_t) * (size_t)VOCAB_COUNT);
    s_lock = malloc(sizeof(int) * (size_t)VOCAB_COUNT);
    if (!s_prog_bytes || !s_idx_storage || !s_due || !s_lock) {
        ESP_LOGE(TAG, "内存不足：进度/索引/到期/锁死表");
        return;
    }
    memset(s_prog_bytes, 0, (size_t)VOCAB_COUNT);
    vocab_prog_init(&s_prog, s_prog_bytes, VOCAB_COUNT);
    progress_load();
    due_load();
    if (s_saved_index >= 0) {
        ESP_LOGI(TAG, "恢复断点: 上次词条 id=%d", s_saved_index + 1);
    }

    if (vocab_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "音频分区不可用，发音功能将静默跳过");
    }

    s_menu_sel = 0;
    s_last_key_tick = lv_tick_get();
    menu_build();
    lv_timer_create(sleep_check_cb, 10000, NULL);   // 闲置息屏巡检（LVGL 任务内）
    ESP_LOGI(TAG, "词库就绪: %d 条, 模式 %d 个", VOCAB_COUNT, MODE_COUNT);
}
