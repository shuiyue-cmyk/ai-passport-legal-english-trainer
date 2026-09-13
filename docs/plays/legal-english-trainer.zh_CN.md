<p align="right">
  <strong>简体中文</strong> · <a href="legal-english-trainer.md">English</a>
</p>

# 法律英语背单词（Legal English Trainer）

把 AI Passport 变成一台口袋里的法律英语背诵机：把《American Law and Legal
Systems》（Calvi & Coleman）14 章中英对照教材里被标注的法律术语全部抽出来，
再依《元照英美法词典》补齐教材漏标的英美体制术语，共 **1679 条**，全部内嵌固件，
每条都配 **Opus 发音**，不联网也能用。

## 词库来源与规模

| 项目 | 数量 | 说明 |
| --- | --- | --- |
| 教材已标注术语 | 1634 | 教材中文段落中以 `**中文术语**(English term)` 标注，去重并合并规则复数后 |
| 词典补注术语 | 45 | 教材英文正文出现、词典有条目、但教材未标注的英美体制术语 |
| **合计** | **1679** | 其中 1499 条带国际音标 |

教材标注出现总次数 7184 处，去重后 1683 条；再去掉冠词差异（`the/a/an`）得 1681 条；
最后把 47 个规则复数并入单数词条（义项全部保留），得 1634 条。

补注的 45 条包含 `Congress`、`Senate`、`House of Representatives`、`President`、
`Parliament`、`Cabinet`、`treaty`、`bureaucracy`、`federal court`、`federal system`、
`interest group`、`executive branch`、`High Court`、`King's Bench`、`justice of the peace`、
`circuit court`、`Inns of Court`、`Associate Justice`、`prosecuting attorney`、`bail`、
`sentencing`、`Miranda rights`、`Restatement` 等。其中 29 条直接采用词典原文释义，
16 条为人工补注（词典无对应词条或义项不匹配）。

> 明确排除：`information`、`reverse` 在教材中多数为日常义（"信息"、"相反"），
> 补注会造成误导，故不收录。

## 玩法

三键操作（UP / DOWN / OK），240×320 彩屏。

主菜单五项：**英→中 / 中→英 / 混合练习 / 错词本 / 学习统计**，底部另有**重置进度**按钮。

### 卡片页（英→中、中→英、混合练习）

| 操作 | 行为 |
| --- | --- |
| OK 单击 | 未翻面：显示答案；已翻面：记为「认识」并进入下一词 |
| OK 双击 | 播放当前词发音 |
| OK 长按 | 返回主菜单 |
| ↑ / ↓ 单击 | 未翻面：上/下一个词；已翻面：↑ 记为「不认识」、↓ 记为「认识」，均进入下一词 |
| ↑ / ↓ 双击 | 跳 10 条 |
| ↑ / ↓ 长按 | 跳到相邻章节的第一条 |

### 重置进度

主菜单底部的整宽按钮。进入后需二次确认（默认停在「返回菜单」防误触）；
确认删除会清空全部掌握度、错词计数与上次位置并写回 NVS。

### 掌握度与进度

每词 1 字节记录：低 2 位为掌握度（0 未学 / 1 初识 / 2 熟悉 / 3 熟练），
高 4 位为答错次数（上限 15）。答对掌握度 +1（上限 3）；答错回落为 1 并累加错次。

- **错词本** = 答错过、且尚未练到「熟练」的词。
- 进度与上次位置存在 NVS，断电不丢。
- 顺序模式会按词条 id 断点续背；混合练习每轮重新洗牌。

## 硬件与资源占用

| 项目 | 数值 |
| --- | --- |
| 芯片 | ESP32-C3，8 MB Flash，无 PSRAM |
| 词库数据（内嵌 app） | 1679 条，`vocab_data.c` 约 148 KB |
| 中文字体（内嵌 app） | 黑体 16px 子集，1107 个汉字 |
| Opus 音频分区 `vocabfs` | 1679 条 / 99,296 个 Opus 包 / **1.97 MB** |
| 音频编码 | Opus 8 kbps，16 kHz 单声道，20 ms 帧，实测约 850 B/s |
| 分区布局 | 在受保护的 `cardid@0x356000` 之后追加 `vocabfs@0x35a000`（0x4A6000） |

音频镜像格式（`tools/gen_vocab_audio.py` 生成）：

```
0x00  magic "LVOC" (4B) | version uint16 (2B) | count uint16 (2B) | reserved (8B)
0x10  offsets[count+1] uint32 LE   相对分区起始的字节偏移
...   数据：第 i 条 = [offsets[i], offsets[i+1])
      每条内部为裸包流：重复 { uint16 LE 包长, Opus 帧 }
```

## 生成与构建

```bash
# 1) 由词库 JSON 生成 C 数据与字体字符集
python tools/gen_vocab_data.py

# 2) 生成 16px 中文字体子集
npx lv_font_conv@1.5.3 --font <中文字体.ttf> \
    --symbols "$(cat tools/font_charset.txt)" --range 0x20-0x7E \
    --size 16 --bpp 4 --format lvgl --no-compress --lv-include lvgl.h \
    --lv-font-name vocab_cjk_16 -o main/fonts/vocab_cjk_16.c

# 3) 打包音频分区镜像
python tools/gen_vocab_audio.py <opus目录> dist/vocabfs.bin 1679

# 4) 构建固件（需 ESP-IDF 5.5.3）
idf.py build
idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
```

## 烧录

**烧录会覆盖设备上的原厂固件。务必先整片备份。**

```bash
PORT=COM6
# 备份原厂 8MB 固件
python -m esptool --chip esp32c3 -p $PORT -b 460800 read-flash 0 0x800000 factory-backup.bin

# 烧录应用（合并镜像，从 0x0）
python -m esptool --chip esp32c3 -p $PORT -b 460800 write-flash 0x0 build/FoloToy-AI-Passport-full.bin

# 烧录音频数据分区
python -m esptool --chip esp32c3 -p $PORT -b 460800 write-flash 0x35a000 dist/vocabfs.bin
```

## 源码结构

| 文件 | 职责 |
| --- | --- |
| `main/vocab_app.c` | LVGL 界面与按键分发（主菜单 / 卡片 / 重置确认 / 统计） |
| `main/vocab_model.c` | 掌握度与出题顺序的纯逻辑（不依赖 ESP-IDF，可 host 测试） |
| `main/vocab_audio.c` | `vocabfs` 分区读取 + libopus 解码 + ES8311 播放 |
| `main/vocab_data.c` | 词库数据（自动生成） |
| `components/opus/` | libopus 定点解码器组件（ESP32-C3 无 FPU） |
| `tools/gen_vocab_data.py` | 词库 JSON → C 数据 + 字体字符集 |
| `tools/gen_vocab_audio.py` | Ogg Opus → `vocabfs` 裸分区镜像 |
| `tests/test_vocab_model.c` | 掌握度与出题顺序的 host 测试 |

## 数据出处

- 教材：《American Law and Legal Systems》(Calvi & Coleman) 中英对照 Markdown，14 章。
- 词典：《元照英美法词典》（薛波主编，北京大学出版社 2014）OCR 文本。
- 发音：Windows SAPI（Microsoft Zira, en-US）离线合成，再以 ffmpeg/libopus 编码。

## 已知边界

- 发音为机器合成，专有名词（案名、拉丁语）读音可能不准确。
- 未做间隔重复（SRS）排程，只有掌握度分级与错词本。
