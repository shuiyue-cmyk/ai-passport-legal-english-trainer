#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将经《元照英美法词典》复核的高置信翻译修订应用到游戏词库。

不修改教材 Markdown。只更新游戏用 vocab_master.json，并保留：
- zh_original：修订前教材/旧词库译名
- translation_review：修订原因、词典依据、是否为教材语境保留项

可重复运行：已有 zh_original 时不覆盖原始值。
"""
import copy
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MASTER = os.path.join(ROOT, "tools", "vocab_master.json")
OUT_LOG = os.path.join(ROOT, "dist", "translation_revisions_applied.json")

# 13 条均已在 dist/translation_review_verified.md 中逐条给出词典证据。
# 修订原则：教材语境正确的义项保留在首位，用括号标注；词典的其他法律义另列。
REVISIONS = {
    "contract": {
        "zh": ["合同", "契约"],
        "definition": "《元照》：合同；契约。原词库中“合同法”“有效合同”“买卖合同”属于独立或下位概念，已从 bare contract 卡片移出。",
        "kind": "aggregation_cleanup",
        "evidence": "contract n. ①合同；契约。",
    },
    "criminal law": {
        "zh": ["刑法", "刑事法律"],
        "definition": "《元照》：刑法；刑事法律。“实体刑法”“程序刑法”已由 substantive criminal law、procedural criminal law 独立表示。",
        "kind": "aggregation_cleanup",
        "evidence": "criminal law 刑法；刑事法律。",
    },
    "prohibition": {
        "zh": ["禁酒令（教材语境）", "禁审令", "禁止令"],
        "definition": "《元照》：①禁审令；禁止令；②〈美〉禁酒。教材第一章的美国历史语境中“禁酒令”正确。",
        "kind": "contextual_supplement",
        "evidence": "prohibition n. ①禁审令；禁止令……②〈美〉禁酒。",
    },
    "establishment clause": {
        "zh": ["政教分离条款", "禁止确立国教条款"],
        "definition": "《元照》：〈美〉政教分离条款。禁止政府确立某种宗教为国教；原“国教条款”简称已删除以避免方向性误解。",
        "kind": "ambiguous_label_cleanup",
        "evidence": "establishment clause（美）政教分离条款。",
    },
    "principal": {
        "zh": ["本金", "主犯"],
        "definition": "《元照》：可指本金、本人/委托人、主债务人、主犯等。原“一级二级主犯”属 principals of the first and second degrees 的复合概念。",
        "kind": "aggregation_cleanup",
        "evidence": "principal n. ③本金；资本……⑥主犯。",
    },
    "testimony": {
        "zh": ["证人证言"],
        "definition": "《元照》：证人证言。指具备作证资格的证人在庭审、宣誓书或书面证词中提供的证据。",
        "kind": "translation_correction",
        "evidence": "testimony n. 证人证言。",
    },
    "negotiation": {
        "zh": ["谈判", "（票据）转让"],
        "definition": "《元照》：①谈判；②转让（由一人向另一人转移汇票或其他流通证券）。教材合同语境的“谈判”保留。",
        "kind": "contextual_supplement",
        "evidence": "negotiation n. ①谈判……②转让。",
    },
    "property settlement": {
        "zh": ["财产分割"],
        "definition": "《元照》：财产分割，适用于离婚案件。包括双方协议并经法院批准，或依据法院判决进行的分割。",
        "kind": "translation_correction",
        "evidence": "property settlement 财产分割。",
    },
    "filing": {
        "zh": ["提交（文件）", "登记；归档"],
        "definition": "《元照》：文件归档。教材“Authorities there file the papers”语境中，指当地机关提交、登记文件，而非“立案”这一程序结果。",
        "kind": "translation_correction",
        "evidence": "filing n. 文件归档。",
    },
    "legal process": {
        "zh": ["法律程序（教材语境）", "法律传票；法律令状（术语义）"],
        "definition": "教材第 4 章“正式法律程序”译为法律程序正确；《元照》收录的专门术语义为经合法程序签发的法律传票、法律令状。",
        "kind": "contextual_supplement",
        "evidence": "legal process 法律传票；法律令状。",
    },
    "appellate": {
        "zh": ["上诉的", "有关上诉的", "受理上诉的"],
        "definition": "《元照》：上诉的；有关上诉的；受理上诉的。教材该处搭配 appellate court，词库另有 appellate court＝上诉法院。",
        "kind": "translation_correction",
        "evidence": "appellate a. 上诉的；有关上诉的；受理上诉的。",
    },
    "civil disobedience": {
        "zh": ["公民不服从", "非暴力抵抗"],
        "definition": "《元照》：非暴力抵抗。为抗议某一特定法律不公正而采取的违反法律行为。教材通行译名“公民不服从”保留。",
        "kind": "contextual_supplement",
        "evidence": "civil disobedience 非暴力抵抗。",
    },
    "criminal liability": {
        "zh": ["刑事责任"],
        "definition": "《元照》：刑事责任。原“公司刑事责任”“公司对杀人罪的刑事责任”属于 corporate criminal liability 或具体案例语境，已从一般词头移出。",
        "kind": "aggregation_cleanup",
        "evidence": "criminal liability 刑事责任。",
    },
}

vocab = json.load(open(MASTER, encoding="utf-8"))
updated = []
seen = set()
for entry in vocab:
    key = entry["en"].lower()
    rev = REVISIONS.get(key)
    if not rev:
        continue
    seen.add(key)
    if "zh_original" not in entry:
        entry["zh_original"] = copy.deepcopy(entry["zh"])
    entry["zh"] = rev["zh"]
    entry["definition"] = rev["definition"]
    entry["translation_review"] = {
        "dictionary_basis": "元照英美法词典",
        "kind": rev["kind"],
        "evidence": rev["evidence"],
        "reviewed": "2026-09-13",
    }
    updated.append({
        "id": entry["id"],
        "en": entry["en"],
        "zh_original": entry["zh_original"],
        "zh_revised": entry["zh"],
        "kind": rev["kind"],
        "evidence": rev["evidence"],
    })

missing = set(REVISIONS) - seen
if missing:
    raise SystemExit("词库中未找到待修订词：" + ", ".join(sorted(missing)))

json.dump(vocab, open(MASTER, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
json.dump({
    "summary": {
        "updated_entries": len(updated),
        "source_textbook_markdown_changed": False,
        "note": "仅更新游戏词库，保留 zh_original 以便追溯；教材 Markdown 未改动。",
    },
    "revisions": updated,
}, open(OUT_LOG, "w", encoding="utf-8"), ensure_ascii=False, indent=2)

print("已修订游戏词库 %d 条" % len(updated))
for x in updated:
    print("  #%d %-24s %s -> %s" % (x["id"], x["en"], "；".join(x["zh_original"]), "；".join(x["zh_revised"])))
