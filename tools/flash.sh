#!/usr/bin/env bash
# 把「法律英语背单词」烧录到 FoloToy AI Passport（ESP32-C3 / 8MB Flash）。
#
# ⚠ 烧录会覆盖设备上的原厂固件。脚本默认先整片备份到 backup/，
#    备份成功后才继续。备份文件已存在则跳过（避免重复读 8MB）。
#
# 用法:
#   tools/flash.sh [PORT] [BAUD]
# 例:
#   tools/flash.sh COM6 460800
#   tools/flash.sh            # 自动探测 ESP32-C3 原生 USB 串口
set -euo pipefail

PORT="${1:-}"
BAUD="${2:-460800}"
REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_PY="${IDF_PY:-${HOME}/.espressif/python_env/idf5.5_py3.13_env/Scripts/python.exe}"

APP_IMAGE="${REPO_ROOT}/build/FoloToy-AI-Passport-full.bin"
AUDIO_IMAGE="${REPO_ROOT}/dist/vocabfs.bin"
BACKUP="${REPO_ROOT}/backup/factory-backup-8MB.bin"

VOCABFS_OFFSET="0x35a000"
FLASH_SIZE="0x800000"

if [[ ! -x "${IDF_PY}" ]]; then
    echo "找不到 IDF Python 环境: ${IDF_PY}" >&2
    echo "请用 IDF_PY 环境变量指向 <idf_python_env>/Scripts/python.exe" >&2
    exit 1
fi

# 自动探测：ESP32-C3 原生 USB Serial/JTAG 的 VID:PID 为 303a:1001
if [[ -z "${PORT}" ]]; then
    echo "==> 未指定端口，自动探测 ESP32-C3 原生 USB ..."
    PORT="$("${IDF_PY}" - <<'PY'
import serial.tools.list_ports as lp
for p in lp.comports():
    if (p.vid, p.pid) == (0x303A, 0x1001) or "JTAG" in (p.description or ""):
        print(p.device); break
PY
)"
    if [[ -z "${PORT}" ]]; then
        echo "未找到 ESP32-C3 原生 USB 串口。请确认设备已用数据线连接。" >&2
        echo "当前可用串口：" >&2
        "${IDF_PY}" -c "import serial.tools.list_ports as lp; [print('   ',p.device,p.description) for p in lp.comports()]" >&2
        exit 1
    fi
    echo "    使用端口 ${PORT}"
fi

esptool() { "${IDF_PY}" -m esptool --chip esp32c3 -p "${PORT}" -b "${BAUD}" "$@"; }

for f in "${APP_IMAGE}" "${AUDIO_IMAGE}"; do
    if [[ ! -f "${f}" ]]; then
        echo "缺少镜像: ${f}" >&2
        echo "先运行 tools/gen_vocab_audio.py 与 idf.py build / merge-bin。" >&2
        exit 1
    fi
done

mkdir -p "${REPO_ROOT}/backup"

if [[ ! -f "${BACKUP}" ]]; then
    echo "==> 备份原厂 8MB 固件到 ${BACKUP}"
    esptool read_flash 0 "${FLASH_SIZE}" "${BACKUP}"
    echo "==> 备份完成: $(wc -c < "${BACKUP}") 字节"
else
    echo "==> 已存在备份 ${BACKUP}，跳过备份"
fi

echo "==> 烧录应用合并镜像到 0x0"
esptool write_flash 0x0 "${APP_IMAGE}"

echo "==> 烧录音频分区 vocabfs 到 ${VOCABFS_OFFSET}"
esptool write_flash "${VOCABFS_OFFSET}" "${AUDIO_IMAGE}"

echo "==> 完成。串口日志: idf.py -p ${PORT} monitor"
echo "    回滚原厂固件: esptool write_flash 0x0 ${BACKUP}"
