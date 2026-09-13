<p align="right">
  <strong>English</strong> · <a href="legal-english-trainer.zh_CN.md">简体中文</a>
</p>

# Legal English Trainer

Turns the AI Passport into a pocket vocabulary trainer for legal English. Every
legal term marked in the 14-chapter bilingual textbook *American Law and Legal
Systems* (Calvi & Coleman) is extracted, and terms about the Anglo-American
legal system that the textbook left unmarked are filled in from the
*English-Chinese Dictionary of Anglo-American Law*. The result is **1679
entries**, all embedded in firmware, each with **Opus pronunciation**, working
fully offline.

## Corpus

| Item | Count | Notes |
| --- | --- | --- |
| Textbook-marked terms | 1634 | Marked in the Chinese paragraphs as `**中文术语**(English term)`; de-duplicated, regular plurals merged |
| Dictionary-supplemented terms | 45 | Anglo-American institutional terms present in the textbook's English body and in the dictionary, but unmarked |
| **Total** | **1679** | 1499 entries carry IPA |

7184 marked occurrences de-duplicate to 1683 entries; stripping article
differences (`the/a/an`) gives 1681; merging 47 regular plurals (all senses
preserved) gives 1634.

The 45 supplements include `Congress`, `Senate`, `House of Representatives`,
`President`, `Parliament`, `Cabinet`, `treaty`, `bureaucracy`, `federal court`,
`federal system`, `interest group`, `executive branch`, `High Court`,
`King's Bench`, `justice of the peace`, `circuit court`, `Inns of Court`,
`Associate Justice`, `prosecuting attorney`, `bail`, `sentencing`,
`Miranda rights`, `Restatement`, and others. 29 use the dictionary definition
verbatim; 16 are hand-written (no matching dictionary headword or sense).

> Deliberately excluded: `information` and `reverse` are used in the textbook
> mostly in their everyday senses ("information", "to reverse"), so annotating
> them would mislead.

## Gameplay

Three buttons (UP / DOWN / OK), 240x320 colour screen. The header shows the
battery level (CW2017 fuel gauge) on the right.

Main menu: **EN to ZH / ZH to EN / Mixed / Wrong-words / Stats**, plus a
**reset** button at the bottom.

### Card view (EN to ZH, ZH to EN)

| Input | Action |
| --- | --- |
| OK click | Unflipped: reveal the answer. Flipped: mark "known" and advance |
| OK double | Play pronunciation |
| OK long | Back to main menu |
| UP / DOWN click | Unflipped: previous/next entry. Flipped: UP marks "unknown", DOWN marks "known", both advance |
| UP / DOWN double | Jump 10 entries |
| UP / DOWN long | Jump to the first entry of the adjacent chapter |

### Quiz view (Mixed, Wrong-words)

Shows the English term with 4 Chinese options (3 distractors drawn from the
whole vocabulary). Move with UP/DOWN, confirm with OK.

| Input | Action |
| --- | --- |
| UP / DOWN click | Move the option cursor |
| OK click | Confirm the option; any click after grading advances |
| OK long | Back to main menu |

- **Mixed**: unlearned entries first, 20 per batch in vocabulary order; the next
  batch starts automatically and the batch number plus within-batch progress
  persist in NVS (quitting mid-batch resumes where you left off).
- **Wrong-words**: incorrect-and-not-yet-mastered entries, 20 per group
  (a short final group still counts as one).
- A correct answer raises mastery and passes the entry; a wrong answer marks it
  wrong, highlights the correct option, and re-queues the entry 3 words later
  until answered correctly.

### Reset

A full-width button at the bottom of the main menu. It asks for confirmation
(defaulting to "back to menu" to prevent accidents); confirming wipes all
mastery levels, wrong counters and the saved position, then writes the cleared
state back to NVS.

### Mastery and progress

One byte per entry: the low 2 bits are the mastery level (0 new / 1 started /
2 familiar / 3 mastered) and the high 4 bits count wrong answers (capped at 15).
A correct answer raises mastery by one (max 3); a wrong answer drops it to 1 and
increments the wrong counter.

- **Wrong-words list** = answered incorrectly at least once and not yet mastered,
  re-drilled as quizzes.
- Progress, last position and mixed/wrong-word batch numbers persist in NVS
  across power loss.
- Total study time (card/quiz screens) accumulates in NVS and shows on
  the stats screen.
- Sequential study modes resume from the saved entry id.

## Hardware and resource usage

| Item | Value |
| --- | --- |
| Chip | ESP32-C3, 8 MB flash, no PSRAM |
| Vocabulary data (in app) | 1389 entries, `vocab_data.c` about 126 KB |
| CJK font (in app) | Hei 16 px subset, 1051 characters |
| Opus audio partition `vocabfs` | 1389 entries / 81,790 Opus packets / **about 1.7 MB** |
| Audio coding | Opus 8 kbps, 16 kHz mono, 20 ms frames, about 850 B/s measured |
| Partition layout | `vocabfs@0x35a000` (0x4A6000) appended after the protected `cardid@0x356000` |

Audio image format (produced by `tools/gen_vocab_audio.py`):

```
0x00  magic "LVOC" (4B) | version uint16 (2B) | count uint16 (2B) | reserved (8B)
0x10  offsets[count+1] uint32 LE   byte offsets from the partition start
...   data: entry i occupies [offsets[i], offsets[i+1])
      each entry is a raw packet stream: repeated { uint16 LE packet length, Opus frame }
```

## Generating and building

```bash
# 1) Vocabulary JSON -> C data + font charset
python tools/gen_vocab_data.py

# 2) 16 px CJK font subset (do NOT pass CJK symbols via npx on Windows
#    PowerShell - it mangles UTF-8 args; use the repo script with local simhei.
#    One-time setup inside build/: cd build && npm init -y && npm i lv_font_conv@1.5.3)
node tools/makefont.js

# 3) Audio partition image
python tools/gen_vocab_audio.py <opus-dir> dist/vocabfs.bin 1679

# 4) Firmware (requires ESP-IDF 5.5.3)
idf.py build
idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
```

## Flashing

**Flashing replaces the factory firmware. Back up the whole chip first.**

```bash
PORT=COM6
# Back up the factory 8 MB image
python -m esptool --chip esp32c3 -p $PORT -b 460800 read-flash 0 0x800000 factory-backup.bin

# Application (merged image, from 0x0)
python -m esptool --chip esp32c3 -p $PORT -b 460800 write-flash 0x0 build/FoloToy-AI-Passport-full.bin

# Audio data partition
python -m esptool --chip esp32c3 -p $PORT -b 460800 write-flash 0x35a000 dist/vocabfs.bin
```

## Source layout

| File | Responsibility |
| --- | --- |
| `main/vocab_app.c` | LVGL screens and key dispatch (menu / card / reset-confirm / stats) |
| `main/vocab_model.c` | Pure mastery and session logic (no ESP-IDF dependency, host-testable) |
| `main/vocab_audio.c` | `vocabfs` partition reads + libopus decode + ES8311 playback |
| `main/vocab_data.c` | Vocabulary data (generated) |
| `components/opus/` | libopus fixed-point decoder component (ESP32-C3 has no FPU) |
| `tools/gen_vocab_data.py` | Vocabulary JSON to C data and font charset |
| `tools/gen_vocab_audio.py` | Ogg Opus to `vocabfs` raw partition image |
| `tests/test_vocab_model.c` | Host tests for mastery and session ordering |
| `tests/test_resume_logic.c` | Host test for resume-by-entry-id behaviour |

## Data provenance

- Textbook: *American Law and Legal Systems* (Calvi & Coleman), 14 chapters,
  bilingual Markdown.
- Dictionary: *English-Chinese Dictionary of Anglo-American Law* (ed. Xue Bo,
  Peking University Press, 2014), OCR text.
- Pronunciation: offline Windows SAPI (Microsoft Zira, en-US), encoded with
  ffmpeg/libopus.

## Known limits

- Pronunciation is synthetic; proper nouns (case names, Latin terms) may be
  mispronounced.
- No spaced-repetition scheduling; only mastery levels and a wrong-words list.
