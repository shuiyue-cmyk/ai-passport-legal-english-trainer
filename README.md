# Legal English Trainer · FoloToy AI Passport

**A complete, offline legal-English vocabulary trainer for the FoloToy AI Passport (ESP32-C3).**
Built for Chinese law students studying *American Law and Legal Systems* (Calvi & Coleman).

> 中文说明见 [`README.zh_CN.md`](README.zh_CN.md)

## What it does

- Turns the AI Passport into a pocket legal-English flashcard device.
- Ships with **1,679 legal terms** extracted from a 14-chapter bilingual textbook, plus
  **45 dictionary-verified supplements** from the *English-Chinese Dictionary of Anglo-American Law* (《元照英美法词典》).
- **Full offline pronunciation** for every entry (Opus, ~1.97 MB audio partition).
- **Two study directions**: EN → ZH and ZH → EN, plus mixed drills and a wrong-word list.
- **Mastery tracking + wrong-word list** saved in NVS (persists across power loss).
- **Resume**: sequential study modes pick up where you left off (fix included).
- **Reset**: a reset button at the bottom of the main menu clears all progress (with confirmation).
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
2. Build with ESP-IDF 5.5.3:

   ```bash
   idf.py build
   idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
   ```

3. Flash the merged image at `0x0` and the audio partition at `0x35a000`:

   ```bash
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
   python -m esptool --chip esp32c3 -p COM6 -b 460800 write_flash 0x35a000 dist/vocabfs.bin
   ```

4. Power on. The main menu shows: **英→中 / 中→英 / 混合练习 / 错词本 / 学习统计**, plus **重置进度** at the bottom.

See [`docs/plays/legal-english-trainer.md`](docs/plays/legal-english-trainer.md) for full
build/flash instructions, partition layout, and design notes.

## Translation review

A dictionary-driven review was performed on the vocabulary. Verified reports are included:

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
