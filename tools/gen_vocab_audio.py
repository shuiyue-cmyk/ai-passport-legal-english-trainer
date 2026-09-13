#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把逐条 Ogg Opus 音频打包成 vocabfs 裸分区镜像。

用法:
    python gen_vocab_audio.py <audio_dir> <out_bin> [expected_count]

分区格式:
    0x00  magic "LVOC"          4B
    0x04  version uint16 LE = 1
    0x06  count   uint16 LE
    0x08  reserved uint32
    0x10  offsets[count+1] uint32 LE   相对分区起始的字节偏移
    ...   数据：第 i 条 = [offsets[i], offsets[i+1])
          每条内部为裸包流：重复 { uint16 LE 包长, Opus 帧 }

Ogg 容器解析：Opus 流的前两个包是 OpusHead / OpusTags，需跳过。
"""
import os
import re
import struct
import sys


def ogg_packets(path):
    """解析 Ogg 页面，产出 (packet_bytes) 序列。"""
    data = open(path, 'rb').read()
    pos = 0
    pending = bytearray()
    while pos + 27 <= len(data):
        if data[pos:pos + 4] != b'OggS':
            break
        nsegs = data[pos + 26]
        seg_table = data[pos + 27:pos + 27 + nsegs]
        payload = data[pos + 27 + nsegs:pos + 27 + nsegs + sum(seg_table)]
        off = 0
        for sl in seg_table:
            pending += payload[off:off + sl]
            off += sl
            if sl < 255:                 # 包边界
                yield bytes(pending)
                pending = bytearray()
        pos += 27 + nsegs + sum(seg_table)


def opus_frames(path):
    """取出该文件中的全部 Opus 音频包（跳过 OpusHead / OpusTags）。"""
    out = []
    for pkt in ogg_packets(path):
        if pkt[:8] == b'OpusHead' or pkt[:8] == b'OpusTags':
            continue
        out.append(pkt)
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    audio_dir, out_bin = sys.argv[1], sys.argv[2]
    expected = int(sys.argv[3]) if len(sys.argv) > 3 else None

    files = [f for f in os.listdir(audio_dir) if f.endswith('.opus')]
    files.sort(key=lambda f: int(re.match(r'(\d+)', f).group(1)))
    if expected is not None and len(files) != expected:
        print(f"警告: 音频文件 {len(files)} 个，与词库 {expected} 条不一致")

    blobs = []
    total_frames = 0
    for f in files:
        frames = opus_frames(os.path.join(audio_dir, f))
        if not frames:
            raise RuntimeError(f"{f}: 未解析到 Opus 包")
        buf = bytearray()
        for fr in frames:
            if len(fr) > 0xFFFF:
                raise RuntimeError(f"{f}: 单包 {len(fr)} 字节超长")
            buf += struct.pack('<H', len(fr))
            buf += fr
        blobs.append(bytes(buf))
        total_frames += len(frames)

    count = len(blobs)
    # 4B magic + 2B version + 2B count + 8B reserved = 16B
    header = b'LVOC' + struct.pack('<HH', 1, count) + b'\x00' * 8
    assert len(header) == 0x10, len(header)

    offs = [0x10 + 4 * (count + 1)]
    for b in blobs:
        offs.append(offs[-1] + len(b))

    with open(out_bin, 'wb') as fh:
        fh.write(header)
        fh.write(struct.pack('<%dI' % (count + 1), *offs))
        for b in blobs:
            fh.write(b)

    size = os.path.getsize(out_bin)
    print(f"音频条目   : {count}")
    print(f"Opus 包总数: {total_frames}")
    print(f"输出镜像   : {out_bin}")
    print(f"镜像大小   : {size} B = {size/1024/1024:.2f} MB")
    print(f"索引表     : {4*(count+1)} B")
    print(f"数据区     : {offs[-1]} B = {offs[-1]/1024/1024:.2f} MB")


if __name__ == '__main__':
    main()
