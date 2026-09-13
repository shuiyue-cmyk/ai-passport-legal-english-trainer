# 法律英语背单词 · FoloToy AI Passport

**给 FoloToy AI Passport（ESP32-C3）准备的完整离线法律英语背单词玩法。**
面向正在学《American Law and Legal Systems》（Calvi & Coleman）的法学院学生。

## 这是什么

- 把 AI Passport 变成一台口袋里的法律英语背诵机。
- 内置 **1,679 条**法律术语：14 章中英对照教材已标注术语 + **45 条**依《元照英美法词典》补注的英美体制术语。
- **全词库离线发音**（Opus，约 1.97 MB 音频分区）。
- **三种学习方向**：英→中、中→英、拼写。
- **掌握度记录 + 错词本**，进度存在 NVS，断电不丢。
- **断点续背**：顺序模式会从上次位置继续。
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
2. 用 ESP-IDF 5.5.3 编译：

   ```bash
   idf.py build
   idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
   ```

3. 烧录合并镜像到 `0x0`，并把音频分区烧到 `0x35a000`：

   ```bash
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x35a000 dist/vocabfs.bin
   ```

4. 开机后主菜单为：**英→中 / 中→英 / 拼写 / 混合练习 / 错词本 / 学习统计**。

完整构建、烧录、分区与数据格式说明见
[`docs/plays/legal-english-trainer.zh_CN.md`](docs/plays/legal-english-trainer.zh_CN.md)。

## 翻译审查

词库已按《元照英美法词典》做了一轮核验，核验结果已纳入仓库：

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
