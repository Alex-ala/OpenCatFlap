#pragma once

#include <cstdint>
#include <map>

#include <esp_err.h>

typedef enum
{
    PCNT_UNIT_0 = 0,
    PCNT_UNIT_1,
    PCNT_UNIT_2,
    PCNT_UNIT_3,
    PCNT_UNIT_MAX,
} pcnt_unit_t;

typedef enum
{
    PCNT_CHANNEL_0 = 0,
    PCNT_CHANNEL_1,
    PCNT_CHANNEL_MAX,
} pcnt_channel_t;

typedef enum
{
    PCNT_MODE_KEEP = 0,
    PCNT_MODE_REVERSE,
    PCNT_MODE_DISABLE,
} pcnt_ctrl_mode_t;

typedef enum
{
    PCNT_COUNT_DIS = 0,
    PCNT_COUNT_INC,
    PCNT_COUNT_DEC,
} pcnt_count_mode_t;

#define PCNT_PIN_NOT_USED (-1)

typedef struct
{
    int pulse_gpio_num;
    int ctrl_gpio_num;
    pcnt_ctrl_mode_t lctrl_mode;
    pcnt_ctrl_mode_t hctrl_mode;
    pcnt_count_mode_t pos_mode;
    pcnt_count_mode_t neg_mode;
    int16_t counter_h_lim;
    int16_t counter_l_lim;
    pcnt_unit_t unit;
    pcnt_channel_t channel;
} pcnt_config_t;

namespace mock
{
    struct PcntUnit
    {
        bool configured = false;
        pcnt_config_t config = {};
        uint16_t filter = 0;
        bool filter_enabled = false;
        bool paused = true;
        int16_t count = 0;
        int clears = 0;
    };
    inline std::map<int, PcntUnit> pcnt;
    inline esp_err_t pcnt_config_result = ESP_OK;
}

inline esp_err_t pcnt_unit_config(const pcnt_config_t *cfg)
{
    if (mock::pcnt_config_result != ESP_OK) return mock::pcnt_config_result;
    mock::PcntUnit &u = mock::pcnt[cfg->unit];
    u.configured = true;
    u.config = *cfg;
    return ESP_OK;
}
inline esp_err_t pcnt_set_filter_value(pcnt_unit_t unit, uint16_t value) { mock::pcnt[unit].filter = value; return ESP_OK; }
inline esp_err_t pcnt_filter_enable(pcnt_unit_t unit) { mock::pcnt[unit].filter_enabled = true; return ESP_OK; }
inline esp_err_t pcnt_counter_pause(pcnt_unit_t unit) { mock::pcnt[unit].paused = true; return ESP_OK; }
inline esp_err_t pcnt_counter_resume(pcnt_unit_t unit) { mock::pcnt[unit].paused = false; return ESP_OK; }
inline esp_err_t pcnt_counter_clear(pcnt_unit_t unit)
{
    mock::pcnt[unit].count = 0;
    mock::pcnt[unit].clears++;
    return ESP_OK;
}
inline esp_err_t pcnt_get_counter_value(pcnt_unit_t unit, int16_t *count)
{
    *count = mock::pcnt[unit].count;
    return ESP_OK;
}
