#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""导出游戏词库为带修订追溯信息的 CSV。"""
import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "vocab_master.json")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "dist", "legal_english_vocab_verified.csv")

vocab = json.load(open(SRC, encoding="utf-8"))
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)

with open(OUT, "w", encoding="utf-8-sig", newline="") as fh:
    w = csv.writer(fh)
    w.writerow([
        "id", "英文术语", "当前中文译名（游戏）", "教材/旧词库原译", "音标",
        "词典说明/卡片释义", "出现章节", "教材频次", "来源", "审查类型", "词典依据"
    ])
    for x in vocab:
        r = x.get("translation_review", {})
        w.writerow([
            x["id"], x["en"], "；".join(x["zh"]), "；".join(x.get("zh_original", x["zh"])),
            x.get("ipa", ""), x.get("definition", ""),
            ",".join(map(str, x.get("chapters", []))), x.get("count", ""),
            x.get("source", ""), r.get("kind", ""), r.get("evidence", ""),
        ])

print("导出 %d 条到 %s" % (len(vocab), OUT))
