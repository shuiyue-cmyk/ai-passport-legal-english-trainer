<p align="right">
  <strong>English</strong> · <a href="legal-english-trainer.zh_CN.md">简体中文</a>
</p>

# Legal English Trainer

Turns the AI Passport into a pocket vocabulary trainer for legal English. The
vocabulary comes from the 14-chapter bilingual textbook *American Law and Legal
Systems* (Calvi & Coleman): **2,153 entries**, all embedded in firmware, each with
**Opus pronunciation** and a **short example sentence**, working fully offline.

## Corpus

Built for students starting common law from zero, the list is **word-based**
(rebuilt 2026-10; it used to be a 1,639-entry, phrase-heavy term list):

| Kind | Count | Notes |
| --- | --- | --- |
| Legal words | 1,045 | Words with a legal meaning in the textbook; everyday words only when the legal sense differs (consideration, party, standing) |
| Academic words | 911 | General words at CET-6 / postgraduate-exam / IELTS level or above that occur in the text (promulgate, pretext, ambiguous) |
| Terms of art | 197 | Phrases whose meaning is lost when split (due process, habeas corpus, mens rea, strict scrutiny) |
| **Total** | **2,153** | |

- Basic high-school / CET-4 words used in their everyday sense (court, law, judge...)
  are left out, as are case, statute and agency names.
- **Chapters**: each word belongs to the chapter where it first appears in the English
  body text, ordered by first appearance; later chapters do not repeat it. Words per
  chapter 1-14: 284 / 332 / 179 / 261 / 115 / 151 / 166 / 58 / 77 / 93 / 128 / 170 / 108 / 31.
- **Chinese**: the sense used in this textbook (legal sense first), 1-2 senses; legal
  renderings follow the *English-Chinese Dictionary of Anglo-American Law*.
- **Examples**: one newly written 6-12 word sentence per entry (basic surrounding
  vocabulary) plus a Chinese translation, replacing the long textbook sentences.
- **No IPA**; the part of speech (n./v./adj./adv./phr.) and the 元照 lookup are shown under the word.
- **Aligned with the *English-Chinese Dictionary of Anglo-American Law* (元照)**, the only
  reference allowed in the learner's open-book exam: the 1,537 entries the dictionary lists
  with a matching sense use its Chinese rendering; the card shows where to look it up, e.g.
  `n.  元照 p.620`, naming the dictionary headword when it differs
  (`phr.  元照 p.1298 查 strict scrutiny test`); the 559 entries it lacks (mostly academic
  words) show `元照无` - they must be memorised. Page numbers are the printed book pages.

## Gameplay

Three buttons (UP / DOWN / OK), 240x320 colour screen. The header shows the
battery level (CW2017 fuel gauge) on the right.

The screen stays always on: no auto sleep and no auto power-off (the former
screen-sleep/deep-sleep machinery was removed in 2026-09). Progress is written
to NVS on every answer or page turn, so cutting power loses nothing;
disconnect power when the device is not in use to save battery.

Main menu: **学习单词** (word drills) full-width on top, **EN to ZH /
ZH to EN / Wrong-words / 复习** (review) in the middle, and **Stats** left +
**reset** right on the bottom row.

### Card view (EN to ZH, ZH to EN)

| Input | Action |
| --- | --- |
| OK click | Unflipped: reveal the answer. Flipped: next entry |
| OK double | Play pronunciation |
| OK long | Back to main menu |
| UP / DOWN click | Previous/next entry (no scoring, browsing only) |
| UP / DOWN double | Jump 10 entries |
| UP / DOWN long | Jump to the first entry of the adjacent chapter |

Every entry shows its short example sentence below the card (English + Chinese);
the English sentence always shows, the Chinese one only
after the answer is revealed (card flip, quiz grading).
Long texts bounce-scroll in a single line after a 3-second still preview;
short ones stay still. A new word auto-plays its pronunciation once
(flipping/grading does not replay).

### Quiz view (学习单词, Wrong-words, Review)

Shows the English term with 4 Chinese options (3 distractors drawn from the
whole vocabulary), plus **don't-know** and **play-audio** sharing the last row.
Move with UP/DOWN, confirm with OK. Each new question auto-plays once.

| Input | Action |
| --- | --- |
| UP / DOWN click | Move the option cursor |
| OK click | Confirm the option; any click after grading advances |
| OK long | Back to main menu |

- **学习单词** (word drills): opens a chapter picker first (UP/DOWN to choose, OK to
  start; each row shows learned/total, finished chapters turn green). Each chapter is
  split evenly into ~20-word batches in textbook order, shuffled within the batch;
  it starts from the chapter's first batch that still has unlearned words. When the
  last new word of a chapter is learned, it returns to the picker with the next
  chapter selected; finished chapters can be re-entered to drill again. Batch number plus
  within-batch progress persist in NVS (quitting mid-batch resumes where
  you left off).
- **Wrong-words**: incorrect-and-not-yet-mastered entries, 20 per group
  (a short final group still counts as one).
- A correct answer raises mastery and passes the entry; a wrong answer marks it
  wrong, highlights the correct option, and re-queues the entry 3 words later
  until answered correctly.
- **Don't-know** counts as a wrong answer (marked, revealed, re-queued);
  **play-audio** only plays pronunciation without grading.
- Correct answers in drills/wrong-words enter **review** after 30 study minutes
  with 1/4 probability (the rest graduate right away).
- Quiz screens always show the English sentence in full (bounce-scroll for long ones);
  the Chinese sentence appears after grading.

### Review

Correct answers enter review with 1/4 probability 30 study minutes later
(the rest graduate); each round takes the 20 earliest-due entries, skipping
words reviewed in the previous group (at least one group gap between two
reviews of the same word).

| Stage | Action |
| --- | --- |
| Recognize | English only (+English sentence): known reveals Chinese, OK goes to quiz; unknown skips straight to a later quiz round without revealing |
| Quiz | Same rules as drills; correct reschedules (1/4 keep, else graduate), wrong falls back to wrong-words |

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

- **Wrong-words list** = answered incorrectly in quizzes/review at least once
  and not yet mastered (cards are browsing-only and never feed it),
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
| Vocabulary data (in app) | 2,153 entries, `vocab_data.c` about 373 KB |
| CJK font (in app) | Hei 16 px subset, 1,761 characters |
| Opus audio partition `vocabfs` | 2,153 entries / 173,244 Opus packets / **about 2.6 MB** (built by `tools/synth_vocab_audio.py`) |
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
