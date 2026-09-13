#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成经人工复核的《元照英美法词典》词库翻译审查报告。

设计原则：
- 教材原译优先保留；只对会导致错记的情况标为“建议修正”。
- 同一英文词在不同上下文的派生/复合概念，被去重聚合进同一卡片时，
  标为“聚合失真”，而非笼统判教材翻译错误。
- “词典有另一义项”不等于教材当前语境错误，单列为“补充义项”。
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOCAB = os.path.join(ROOT, 'tools', 'vocab_master.json')
COVERAGE = os.path.join(ROOT, 'dist', 'translation_headword_coverage.json')
OUT_JSON = os.path.join(ROOT, 'dist', 'translation_review_verified.json')
OUT_MD = os.path.join(ROOT, 'dist', 'translation_review_verified.md')

vocab = json.load(open(VOCAB, encoding='utf-8'))
by_en = {x['en'].lower(): x for x in vocab}
coverage = json.load(open(COVERAGE, encoding='utf-8'))['summary']

# 13 项均已逐条回读元照 OCR 原文确认。record 的 classification 明确区分
# “建议修正”与“教材语境正确但可补充”，避免把课程教材机械改坏。
RECORDS = [
    {
        'en': 'contract',
        'classification': 'aggregation',
        'action': '建议修正卡片义项',
        'zh_suggested': ['合同', '契约'],
        'dictionary_evidence': 'contract n. ①合同；契约。',
        'reason': '“合同法”已作为独立词条 contract law 存在；“有效合同”“买卖合同”是合同分类或复合概念，不宜与 bare contract 并列。',
        'confidence': 'high',
    },
    {
        'en': 'criminal law',
        'classification': 'aggregation',
        'action': '建议修正卡片义项',
        'zh_suggested': ['刑法', '刑事法律'],
        'dictionary_evidence': 'criminal law 刑法；刑事法律。',
        'reason': '“实体刑法”“程序刑法”已作为 substantive criminal law、procedural criminal law 的独立词条存在，不能作为 bare criminal law 的并列译名。',
        'confidence': 'high',
    },
    {
        'en': 'prohibition',
        'classification': 'contextual_supplement',
        'action': '保留教材原译，补充术语义',
        'zh_suggested': ['〈美〉禁酒', '禁审令', '禁止令'],
        'dictionary_evidence': 'prohibition n. ①禁审令；禁止令……②〈美〉禁酒。',
        'reason': '教材第一章的美国历史语境下“禁酒令”正确；但脱离语境做闪卡时，建议加注“〈美〉禁酒”，并补充法院禁审令/禁止令义。',
        'confidence': 'high',
    },
    {
        'en': 'establishment clause',
        'classification': 'ambiguous_label',
        'action': '建议删除含混简称',
        'zh_suggested': ['政教分离条款', '禁止确立国教条款'],
        'dictionary_evidence': 'establishment clause（美）政教分离条款；国会不得制定关于确立某种宗教为国教的法律。',
        'reason': '“国教条款”省掉了“禁止确立”的方向性，容易被误解为“设立国教的条款”。',
        'confidence': 'high',
    },
    {
        'en': 'principal',
        'classification': 'aggregation',
        'action': '建议清理聚合义项',
        'zh_suggested': ['本金', '主犯'],
        'dictionary_evidence': 'principal n. ③本金；资本……⑥主犯；另可指本人/委托人、主债务人等。',
        'reason': '“一级二级主犯”对应 principals of the first and second degrees，是更具体的复合术语，不应充作 bare principal 的一般义。',
        'confidence': 'high',
    },
    {
        'en': 'testimony',
        'classification': 'translation_error',
        'action': '建议修正',
        'zh_suggested': ['证人证言'],
        'dictionary_evidence': 'testimony n. 证人证言，指具备作证资格的证人宣誓或确认后提供的证据。',
        'reason': 'testimony 是证言/证据内容；“作证”是行为（testify），“证明”过宽。',
        'confidence': 'high',
    },
    {
        'en': 'negotiation',
        'classification': 'contextual_supplement',
        'action': '保留教材原译，补充票据法义',
        'zh_suggested': ['谈判', '（票据）转让'],
        'dictionary_evidence': 'negotiation n. ①谈判……②转让，由一人向另一人转移汇票或其他流通证券。',
        'reason': '教材合同语境的“谈判”正确；若作为跨章节总词库，补入票据法义能减少歧义。',
        'confidence': 'high',
    },
    {
        'en': 'property settlement',
        'classification': 'translation_error',
        'action': '建议修正',
        'zh_suggested': ['财产分割'],
        'dictionary_evidence': 'property settlement 财产分割，适用于离婚案件。',
        'reason': '离婚语境中 property settlement 指财产分割安排，不是一般“和解”。',
        'confidence': 'high',
    },
    {
        'en': 'filing',
        'classification': 'translation_error',
        'action': '建议按教材语境修正',
        'zh_suggested': ['提交（文件）', '登记', '文件归档'],
        'dictionary_evidence': 'filing n. 文件归档；filing articles of incorporation 为社团章程归档。',
        'reason': '教材句子为“Authorities there file the papers”，描述当地机关提交/登记文件；“立案”是可能的程序后果，不是 filing 的基本词义。',
        'confidence': 'high',
    },
    {
        'en': 'legal process',
        'classification': 'contextual_supplement',
        'action': '保留教材原译，增加术语义注释',
        'zh_suggested': ['法律程序（普通语境）', '法律传票', '法律令状（术语义）'],
        'dictionary_evidence': 'legal process （为合法目的经合法程序签发的）法律传票；法律令状。',
        'reason': '教材第 4 章“正式法律程序”是普通组合义，译作“法律程序”正确；词典收录的是法院文书的专门术语义，应并列提示而非替换。',
        'confidence': 'high',
    },
    {
        'en': 'appellate',
        'classification': 'translation_error',
        'action': '建议修正并关联复合术语',
        'zh_suggested': ['上诉的', '有关上诉的', '受理上诉的'],
        'dictionary_evidence': 'appellate a. 上诉的；有关上诉的；受理上诉的。',
        'reason': '该词是形容词；现有“上诉”丢失词性。教材该处实际搭配为 appellate court，而词库中已有独立的 appellate court＝上诉法院。',
        'confidence': 'high',
    },
    {
        'en': 'civil disobedience',
        'classification': 'contextual_supplement',
        'action': '保留教材原译，补充定义',
        'zh_suggested': ['公民不服从', '非暴力抵抗（定义性说明）'],
        'dictionary_evidence': 'civil disobedience 非暴力抵抗：为抗议特定法律不公正而采取的违反法律的行为。',
        'reason': '“公民不服从”是通行译名，教材原译可以保留；建议在卡片释义补上非暴力、抗议不公正法律的限定。',
        'confidence': 'high',
    },
    {
        'en': 'criminal liability',
        'classification': 'aggregation',
        'action': '建议清理聚合义项',
        'zh_suggested': ['刑事责任'],
        'dictionary_evidence': 'criminal liability 刑事责任，指因触犯刑法规定应受刑事处罚或处理的责任。',
        'reason': '“公司刑事责任”“公司对杀人罪的刑事责任”属于 corporate criminal liability 或具体案例语境，应独立保存，不能并入一般词头。',
        'confidence': 'high',
    },
]

issues = []
for r in RECORDS:
    x = by_en[r['en']]
    issues.append({
        'id': x['id'],
        'en': x['en'],
        'zh_current': x['zh'],
        'zh_suggested': r['zh_suggested'],
        'source': x['source'],
        'classification': r['classification'],
        'action': r['action'],
        'dictionary_evidence': r['dictionary_evidence'],
        'note': r['reason'],
        'confidence': r['confidence'],
    })

# 统计
by_class = {}
for x in issues:
    by_class[x['classification']] = by_class.get(x['classification'], 0) + 1

summary = {
    'total_vocab_entries': len(vocab),
    'mechanically_headword_checked': coverage['total'],
    'direct_or_inflectional_dictionary_headword_match': coverage['exact_or_inflectional_headword_match'],
    'no_direct_headword_match': coverage['no_direct_headword_match'],
    'manually_semantic_verified_issues': len(issues),
    'issue_breakdown': by_class,
    'methodology': [
        '对全部 1679 条先做 OCR 词典词头的精确/词形回退匹配；无直接匹配不等于词典无条目或译名错误。',
        '对高风险候选逐条回读《元照英美法词典》主条目，并回看教材语境。',
        '教材原译与词典有不同表述时，优先标为“补充义项”或“聚合失真”，不机械判定教材错误。',
    ],
    'do_not_auto_apply': True,
    'note': '本报告是词库修订建议。建议先确认 13 项后，再修改 flashcard 数据；不直接改写教材原文。'
}

out = {'summary': summary, 'issues': issues, 'headword_coverage_file': 'translation_headword_coverage.json'}
json.dump(out, open(OUT_JSON, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)

# 可读 Markdown
lines = []
lines += ['# 法律英语词库译名质量审查（经人工复核）', '']
lines += ['## 结论', '']
lines += [
    f'- 词库总量：**{len(vocab)} 条**。',
    f'- OCR 词典词头直接或词形回退命中：**{coverage["exact_or_inflectional_headword_match"]} 条**。',
    f'- 未直接命中：**{coverage["no_direct_headword_match"]} 条**；这包括案名、缩写、复合词、OCR 格式差异，**不等于译名错误**。',
    f'- 经过词典原文与教材语境双重复核的实质性关注点：**{len(issues)} 项**。',
    '- 其中建议直接修正 4 项；建议清理去重聚合造成的义项混入 4 项；建议保留教材原译但补充词典术语义 4 项；建议删除含混简称 1 项。',
    '- **未自动改写教材或游戏词库**；因为大部分词条来自课程教材，需保留教材语境。',
    ''
]
lines += ['## 建议清单', '']
lines += ['| 英文术语 | 当前词库 | 建议 | 分类 | 依据（元照） |', '| --- | --- | --- | --- | --- |']
for x in issues:
    cur = '；'.join(x['zh_current'])
    sug = '；'.join(x['zh_suggested'])
    ev = x['dictionary_evidence'].replace('|', '／')
    lines.append(f'| `{x["en"]}` | {cur} | {sug} | {x["action"]} | {ev} |')
lines += ['', '## 使用建议', '',
          '1. 建议优先修正：`testimony`、`property settlement`、`filing`、`appellate`。',
          '2. 建议清理由去重带来的混入：`contract`、`criminal law`、`principal`、`criminal liability`。',
          '3. 建议保留教材主译、在卡片释义中补充：`prohibition`、`negotiation`、`legal process`、`civil disobedience`。',
          '4. `establishment clause` 保留“政教分离条款／禁止确立国教条款”，删除“国教条款”这个容易误解的简称。',
          '', '## 方法与边界', ''] + [f'- {m}' for m in summary['methodology']]
open(OUT_MD, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')

print('写入:', OUT_JSON)
print('写入:', OUT_MD)
print(json.dumps(summary, ensure_ascii=False, indent=2))
