#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""导出按章节排列的词表 CSV（Excel 可直接打开，便于打印复习）。

用法: python tools/export_vocab_csv.py [输出路径]   默认 dist/legal_english_vocab.csv
"""
import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "vocab_master.json")
EX_SRC = os.path.join(ROOT, "tools", "vocab_sentences.json")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "dist", "legal_english_vocab.csv")

KIND = {"legal": "法律词", "academic": "学术词", "term": "术语"}

vocab = json.load(open(SRC, encoding="utf-8"))
ex = json.load(open(EX_SRC, encoding="utf-8")) if os.path.isfile(EX_SRC) else {}
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)

with open(OUT, "w", encoding="utf-8-sig", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["序号", "章", "英文", "词性", "中文", "类别", "其余出现章节", "教材频次", "例句", "例句译文"])
    for x in vocab:
        e = ex.get(x["en"]) or {}
        chs = x.get("chapters", [])
        w.writerow([
            x["id"], chs[0] if chs else "", x["en"], x.get("pos", ""), "；".join(x["zh"]),
            KIND.get(x.get("kind", ""), ""), ",".join(map(str, chs[1:])), x.get("count", ""),
            e.get("s", ""), e.get("zh", ""),
        ])
print("已导出 %d 条 -> %s" % (len(vocab), OUT))
