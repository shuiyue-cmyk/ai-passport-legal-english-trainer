#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""由 vocab_master.json 生成 main/vocab_data.c 与各字体字符集。

产出:
  main/vocab_data.c            词库数据
  tools/font_charset.txt       16px 中文字体字符集（含箭头等 UI 符号）
  tools/font_charset_ipa.txt   音标字体字符集
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(HERE, "vocab_master.json")
V = json.load(open(SRC, encoding='utf-8'))


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def chapters_mask(chs):
    m = 0
    for c in chs:
        if 1 <= c <= 14:
            m |= (1 << (c - 1))
    return m


# 音标里用连字 ʤ/ʧ；Arial 等常见字体没有这两个字形，替换为等价写法，
# 学习者读起来也更清楚。
IPA_FIX = {'\u02a4': 'd\u0292', '\u02a7': 't\u0283'}


def fix_ipa(s):
    for k, v in IPA_FIX.items():
        s = s.replace(k, v)
    return s


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
for v in V:
    zh = "；".join(v["zh"])
    ipa = fix_ipa(v["ipa"] or "")
    di = def_idx.get(v.get("definition") or "", 0xFFFF)
    dfield = "0xFFFF" if di == 0xFFFF else str(di)
    buf.append("    { %s, %s, %s, %d, %s, %d },"
               % (cstr(v["en"]), cstr(zh), cstr(ipa),
                  chapters_mask(v["chapters"]), dfield, v["count"]))
buf.append("};")
buf.append("")
buf.append("const int VOCAB_COUNT = %d;" % len(V))
buf.append("const int VOCAB_DEF_COUNT = %d;" % len(defs))
buf.append("")
buf.append("const char *vocab_def(int i) {")
buf.append('    return (i >= 0 && i < VOCAB_DEF_COUNT) ? S_DEF[i] : "";')
buf.append("}")

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
)
SYMBOLS = "→←↑↓▲▼·～、。！？：（）《》✓"

chars = set()
for v in V:
    for s in ["；".join(v["zh"]), v.get("definition") or ""]:
        for ch in s:
            if ord(ch) > 127:
                chars.add(ch)
for ch in UI_TEXT + SYMBOLS:
    if ord(ch) > 127:
        chars.add(ch)
charset = "".join(sorted(chars))
open(os.path.join(HERE, "font_charset.txt"), "w", encoding='utf-8').write(charset)

# 音标字体：词库音标用到的全部字符（含 ASCII 字母）
ipa_chars = set()
for v in V:
    ipa_chars |= set(fix_ipa(v["ipa"] or ""))
ipa_charset = "".join(sorted(c for c in ipa_chars if ord(c) > 31))
open(os.path.join(HERE, "font_charset_ipa.txt"), "w", encoding='utf-8').write(ipa_charset)

# 界面专用字符集：只含页脚/表头/提示文案，供小字号字体使用（体积小）
HINT_TEXT = (
    "法律英语英中拼写混合练习错词本学习统计"
    "已学熟练词库总量条数量进度"
    "看答案换词长按跳章不认识发音全书"
    "选字母确认双击删除下一返回菜单首共个字母正确了"
    "该筛选下没有可拼的当前错是空先去做几组吧"
    "保存设备断电不丢"
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
print("音标字符集    : %d 个字符 -> %s" % (len(ipa_charset), ipa_charset))
