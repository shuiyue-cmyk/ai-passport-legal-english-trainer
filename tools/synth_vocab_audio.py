#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按 vocab_master.json 逐条合成发音，并打包成 vocabfs 分区镜像。

用法:
    python tools/synth_vocab_audio.py [--work build/audio_work] [--out dist/vocabfs.bin]

流程（全部在本机离线完成）:
  1. Windows SAPI（Microsoft Zira Desktop，en-US，与历史语料同一声线）逐条合成 16 kHz 单声道 wav；
  2. ffmpeg 转 Ogg Opus：16 kHz 单声道 8 kbps 20 ms 帧；
  3. 调 gen_vocab_audio.py 按词库顺序打包（第 i 条音频 ↔ 词库第 i 条，纯位置对应）。

缓存: 工作目录里的 manifest.json 记着“序号 → 合成文本”，文本没变的条目不重复合成，
所以词库调整后重跑只会补合成变动的那几条。打包前会核对条数与非空。
"""
import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VOICE = "Microsoft Zira Desktop"

PS_SYNTH = r"""
param([string]$List, [string]$Dir, [string]$Voice)
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$s.SelectVoice($Voice)
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000,
    [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,
    [System.Speech.AudioFormat.AudioChannel]::Mono)
foreach ($line in [System.IO.File]::ReadAllLines($List, [System.Text.Encoding]::UTF8)) {
    $parts = $line.Split("`t", 2)
    $s.SetOutputToWaveFile((Join-Path $Dir ($parts[0] + '.wav')), $fmt)
    $s.Speak($parts[1])
}
$s.SetOutputToNull()
$s.Dispose()
"""


def speech_text(en):
    """合成用文本：词条原样朗读；全大写缩写（如 IRS）加空格逐字母读。"""
    if en.isupper() and en.isalpha() and len(en) <= 6:
        return " ".join(en)
    return en


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--work", default=os.path.join(ROOT, "build", "audio_work"))
    ap.add_argument("--out", default=os.path.join(ROOT, "dist", "vocabfs.bin"))
    args = ap.parse_args()

    if not shutil.which("ffmpeg"):
        sys.exit("找不到 ffmpeg：请把 ffmpeg 加入 PATH 后重试")

    vocab = json.load(open(os.path.join(HERE, "vocab_master.json"), encoding="utf-8"))
    texts = [speech_text(v["en"]) for v in vocab]

    wav_dir = os.path.join(args.work, "wav")
    opus_dir = os.path.join(args.work, "opus")
    os.makedirs(wav_dir, exist_ok=True)
    os.makedirs(opus_dir, exist_ok=True)
    man_path = os.path.join(args.work, "manifest.json")
    manifest = json.load(open(man_path, encoding="utf-8")) if os.path.isfile(man_path) else {}

    # 词库变短时，多出来的旧 opus 会被打包脚本一并收进去 —— 先清掉。
    for f in os.listdir(opus_dir):
        stem = f.split(".")[0]
        if stem.isdigit() and int(stem) >= len(vocab):
            os.remove(os.path.join(opus_dir, f))
            manifest.pop(stem, None)

    todo = [i for i, t in enumerate(texts)
            if manifest.get("%04d" % i) != t
            or not os.path.isfile(os.path.join(opus_dir, "%04d.opus" % i))]
    print("词条 %d 条，需合成 %d 条" % (len(texts), len(todo)))

    if todo:
        list_path = os.path.join(args.work, "synth_list.tsv")
        with open(list_path, "w", encoding="utf-8") as fh:
            for i in todo:
                fh.write("%04d\t%s\n" % (i, texts[i]))
        ps_path = os.path.join(args.work, "synth.ps1")
        open(ps_path, "w", encoding="utf-8-sig").write(PS_SYNTH)
        subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
                        "-File", ps_path, "-List", list_path, "-Dir", wav_dir,
                        "-Voice", VOICE], check=True)

        def encode(i):
            src = os.path.join(wav_dir, "%04d.wav" % i)
            dst = os.path.join(opus_dir, "%04d.opus" % i)
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src,
                            "-ar", "16000", "-ac", "1", "-c:a", "libopus",
                            "-b:a", "8k", "-frame_duration", "20", dst], check=True)
            return i

        with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
            for n, i in enumerate(ex.map(encode, todo), 1):
                manifest["%04d" % i] = texts[i]
                if n % 200 == 0:
                    print("  已转码 %d / %d" % (n, len(todo)))
        json.dump(manifest, open(man_path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    missing = [i for i in range(len(texts))
               if not os.path.isfile(os.path.join(opus_dir, "%04d.opus" % i))
               or os.path.getsize(os.path.join(opus_dir, "%04d.opus" % i)) == 0]
    if missing:
        sys.exit("以下序号缺音频: %s" % missing[:20])

    subprocess.run([sys.executable, os.path.join(HERE, "gen_vocab_audio.py"),
                    opus_dir, args.out, str(len(texts))], check=True)


if __name__ == "__main__":
    main()
