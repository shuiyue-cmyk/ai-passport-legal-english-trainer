# Legal English Trainer · FoloToy AI Passport

**A complete, offline legal-English vocabulary trainer for the FoloToy AI Passport (ESP32-C3).**
Built for Chinese law students studying *American Law and Legal Systems* (Calvi & Coleman).

> 中文说明见 [`README.zh_CN.md`](README.zh_CN.md)

## What it does

- Turns the AI Passport into a pocket legal-English flashcard device.
- Ships with **1,389 legal terms** extracted from a 14-chapter bilingual textbook, after
  de-duplication, dictionary supplementation, non-legal pruning and translation review.
- **Full offline pronunciation** for every entry (Opus, ~1.7 MB audio partition).
- **Two study directions**: EN → ZH and ZH → EN, plus word drills (学习单词) and a wrong-word list.
- **学习单词/wrong-word drills are 4-choice quizzes**: pick the Chinese for the English,
  drills use locked batches by primary chapter (20 per batch, shuffled within);
  a wrong answer reappears 3 words later until answered correctly.
  Last row shares don't-know (marks wrong and reveals) and play-audio.
- **Review**: correct answers enter review after 30 study minutes with 1/4
  probability (recognize → reveal → quiz; wrong answers fall back to wrong-words).
- **Textbook sentences**: 1067 entries carry the source sentence (EN+ZH),
  shown on cards and quizzes.
- **Mastery tracking + wrong-word list** saved in NVS (persists across power loss).
- **Resume**: sequential study modes pick up where you left off (fix included);
  mixed/wrong-word batches resume mid-batch too.
- **Reset**: a reset button at the bottom of the main menu clears all progress (with confirmation).
- **Battery + study time**: battery percentage in the header; total study time on the stats screen.
- **Screen sleep**: auto-sleeps after 5 idle minutes (long-press UP/DOWN in the menu sleeps
  at once); the first keypress after sleep only wakes the screen.
- **Portable UI**: 240×320 LVGL screen with CJK, IPA, and arrow glyphs correctly rendered.

## What's in this repo

| Path | Purpose |
|---|---|
| `main/` | ESP-IDF application (UI, state machine, audio playback) |
| `components/bsp/` | Board support for the FoloToy AI Passport |
| `components/opus/` | Fixed-point Opus decoder component for ESP32-C3 (no FPU) |
| `tools/` | Data generators, font tools, dictionary review, flash helpers |
| `tests/` | Host-side unit tests for the study logic |
| `docs/plays/` | Detailed play documentation (EN / 中文) |
| `dist/` | Ready-to-flash audio partition and verified translation review |
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

## Translation review

Two rounds of dictionary-driven review were performed on the vocabulary
(full-corpus baseline plus post-pruning re-review). Verified reports are included:

- `dist/translation_review_verified.md`
- `dist/translation_review_verified.json`
- `dist/translation_revisions_applied.json`

The source textbook is **not modified**; revisions live in the game vocabulary data.

## License

MIT (matches the upstream FoloToy AI Passport project license).

## Credits

- FoloToy AI Passport hardware and BSP baseline.
- Textbook corpus: *American Law and Legal Systems* (Calvi & Coleman), bilingual Markdown.
- Dictionary reference: 《元照英美法词典》, Peking University Press.
