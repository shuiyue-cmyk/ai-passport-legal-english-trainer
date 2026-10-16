#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""由 vocab_master.json 生成 main/vocab_data.c 与各字体字符集。

产出:
  main/vocab_data.c            词库数据
  tools/font_charset.txt       16px 中文字体字符集（含箭头等 UI 符号）
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(HERE, "vocab_master.json")
V = json.load(open(SRC, encoding='utf-8'))


def cstr(s):
    s = s.replace('\r', ' ').replace('\n', ' ')
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def clean_md(s):
    """去教材标注残留：**加粗** 保留文字，压缩空白。
    另滤掉脚注标记（上标数字²³¹⁰⁴⁻⁹与纯数字方括号[30]，屏上无用且黑体无字形）。"""
    import re as _re
    s = _re.sub(r'\*\*(.*?)\*\*', r'\1', s or '')
    s = _re.sub(r'[¹²³⁰⁴⁵⁶⁷⁸⁹]', '', s)
    s = _re.sub(r'\[\d+\]', '', s)
    return _re.sub(r'\s+', ' ', s).strip()


# 例句（子代理从教材挖掘，可选文件；缺席的词条置空串）
EX_SRC = os.path.join(HERE, "vocab_sentences.json")
EX = json.load(open(EX_SRC, encoding='utf-8')) if os.path.isfile(EX_SRC) else {}


def chapters_mask(chs):
    m = 0
    for c in chs:
        if 1 <= c <= 14:
            m |= (1 << (c - 1))
    return m


def sub_line(v):
    """词条下方的小字：词性 + 元照查词提示（不再显示音标，学习者没学过）。
    开卷考试只能查《元照英美法词典》，所以告诉学习者去哪一页查哪个词头：
      词头与词条相同  -> "n.  元照 p.620"
      词头不同        -> "phr.  元照 p.1298 查 strict scrutiny test"
      词典未收        -> "adj.  元照无"（考试查不到，要背下来）"""
    head, page = v.get("yz_head"), v.get("yz_page")
    if head and page:
        yz = "元照 p.%d" % page
        if head.lower().rstrip(".") != v["en"].lower():
            yz += " 查 " + head
    else:
        yz = "元照无"
    return "  ".join(p for p in [v.get("pos") or "", yz] if p)


# 英文句子在屏上用 LVGL 内置 Montserrat（只覆盖 ASCII + ° •），教材原文里的
# 排版符号（弯引号、en/em 破折号、省略号、§）没有字形，上屏就是方框 ——
# 生成时折成 ASCII 等价写法。中文字体（黑体）有这些字形，中文句子不折。
ASCII_FOLD = {
    '\u2018': "'", '\u2019': "'",     # ‘ ’
    '\u201c': '"', '\u201d': '"',     # “ ”
    '\u2013': '-', '\u2014': '-',     # – —（原文的 –– 折成 --）
    '\u2026': '...',                  # …
    '\u00a7': 'Sec.',                 # §
}


def fold_ascii(s):
    for k, v in ASCII_FOLD.items():
        s = s.replace(k, v)
    return s


def require_ascii(s, where, i):
    """ASCII 字体渲染的字段不得夹带无字形字符，否则上屏是方框。"""
    bad = sorted({c for c in s if ord(c) > 127})
    if bad:
        raise SystemExit('%s[%d] 有 ASCII 字体无字形的字符: %s\n  %s'
                         % (where, i,
                            ' '.join('U+%04X(%s)' % (ord(c), c) for c in bad), s))


# ---------------------------------------------------------------- 词库数据
defs, def_idx = [], {}
for v in V:
    d = v.get("definition") or ""
    if d and d not in def_idx:
        def_idx[d] = len(defs)
        defs.append(d)

buf = []
buf.append("// main/vocab_data.c —— 自动生成，请勿手改。")
buf.append("// 生成脚本: tools/gen_vocab_data.py  数据源: tools/vocab_master.json")
buf.append('#include "vocab_data.h"')
buf.append("")
buf.append("static const char *const S_DEF[] = {")
for d in defs:
    buf.append("    %s," % cstr(d))
buf.append("};")
buf.append("")
buf.append("const vocab_entry_t VOCAB[] = {")
for i, v in enumerate(V):
    require_ascii(v["en"], "VOCAB.en", i)
    require_ascii(v.get("pos") or "", "VOCAB.pos", i)
    zh = "；".join(v["zh"])
    sub = sub_line(v)
    di = def_idx.get(v.get("definition") or "", 0xFFFF)
    dfield = "0xFFFF" if di == 0xFFFF else str(di)
    buf.append("    { %s, %s, %s, %d, %s, %d, %s },"
               % (cstr(v["en"]), cstr(zh), cstr(sub),
                  chapters_mask(v["chapters"]), dfield, v["count"], cstr(v.get("pos") or "")))
buf.append("};")
buf.append("")
buf.append("const int VOCAB_COUNT = %d;" % len(V))
buf.append("const int VOCAB_DEF_COUNT = %d;" % len(defs))
buf.append("")
buf.append("const char *vocab_def(int i) {")
buf.append('    return (i >= 0 && i < VOCAB_DEF_COUNT) ? S_DEF[i] : "";')
buf.append("}")
buf.append("")
buf.append("// 教材例句（与 VOCAB 同下标平行；无例句为空串）")
buf.append("static const char *const S_EX_EN[] = {")
for i, v in enumerate(V):
    e = EX.get(v["en"]) or {}
    s = fold_ascii(clean_md(e.get("s") or ""))
    require_ascii(s, "S_EX_EN", i)
    buf.append("    %s," % cstr(s))
buf.append("};")
buf.append("static const char *const S_EX_ZH[] = {")
for v in V:
    e = EX.get(v["en"]) or {}
    buf.append("    %s," % cstr(clean_md(e.get("zh") or "")))
buf.append("};")
buf.append("")
buf.append("const int VOCAB_EX_COUNT = %d;" % len(V))
buf.append("const char *vocab_ex_en(int i) {")
buf.append('    return (i >= 0 && i < VOCAB_EX_COUNT) ? S_EX_EN[i] : "";')
buf.append("}")
buf.append("const char *vocab_ex_zh(int i) {")
buf.append('    return (i >= 0 && i < VOCAB_EX_COUNT) ? S_EX_ZH[i] : "";')
buf.append("}")
nex = sum(1 for v in V if (EX.get(v["en"]) or {}).get("s"))
print("例句覆盖      : %d / %d" % (nex, len(V)))

out_c = os.path.join(ROOT, "main", "vocab_data.c")
open(out_c, "w", encoding='utf-8').write("\n".join(buf) + "\n")

# ---------------------------------------------------------------- 字体字符集
# 16px 中文字体：词库中文 + 释义 + 全部 UI 文案 + 需要的符号
# ⚠ 这里必须覆盖 vocab_app.c 里**所有会上屏的中文**。改界面文案后请重跑本脚本，
#   并执行 tools/check_fonts.py 自检（它会报出漏掉的字符）。
UI_TEXT = (
    # 标题与菜单项
    "法律英语英中拼写混合练习错词本学习统计"
    # 顶栏 / 底栏
    "已学熟练词库总量条数量进度保存设备断电不丢"
    # 卡片页提示
    "看答案换词长按跳章不认识发音全书"
    # 拼写页
    "选字母确认双击删除首共个字母正确了"
    # 空状态与提示
    "该筛选下没有可拼的当前错是空先去做几组吧"
    # 确认框/菜单文案用字（曾由词库中文间接覆盖，词库删减后须显式保留）
    "找菜"
    # 选择题动作行
    "不认识播放音频"
    # 选章页（章名 + 顶栏/底栏）
    "选择章节第开始完这一可练习的条"
    "导论历史与法院组织程序证据权力限制宪刑行政环境侵合同财产家庭结"
    # 复习空状态（“最早N分钟后到期”“复习是空的——”）
    "钟—"
)
# 注：✓ 不在黑体(simhei)字形内，界面已改用“对”字，此处不再收录。
SYMBOLS = "→←↑↓▲▼·～、。！？：（）《》"

chars = set()
for v in V:
    ex = EX.get(v["en"]) or {}
    for s in ["；".join(v["zh"]), v.get("definition") or "", sub_line(v),
              clean_md(ex.get("zh") or "")]:
        for ch in s:
            if ord(ch) > 127:
                chars.add(ch)
for ch in UI_TEXT + SYMBOLS:
    if ord(ch) > 127:
        chars.add(ch)
charset = "".join(sorted(chars))
open(os.path.join(HERE, "font_charset.txt"), "w", encoding='utf-8').write(charset)


# 界面专用字符集：只含页脚/表头/提示文案，供小字号字体使用（体积小）
HINT_TEXT = (
    "法律英语英中拼写混合练习错词本学习统计"
    "已学熟练词库总量条数量进度"
    "看答案换词长按跳章不认识发音全书"
    "选字母确认双击删除下一返回菜单首共个字母正确了"
    "该筛选下没有可拼的当前错是空先去做几组吧"
    "保存设备断电不丢，在"
    # 复习顶栏（小字体）
    "复"
    # 选章页顶栏/底栏、学习单词顶栏“第N章”
    "选择章节第开始完"
)
hint_chars = set()
for ch in HINT_TEXT + SYMBOLS:
    if ord(ch) > 127:
        hint_chars.add(ch)
hint_charset = "".join(sorted(hint_chars))
open(os.path.join(HERE, "font_charset_hint.txt"), "w", encoding='utf-8').write(hint_charset)

print("词条数        : %d" % len(V))
print("释义条数      : %d" % len(defs))
print("vocab_data.c  : %.1f KB" % (os.path.getsize(out_c) / 1024))
print("中文字符集    : %d 个字符" % len(charset))
print("界面字符集    : %d 个字符" % len(hint_charset))
