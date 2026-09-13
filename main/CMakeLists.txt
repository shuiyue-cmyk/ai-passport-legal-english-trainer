idf_component_register(
    SRCS "main.c"
         "vocab_app.c"
         "vocab_model.c"
         "vocab_audio.c"
         "vocab_data.c"
         "fonts/vocab_cjk_16.c"
         "fonts/vocab_hint_14.c"
         "fonts/vocab_ipa_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl esp_timer nvs_flash opus
)

# 让 lv_font_conv 生成的字体用简单 include(<lvgl.h>)，而非带 lvgl/ 前缀的路径
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_LVGL_H_INCLUDE_SIMPLE=1)
