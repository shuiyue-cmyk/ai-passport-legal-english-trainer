# 交接文档（HANDOFF）— 法律英语背单词 / FoloToy AI Passport

> 写给接任的 agent / 开发者。本文记录 **2026-10-14 的当前状态**（词库重建 + 按章学习 + 对照元照）。
> 仓库根目录另有一份 `PROJECT_SUMMARY.zh_CN.md`，那是 **2026-09-13 的历史快照**（词库 1,679 条、6 项菜单那个年代），
> 两者冲突时**以本文为准**，那份只当史料读。
>
> 截至本文：2026-10-14「词库重建 + 按章学习」与 2026-10-15「对照元照」两个批次**只在本地提交**，尚未 push、尚未烧录（等用户口令）。
> 远端与设备仍是 2026-09-28「息屏/睡眠剥离 + 烧录保进度」批次。

---

## 0. 三十秒上手

| 问题 | 答案 |
| --- | --- |
| 代码在哪 | `C:\esp\ai-passport-legal-english`（GitHub `shuiyue-cmyk/ai-passport-legal-english-trainer`，分支 `main`） |
| 怎么编 | 见 §2 三条命令；产物 `build/FoloToy-AI-Passport.bin`，应用分区余量 56% |
| 怎么烧 | `python tools\flash_device.py COM6`（同时刷固件与音频分区，烧前先备份原厂镜像；**默认跳过 NVS，保留学习进度**，干净安装加 `--wipe`） |
| **什么时候不许烧/推** | **用户没说"设备连上了 / 烧录下"就绝对不动设备**，一次口令只覆盖一批（见 §3） |
| 现在最该做的 | 等口令烧录 2026-10-14 批次（**词条数 1639→2153，进度会清零，用户已同意**），再请用户按 §12 现场验证 |
| 用户怎么沟通 | 一律简体中文回复；选择类问题用 question 工具问；**不要截图读屏**，视觉核验等用户拍照 |

---

## 1. 项目是什么 / 现状坐标

把 FoloToy AI Passport（ESP32-C3，8 MB Flash，无 PSRAM，240×320 屏，三键 + ES8311 音频 + CW2017 电量计）
改造成一台**完全离线**的法律英语背单词设备，词源是《American Law and Legal Systems》（Calvi & Coleman）14 章。

用户是**从零学普通法**的学生，反馈旧词库（短语为主、教材长例句）"学不进去"。2026-10-14 按用户选择整体重建：
以单词为主（拆开会失义的术语才留短语，案例/法规名不收）、并入六级/考研以上的学术词、
每词一句 6–12 词新写短例句、**不显示音标**（用户没学过音标）、学习单词**先选章**。

### 当前内容规模

| 项目 | 数值 |
| --- | --- |
| 词库 | **2,153 条**（`tools/vocab_master.json`，id 1..2153 连续，按「首现章 → 课文首现位置」排序）：法律词 1,045 / 学术词 911 / 术语 197 |
| 各章词数 | 1–14 章：284 / 332 / 179 / 261 / 115 / 151 / 166 / 58 / 77 / 93 / 128 / 170 / 108 / 31 |
| 例句 | **2,153 条全覆盖**，新写 6–12 词短句 + 中文（`tools/vocab_sentences.json`） |
| 音频 | 2,153 条 Opus，173,244 个包，`dist/vocabfs.bin` = 2,717,135 B（2.59 MB） |
| 中文字体 | 黑体 16px，字符集 1,761 字 |
| 元照对照 | 元照收录且义项相符 1,537 / 有词条但义项不符 57 / 未收 559（多为学术词）；664 条中文改为元照译名 |
| 应用固件 | `0x159a70` = 1.35 MB，应用分区 3 MB，**余量 55%**；DRAM 静态占用 47% |

### 分区布局（受保护模板契约，**不得改动**）

```
nvs       data nvs       0x9000    0x6000
phy_init  data phy       0xf000    0x1000
factory   app  factory   0x10000   0x300000      # 应用 3 MB
cardid    data nvs       0x356000  0x4000        # 模板契约，绝不覆盖
vocabfs   data 0x40      0x35a000  0x4A6000      # 本项目自定义：Opus 音频库
```

---

## 2. 硬件与工具链

- **ESP-IDF 5.5.3**，装于 `C:\esp\esp-idf-v5.5.3`；`riscv32-esp-elf` 14.2.0。
- **必须**覆盖 Python 虚拟环境，否则 IDF 会挑到缺依赖的解释器：

```powershell
$env:IDF_PYTHON_ENV_PATH = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.13_env"
. "C:\esp\esp-idf-v5.5.3\export.ps1"
idf.py build            # 工作目录 C:\esp\ai-passport-legal-english
```

- **本机没有 C 工具链**（无 cc/gcc/clang），所以 `tests/*.c` 这些 host 测试**跑不了**；
  `tools/validate.sh` 也已失效（见 §10）。真正的门禁是 §5 的 `check_fonts.py` + `idf.py build`。
- 其他工具：node v24 + `build/node_modules/lv_font_conv@1.5.3`（字体转换）；ffmpeg 9.0.1（winget 用户级，会话内需自行加 PATH）；gh 身份 `shuiyue-cmyk`。
- 设备串口：ESP32-C3 原生 USB（VID:PID `303A:1001`），历史上出现过 COM4/5/6，**烧录前现列端口**，最近一次用 COM6。

---

## 3. 交付流程（这一节最重要）

### 3.1 口令规则

- **设备写入只在用户明确说出口令后执行**，口令形如"设备连上了 / 连上了 / 烧录下 / 我连上了你直接烧就行"。
- **一次口令只覆盖一批**，不构成长期授权。口令被某一批消费掉之后，下一批必须重新等。
- 没有口令时：可以改代码、编译、本地提交；**push 与设备写入一起走**（下面的"老三样"，
  即"改完 → 本地 commit → 等口令 → push + 出镜像 + 烧录"一次走完）。想单独 push 就先问用户。

### 3.2 老三样（拿到口令后）

```powershell
git push origin main

# 出完整镜像：★ 必须显式 -o，否则 merge-bin 写的是 build/merged-binary.bin
$env:IDF_PYTHON_ENV_PATH = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.13_env"
. "C:\esp\esp-idf-v5.5.3\export.ps1"
idf.py merge-bin -o C:\esp\ai-passport-legal-english\build\FoloToy-AI-Passport-full.bin
idf.py merge-bin -o C:\esp\ai-passport-legal-english\dist\FoloToy-AI-Passport-full.bin

# 烧录（先确认脚本打印的"镜像时间"是刚生成的）
python tools\flash_device.py COM6
```

`tools/flash_device.py` 会：整片备份原厂 8 MB 镜像（已有则跳过）→ 把合并镜像拆成 `[0x0,0x9000)` 与 `[0x10000,末尾)` 两段写入（**跳过 NVS 与 phy_init 扇区，学习进度保留**）→ 音频写 `0x35a000`。
`--wipe` 则整片写 `0x0`（NVS 一并清空），用于干净安装；镜像比刚构建的 app 旧时脚本**直接中止**（防 §3.3 那次事故重演）。
仓库根的 `flash.bat` 是同一个脚本的 Windows 双击入口（硬编码了上面的 python 路径，末尾 `pause`）。

### 3.3 镜像新鲜度陷阱（真踩过）

`merge-bin` **不带 `-o` 时输出的是 `build/merged-binary.bin`**，而不是 `build/FoloToy-AI-Passport-full.bin`。
2026-09-19 因此烧进过一次 9/18 的旧固件（把上一批修复整批顶掉）。已修（`00384ba`）：
`find_file()` 改为在候选里**取最新**的那份，并把 `merged-binary.bin` 纳入候选，烧前打印镜像时间戳，
当镜像比刚构建的 app 二进制还旧时给出警告。**看到那条警告就停下来核镜像，别继续。**

### 3.4 验证与交付格式

本项目习惯的交付格式（沿用原模板规范）：

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN     # 本机跑不了，写 NOT RUN 并说明
Device tests: PASS / FAIL / NOT RUN   # 需用户在场
Unverified: 仍需板卡或用户确认的事项
```

**不得把"编译通过"说成"验证通过"**；设备侧行为一律标注"待用户现场验证"。

---

## 4. 仓库结构与文件职责

```
main/
  main.c          启动、NVS 初始化、BSP 初始化、按键回调入口（回调里持 LVGL 锁再分发）
  vocab_app.c     ★ 全部 UI 与状态机（菜单/卡片/选择题/复习/统计/按键毛刺过滤）约 1800 行
  vocab_model.c   掌握度分级、会话位置、断点续背（可 host 测试，不依赖 LVGL）
  vocab_audio.c   vocabfs 读取、Opus 解码、ES8311 播放（含播放后关 codec 消底噪）
  vocab_data.c    自动生成（禁用中文注释那套，勿手改）：VOCAB[]、S_DEF[]、S_EX_EN/ZH[] 与访问器
  vocab_data.h    vocab_entry_t 定义
  fonts/          vocab_cjk_16.c / vocab_hint_14.c（由 tools/makefont.js 生成；音标字体 2026-10-15 已删除）
components/
  bsp/            板级：display（含背光 PWM/息屏）、button（三键 ADC）、audio、battery、i2c、pins.h
  opus/           定点 Opus 解码器
tools/
  gen_vocab_data.py   ★ 词库 JSON → vocab_data.c + 三个字体字符集 txt（含 ASCII 折字符与硬校验）
  makefont.js         字符集 txt → main/fonts/*.c（用 simhei.ttf / arial.ttf）
  check_fonts.py      ★ 字体覆盖自检（门禁）
  synth_vocab_audio.py ★ 词库 → SAPI(Zira) 合成 → ffmpeg 转 Opus → 调 gen_vocab_audio 打包（带缓存，只补变动条目）
  gen_vocab_audio.py  逐条 Opus → vocabfs 镜像
  export_vocab_csv.py 按章词表 → dist/legal_english_vocab.csv
  flash_device.py     ★ 备份 + 烧录（见 §3）
  vocab_master.json   2,153 条词库（唯一数据源；字段 en/zh/pos/kind/chapters/count + yz_head/yz_page/yz_fit，
                      ipa 与 definition 留空）
  vocab_sentences.json  2,153 条短例句（键=en；s 英文 / zh 中文 / ch 章）
  check_repo.py / validate.sh / install-actionlint.sh   模板残留，当前**必定失败**，见 §10
tests/          test_vocab_model.c / test_resume_logic.c（host 测试，本机无编译器）
dist/           发布产物：完整镜像、音频镜像、按章词表 CSV（旧词库的翻译核验报告已于 2026-10-14 删除，git 历史可查）
backup/         原厂 8 MB 整片备份（只此一份，别删）
docs/plays/     项目说明（模板目录下仅此一项）
```

---

## 5. 数据管线与不变量（改词库必读）

```
tools/vocab_master.json  ──gen_vocab_data.py──▶  main/vocab_data.c
                          └──────────────────▶  tools/font_charset*.txt ──makefont.js──▶ main/fonts/*.c
                          └──────────────────▶  （同时用 check_fonts.py 自检）
```

```powershell
python tools\gen_vocab_data.py     # 词条数 / 例句覆盖 / 字符集字数会在 stdout 报告
node tools\makefont.js             # 字符集变了才需要（需先 cd build && npm i lv_font_conv@1.5.3，已装）
python tools\check_fonts.py        # 必须 PASS
idf.py build
```

### 不变量（都是踩出来的）

1. **上屏字符必须在"已编译字体"里**。`check_fonts.py` 的 ground truth 是 `main/fonts/*.c` 头部注释里的 `--symbols`，
   不是字符集 txt（txt 可能比字体新）。它检查五项：界面字面量、底栏/顶栏小字体、音标、中文译名与释义、
   以及**用 LVGL 内置字体渲染的英文例句与英文词条**（内置 Montserrat 只有 ASCII + `°` `•`）。
2. **simhei 没有 `✓`（U+2713）**，转换器会静默丢字形；界面已改用"对"字。
3. **英文句子在生成时折成 ASCII**（`gen_vocab_data.py` 的 `ASCII_FOLD`）：`’ → '`、`“ ” → "`、
   `– — → -`、`… → ...`、`§ → Sec.`。原因是 §5-1 那条（内置字体没有这些字形，上屏是方框）。
   中文字体有这些字形，中文句子**不折**。`require_ascii()` 会对折叠表以外的字符直接报错退出。
4. **英文词条自身必须是 ASCII**，同一处硬校验守着。
5. `vocab_sentences.json` 的落盘格式是 **indent=1 + 末尾换行**；用 indent=2 重存会整文件重写（历史上造成 +11600/−5335 的假 diff）。
6. 改界面文案后必须重跑 `gen_vocab_data.py`（它把 `UI_TEXT` 里的字并入字符集）再 `check_fonts.py` 自检。
7. **词条数被多处引用**：`README.md` / `README.zh_CN.md` 里的条数、音频打包的 expected_count、
   `dist/vocabfs.bin` 与固件必须一致。改词条数时**逐处同步**，否则会出现"固件 1639 而音频 1412"这种错位（放错发音）。

---

## 6. 音频管线

- 语料规格：**Ogg Opus，16 kHz 单声道，8 kbps，20 ms 帧**；TTS 用 Windows SAPI `Microsoft Zira Desktop`（en-US，与既有语料同一声线）。
- 分区格式（`gen_vocab_audio.py` docstring 有完整说明）：`LVOC` magic + version + count + 偏移表 + 数据区，每条内部是重复 `{uint16 LE 包长, Opus 包}`。
- 整体重打包：

```powershell
python tools\gen_vocab_audio.py <音频目录> dist\vocabfs.bin 1639     # 第三参=期望条数，不符会警告
```

- **顺序不变量**：音频第 i 条 ↔ `vocab_master.json` 第 i 条，**纯位置对应**。所以扩充只能**追加**，
  且必须与词库追加顺序一致；任何错位都会让设备念出别的词。**永不同步烧"固件词条数 ≠ 音频条数"的组合。**
- **正式脚本** `python tools\synth_vocab_audio.py`（2026-10-14 固化）：按词库顺序逐条 SAPI 合成 → ffmpeg 转 Opus →
  打包到 `dist/vocabfs.bin` 并核对条数。缓存在 `build/audio_work/`（`manifest.json` 记"序号→文本"），
  词库任意改动（插入/删除/重排都行）后重跑即可，只重合成文本变了的序号。ffmpeg 不在 PATH 时先把
  `%LOCALAPPDATA%\Microsoft\WinGet\Links` 加进 PATH。
- 全大写 ≤6 字母的缩写会逐字母朗读（`speech_text()`）。
- 已不显示音标：`ipa` 字段全部留空；`vocab_entry_t.sub`（原 `.ipa`）是「词性 + 元照查词提示」，
  由 `gen_vocab_data.py` 的 `sub_line()` 生成，用中文字体渲染（见 §16）。

---

## 7. NVS 状态布局与迁移规则

命名空间内的键（`main/vocab_app.c` 顶部）：

| 键 | 内容 | 大小 |
| --- | --- | --- |
| `prog` | 每词掌握度字节数组 | **恰好 `VOCAB_COUNT` 字节** |
| `stat` | `vocab_stat_t`：magic `LEVO` + 位置/模式/章节掩码/批次计数/学习秒数 + **quiz_ch / ch_batch[14]**（2026-10 后加；`batch_mixed` 已弃用占位） | 结构体长度 |
| `qstat` | 选择题"批内续背"：模式 + 批号 + 已过关词条 id 列表。学习单词的批号编码为 `章<<8 | 章内批号` | — |
| `duea`/`dueb` | 复习到期表（累计学习分钟），按 700 切两块 | 2×u16 数组 |
| `miss` | 连续未抽中计数（复习保底用） | `VOCAB_COUNT` 字节 |
| `rvp` | 上一组已复习词条 id（防连着两次被抽中） | 1 + 20×u16 = 41 B |

**关键规则**：

- `prog` 的读取带长度校验：取出的长度必须等于当前 `VOCAB_COUNT`，否则**整表清零（进度重来）**。
  ⇒ **改词条数 = 用户进度重置**，烧这种固件前必须明确告诉用户。
- `stat` 里后加的字段（批次计数、学习秒数）用 `offsetof` 逐个判断长度，老 blob 短读时保持默认值，**不要直接扩结构体就完事**。
- 模式编号做过一次迁移（删除"拼写"模式时）：老号 2 → 英→中，老 3..5 依次前移。**不要复用旧号**。
- 深度睡眠/自动关机已于 2026-09-28 整体移除（§8.7）；NVS 恢复对正常重启（断电重开）依旧适用：进度、到期表、批次号都在盘上，学习时长在回菜单/切批时结算。
- **烧录保进度**：`flash_device.py` 默认跳过 NVS 扇区（§3.2），重烧固件不清进度；但**改词条数依旧 = 进度重置**（`prog` 长度校验），这条不变。

---

## 8. 固件行为现状

### 8.1 菜单与模式

`mode_t` 枚举（NVS 存的是这个编号，**只增不改**）：`EN2ZH=0, ZH2EN=1, MIXED=2, WEAK=3, STATS=4, REVIEW=5`。

显示顺序是另一张表 `MENU_ORDER[] = { MIXED, EN2ZH, ZH2EN, WEAK, REVIEW, STATS }`，布局（`menu_build()`）：

```
┌──────────── 学习单词（独占顶行，全宽） ────────────┐
│  英→中 (106×42)  │  中→英 (106×42)  │
│  错词本          │  复习            │
│  学习统计 (106)  │  重置进度 (106)  │   ← 并排底行
```

> 注：2026-08 有一条旧待办"重置和统计放一起、学习单词独占一行"——**代码现状已符合**，只差用户肉眼确认一次（见 §11）。

### 8.2 学习单词（MIXED，先选章，4 选 1 闯关）

- 菜单进「学习单词」先到**选章页**（`VIEW_CHAPTER`，`chapter_build()`）：14 章，10 行窗口随光标滚动，
  每行「第N章 章名」+「已学/总数」，学完的章绿色。↑↓ 抬起移动、OK 单击开始（与主菜单同手感）。
- 主章节 = `chapters` 掩码最低位（生成器保证就是归属章）。`q_lock_build` 按主章节做稳定计数排序，章内保持 id 顺序 = 课文顺序；
  每章均分成 `ceil(n/20)` 批（`ch_batch_range`，不会出现零头小批）。旧版的 FNV 哈希打散已删除（新词库不存在"后补词挤章尾"问题）。
- **批内出题顺序**每次进入都用 tick 播种的 Fisher-Yates 重洗（`quiz_build`）。
- 入口：从该章存档批号 `s_ch_batch[ch-1]` 开始在章内绕圈找第一个"含生词"的批；全章学会则按存档批轮着复练。
- 本批结束时若本章刚好学完（批前有生词、批后没有）→ 回选章页、顶栏显示「第N章学完」、光标落到下一章。
- 答错 → 记入错词本、本题重现直到答对；答对 → 更新掌握度并安排复习（见 8.4）。
- 底部动作行：`不认识`（按答错处理）+ `播放音频`（只发音、不判分）。

### 8.3 卡片模式（英→中 / 中→英）

- **只浏览、不计分**（`473c945`）：卡片页翻面不再改掌握度，底部"已学"也只统计**学习单词**里的作答。
- 中→英方向的中文例句**只在翻面后**显示（中文例句=答案的一部分）；复习的提问阶段也只给英文。
- 新词条**首次出现自动播一次发音**（`s_audio_last_id` 守卫，同词翻面/判分/切阶段不重播）；OK 双击可手动重播。

### 8.4 复习（REVIEW，SRS 过关流）

用户给定的规则（逐条落码，改动前先确认口径）：

1. 在学习/错词本里**答对**过 → 首遍**必定**进入复习队列（覆盖优先）；答错的路径不安排。
2. 复习中答对 → **25% 概率**续期（`RV_ADMIT_PCT`），否则毕业；连续 3 次未抽中，**第 4 次必中**（`RV_PITY_MAX`）。
3. 间隔 = 到期后 **30 学习分钟**（`RV_DUE_MIN`）；到期时间按"累计学习分钟"计，不是墙上时钟。
4. **同一词不与上一组重复**（`rvp` blob 存上一组 20 条，构建时排除；排除后为空则放行，自终止）。
5. 提问阶段 `不认识` = 重新入队且不揭示中文；`重置进度` 会清掉到期表、保底计数与上一组记录。

### 8.5 例句显示（跑马灯）

- 释义与例句都是单行滚动；**先静止 3 秒**（`MARQUEE_STILL_MODE = LV_LABEL_LONG_DOT`，单行省略号，不会把版面撑高）再开始滚。
- 每次换词/换题都要重布防（`marquee_arm()`），否则首停只在第一题出现。
- 用 **bounce**（来回）而不是 circular：circular 在恰好超宽时会看到"被拉回一点"的重复起跳。

### 8.6 键位与按键过滤（近期改动集中区）

- 三个键（上/下/确定）**共用一根 ADC 线 GPIO0**，靠分压电阻区分：窗口在
  `components/bsp/include/bsp_pins.h` 的 `BSP_BTN_MV_TABLE`（上 `0..150`、下 `150..447`、确定 `447..1900`、松开约 3300 mV）。
- 语义：OK 长按（1 s）= 回主菜单（全局）；菜单 ↑↓ 抬起即移动光标、OK 单击进入；卡片 OK 单击翻面/双击发音/↑↓ 记认识与否；
  选择题所有键都可选。
- **毛刺过滤（两层）**，因为噪声会让按键组件报出一次完整的"按下即抬起"：
  1. 抬起时长 < 60 ms（`PRESS_MIN_MS`）的整个手势丢弃，**并记下这一次的时长**；
     单击/双击回声按"这一次"的时长判定（曾经读的是上一次的时长，导致毛刺回声照样穿过滤网）。
  2. 按下 **30 ms 后独立复采一次 ADC**（`bsp_button_key_held()`）：真按住的键此刻仍在窗口内，噪声早已消失。
     复采没出结果就放行（不改手感）。
  原有的第三层（唤醒吞键）与"息屏下必须复采通过才唤醒"已随息屏/睡眠一并删除（§8.7）。
- ⚠ `bsp_button_key_held()` 读的是与按键组件**同一路 ADC、同一张窗口表**（同衰减/同位宽/同校准），
  所以"组件认为按下"与"电压落在窗口内"物理上必然一致；读不到时返回 `-1`，调用方按"未知"放行。

### 8.7 息屏与自动关机（2026-09-28 已整体移除）

- 原机制（5 分钟闲置息屏 + 再 10 分钟深度睡眠关机 + 任意键唤醒/吞键）**应用户决定整体剥离**：
  唤醒路径在 ADC 噪声下屡出怪问题（"息屏后自动亮屏""深度睡眠不工作"），两轮修复（`ae45dc2`、`e570d18`）仍无法现场验证，遂删除。
- 现状：**屏幕常亮**，无自动息屏/关机，主菜单长按↑↓不再有息屏语义；直接断电进度也不丢（每次作答都落盘）。
  BSP 的 `bsp_display_sleep()` 保留但已无人调用；`marquee_still()` 已删除。
- 背光 PWM 频率 **25 kHz**（`BSP_BL_LEDC_FREQ_HZ`）：5 kHz 时载波会驱动 MLCC 压电发声，能听见啸叫。
- 音频空闲时关 codec（`bsp_audio_idle()`）：PA 使能脚是硬连常开，只有关 codec 才能消掉 DAC 静态底噪。

### 8.8 其他已修问题（避免重复诊断）

- 大范围白框：自建对象先 `lv_obj_remove_style_all()`，容器透明无边框，屏幕背景 `LV_OPA_COVER`。
- 箭头/音标方框：CJK 字体补箭头；IPA 另用 Arial 生成；Arial 缺的连字 `ʤ/ʧ` 转写为 `dʒ/tʃ`。
- 按键延迟：菜单/选择题/确认页走 `RELEASE` 快速路径，`CLICK` 回声忽略；NVS 合并为一次事务；长按从 1.5 s 压到 1 s。

---

## 9. 红线与踩过的坑

1. **设备写入必须等口令**，一次口令只覆盖一批（§3.1）。这条最容易被违反。
2. **同名镜像多份存在时取最新的**（`flash_device.py` 已修）。`merge-bin` 不带 `-o` 的输出名是 `merged-binary.bin`。
3. **不要在没有 `-o` 的情况下 merge-bin 后又去 `build/` 取旧文件**——这一族事故已经发生过一次（§3.3）。
4. **词条数变动 = 进度重置**（`prog` blob 长度校验），且必须同步固件/音频/文档三处条数。
5. **音频按位置对应词库**，错位会念错词；词条数不一致的组合绝不烧。
6. **managed_components/ 不要手改**：那是 IDF 组件管理器的产物，会被依赖更新覆盖。要改行为就在 `components/bsp` 或 `main` 里包一层。
7. **LVGL 非线程安全**：LVGL 任务之外访问 LVGL 必须持 `bsp_lvgl_lock()`；按键回调运行在 button 组件的定时器任务里，
   `main.c` 的 `on_key()` 已先加锁再分发。按键回调里不要做重活。
8. **不要用 PowerShell 的 `Get-Content -Raw` + `Set-Content` 改本仓源文件**：`main` 与 `tools` 下的文件含大量 UTF-8 中文，
   这么改会双重编码且**不可逆**。用 `edit`/`apply_patch` 工具，或 node/python 做字节级读写。
9. **UI 文案只能用已编译字体里有的字形**，改完必须 `check_fonts.py`（§5-1）。simhei 没有 ✓。
10. **本机是 PowerShell（win32）**：不支持 `&&`；`curl` 是 `Invoke-WebRequest` 别名；`2>/dev/null` 会去建 `C:\dev\null`；
    没有 `grep`/`head`（用工具而不是 shell 命令）。含中文的临时脚本优先用 python 写而不是 PS 拼字符串。
11. **不要截图读屏**：用户明确禁止过，视觉核验一律等用户拍照。
12. **（历史教训）深度睡眠的 GPIO 唤醒没有去抖，且无法用软件事后校验**：
    开机耗时（数百 ms）比一次点按更长，若在开机后检查"键是否仍被按住"会**拒掉所有正常唤醒**（设备从此唤不醒）。
    ——深度睡眠已于 2026-09-28 整体移除（§8.7），此条留作警示：**若将来重新引入睡眠/唤醒，别再走"开机后复检"这条路**；
    噪声唤醒只能换硬件思路（RC 滤波 / 换唤醒引脚）。
13. **不要把 `§` `’` `—` 这类排版符号直接写进英文界面/例句**（内置字体没有字形），走生成器的折叠表。
14. 每次扩充词库后，`README*.md` 里的条数说明是**最容易被忘记同步**的一处。

---

## 10. 已知失效 / 无关物（别去追）

- `tools/validate.sh` 与 `tools/check_repo.py` 是**模板残留**，在本 fork 里**必定失败**。当前失败原因（已实测）：
  缺 `docs/CHANGELOG.md`、`.github/` 下的社区文件与 issue 表单；要求根目录 Markdown 只放英文默认版、
  且中文版必须是 `.zh_CN.md` 配对（本仓 `README.md` 是中文散文、`PROJECT_SUMMARY.zh_CN.md` 在根目录）；
  `dist/*.md` 缺英文默认版；`.github/workflows/` 不存在。它还要求 `main/ui_pixel_math.c`、`tests/test_ui_pixel_math.c`、
  `tests/test_verify_firmware.py`、`tools/install-actionlint.sh` —— **这四个文件本仓都没有**。
  ⇒ **它不是本项目门禁**。门禁就按 §5 的 `check_fonts.py` + `idf.py build`。
  想适配它是另一件事（要改模板校验或补齐文档树），**先问用户**再动。
- `AGENTS.zh_CN.md` / `AGENTS.md` 同样来自模板，其"按任务加载上下文"表格指向 `docs/development/**`、`docs/hardware-design/**` 等
  **本仓不存在的目录**（`docs/` 下只有 `plays/`）。其**通用条款仍然有效**（硬件事实优先级、BSP/main 边界、LVGL 加锁、不得提交密钥、
  多语言文档配对等），但**别按它去读那些不存在的文件**。`CLAUDE.zh_CN.md` 是空文件。
- `main/vocab_app.c` 里"5 个模式（下标 0..4）"的注释已过期（加了复习后是 6 个）；以 `MODE_NAME` 与 `_Static_assert` 为准。

---

## 11. 待办与开放问题

1. ~~现场验证"息屏后自动亮屏"是否修好~~ **已关闭（2026-09-28）**：息屏/睡眠整体剥离（§8.7），该问题不复存在。
2. ~~"目前深度休眠貌似没有工作"报障~~ **已关闭（2026-09-28）**：同上，深度睡眠已删除。改为现场验证"屏幕常亮 + 重烧后进度保留"（§12）。
3. **【待用户确认即可关闭】** 菜单底行布局（旧待办"重置和统计放一起、学习单词独占一行"）：**代码现状已符合**（§8.1），
   请用户看一眼确认，确认后从待办里划掉。
4. ~~合成音频脚本固化~~ **已关闭（2026-10-14）**：`tools/synth_vocab_audio.py`。
5. ~~README 例句条数~~ **已关闭（2026-10-14）**：README 与 `docs/plays/` 已按新词库重写。
6. **【可选】** `PROJECT_SUMMARY.zh_CN.md` 是 9/13 的历史快照，内容全面过期（1679 词、6 项菜单、无复习/息屏）。
   用户此前指示"不要动它"。若用户同意，可用本文替换或标注"历史文档"。
7. **【可选】** §10 的模板校验物：适配（让它在本 fork 通过）或删除，需用户决定。
8. **【随用户反馈】** 词库是两个子代理逐章判断 + 主控合并（做法见 §15）。用户学的过程中若觉得某些词太基础/太偏，
   直接改 `tools/vocab_master.json` + `tools/vocab_sentences.json`，重跑 §5 管线与 `synth_vocab_audio.py`（词条数变 = 进度清零）。

---

## 12. 设备侧验证清单（交给用户照着验）

刷完固件后按顺序验，出问题记下"亮/响/动的具体现象 + 当时按了什么键"：

1. **屏幕常亮**：放置 5 分钟以上不碰 → 屏幕保持点亮，不再自动息屏/关机。
2. **按键**：菜单 ↑↓ 移动光标、OK 进入/长按返回；卡片翻面/发音；选择题四选一 + 不认识 + 播放音频。
3. **烧录保进度**：烧录新固件重启后，学习进度/学习时长/批次号应与烧录前一致（NVS 未被触碰）。
4. **学习单词选章**：进「学习单词」先出选章页（10 行窗口滚动，显示已学/总数）；选第 N 章后出的都是该章的词，
   按课文顺序约 20 词一批；退出再进，本批成员不变、只重排出题顺序；学完一章自动回选章页并跳到下一章。
5. **复习**：学习里答对过的词才进复习；到期按"学习分钟"计 30 分钟；同一词不会连着两组出现；连续 3 次没抽中第 4 次必中。
6. **例句**：每词一句短例句、无方框，首停 3 秒后再滚动；中文例句只在翻面/判分后出现。卡片上不再有音标，只显示词性。
7. **发音**：新词首次出现自动播一次；念的必须是屏幕上那个词（验证音频与词库没有错位）。
8. **底噪与啸叫**：安静环境下贴耳听，不应有持续底噪或高频啸叫。
9. **电量/学习时长**：顶栏电量显示；统计页学习时长随时间增长；重置进度会清零并确认。

---

## 13. 与用户协作的习惯（重要）

- **一律简体中文回复**（思考用什么语言不限）。
- 选择类、方案类问题用 question 工具问，并给推荐项；用户会用自由文本回答，**照原话记录，不要硬套选项**。
- 用户常在电脑旁但有时会离开：需要放行按钮的操作尽量别依赖即时确认。
- **一次性口令**：设备写入等"设备连上了 / 烧录下"这类明确口令，且一次只管一批。
- **禁止截图读屏**；视觉核验等用户拍照后判断。
- 工作痕迹（临时脚本、探针、dumps）放到 `%TEMP%\acu_work\` 或 `build/`；**清理一律走回收站**，
  不要永久删除；`backup/` 与原厂备份永不清理。
- 交付时如实报告：编译过 ≠ 验证过；设备侧结论一律标"待用户确认"。

---

## 14. 更早的历史在哪

如果需要 2026-09-13 之前的任务细节（每一批改了什么、当时为什么这么定），开发机上还有会话记忆：

```
C:\Users\zouyu\.local\share\mimocode\memory\sessions\ses_-ffe5f6555af92ffeB7OiGqt17\
  checkpoint.md        当前任务史与决策记录
  checkpoint-*.md      早期批次与稳定的经验块
```

另外 `git log` 是可靠的第二来源：从 `6709640`（初始提交）到 `20aa93a` 共 45 笔，
每笔的提交信息都写明了动机与做法（例如 `ae45dc2`、`e570d18` 记录了本次息屏误唤醒的两个根因）。

---

## 15. 2026-10-14 词库重建的做法（再做一次时参考）

1. 从 `C:\Users\zouyu\Downloads\法律英语\教材\CHAPTER *.md` 抽英文正文（跳过含中文的行与「### 脚注」之后），
   粗词形还原，记录每个词首现章/位置 → 每章一份候选清单。
2. 两个子代理各领 7 章逐章判定（规则：法律义与日常义不同的才收基础词；六级/考研以上学术词；
   拆开失义的短语才留术语；专有名词/案例/法规名不收），给中文（≤2 义项、每项 ≤8 字）与词性。
3. 主控合并：跨章去重留最早章；按「章 → 课文首现位置」排序；`chapters` 只并入归属章**之后**的出现章
   （固件以最低位为主章节）；手工补回被拆的术语 strict scrutiny / judicial restraint / executive privilege /
   vicarious liability / contributory negligence，补回两边都漏掉的 derail、vow。
4. 两个子代理按章写例句（6–12 词、纯 ASCII、周围只用四级词），并标出可疑中文；主控修正 20 条释义。
5. 中间产物不入库（在会话 scratchpad）；入库的是最终的 `vocab_master.json` / `vocab_sentences.json`。

---

## 16. 2026-10-15 对照《元照英美法词典》

**原因**：用户考试开卷、只允许查元照，所以设备教的中文要与元照一致，并告诉用户去哪一页查。

- **词典文本**：`C:\Users\zouyu\Downloads\法律英语\词典\OvisOCR2_20261015_080409.md`（用户重新 OCR 的带页标记版；
  旧的 `元照英美法词典_OCR合并.md` 漏了约一成页面，已被用户替换）。每个 PDF 页以「###### 📄 … · 第 N 页」开头，
  **纸书印刷页码 = N − 35**（1,434 页核对一致）。词条一行一条：「词头 [词性] 〈标签〉 中文译名 释义」。
- **做法**：主控按词头精确/变形/前缀匹配出候选 → 两个子代理按章逐条判定 `yz_head`（照词典原样的查词词头）、
  `yz_page`、`yz_fit`（义项是否相符），相符的中文换成元照译名（≤2 义项）并同步例句中文 → 主控合并并校验
  每个 `yz_head` 确实是 OCR 某行行首。已知 OCR 误识：p.831 的 lessor 被识成 lesser（合并脚本里列为别名）。
  shareholder→查 stockholder、equivocate→查 equivocal 是主控补的近义词头（`yz_fit=false`）。
- **卡片显示**（`sub_line()`）：`n.  元照 p.620`；词头不同 `phr.  元照 p.1298 查 strict scrutiny test`；
  未收 `adj.  元照无`。中→英方向这行含英文词头，只在翻面后显示（原 `s_lbl_ans2` 的位置）。
- 元照有些译名偏旧（burglary 夜盗罪、living will 安乐死志愿书、promissory estoppel 允诺性不容否定），
  这是按用户「改用元照译名」的决定照搬，不要"改回通俗译法"。
- `yz_fit=false` 的 57 条保留原中文，但照样给页码（词典里只有别的义项）。
