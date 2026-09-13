// main/main.c —— 「法律英语背单词」玩法入口
//
// 开机直接进入本玩法（不再保留 BSP 参考示例菜单）。
// 全局按键语义：OK 长按 = 返回主菜单，由 vocab_app 统一处理。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_pins.h"
#include "vocab_app.h"
#include "lvgl.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

// 按键回调运行在 button 组件的定时器任务里，操作 LVGL 必须加锁，且不得阻塞。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    vocab_app_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "法律英语背单词 启动");

    // 背诵进度存在 NVS；首次启动或分区版本变更时需要重建。
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要重建: %s", esp_err_to_name(nvs));
        nvs_flash_erase();
        nvs = nvs_flash_init();
    }
    if (nvs != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s（进度将无法保存）", esp_err_to_name(nvs));
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool btn_ok = (bsp_button_init(on_key, NULL) == ESP_OK);
    bool aud_ok = (bsp_audio_init() == ESP_OK);

    if (bsp_lvgl_lock(1000)) {
        vocab_app_start();
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "就绪: Button=%d Audio=%d", btn_ok, aud_ok);
}
