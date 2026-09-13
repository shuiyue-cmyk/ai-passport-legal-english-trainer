# 法律英语背单词 · FoloToy AI Passport

**给 FoloToy AI Passport（ESP32-C3）准备的完整离线法律英语背单词玩法。**
面向正在学《American Law and Legal Systems》（Calvi & Coleman）的法学院学生。

## 这是什么

- 把 AI Passport 变成一台口袋里的法律英语背诵机。
- 内置 **1,389 条**法律术语：14 章中英对照教材已标注术语，经去重、词典补注、非法律词削减与翻译复核。
- **全词库离线发音**（Opus，约 1.7 MB 音频分区）。
- **两种学习方向**：英→中、中→英，另有学习单词与错词本。
- **学习单词/错词本是 4 选 1**：给英文选中文，每批 20 个，答错隔 3 词重练直到答对；
  末行有**不认识**（记错亮答案）与**播放音频**。
- **复习**：学习/错词答对的词 1 学习小时后到期重现（认→亮中文→四选一，答错回错词本）。
- **教材例句**：1067 条配教材原句（英文+中文），卡片与选择题上显示。
- **掌握度记录 + 错词本**，进度存在 NVS，断电不丢。
- **断点续背**：顺序模式会从上次位置继续；混合/错词按批次推进，中途退出也能续接。
- **重置进度**：主菜单底部可清空全部进度（二次确认防误触）。
- **顶栏电量 + 学习时长**：顶栏右侧显示电池百分比，累计学习时长显示在学习统计页。
- **息屏**：5 分钟无操作自动息屏，主菜单长按↑/↓立即息屏，任意键唤醒（第一下只唤醒不动作）。
- **240×320 LVGL 界面**：中文、箭头、音标均已按设备字体能力修正渲染。

## 仓库结构

| 路径 | 作用 |
|---|---|
| `main/` | ESP-IDF 应用（界面、状态机、音频播放） |
| `components/bsp/` | FoloToy AI Passport 板级支持 |
| `components/opus/` | ESP32-C3（无 FPU）的定点 Opus 解码组件 |
| `tools/` | 数据生成、字体工具、词典审查、烧录辅助 |
| `tests/` | 学习逻辑的主机侧单元测试 |
| `docs/plays/` | 详细玩法文档（英文 / 中文） |
| `dist/` | 可直接烧录的音频分区与核验过的翻译审查结果 |
| `partitions.csv` | 保护布局并在 cardid 后追加 `vocabfs` 数据分区 |

## 给其他用户的使用方式

1. **先整片备份原厂固件。**
2. 用 ESP-IDF 5.5.3 编译，再进 `build/` 目录打合并镜像
   （merge-bin 在 `build/` 内执行，根目录的相对输出路径会失败）：

   ```bash
   idf.py build
   cd build && idf.py merge-bin -o FoloToy-AI-Passport-full.bin && cd ..
   ```

   也可直接用仓库脚本一键烧录（自动找镜像、先备份再写）：

   ```bash
   python tools/flash_device.py COM6
   ```

3. 烧录合并镜像到 `0x0`，并把音频分区烧到 `0x35a000`：

   ```bash
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x35a000 dist/vocabfs.bin
   ```

4. 开机后主菜单为：顶部**学习单词**独占一行，中部**英→中 / 中→英 / 错词本 / 复习**，
   底行左**学习统计**、右**重置进度**。

完整构建、烧录、分区与数据格式说明见
[`docs/plays/legal-english-trainer.zh_CN.md`](docs/plays/legal-english-trainer.zh_CN.md)。

## 翻译审查

词库已按《元照英美法词典》做了两轮核验（全库基线 + 削减后复核），核验结果已纳入仓库：

- `dist/translation_review_verified.md`
- `dist/translation_review_verified.json`
- `dist/translation_revisions_applied.json`

教材原文未改动；修订只发生在游戏词库数据中。

## 许可

MIT（与上游 FoloToy AI Passport 项目一致）。

## 致谢

- FoloToy AI Passport 硬件与 BSP 基线。
- 教材：《American Law and Legal Systems》（Calvi & Coleman）中英对照 Markdown。
- 词典：《元照英美法词典》，北京大学出版社。
