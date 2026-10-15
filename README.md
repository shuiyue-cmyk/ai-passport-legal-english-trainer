# Legal English Trainer · FoloToy AI Passport

**A complete, offline legal-English vocabulary trainer for the FoloToy AI Passport (ESP32-C3).**
Built for Chinese law students studying *American Law and Legal Systems* (Calvi & Coleman).

> 中文说明见 [`README.zh_CN.md`](README.zh_CN.md)

## What it does

- Turns the AI Passport into a pocket legal-English flashcard device.
- Ships with **2,153 entries, mostly single words**: words with a legal meaning in the
  textbook, academic words at CET-6 level and above, plus 197 fixed terms of art whose
  meaning is lost when split (due process, habeas corpus…). No case or statute names.
  Each word belongs to the chapter where it first appears, in textbook order.
- **Full offline pronunciation** for every entry (Opus, ~2.6 MB audio partition;
  rebuild with `tools/synth_vocab_audio.py`).
- **Two study directions**: EN → ZH and ZH → EN, plus word drills (学习单词) and a wrong-word list.
- **学习单词 starts with a chapter picker** showing learned/total per chapter; each chapter is
  split evenly into ~20-word batches in textbook order; finishing a chapter returns to the
  picker with the next chapter selected.
- **学习单词/wrong-word drills are 4-choice quizzes**: pick the Chinese for the English,
  shuffled within each batch;
  a wrong answer reappears 3 words later until answered correctly.
  Last row shares don't-know (marks wrong and reveals) and play-audio.
- **Review**: correct answers enter review after 30 study minutes with 1/4
  probability (recognize → reveal → quiz; wrong answers fall back to wrong-words).
- **元照 lookups**: Chinese follows the *Dictionary of Anglo-American Law* (元照); each card
  shows the headword and printed page to look up (e.g. `元照 p.620`), or `元照无` if it is absent.
- **Short example sentences**: every entry has a simple 6-12 word sentence (basic
  surrounding vocabulary) with a Chinese translation, shown on cards and quizzes.
- **Mastery tracking + wrong-word list** saved in NVS (persists across power loss).
- **Resume**: sequential study modes pick up where you left off (fix included);
  mixed/wrong-word batches resume mid-batch too.
- **Reset**: a reset button at the bottom of the main menu clears all progress (with confirmation).
- **Battery + study time**: battery percentage in the header; total study time on the stats screen.
- **Always-on screen**: no auto sleep or power-off; progress is written to NVS on every
  answer, so cutting power loses nothing.
- **Portable UI**: 240×320 LVGL screen with CJK and arrow glyphs correctly rendered;
  the part of speech and 元照 lookup are shown under each word (no IPA).

## What's in this repo

| Path | Purpose |
|---|---|
| `main/` | ESP-IDF application (UI, state machine, audio playback) |
| `components/bsp/` | Board support for the FoloToy AI Passport |
| `components/opus/` | Fixed-point Opus decoder component for ESP32-C3 (no FPU) |
| `tools/` | Data generators, font tools, dictionary review, flash helpers |
| `tests/` | Host-side unit tests for the study logic |
| `docs/plays/` | Detailed play documentation (EN / 中文) |
| `dist/` | Ready-to-flash full image, audio partition, and the chapter-ordered vocab CSV |
| `partitions.csv` | Protected flash layout with added `vocabfs` data partition |

## Quick start for other users

1. **Back up your factory firmware first.**
2. Build with ESP-IDF 5.5.3, then merge from inside `build/`
   (merge-bin runs in `build/`, so the relative output path from the root fails):

   ```bash
   idf.py build
   cd build && idf.py merge-bin -o FoloToy-AI-Passport-full.bin && cd ..
   ```

   Or flash in one step with the repo script (finds images, backs up first):

   ```bash
   python tools/flash_device.py COM6
   ```

3. Flash the merged image at `0x0` and the audio partition at `0x35a000`:

   ```bash
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x35a000 dist/vocabfs.bin
   ```

4. Power on. The main menu shows **学习单词** on top, **英→中 / 中→英 / 错词本 / 复习**
   in the middle, and **学习统计** left + **重置进度** right on the bottom row.

See [`docs/plays/legal-english-trainer.md`](docs/plays/legal-english-trainer.md) for full
build/flash instructions, partition layout, and design notes.

## Vocabulary list

The full list, ordered by chapter with Chinese, part of speech and example
sentences, is exported to `dist/legal_english_vocab.csv` (`python tools/export_vocab_csv.py`).
The source textbook is **not modified**.

## License

MIT (matches the upstream FoloToy AI Passport project license).

## Credits

- FoloToy AI Passport hardware and BSP baseline.
- Textbook corpus: *American Law and Legal Systems* (Calvi & Coleman), bilingual Markdown.
- Dictionary reference: 《元照英美法词典》, Peking University Press.
