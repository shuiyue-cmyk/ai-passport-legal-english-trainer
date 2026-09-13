// main/vocab_audio.c —— vocabfs 分区读取 + libopus 解码 + ES8311 播放
//
// 分区格式（由 tools/gen_vocab_audio.py 生成）：
//   0x00  magic  "LVOC"            4 字节
//   0x04  version uint16 LE = 1
//   0x06  count   uint16 LE
//   0x08  reserved uint32
//   0x10  offsets[count+1]  uint32 LE   相对分区起始的字节偏移
//   ...   音频数据：第 i 条 = [offsets[i], offsets[i+1])
//         每条内部为裸包流：重复 { uint16 LE 包长, Opus 帧数据 }
//
// 解码与播放放在常驻工作任务里（bsp_audio_write 阻塞较久，不得放按键回调或 LVGL 任务）。
// Opus SILK 单帧解码栈需求较大，播放任务使用静态 16KB 栈。
#include "vocab_audio.h"
#include "vocab_data.h"
#include "bsp_audio.h"

#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "opus.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "vocab_audio";

#define VOCAB_FS_PARTITION  "vocabfs"
#define AUDIO_SAMPLE_RATE   16000
#define OPUS_FRAME_SAMPLES  960      // 最大采样数（60ms @16kHz）
#define OPUS_MAX_PACKET     1500     // 单包最大字节
#define PLAYER_STACK_BYTES  16384    // SILK 解码需要大栈

#define MAGIC0 'L'
#define MAGIC1 'V'
#define MAGIC2 'O'
#define MAGIC3 'C'
#define HDR_SIZE 0x10

static const esp_partition_t *s_part = NULL;
static uint16_t s_count = 0;
static uint32_t *s_off = NULL;       // count+1 个偏移，常驻堆（约 6.8 KB）
static bool s_ready = false;
static uint8_t s_vol = 80;

static volatile int  s_req_id = -1;
static volatile bool s_stop = false;
static volatile bool s_playing = false;
static SemaphoreHandle_t s_sem = NULL;
static TaskHandle_t      s_task = NULL;
static StackType_t       s_stack[PLAYER_STACK_BYTES / sizeof(StackType_t)];
static StaticTask_t      s_tcb;

static bool read_at(uint32_t off, void *dst, size_t len)
{
    return esp_partition_read(s_part, off, dst, len) == ESP_OK;
}

static void play_one(int id)
{
    if (id < 0 || id >= (int)s_count) return;
    uint32_t start = s_off[id];
    uint32_t end   = s_off[id + 1];
    if (end <= start) return;

    if (bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGW(TAG, "设置音频格式失败");
        return;
    }
    bsp_audio_set_volume(s_vol);

    int dec_err = 0;
    OpusDecoder *dec = opus_decoder_create(AUDIO_SAMPLE_RATE, 1, &dec_err);
    if (!dec) {
        ESP_LOGE(TAG, "opus_decoder_create 失败: %d", dec_err);
        return;
    }

    static uint8_t pkt[OPUS_MAX_PACKET];
    static int16_t pcm[OPUS_FRAME_SAMPLES];

    uint32_t p = start;
    while (p + 2 <= end && !s_stop) {
        uint8_t hdr[2];
        if (!read_at(p, hdr, 2)) break;
        p += 2;
        uint16_t plen = (uint16_t)(hdr[0] | (hdr[1] << 8));
        if (plen == 0 || plen > OPUS_MAX_PACKET || p + plen > end) {
            ESP_LOGW(TAG, "非法包长 %u (剩余 %u)", plen, (unsigned)(end - p));
            break;
        }
        if (!read_at(p, pkt, plen)) break;
        p += plen;
        if (s_stop) break;

        int nsamp = opus_decode(dec, pkt, plen, pcm, OPUS_FRAME_SAMPLES, 0);
        if (nsamp < 0) {
            ESP_LOGW(TAG, "opus_decode 错误 %d", nsamp);
            break;
        }
        bsp_audio_write(pcm, (size_t)nsamp * 2u);
    }

    opus_decoder_destroy(dec);
}

static void player_worker(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "播放任务启动 (栈 %d B)", PLAYER_STACK_BYTES);
    while (1) {
        xSemaphoreTake(s_sem, portMAX_DELAY);
        s_stop = false;
        int id = s_req_id;
        s_req_id = -1;
        if (id >= 0) {
            s_playing = true;
            play_one(id);
            s_playing = false;
        }
    }
}

esp_err_t vocab_audio_init(void)
{
    if (s_ready) return ESP_OK;

    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                      (esp_partition_subtype_t)0x40,
                                      VOCAB_FS_PARTITION);
    if (!s_part) {
        ESP_LOGE(TAG, "找不到数据分区 %s", VOCAB_FS_PARTITION);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t hdr[HDR_SIZE];
    if (!read_at(0, hdr, HDR_SIZE)) return ESP_FAIL;
    if (hdr[0] != MAGIC0 || hdr[1] != MAGIC1 || hdr[2] != MAGIC2 || hdr[3] != MAGIC3) {
        ESP_LOGE(TAG, "分区魔数不匹配（未烧录音频数据？）");
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t ver   = (uint16_t)(hdr[4] | (hdr[5] << 8));
    s_count        = (uint16_t)(hdr[6] | (hdr[7] << 8));
    if (ver != 1 || s_count == 0) {
        ESP_LOGE(TAG, "分区版本/条目数异常: ver=%u count=%u", ver, s_count);
        return ESP_ERR_INVALID_VERSION;
    }
    if (s_count != (uint16_t)VOCAB_COUNT) {
        ESP_LOGW(TAG, "分区条目数 %u 与词库 %d 不一致，按较小者处理", s_count, VOCAB_COUNT);
        if (s_count > (uint16_t)VOCAB_COUNT) s_count = (uint16_t)VOCAB_COUNT;
    }

    size_t nbytes = (size_t)(s_count + 1) * sizeof(uint32_t);
    s_off = malloc(nbytes);
    if (!s_off) {
        ESP_LOGE(TAG, "偏移表内存不足 (%u B)", (unsigned)nbytes);
        return ESP_ERR_NO_MEM;
    }
    if (!read_at(HDR_SIZE, s_off, nbytes)) {
        free(s_off); s_off = NULL;
        return ESP_FAIL;
    }

    s_sem = xSemaphoreCreateBinary();
    if (!s_sem) { free(s_off); s_off = NULL; return ESP_ERR_NO_MEM; }
    s_task = xTaskCreateStatic(player_worker, "vocab_play", PLAYER_STACK_BYTES,
                               NULL, 5, s_stack, &s_tcb);
    if (!s_task) {
        vSemaphoreDelete(s_sem); s_sem = NULL;
        free(s_off); s_off = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;
    ESP_LOGI(TAG, "音频就绪: 分区 %s, %u 条, 数据 %u 字节",
             VOCAB_FS_PARTITION, s_count, (unsigned)s_part->size);
    return ESP_OK;
}

bool vocab_audio_ready(void) { return s_ready; }

void vocab_audio_play(int id)
{
    if (!s_ready || id < 0 || id >= (int)s_count) return;
    s_req_id = id;
    s_stop = true;                 // 打断当前播放
    xSemaphoreGive(s_sem);
}

void vocab_audio_stop(void)
{
    s_stop = true;
}

bool vocab_audio_playing(void)
{
    return s_playing;
}

void vocab_audio_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_vol = percent;
    bsp_audio_set_volume(percent);
}
