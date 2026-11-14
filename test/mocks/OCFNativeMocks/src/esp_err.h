#pragma once

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102

inline const char *esp_err_to_name(esp_err_t code)
{
    switch (code)
    {
    case ESP_OK: return "ESP_OK";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    default: return "ESP_FAIL";
    }
}
