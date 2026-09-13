# 项目总结与交接说明

> 本文件用于把项目交接给其他开发者、Agent 或开发 Harness。它不是新的需求说明，而是截至 2026-09-13 的事实记录、验证结果和后续边界。

## 1. 项目目标

把 FoloToy AI Passport（ESP32-C3，8 MB Flash）改造成一台离线法律英语背单词设备，服务于学习《American Law and Legal Systems》（Calvi & Coleman）的法学院学生。

设备不依赖网络，提供法律英语词库、中文释义、国际音标、离线发音、掌握度记录、错词本和断点续背。

## 2. 最终内容规模

- 教材原始标注：7,184 处。
- 去重后教材词条：1,634 条。
- 依《元照英美法词典》补注英美体制术语：45 条。
- 最终词库：**1,679 条**。
- 带 IPA 音标：1,499 条。
- 全词库 Opus 发音：1,679 条，音频分区约 1.97 MB。
- 音频分区：`vocabfs@0x35a000`，大小 `0x4A6000`，位于受保护的 `cardid@0x356000` 之后。

### 去重规则

1. 7,184 处标注按英文原样去重为 1,683 条。
2. 合并冠词差异后为 1,681 条。
3. 合并 47 组规则复数后为 1,634 条，合并时保留全部中文义项。

## 3. 玩法

主菜单共 6 项：

1. 英→中
2. 中→英
3. 拼写
4. 混合练习
5. 错词本
6. 学习统计

卡片页：

- OK 单击：显示答案；答案已显示时记为“认识”并进入下一词。
- OK 双击：播放发音。
- OK 长按：返回主菜单。
- ↑ / ↓：未翻面时切换词；已翻面时分别记“不认识”/“认识”。
- ↑ / ↓ 双击：跳 10 条。
- ↑ / ↓ 长按：跳到相邻章节。

拼写页只处理纯字母单词；含空格或连字符的词条自动跳过。

## 4. 已解决的问题

### 4.1 大范围白框

原因：LVGL `lv_obj_create()` 默认会附带主题背景、圆角、边框或阴影；部分容器未清除默认样式，且屏幕背景没有强制不透明。

修复：

- 所有自建对象先调用 `lv_obj_remove_style_all()`。
- 纯布局容器设为透明并关闭边框。
- 屏幕背景显式使用 `LV_OPA_COVER`。

### 4.2 箭头和音标方框

原因：原先 CJK 字体子集未包含箭头；内置 Montserrat 不覆盖 IPA 扩展字符。

修复：

- 重新生成 CJK 字体，加入全部 UI 符号和箭头。
- 使用 Arial 生成专用 IPA 字体。
- 将 Arial 缺少的连字 `ʤ` / `ʧ` 转写为 `dʒ` / `tʃ`。
- 增加 `tools/check_fonts.py`，自动检查界面和词库字符是否都被字体覆盖。

### 4.3 “学习统计”菜单缺失

原因：C 枚举的 `MODE_COUNT` 与 `MODE_NAME[]` 数量不一致，多出的第 6 个初始化项被静默丢弃。

修复：

- 增加 `MODE_STATS` 枚举成员。
- 增加 `_Static_assert`，防止模式数量再次不一致。

### 4.4 退回主菜单后从第 1 条开始

原因：原程序只在答题时保存当前位置；通过 ↑ / ↓、跳 10 条或跳章节改变位置后没有立即写入 NVS。

修复：

- 新增 `vocab_session_set_current_id()`。
- 当前词条变化后及时保存当前位置。
- 英→中、中→英、拼写等顺序模式重新进入时按词条 id 恢复。
- 混合练习是随机模式，每次重新进入会重新洗牌，不保证接续上一轮随机顺序。

## 5. 翻译质量核验

已按《元照英美法词典》核对词库，并结合教材语境审查。对 1,679 条做了 OCR 词典词头覆盖检查；词头未直接命中不等于译名错误，因为其中包含案名、缩写、复合词和 OCR 格式差异。

逐条回读后确认 13 项关注点，已应用到游戏词库：

- `testimony`：作证/证明 → 证人证言。
- `property settlement`：财产和解 → 财产分割。
- `filing`：立案 → 提交（文件）；登记；归档。
- `appellate`：上诉 → 上诉的；有关上诉的；受理上诉的。
- 清理 `contract`、`criminal law`、`principal`、`criminal liability` 中由去重聚合带来的复合概念混入。
- 为 `prohibition`、`negotiation`、`legal process`、`civil disobedience` 补充词典术语义，同时保留教材语境。
- 删除 `establishment clause` 中容易造成方向误解的“国教条款”简称。

教材 Markdown 未修改。原译名保存在词库字段 `zh_original`。

审查文件：

- `dist/translation_review_verified.md`
- `dist/translation_review_verified.json`
- `dist/translation_revisions_applied.json`
- `dist/legal_english_vocab_verified.csv`

## 6. 构建与烧录事实

- 工具链：ESP-IDF 5.5.3，`riscv32-esp-elf` 14.2.0。
- 芯片：ESP32-C3，单核，无 PSRAM。
- Host 测试：1,723 项断言通过。
- 断点续背测试：通过。
- 字体覆盖检查：通过。
- 最终应用镜像：约 1.10 MB，3 MB 应用分区余量约 65%。
- 最终合并镜像：约 1.16 MB。
- 受保护布局检查：PASS，未覆盖 `cardid@0x356000`。
- 已烧录到 AI Passport 的 ESP32-C3 原生 USB 端口 COM6（VID:PID `303A:1001`）。
- 原厂 8 MB 固件已备份到本地工程的 `backup/factory-backup-8MB.bin`，大小 8,388,608 字节。

设备启动日志已确认：

- 显示 240×320 初始化成功。
- LVGL 初始化成功。
- 三键初始化成功。
- ES8311 音频初始化成功。
- `vocabfs` 解析到 1,679 条。
- 词库加载到 1,679 条。
- 6 个菜单模式已创建。
- 无 panic、看门狗复位或 brownout。

**最终屏幕画面由用户肉眼确认：白框消失、音标和箭头正常、菜单有六项、断点续背正常。后续不要自动尝试截图读取屏幕；如需视觉核验，等待用户拍照。**

## 7. 关键文件

- `main/vocab_app.c`：LVGL 界面、菜单、卡片、拼写、统计、NVS 断点续背。
- `main/vocab_model.c/.h`：掌握度、出题顺序、按词条 id 恢复位置。
- `main/vocab_audio.c/.h`：`vocabfs` 读取、Opus 解码、ES8311 播放。
- `main/vocab_data.c`：自动生成的 1,679 条词库 C 数据。
- `main/fonts/`：CJK、界面提示、IPA 三套字体。
- `components/bsp/`：AI Passport 板级支持。
- `components/opus/`：定点 Opus 解码器。
- `tools/gen_vocab_data.py`：词库 JSON → C 数据和字体字符集。
- `tools/gen_vocab_audio.py`：音频 → `vocabfs.bin`。
- `tools/flash_device.py`：备份、烧录应用和音频分区。
- `tests/test_vocab_model.c`：掌握度和出题顺序测试。
- `tests/test_resume_logic.c`：断点续背测试。

## 8. 当前边界

- 拼写模式跳过含空格或连字符的词条。
- 发音使用 Microsoft Zira 的离线 SAPI 合成，案名和拉丁语可能不够准确。
- 没有完整 SRS 间隔重复算法，当前是掌握度分级 + 错词本。
- 用户设备外观、按键手感和发音音量仍以实际设备为准。
