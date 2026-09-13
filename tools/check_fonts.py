#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""字体覆盖自检：确认界面上出现的每个字符都在对应字体的字符集里。

检查项:
  1) vocab_app.c 中所有**字符串字面量**里的非 ASCII 字符 -> 必须在中文字符集内
  2) 传给 set_ftr() 的底栏文案 -> 必须在界面字符集（hint_14）内
  3) VOCAB[] 中每条的音标 -> 必须在音标字符集内
  4) VOCAB[] 中每条的中文译名、以及 S_DEF[] 释义 -> 必须在中文字符集内

注意：必须先剥掉 C 注释，否则注释里的中文会被误当成界面文案。

用法: python tools/check_fonts.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def read(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


def strip_comments(src):
    """去掉 // 行注释与 /* */ 块注释，保留字符串字面量。"""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':                      # 字符串字面量，原样保留
            j = i + 1
            while j < n:
                if src[j] == '\\':
                    j += 2
                    continue
                if src[j] == '"':
                    j += 1
                    break
                j += 1
            out.append(src[i:j])
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def charset(p):
    return set(read(p).strip())


def built_symbols(font_c):
    """已编译字体文件的真实覆盖：取其头部 --symbols 注释 + ASCII 可见区。
    必须以它为准，而不是字符集 txt——字体二进制可能比 txt 旧。"""
    src = read(font_c)
    i = src.find('--symbols')
    j = src.find('--range', i)
    syms = re.sub(r'\s+', '', src[i + len('--symbols'):j])
    return set(syms) | {chr(c) for c in range(0x20, 0x7F)}


def block(src, header):
    """取出以 header 开头、到与之配对的 '};' 为止的代码块。"""
    i = src.find(header)
    if i < 0:
        return ''
    j = src.find('};', i)
    return src[i:j] if j > 0 else ''


errors = []

cjk_txt = charset('tools/font_charset.txt')
hint_txt = charset('tools/font_charset_hint.txt')
ipa_txt = charset('tools/font_charset_ipa.txt')

# 以已编译字体为 ground truth：txt 里有但字体二进制里没有的字，上屏是方框。
# 下方 1)/2) 检查直接用 built 集合。
cjk = built_symbols('main/fonts/vocab_cjk_16.c')
hint = built_symbols('main/fonts/vocab_hint_14.c')
ipa = built_symbols('main/fonts/vocab_ipa_16.c')
for name, txt, built in [('中文', cjk_txt, cjk), ('界面', hint_txt, hint),
                         ('音标', ipa_txt, ipa)]:
    lag = sorted(txt - built)
    if lag:
        errors.append('%s字符集 txt 比已编译字体多 %d 字（字体需重打）: %s'
                      % (name, len(lag), ' '.join(lag[:20])))

# ---------------------------------------------------------------- 1) & 2)
app = strip_comments(read('main/vocab_app.c'))

# 串口日志字符串不上屏，不需要字体覆盖 —— 先把 ESP_LOG* 调用整段去掉
app_ui = re.sub(r'ESP_LOG[A-Z]\([^;]*\);', '', app, flags=re.S)

lits = re.findall(r'"((?:[^"\\]|\\.)*)"', app_ui)

nonascii = set()
for s in lits:
    for ch in s:
        if ord(ch) > 127:
            nonascii.add(ch)
missing = sorted(c for c in nonascii if c not in cjk)
if missing:
    errors.append('vocab_app.c 有 %d 个非 ASCII 字符不在中文字符集内: %s'
                  % (len(missing), ' '.join('U+%04X(%s)' % (ord(c), c) for c in missing)))

hint_lits = re.findall(r'set_ftr\(\s*"((?:[^"\\]|\\.)*)"', app_ui)
for s in hint_lits:
    miss = sorted(c for c in s if ord(c) > 127 and c not in hint)
    if miss:
        errors.append('底栏文案 "%s" 有字符不在界面字符集内: %s'
                      % (s, ' '.join('U+%04X(%s)' % (ord(c), c) for c in miss)))

# 2b) 顶栏与小字体标签：找出以 hint 字体创建的 label 变量（顶栏左右、底栏、
#     统计页备注等都是 hint 字体），其字面量文案也必须在界面字符集内。
#     顶栏中间是 montserrat（只放 ASCII 数字），不检查。
hint_vars = set(re.findall(r'(\w+)\s*=\s*mk_label\([^;]*?&vocab_hint_14',
                           app_ui, flags=re.S))
hint_vars |= {'s_ftr', 's_hdr_l', 's_hdr_r'}
for var, s in re.findall(r'lv_label_set_text\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"',
                          app_ui):
    if var in hint_vars:
        miss = sorted(c for c in s if ord(c) > 127 and c not in hint)
        if miss:
            errors.append('小字体标签 %s 文案 "%s" 有字符不在界面字符集内: %s'
                          % (var, s, ' '.join('U+%04X(%s)' % (ord(c), c) for c in miss)))
for s in re.findall(r'set_hdr\(\s*"((?:[^"\\]|\\.)*)"', app_ui):
    miss = sorted(c for c in s if ord(c) > 127 and c not in hint)
    if miss:
        errors.append('顶栏文案 "%s" 有字符不在界面字符集内: %s'
                      % (s, ' '.join('U+%04X(%s)' % (ord(c), c) for c in miss)))

# ---------------------------------------------------------------- 3) & 4)
data = strip_comments(read('main/vocab_data.c'))
vocab_block = block(data, 'const vocab_entry_t VOCAB[]')
entries = re.findall(
    r'\{\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"',
    vocab_block)
print('解析到词条: %d 条' % len(entries))

ipa_bad, zh_bad = set(), set()
for en, zh, ip in entries:
    for ch in ip:
        if ord(ch) > 127 and ch not in ipa:
            ipa_bad.add(ch)
    for ch in zh:
        if ord(ch) > 127 and ch not in cjk:
            zh_bad.add(ch)

if ipa_bad:
    errors.append('音标有 %d 个字符不在音标字体里: %s'
                  % (len(ipa_bad), ' '.join('U+%04X(%s)' % (ord(c), c) for c in sorted(ipa_bad))))
if zh_bad:
    errors.append('译名有 %d 个字符不在中文字符集里: %s'
                  % (len(zh_bad), ' '.join('U+%04X(%s)' % (ord(c), c) for c in sorted(zh_bad))))

def_block = block(data, 'static const char *const S_DEF[]')
defs = re.findall(r'"((?:[^"\\]|\\.)*)"', def_block)
def_bad = set()
for d in defs:
    for ch in d:
        if ord(ch) > 127 and ch not in cjk:
            def_bad.add(ch)
if def_bad:
    errors.append('释义有 %d 个字符不在中文字符集里: %s'
                  % (len(def_bad), ' '.join('U+%04X(%s)' % (ord(c), c) for c in sorted(def_bad))))

# ---------------------------------------------------------------- 报告
print('中文字符集: %d 字  界面字符集: %d 字  音标字符集: %d 字'
      % (len(cjk), len(hint), len(ipa)))
print('底栏文案: %d 条；释义: %d 条' % (len(hint_lits), len(defs)))
print()
if errors:
    print('❌ 发现 %d 个字体覆盖问题:' % len(errors))
    for e in errors:
        print('   · %s' % e)
    sys.exit(1)

print('✅ 字体覆盖自检通过：界面上不会出现方框（豆腐块）')
print('   · 界面字符串里的非 ASCII 字符 %d 个，全部在字符集内' % len(nonascii))
print('   · 音标字符集 %d 个，全部覆盖' % len(ipa))
