#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>
#include <FS.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <Servo.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <driver/pcnt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <ArduinoJson.h>
#include <ArxContainer.h>
#include <unity.h>

#define private public
#include <definitions.h>
#include <OCFHelper.h>
#include <OCFCat.h>
#include <OCFFilesystem.h>
#include <OCFLock.h>
#include <OCFRFID.h>
#include <OCFStateMachine.h>
#include <OCFConfig.h>
#include <OCFFlapControl.h>
#include <OCFMQTT.h>
#include <OCFWifi.h>
#include <OCFWebserver.h>
#undef private

namespace ocf_test
{
    // Frame captured from a real implant with this board, see "debug rfid.txt".
    inline const char *CAPTURED_FRAME =
        "00000000001"
        "001101111" "010110101" "111001011" "100100111" "001001001" "100001111"
        "100000001" "000000011" "110100001" "101100001" "000000001" "000000001" "000000001";
    inline const unsigned long long CAPTURED_TAG = 900158002010860ULL;

    inline const int16_t SHORT_EDGE = 16;
    inline const int16_t LONG_EDGE = 32;

    inline void reset()
    {
        OCFLock::servoInside.detach();
        OCFLock::servoOutside.detach();
        OCFLock::initialized = false;
        OCFLock::armedInside = false;
        OCFLock::armedOutside = false;
        OCFLock::lockedInside = false;
        OCFLock::lockedOutside = false;

        OCFRFID::active_instance = nullptr;
        OCFRFID::inside_enabled = false;
        OCFRFID::outside_enabled = false;
        if (OCFRFID::tag_queue) OCFRFID::tag_queue->items.clear();
        for (OCFRFID *reader : {OCFRFID::inside, OCFRFID::outside})
        {
            reader->powered = false;
            reader->total_edges = 0;
            reader->edge_mark = 0;
            reader->last_count = 0;
            reader->frequency_hz = 0;
            reader->measured_ms = 0;
            reader->last_update_us = 0;
        }

        OCFState::cats.clear();
        OCFState::config = OCFConfig();

        OCFMQTT::connected = false;
        OCFMQTT::configured = false;
        OCFMQTT::config = MQTTConfiguration();
        OCFMQTT::mqtt_secure = WiFiClientSecure();
        OCFMQTT::outbox->items.clear();
        OCFMQTT::reconnect_requested = false;
        OCFMQTT::last_attempt_ms = 0;

        OCFWifi::configured = false;
        OCFWifi::connected = false;
        OCFWifi::config = WifiConfiguration();

        mock::reset_arduino();
        mock::reset_fs();
        mock::servo_events.clear();
        mock::servo_attach_result = true;
        mock::pcnt.clear();
        mock::pcnt_config_result = ESP_OK;
        mock::critical_depth = 0;
        mock::critical_max_depth = 0;
        mock::yields_from_isr = 0;
        mock::reset_wifi();
        mock::reset_ntp();
        mock::reset_mqtt();
        mock::reset_web_request();
        mock::reset_update();
    }

    inline std::vector<bool> bits(const char *s)
    {
        std::vector<bool> out;
        for (; *s; s++)
            if (*s == '0' || *s == '1') out.push_back(*s == '1');
        return out;
    }

    inline std::string to_string(const std::vector<bool> &b)
    {
        std::string s;
        for (bool v : b) s += v ? '1' : '0';
        return s;
    }

    inline std::vector<int16_t> encode(const std::vector<bool> &frame, int16_t short_edge = SHORT_EDGE, int16_t long_edge = LONG_EDGE)
    {
        std::vector<int16_t> edges;
        for (bool bit : frame)
        {
            if (bit)
                edges.push_back(long_edge);
            else
            {
                edges.push_back(short_edge);
                edges.push_back(short_edge);
            }
        }
        return edges;
    }

    inline int feed(int dmod_pin, pcnt_unit_t unit, const std::vector<int16_t> &edges)
    {
        int delivered = 0;
        for (int16_t count : edges)
        {
            mock::PcntUnit &u = mock::pcnt[unit];
            u.count = (u.count + count) % OCF_RFID_PCNT_LIMIT;
            if (mock::fire_interrupt(dmod_pin)) delivered++;
        }
        return delivered;
    }

    inline void run_clock(pcnt_unit_t unit, uint32_t hz, uint32_t ms, uint32_t step_ms = 10)
    {
        for (uint32_t t = 0; t < ms; t += step_ms)
        {
            mock::PcntUnit &u = mock::pcnt[unit];
            if (!u.paused) u.count = (u.count + hz * step_ms / 1000) % OCF_RFID_PCNT_LIMIT;
            mock::advance(step_ms);
            OCFRFID::process();
        }
    }

    inline int feed_inside(const std::vector<int16_t> &edges) { return feed(OCF_RFID_INSIDE_DMOD, PCNT_UNIT_1, edges); }
    inline int feed_outside(const std::vector<int16_t> &edges) { return feed(OCF_RFID_OUTSIDE_DMOD, PCNT_UNIT_0, edges); }

    inline void set_bit(std::vector<bool> &frame, int index, bool value) { frame[index] = value; }

    inline std::vector<bool> frame_with_crc(std::vector<bool> frame)
    {
        auto byte_at = [&](int block) {
            uint8_t v = 0;
            for (int p = 0; p < 8; p++)
                if (frame[11 + block * 9 + p]) v |= (1 << p);
            return v;
        };
        uint16_t crc = 0;
        for (int block = 0; block < 8; block++)
        {
            crc ^= byte_at(block);
            for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ OCF_RFID_CRC_POLY : (crc >> 1);
        }
        for (int p = 0; p < 8; p++)
        {
            frame[11 + 8 * 9 + p] = (crc >> p) & 1;
            frame[11 + 9 * 9 + p] = (crc >> (8 + p)) & 1;
        }
        return frame;
    }

    inline void set_motion(int pir_pin, bool motion) { digitalWrite(pir_pin, motion ? LOW : HIGH); }
    inline int pir_for(OCFDirection machine) { return machine == OCFDirection::INWARD ? OCF_MOTION_OUTSIDE_PIN : OCF_MOTION_INSIDE_PIN; }

    inline OCFCat make_cat(const char *name, unsigned long long rfid, bool allow_in = true, bool allow_out = true)
    {
        OCFCat cat;
        copyString(cat.name, sizeof(cat.name), name);
        cat.rfid = rfid;
        cat.allow_in = allow_in;
        cat.allow_out = allow_out;
        return cat;
    }

    inline void add_cat(const OCFCat &cat) { OCFState::cats.insert_or_assign(cat.rfid, cat); }

    inline void connect_mqtt(const char *name = "flap")
    {
        copyString(OCFMQTT::config.name, sizeof(OCFMQTT::config.name), name);
        OCFMQTT::connected = true;
    }

    inline std::vector<std::string> payloads(const std::string &topic)
    {
        OCFMQTT::publishQueued();
        std::vector<std::string> out;
        for (const auto &m : mock::published_to(topic)) out.push_back(m.payload);
        return out;
    }

    inline std::string file(const char *path)
    {
        auto it = mock::files.find(path);
        return it == mock::files.end() ? std::string() : it->second;
    }

    inline void put_file(const char *path, const std::string &content) { mock::files[path] = content; }

    inline JsonDocument parse(const std::string &json)
    {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        TEST_ASSERT_FALSE_MESSAGE(err, json.c_str());
        return doc;
    }

    inline StaticJsonDocument<OCF_MAX_JSON_SIZE> request(const char *json)
    {
        StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
        DeserializationError err = deserializeJson(doc, json);
        TEST_ASSERT_FALSE_MESSAGE(err, json);
        return doc;
    }

    inline void tick(OCFStateMachine &machine, int ticks = 1, uint32_t ms_per_tick = 10)
    {
        for (int i = 0; i < ticks; i++)
        {
            mock::advance(ms_per_tick);
            machine.update();
        }
    }
}

#define TEST_ASSERT_EQUAL_STD_STRING(expected, actual) TEST_ASSERT_EQUAL_STRING(std::string(expected).c_str(), std::string(actual).c_str())
