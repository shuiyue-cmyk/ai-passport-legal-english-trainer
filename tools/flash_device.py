#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把「法律英语背单词」烧录到 FoloToy AI Passport（ESP32-C3 / 8MB Flash）。

用法:
    python flash_device.py [PORT] [BAUD] [--wipe]

不指定 PORT 时自动探测 ESP32-C3 原生 USB（VID:PID = 303A:1001）。
镜像会在脚本所在目录及其 build/、dist/ 子目录中自动查找。

默认**保留学习进度**：合并镜像被拆成 [0x0, 0x9000) 与 [0x10000, 末尾) 两段
写入，NVS（0x9000..0xF000，进度/到期表/学习时长所在）与 phy_init
（0xF000..0x10000）的扇区完全不擦不写，设备上的学习数据原样保留。
--wipe 则按整片 0x0 写入（NVS 一并清空），用于需要干净安装的场景
（例如词条数变了，旧 prog blob 本来也会被固件的长度校验清零）。

⚠ 烧录会覆盖设备上的原厂固件。本脚本会先整片备份到 backup/，
   备份失败或大小异常则立即中止，不进行任何写入。
"""
import os
import subprocess
import sys
import tempfile
import time

try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))

# 镜像与备份的候选位置（脚本可能在工程 tools/ 下，也可能被单独拷出来）
SEARCH_DIRS = [
    HERE,
    os.path.join(HERE, 'build'),
    os.path.join(HERE, 'dist'),
    os.path.join(HERE, '..', 'build'),
    os.path.join(HERE, '..', 'dist'),
    os.path.join(HERE, '..', 'backup'),
    os.path.join(HERE, '..', '..', 'build'),
    os.path.join(HERE, '..', '..', 'dist'),
]

APP_NAMES = ['FoloToy-AI-Passport-full.bin', '固件_FoloToy-AI-Passport-full.bin',
             'merged-binary.bin']   # merge-bin 不带 -o 时的默认产物名
APP_BIN_NAME = 'FoloToy-AI-Passport.bin'    # idf.py build 的产物，用于比对镜像新旧
AUDIO_NAMES = ['vocabfs.bin', '音频分区_vocabfs.bin']
BACKUP_NAME = 'factory-backup-8MB.bin'

AUDIO_OFFSET = '0x35a000'
FLASH_SIZE = '0x800000'
ESP32C3_USB = (0x303A, 0x1001)

# partitions.csv 的受保护布局：nvs @0x9000(0x6000)、phy_init @0xF000(0x1000)、
# factory @0x10000。默认烧录跳过 [NVS_START, APP_START)，学习进度得以保留。
NVS_START = 0x9000
APP_START = 0x10000


def find_file(names):
    """同名镜像在 build/ 与 dist/ 里可能各存一份，取**最新**的那份。
    按目录顺序取第一份会烧到上一批遗留的旧镜像（曾经踩过：build/ 里躺着
    昨天的镜像，merge-bin 却输出到了 dist/）。"""
    found = []
    for d in SEARCH_DIRS:
        for n in names:
            p = os.path.normpath(os.path.join(d, n))
            if os.path.isfile(p):
                found.append(p)
    return max(found, key=os.path.getmtime) if found else None


def backup_dir():
    for d in SEARCH_DIRS:
        if os.path.basename(os.path.normpath(d)) == 'backup':
            return os.path.normpath(d)
    return os.path.normpath(os.path.join(HERE, '..', 'backup'))


def detect_port():
    try:
        import serial.tools.list_ports as lp
    except ImportError:
        print('  [警告] 未安装 pyserial，无法自动探测端口')
        return None
    for p in lp.comports():
        if (p.vid, p.pid) == ESP32C3_USB or 'JTAG' in (p.description or ''):
            return p.device
    return None


def list_ports():
    try:
        import serial.tools.list_ports as lp
        ports = list(lp.comports())
    except ImportError:
        return
    if not ports:
        print('          （系统里没有任何串口设备）')
        return
    for p in ports:
        print('            %-8s %s  (VID:PID=%s:%s)'
              % (p.device, p.description, p.vid, p.pid))


def main():
    args = sys.argv[1:]
    wipe = any(a == '--wipe' for a in args)
    pos = [a for a in args if not a.startswith('--')]
    port = pos[0] if len(pos) > 0 and pos[0] else None
    baud = pos[1] if len(pos) > 1 else '460800'

    print('=' * 60)
    print(' 法律英语背单词 · 烧录到 AI-PASSPORT')
    print('=' * 60)

    app_image = find_file(APP_NAMES)
    audio_image = find_file(AUDIO_NAMES)

    missing = []
    if not app_image:
        missing.append('应用固件（%s）' % ' 或 '.join(APP_NAMES))
    if not audio_image:
        missing.append('音频分区（%s）' % ' 或 '.join(AUDIO_NAMES))
    if missing:
        print('\n[错误] 找不到以下镜像：')
        for m in missing:
            print('         · %s' % m)
        print('\n       请把它们和本脚本放在同一目录，或放在其 build/、dist/ 子目录中。')
        return 1

    if not port:
        print('\n[1/4] 自动探测 ESP32-C3 原生 USB ...')
        port = detect_port()

    if not port:
        print('\n[错误] 未找到 AI-PASSPORT 的串口。')
        print('       请确认：')
        print('         1) 用【数据线】连接（不是纯充电线）')
        print('         2) 设备已开机')
        print('\n       当前可用串口：')
        list_ports()
        return 1

    print('      使用端口 %s' % port)
    print('      应用镜像：%s' % app_image)
    print('      音频镜像：%s' % audio_image)
    print('      镜像时间：%s' % time.strftime('%Y-%m-%d %H:%M:%S',
                                              time.localtime(os.path.getmtime(app_image))))

    app_bin = os.path.normpath(os.path.join(HERE, '..', 'build', APP_BIN_NAME))
    if os.path.isfile(app_bin) and os.path.getmtime(app_bin) > os.path.getmtime(app_image):
        print('\n      [警告] 该镜像比刚构建的 app 还旧 —— merge-bin 可能输出到了别的路径，')
        print('             烧下去的是上一批的固件。确认内容无误再继续。')
        return 1

    def esptool(*cmd):
        full = [sys.executable, '-m', 'esptool',
                '--chip', 'esp32c3', '-p', port, '-b', baud] + list(cmd)
        return subprocess.call(full)

    bdir = backup_dir()
    backup = os.path.join(bdir, BACKUP_NAME)
    os.makedirs(bdir, exist_ok=True)

    print('\n[2/4] 备份原厂 8MB 固件')
    if os.path.isfile(backup) and os.path.getsize(backup) == int(FLASH_SIZE, 16):
        print('      已存在完整备份，跳过：%s' % backup)
    else:
        print('      写入 %s（约 1 分钟，请勿拔线）' % backup)
        if esptool('read_flash', '0', FLASH_SIZE, backup) != 0:
            print('\n[错误] 备份失败，已中止，未做任何写入。')
            return 1
        size = os.path.getsize(backup)
        print('      备份完成：%d 字节' % size)
        if size != int(FLASH_SIZE, 16):
            print('      [严重] 备份大小异常（期望 %d），请勿依赖此备份回滚！'
                  % int(FLASH_SIZE, 16))
            return 1

    tmpdir = tempfile.mkdtemp(prefix='passport_flash_')
    try:
        if wipe:
            print('\n[3/4] 烧录应用固件到 0x0（--wipe：NVS 一并清空，进度会重置）')
            if esptool('write_flash', '0x0', app_image) != 0:
                print('\n[错误] 应用烧录失败。')
                return 1
        else:
            # 拆两段写，跳过 NVS+phy_init（0x9000..0x10000）：esptool 只擦写
            # 给定范围，学习进度所在的 NVS 扇区全程不被触碰。
            with open(app_image, 'rb') as f:
                data = f.read()
            if len(data) <= APP_START:
                print('\n[错误] 镜像只有 %d 字节，不含 0x%X 处的应用分区——'
                      '这不是 merge-bin 的合并镜像。' % (len(data), APP_START))
                return 1
            head = os.path.join(tmpdir, 'head-bootloader-pt.bin')
            tail = os.path.join(tmpdir, 'tail-app.bin')
            with open(head, 'wb') as f:
                f.write(data[:NVS_START])
            with open(tail, 'wb') as f:
                f.write(data[APP_START:])
            print('\n[3/4] 烧录应用固件（跳过 NVS 0x9000..0x10000，保留学习进度）')
            print('      [3a] bootloader + 分区表 → 0x0（%d 字节）' % NVS_START)
            if esptool('write_flash', '0x0', head) != 0:
                print('\n[错误] bootloader/分区表烧录失败。重跑本脚本即可续烧。')
                return 1
            print('      [3b] 应用 → 0x%X（%d 字节）' % (APP_START, len(data) - APP_START))
            if esptool('write_flash', hex(APP_START), tail) != 0:
                print('\n[错误] 应用烧录失败。重跑本脚本即可续烧（NVS 不受影响）。')
                return 1

        print('\n[4/4] 烧录音频分区到 %s' % AUDIO_OFFSET)
        if esptool('write_flash', AUDIO_OFFSET, audio_image) != 0:
            print('\n[错误] 音频分区烧录失败。')
            return 1
    finally:
        for name in os.listdir(tmpdir):
            try:
                os.remove(os.path.join(tmpdir, name))
            except OSError:
                pass
        try:
            os.rmdir(tmpdir)
        except OSError:
            pass

    print('\n' + '=' * 60)
    print(' 完成！设备会自动重启进入玩法。')
    if not wipe:
        print(' 本次未触碰 NVS，学习进度原样保留。')
    print('')
    print(' 回滚原厂固件（如需，注意会连学习进度一起清掉）：')
    print('   python -m esptool --chip esp32c3 -p %s -b %s write_flash 0x0 "%s"'
          % (port, baud, backup))
    print('=' * 60)
    return 0


if __name__ == '__main__':
    sys.exit(main())
