#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""对词库做《元照英美法词典》词头覆盖审计（不判断语义正确性）。"""
import json, re, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOCAB = os.path.join(ROOT, 'tools', 'vocab_master.json')
DICT = r'C:/Users/zouyu/Downloads/法律英语/词典/元照英美法词典_OCR合并.md'
OUT = os.path.join(ROOT, 'dist', 'translation_headword_coverage.json')

vocab = json.load(open(VOCAB, encoding='utf-8'))
lines = [l.strip() for l in open(DICT, encoding='utf-8', errors='ignore') if l.strip()]

POS = re.compile(r'^(?:n|v|vt|vi|a|adj|adv|prep|conj|pron|abbr)\.', re.I)


def variants(term):
    term = term.strip().lower()
    seen = [term]
    for prefix in ('the ', 'a ', 'an '):
        if term.startswith(prefix):
            seen.append(term[len(prefix):])
    # 简单复数回退，不把它当作“翻译相同”，只用来寻找词典词头。
    words = term.split()
    if words:
        last = words[-1]
        bases = []
        if last.endswith('ies') and len(last) > 3:
            bases.append(last[:-3] + 'y')
        if last.endswith('es') and len(last) > 2:
            bases += [last[:-1], last[:-2]]
        if last.endswith('s') and not last.endswith('ss') and len(last) > 1:
            bases.append(last[:-1])
        for b in bases:
            seen.append(' '.join(words[:-1] + [b]))
    out=[]
    for x in seen:
        if x not in out: out.append(x)
    return out


def exact_head(line, term):
    lo = line.lower()
    if not lo.startswith(term):
        return False
    rem = line[len(term):]
    if not rem:
        return True
    # 避免 term="court" 错命中 "court of appeals"，跳过空格后若仍是英文字母
    r = rem.lstrip()
    if not r:
        return True
    if POS.match(r):
        return True
    c = r[0]
    if c in '$<〈([{【《,;:：；-–—=' or '\u4e00' <= c <= '\u9fff':
        return True
    # OCR 常见特殊标记和数字编号
    if c.isdigit() or c in '*†‡':
        return True
    return False

matches = {}
for x in vocab:
    term = x['en'].lower().strip()
    hit = None
    used = None
    for cand in variants(term):
        for i, line in enumerate(lines):
            if exact_head(line, cand):
                hit = (i + 1, line[:1200])
                used = cand
                break
        if hit:
            break
    if hit:
        matches[x['id']] = {'term': x['en'], 'matched_head': used,
                            'line': hit[0], 'evidence': hit[1]}

no = [x['en'] for x in vocab if x['id'] not in matches]
summary = {
    'total': len(vocab),
    'exact_or_inflectional_headword_match': len(matches),
    'no_direct_headword_match': len(no),
    'note': '“无直接词头”仅表示 OCR 文本未匹配到同形词头；它可能是缩写、复合词、案名、OCR 格式差异或需从相关词条检索，不能直接推定译名错误。'
}
json.dump({'summary': summary, 'matches': matches, 'no_direct_headword': no},
          open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(json.dumps(summary, ensure_ascii=False, indent=2))
