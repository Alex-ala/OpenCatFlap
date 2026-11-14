#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03

#define D0 0
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D10 10
#define D11 11
#define D12 12
#define D13 13
#define D20 20
#define A0 30
#define A1 31
#define A2 32
#define A3 33
#define A4 34
#define A5 35
#define A6 36
#define A7 37

using std::max;
using std::min;
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

inline void mock_log(const char *, ...) __attribute__((format(printf, 1, 2)));
inline void mock_log(const char *, ...) {}
#define log_v(...) mock_log(__VA_ARGS__)
#define log_d(...) mock_log(__VA_ARGS__)
#define log_i(...) mock_log(__VA_ARGS__)
#define log_w(...) mock_log(__VA_ARGS__)
#define log_e(...) mock_log(__VA_ARGS__)

class String
{
public:
    String() {}
    String(const char *s) { if (s) s_ = s; }
    String(const String &other) = default;
    String(String &&other) = default;
    explicit String(char c) : s_(1, c) {}
    explicit String(int v) : s_(std::to_string(v)) {}
    explicit String(unsigned int v) : s_(std::to_string(v)) {}
    explicit String(long v) : s_(std::to_string(v)) {}
    explicit String(unsigned long v) : s_(std::to_string(v)) {}
    explicit String(long long v) : s_(std::to_string(v)) {}
    explicit String(unsigned long long v) : s_(std::to_string(v)) {}

    String &operator=(const String &other) = default;
    String &operator=(String &&other) = default;
    String &operator=(const char *s)
    {
        if (s) s_ = s; else s_.clear();
        return *this;
    }

    const char *c_str() const { return s_.c_str(); }
    unsigned int length() const { return s_.length(); }
    bool isEmpty() const { return s_.empty(); }
    long toInt() const { return atol(s_.c_str()); }
    char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : 0; }

    bool concat(const char *s)
    {
        if (!s) return false;
        s_ += s;
        return true;
    }
    bool concat(const String &s) { s_ += s.s_; return true; }
    bool concat(char c) { s_ += c; return true; }
    String &operator+=(const char *s) { concat(s); return *this; }
    String &operator+=(const String &s) { concat(s); return *this; }
    String &operator+=(char c) { concat(c); return *this; }

    bool operator==(const String &o) const { return s_ == o.s_; }
    bool operator==(const char *o) const { return s_ == (o ? o : ""); }
    bool operator!=(const String &o) const { return !(*this == o); }
    bool operator!=(const char *o) const { return !(*this == o); }

    const std::string &str() const { return s_; }

private:
    std::string s_;
};

class StringSumHelper : public String
{
public:
    using String::String;
    StringSumHelper(const String &s) : String(s) {}
};

inline StringSumHelper operator+(const String &a, const String &b) { String r(a); r.concat(b); return r; }
inline StringSumHelper operator+(const String &a, const char *b) { String r(a); r.concat(b); return r; }
inline StringSumHelper operator+(const char *a, const String &b) { String r(a); r.concat(b); return r; }

class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t n = 0;
        while (size--) n += write(*buffer++);
        return n;
    }
    size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
    size_t print(const char *s) { return write(s); }
    size_t print(const String &s) { return write(s.c_str()); }
    size_t println(const char *s) { return write(s) + write("\n"); }
    size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)))
    {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        if (len < 0) return 0;
        return write((const uint8_t *)buf, std::min((size_t)len, sizeof(buf) - 1));
    }
};

class Printable
{
public:
    virtual ~Printable() {}
    virtual size_t printTo(Print &p) const = 0;
};

class Stream : public Print
{
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}
    size_t readBytes(char *buffer, size_t length)
    {
        size_t count = 0;
        while (count < length)
        {
            int c = read();
            if (c < 0) break;
            *buffer++ = (char)c;
            count++;
        }
        return count;
    }
    size_t readBytes(uint8_t *buffer, size_t length) { return readBytes((char *)buffer, length); }
};

class HardwareSerial : public Stream
{
public:
    void begin(unsigned long) {}
    void setDebugOutput(bool) {}
    size_t write(uint8_t c) override { output += (char)c; return 1; }
    using Print::write;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    std::string output;
};

inline HardwareSerial Serial;

typedef void (*voidFuncPtr)(void);

namespace mock
{
    inline uint32_t now_ms = 0;
    inline std::map<int, int> pin_level;
    inline std::map<int, int> pin_mode;
    inline std::map<int, voidFuncPtr> interrupts;
    inline uint32_t free_heap = 200000;
    inline int restarts = 0;

    inline void advance(uint32_t ms) { now_ms += ms; }

    inline bool fire_interrupt(int pin)
    {
        auto it = interrupts.find(pin);
        if (it == interrupts.end() || it->second == nullptr) return false;
        it->second();
        return true;
    }

    inline void reset_arduino()
    {
        now_ms = 1000000;
        pin_level.clear();
        pin_mode.clear();
        interrupts.clear();
        restarts = 0;
        Serial.output.clear();
    }
}

inline uint32_t millis() { return mock::now_ms; }
inline uint32_t micros() { return mock::now_ms * 1000; }
inline void delay(uint32_t ms) { mock::advance(ms); }
inline void yield() {}

inline void pinMode(uint8_t pin, uint8_t mode) { mock::pin_mode[pin] = mode; }
inline void digitalWrite(uint8_t pin, uint8_t val) { mock::pin_level[pin] = val ? HIGH : LOW; }
inline int digitalRead(uint8_t pin)
{
    auto it = mock::pin_level.find(pin);
    return it == mock::pin_level.end() ? HIGH : it->second;
}
inline uint16_t analogRead(uint8_t pin) { return digitalRead(pin) ? 4095 : 0; }
inline int8_t digitalPinToGPIONumber(int8_t pin) { return pin; }

inline void attachInterrupt(uint8_t pin, voidFuncPtr fn, int) { mock::interrupts[pin] = fn; }
inline void detachInterrupt(uint8_t pin) { mock::interrupts.erase(pin); }

inline uint32_t esp_get_free_heap_size() { return mock::free_heap; }

class EspClass
{
public:
    void restart() { mock::restarts++; }
};
inline EspClass ESP;
