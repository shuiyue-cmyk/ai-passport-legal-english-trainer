#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把「法律英语背单词」烧录到 FoloToy AI Passport（ESP32-C3 / 8MB Flash）。

用法:
    python flash_device.py [PORT] [BAUD]

不指定 PORT 时自动探测 ESP32-C3 原生 USB（VID:PID = 303A:1001）。
镜像会在脚本所在目录及其 build/、dist/ 子目录中自动查找。

⚠ 烧录会覆盖设备上的原厂固件。本脚本会先整片备份到 backup/，
   备份失败或大小异常则立即中止，不进行任何写入。
"""
import os
import subprocess
import sys

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

APP_NAMES = ['FoloToy-AI-Passport-full.bin', '固件_FoloToy-AI-Passport-full.bin']
AUDIO_NAMES = ['vocabfs.bin', '音频分区_vocabfs.bin']
BACKUP_NAME = 'factory-backup-8MB.bin'

AUDIO_OFFSET = '0x35a000'
FLASH_SIZE = '0x800000'
ESP32C3_USB = (0x303A, 0x1001)


def find_file(names):
    for d in SEARCH_DIRS:
        for n in names:
            p = os.path.normpath(os.path.join(d, n))
            if os.path.isfile(p):
                return p
    return None


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
    port = args[0] if len(args) > 0 and args[0] else None
    baud = args[1] if len(args) > 1 else '460800'

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

    print('\n[3/4] 烧录应用固件到 0x0')
    if esptool('write_flash', '0x0', app_image) != 0:
        print('\n[错误] 应用烧录失败。')
        return 1

    print('\n[4/4] 烧录音频分区到 %s' % AUDIO_OFFSET)
    if esptool('write_flash', AUDIO_OFFSET, audio_image) != 0:
        print('\n[错误] 音频分区烧录失败。')
        return 1

    print('\n' + '=' * 60)
    print(' 完成！设备会自动重启进入玩法。')
    print('')
    print(' 回滚原厂固件（如需）：')
    print('   python -m esptool --chip esp32c3 -p %s -b %s write_flash 0x0 "%s"'
          % (port, baud, backup))
    print('=' * 60)
    return 0


if __name__ == '__main__':
    sys.exit(main())
