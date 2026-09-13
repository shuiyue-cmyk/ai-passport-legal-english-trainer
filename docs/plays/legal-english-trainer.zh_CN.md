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

三键操作（UP / DOWN / OK），240×320 彩屏。顶栏右侧显示电量（CW2017 电量计）。

息屏：5 分钟无按键自动息屏（CPU 与音频不停），主菜单长按↑/↓立即息屏；
息屏后第一下按键只唤醒不动作，抬起后恢复正常。

主菜单六项：顶部**学习单词**独占一行，中部 **英→中 / 中→英 / 错词本 / 复习**，
底行左**学习统计**、右**重置进度**。

### 卡片页（英→中、中→英）

| 操作 | 行为 |
| --- | --- |
| OK 单击 | 未翻面：显示答案；已翻面：记为「认识」并进入下一词 |
| OK 双击 | 播放当前词发音 |
| OK 长按 | 返回主菜单 |
| ↑ / ↓ 单击 | 未翻面：上/下一个词；已翻面：↑ 记为「不认识」、↓ 记为「认识」，均进入下一词 |
| ↑ / ↓ 双击 | 跳 10 条 |
| ↑ / ↓ 长按 | 跳到相邻章节的第一条 |

有例句的词条在卡片下方显示教材原句（英文+中文翻译，1067/1389 条覆盖）；
英文例句常态显示，中文例句仅看到答案后显示（卡片翻面、选择题判分后）。
长文本首停 3 秒后单行来回弹滚动，短文本静止。

### 选择题（混合练习、错词本）

屏上给出英文，下方 4 个中文备选（3 个全库随机干扰项），末行是同行的**不认识**与**播放音频**；
↑↓ 选择、OK 确认。

| 操作 | 行为 |
| --- | --- |
| ↑ / ↓ 单击 | 移动选项光标 |
| OK 单击 | 确认选项；判分后任意单击进下一词 |
| OK 长按 | 返回主菜单 |

- **混合练习**：未学词优先、按词库顺序每批 20 个，做完自动进下一批，批号与批内进度都存 NVS（中途退出重进可续）。
- **错词本**：答错过且未熟练的词，20 个一组（不满 20 也成一组），做完进下一组。
- 答对掌握度 +1 过关；答错记错，正确答案标绿，该词隔 3 词后重现，直到答对。
- **不认识**等于主动认错（记错、亮答案、稍后重练）；**播放音频**只发音，不判分。
- 学习/错词答对的词 30 学习分钟后进**复习**。
- 选择题常态显示英文例句全文（长句自动来回弹滚动）；中文例句判分后才显示。

### 重置进度

主菜单底行右侧按钮（左侧是学习统计）。进入后需二次确认（默认停在「返回菜单」防误触）；
确认删除会清空全部掌握度、错词计数、上次位置、批次与学习时长并写回 NVS。

### 复习

学习答对、错词答对的词，攒够 30 累计学习分钟后到期，每次取到期最早的 20 个。

| 阶段 | 行为 |
| --- | --- |
| 认不认识 | 只给英文（+英文例句）：认识亮中文，OK 进四选一；不认识不亮答案，直接往后放一轮四选一 |
| 四选一 | 与选择题同规则；答对到期顺延 1 小时，答错扔回错词本 |

### 掌握度与进度

每词 1 字节记录：低 2 位为掌握度（0 未学 / 1 初识 / 2 熟悉 / 3 熟练），
高 4 位为答错次数（上限 15）。答对掌握度 +1（上限 3）；答错回落为 1 并累加错次。

- **错词本** = 答错过、且尚未练到「熟练」的词，在错词本里以选择题重练。
- 进度、上次位置与混合/错词批次存在 NVS，断电不丢。
- 卡片与选择题页面的累计学习时长存在 NVS，显示在学习统计页。
- 顺序模式会按词条 id 断点续背；混合练习按批次推进（未学优先）。

## 硬件与资源占用

| 项目 | 数值 |
| --- | --- |
| 芯片 | ESP32-C3，8 MB Flash，无 PSRAM |
| 词库数据（内嵌 app） | 1389 条，`vocab_data.c` 约 126 KB |
| 中文字体（内嵌 app） | 黑体 16px 子集，1051 个字符 |
| Opus 音频分区 `vocabfs` | 1389 条 / 81,790 个 Opus 包 / **约 1.7 MB** |
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

# 2) 生成中文字体子集（不要直接用 npx 传中文符号串，
#    Windows PowerShell 会转坏参数；用仓库脚本，本机黑体即可）
#    初次先在 build/ 里装一次转换器：cd build && npm init -y && npm i lv_font_conv@1.5.3
node tools/makefont.js

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
