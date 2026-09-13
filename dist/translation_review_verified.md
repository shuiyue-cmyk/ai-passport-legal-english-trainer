# 法律英语词库译名质量审查（经人工复核）

## 结论

- 词库总量：**1679 条**。
- OCR 词典词头直接或词形回退命中：**915 条**。
- 未直接命中：**764 条**；这包括案名、缩写、复合词、OCR 格式差异，**不等于译名错误**。
- 经过词典原文与教材语境双重复核的实质性关注点：**13 项**。
- 其中建议直接修正 4 项；建议清理去重聚合造成的义项混入 4 项；建议保留教材原译但补充词典术语义 4 项；建议删除含混简称 1 项。
- **已将 13 项修订应用到游戏词库并重新烧录**；教材 Markdown 未改，旧译名保存在词库字段 `zh_original`，完整变更见 `translation_revisions_applied.json`。

## 建议清单

| 英文术语 | 当前词库 | 建议 | 分类 | 依据（元照） |
| --- | --- | --- | --- | --- |
| `contract` | 合同；有效合同；买卖合同；合同法 | 合同；契约 | 建议修正卡片义项 | contract n. ①合同；契约。 |
| `criminal law` | 刑法；实体刑法；程序刑法 | 刑法；刑事法律 | 建议修正卡片义项 | criminal law 刑法；刑事法律。 |
| `prohibition` | 禁酒令 | 〈美〉禁酒；禁审令；禁止令 | 保留教材原译，补充术语义 | prohibition n. ①禁审令；禁止令……②〈美〉禁酒。 |
| `establishment clause` | 政教分离条款；禁止确立国教条款；国教条款 | 政教分离条款；禁止确立国教条款 | 建议删除含混简称 | establishment clause（美）政教分离条款；国会不得制定关于确立某种宗教为国教的法律。 |
| `principal` | 本金；主犯；一级二级主犯 | 本金；主犯 | 建议清理聚合义项 | principal n. ③本金；资本……⑥主犯；另可指本人/委托人、主债务人等。 |
| `testimony` | 作证；证明 | 证人证言 | 建议修正 | testimony n. 证人证言，指具备作证资格的证人宣誓或确认后提供的证据。 |
| `negotiation` | 谈判 | 谈判；（票据）转让 | 保留教材原译，补充票据法义 | negotiation n. ①谈判……②转让，由一人向另一人转移汇票或其他流通证券。 |
| `property settlement` | 财产和解 | 财产分割 | 建议修正 | property settlement 财产分割，适用于离婚案件。 |
| `filing` | 立案 | 提交（文件）；登记；文件归档 | 建议按教材语境修正 | filing n. 文件归档；filing articles of incorporation 为社团章程归档。 |
| `legal process` | 法律程序 | 法律程序（普通语境）；法律传票；法律令状（术语义） | 保留教材原译，增加术语义注释 | legal process （为合法目的经合法程序签发的）法律传票；法律令状。 |
| `appellate` | 上诉 | 上诉的；有关上诉的；受理上诉的 | 建议修正并关联复合术语 | appellate a. 上诉的；有关上诉的；受理上诉的。 |
| `civil disobedience` | 公民不服从 | 公民不服从；非暴力抵抗（定义性说明） | 保留教材原译，补充定义 | civil disobedience 非暴力抵抗：为抗议特定法律不公正而采取的违反法律的行为。 |
| `criminal liability` | 刑事责任；公司对杀人罪的刑事责任；公司刑事责任 | 刑事责任 | 建议清理聚合义项 | criminal liability 刑事责任，指因触犯刑法规定应受刑事处罚或处理的责任。 |

## 使用建议

1. 建议优先修正：`testimony`、`property settlement`、`filing`、`appellate`。
2. 建议清理由去重带来的混入：`contract`、`criminal law`、`principal`、`criminal liability`。
3. 建议保留教材主译、在卡片释义中补充：`prohibition`、`negotiation`、`legal process`、`civil disobedience`。
4. `establishment clause` 保留“政教分离条款／禁止确立国教条款”，删除“国教条款”这个容易误解的简称。

## 方法与边界

- 对全部 1679 条先做 OCR 词典词头的精确/词形回退匹配；无直接匹配不等于词典无条目或译名错误。
- 对高风险候选逐条回读《元照英美法词典》主条目，并回看教材语境。
- 教材原译与词典有不同表述时，优先标为“补充义项”或“聚合失真”，不机械判定教材错误。
