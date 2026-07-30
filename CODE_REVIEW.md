# OpenCatFlap — Code Review

Review of `src/`, `data/index.html` and `platformio.ini` as of commit `66a16ba`.
Focus: memory leaks, memory safety, concurrency/ISR correctness, antipatterns.

Findings are grouped by severity. Each entry names the exact location and a concrete fix.

---

## Severity summary

| # | Issue | Location | Severity |
|---|-------|----------|----------|
| A1 | LEDC channel leak — servos stop working after ~8 lock cycles | `OCFLock.cpp` / ServoESP32 | **Critical** — ✅ fixed |
| A2 | MQTT cert/key heap buffers leaked on every `configure()` | `OCFMQTT.cpp:29-40` | **Critical** |
| A3 | Heap overflow (off-by-one) writing the NUL terminator | `OCFFilesystem.cpp:102` + `OCFMQTT.cpp:30` | **Critical** |
| A4 | `sprintf` into `char[30]` overflows the stack frame | `OCFStateMachine.cpp:144,153` | **Critical** — ✅ fixed |
| A5 | Dereferencing `std::map::end()` on unknown RFID | `OCFStateMachine.cpp:142`, `OCFWebserver.cpp:158` | **Critical** — ✅ fixed |
| A6 | Unbounded `strcpy` from network JSON into fixed buffers | `OCFMQTT.cpp:22-27`, `OCFWebserver.cpp:163` | **Critical** |
| B1 | `std::mutex` locked inside an ISR | `OCFRFID.cpp:133` | **Critical** |
| B2 | ISRs not in IRAM; call `Serial.printf`, `log_d`, `attachInterrupt` | `OCFRFID.cpp` | **Critical** |
| C1 | `unlock(BOTH)` locks both directions | `OCFLock.cpp:91-95` | **High** |
| C2 | Missing braces → flap re-locks on every 10 ms tick | `OCFStateMachine.cpp:87-89` | **High** |
| C3 | `millis() - 10000` underflows during first 10 s after boot | `OCFStateMachine.cpp:58,73,87` | **High** |
| D1 | No authentication on any endpoint; open setup AP | `OCFWebserver.cpp` | **High** |
| A7 | File handles leaked (`getFileSize`, `writeJsonFile`) | `OCFFilesystem.cpp:80-90, 38-78` | **High** |
| B3 | `OCFState::cats` / PubSubClient shared across tasks unguarded | several | **High** |
| B4 | Busy-wait loops without yield → watchdog resets | `OCFWebserver.cpp:182`, `OCFWifi.cpp:66` | **High** |
| C4 | MQTT message flood: one publish per 10 ms tick | `OCFStateMachine.cpp:139-163` | **High** |
| — | remaining Medium/Low items below | | |

---

## A. Memory leaks and memory safety

### A1. LEDC channel leak — the flap stops being able to unlock (Critical) — ✅ FIXED

> **Fixed.** Both servos now take an explicit LEDC channel (`OCF_SERVO_IN_CHANNEL` /
> `OCF_SERVO_OUT_CHANNEL`) and are attached once from `OCFLock::init()`, called from
> `OCFFlapControl::init()`. `disableServo()` now *parks* a servo — drive it to LOCKED,
> wait `OCF_SERVO_TRAVEL_MS`, stop the pulse train, hold the signal line low — instead of
> releasing the channel. The same change also closed **C1** (`unlock(BOTH)` locking) and
> **C12** (`/debug` clamping and re-attaching). Description of the original defect follows.


`ServoESP32`'s `attach()` hands out a channel from a global counter and `detach()` only
returns it *if it was the most recently allocated one*
(`.pio/libdeps/arduino_nano_esp32/ServoESP32/src/Servo.h:132-136, 165-166`):

```cpp
_channel = channel_next_free; channel_next_free++;   // attach
if (_channel == (channel_next_free - 1)) channel_next_free--;  // detach: LIFO only
```

The two state machines attach/detach independently and therefore almost never in LIFO
order (`OCFLock::enableServo`/`disableServo`, called from
`OCFStateMachine::transitionIdleToReading` / `transitionReadingToIdle`):

```
inside attach  -> ch0
outside attach -> ch1
inside detach  -> ch0 != 1, counter stays at 2   ← channel leaked
inside attach  -> ch2 ...
```

`LEDC_CHANNELS` is `SOC_LEDC_CHANNEL_NUM` = **8** on the ESP32-S3. After roughly eight
motion cycles `attach()` returns `false`, `ledcSetup`/`ledcAttachPin` are never called,
and **both servos silently stop responding until a reboot** — the flap stays locked (or
worse, stays unlocked in whatever position it was left).

`OCFWebserver::handle_debug` (`OCFWebserver.cpp:59-63`) makes this much faster: it calls
`enableServo()` on every request and never disables, and `attach()` does not check whether
this `Servo` is already attached, so each `/debug` hit burns another channel.

**Fix:** allocate the two LEDC channels once and pass them explicitly, never re-attaching:

```cpp
// OCFLock::init(), called once from setup()
servoIn.attach(OCF_SERVO_IN_PIN,  /*channel=*/0);
servoOut.attach(OCF_SERVO_OUT_PIN,/*channel=*/1);
```

If the goal of `disableServo()` is to stop holding torque / save power, do that with
`ledcWrite(channel, 0)` (or `ledcDetachPin`/`ledcAttachPin` on a fixed channel), not by
destroying and re-allocating the channel allocation. Also note `OCFLock::init()` is
declared and implemented but **never called** from `main.cpp` — the state-machine
constructors do the attaching instead.

### A2. MQTT CA/cert/key buffers leaked on every `configure()` (Critical)

`OCFMQTT.cpp:29-40`:

```cpp
char* tmp = (char*)malloc(sizeof(char)*size);
OCFFilesystem::readStringFile(OCF_MQTT_CA_PATH, tmp, size);
config.ca = tmp;                 // previous config.ca never freed
```

Three buffers (CA ≈ 2 KB, cert ≈ 2 KB, key ≈ 3 KB ⇒ ~7 KB) are leaked every time
`configure()` runs. It runs at boot **and** on every `POST /api {"command":"mqttConfig"}`,
which is unauthenticated — so a handful of requests exhaust the heap and panic the device.

The struct makes a correct `free()` impossible as written (`OCFMQTT.h:14-16`):

```cpp
char* ca = "";   // string literal assigned to char* — freeing this is UB
```

`char* x = "literal"` is also ill-formed in C++11 and later; it only compiles because the
Arduino core relaxes it.

**Fix:** own the buffers with `String` or `std::vector<char>` / `std::unique_ptr<char[]>`,
or `free()` the old pointer and use `nullptr` (not `""`) as the "unset" value:

```cpp
struct MQTTConfiguration {
    std::string ca, cert, key;   // or String
    ...
};
```

and read into them once. That removes both the leak and the ownership ambiguity.

### A3. Off-by-one heap overflow when reading cert files (Critical)

`OCFFilesystem::readStringFile` (`OCFFilesystem.cpp:101-105`):

```cpp
bytesRead = file.readBytesUntil('\0', output, max_size);
output[bytesRead] = '\0';        // bytesRead can equal max_size → writes output[max_size]
```

Callers pass `malloc(size)` with `max_size == size` (`OCFMQTT.cpp:30-31`), so a full-size
file writes one byte past the end of the allocation — heap corruption that typically
manifests as a crash much later.

Compounding it: `writeStringFile` (`OCFFilesystem.cpp:120`) does
`file.printf("%s\0", data)` — the embedded `\0` terminates the *format string*, so no NUL
is ever written to the file. `readBytesUntil('\0')` therefore always reads to EOF, which
is exactly the `bytesRead == max_size` case.

**Fix:** allocate `size + 1`, or bound the write:

```cpp
size_t bytesRead = file.readBytes(output, max_size - 1);
output[bytesRead] = '\0';
```
and drop the bogus `"%s\0"` format (use `file.write((const uint8_t*)data, strlen(data))`).

Also: when a cert file does not exist, `getFileSize` returns `0`, `malloc(0)` returns a
non-null 0-byte block, `readStringFile` returns early without writing anything, and
`config.ca` then points at uninitialized memory that is handed straight to
`mqtt_secure.setCACert()` (`OCFMQTT.cpp:76`). Check the return value.

### A4. `sprintf` into `char[30]` overflows the stack (Critical) — ✅ FIXED

> **Fixed.** Every `sprintf` in `src/` is now `snprintf` with `sizeof(buf)`, and the two
> `checkTag()` buffers were grown to 64 bytes. The dead `sprintf` in `OCFRFID::enable()`
> (built a message into a local that was never used, from ISR context) was deleted.

`OCFStateMachine.cpp:144-146`:

```cpp
char message[30];
sprintf(message, "Unknown cat looks %s: %llu", writeDirection(direction), id);
```

`"Unknown cat looks "` is 18 chars, `+ "out"` = 21, `+ ": "` = 23, plus up to 20 digits of
a 64-bit RFID = **up to 44 bytes into a 30-byte buffer**. This is reached with any
unregistered chip — i.e. a neighbour's cat walking past overwrites the task's stack frame.

`OCFStateMachine.cpp:153-155` has the same bug (`"Cat looks %s: %s"` with a 29-char name →
up to 45 bytes). `OCFMQTT.cpp:95-96` is not overflowing today but has only 1 byte of slack.

**Fix:** use `snprintf` with `sizeof(message)` everywhere, and size the buffers for the
worst case (≥ 64 bytes here). Grep for `sprintf(` — every occurrence in this repo should
be `snprintf`.

### A5. Dereferencing `end()` on an unknown RFID (Critical, UB) — ✅ FIXED

> **Fixed** in both places by comparing against `cats.end()`. `checkTag()` also now takes
> a reference to the mapped value instead of copying it out and writing it back with
> `insert_or_assign`.

`OCFStateMachine.cpp:141-142`:

```cpp
std::map<unsigned long long, OCFCat>::iterator it = OCFState::cats.find(id);
if (it->first != id) { ... }     // it == end() when not found → UB
```

Identical bug in `OCFWebserver.cpp:156-158` (`if (it->first == id)`), reachable by POSTing
a `"cat"` command with a new RFID — i.e. on the normal "add a cat" path.

**Fix:**

```cpp
auto it = OCFState::cats.find(id);
if (it == OCFState::cats.end()) { /* unknown */ }
```

### A6. Unbounded `strcpy` from attacker-controlled JSON (Critical)

* `OCFMQTT.cpp:22-27` — `strcpy` into `config.name[20]`, `server[20]`, `user[20]`,
  `password[64]` straight from the POST body. A 200-char server name overwrites adjacent
  statics.
* `OCFWebserver.cpp:163` — `strcpy(cat.name, doc["name"])` into `OCFCat::name[30]`.
  Also crashes on `strcpy(dst, nullptr)` if `"name"` is present but not a string.
* `OCFConfig.cpp:41` — same, from the config file.

These are unauthenticated (see D1), so they are remote memory-corruption primitives.

**Fix:** `strlcpy(dst, src ? src : "", sizeof(dst))`, or switch the fields to `String` /
`std::string`. Validate that the JSON value is a string first
(`doc["name"].is<const char*>()`).

### A7. Leaked file handles and unflushed writes (High)

* `OCFFilesystem::getFileSize` (`:80-90`) **never closes the file** and returns
  `file.available()` rather than `file.size()`. Called three times per `configure()`;
  SPIFFS has a small fixed open-file table, so repeated config posts exhaust it and all
  later filesystem access fails.
* Both `writeJsonFile` overloads (`:38-78`) never call `file.close()` on any path, so data
  may not be flushed and the handle leaks.

**Fix:** close on every path — ideally with a small RAII wrapper, or at minimum a single
exit point per function.

### A8. `JsonDocument` passed by value (Medium)

`OCFFilesystem.h:18-19`:

```cpp
static bool writeJsonFile(const char *path, DynamicJsonDocument document);
static bool writeJsonFile(const char *path, StaticJsonDocument<OCF_MAX_JSON_SIZE> document);
```

Every call deep-copies the whole document (heap allocation under ArduinoJson 7) for no
reason. **Fix:** take `const JsonDocument&` — one overload replaces both.

### A9. Other memory smells

* `OCFConfig.cpp:38` — `char out[500]` allocated per loop iteration purely for a debug log,
  and `serializeJson(elem, out)` has no size bound (use the `serializeJson(elem, out, sizeof(out))`
  overload). Delete the whole debug block or guard it.
* `OCFRFID.cpp:6-7` — `new OCFRFID()` at static-init time, never deleted; all scalar members
  (`state`, `data_index`, `clk_counter`, `direction`, `machine`, pins) are uninitialized until
  `init()` runs. Use two static objects with in-class initializers instead.
* `OCFFlapControl.cpp:7-8` — raw `new` for the state machines, `deinit()` is empty, and
  `getInsideState()`/`getOutsideState()` are called by the webserver task with no null check.
* `OCFFilesystem.h:11` — `static OCFFilesystem filesystem;` is declared but never defined
  (would be a link error if used). Delete it; the class is a namespace, not an object.

---

## B. Concurrency and interrupt handling

### B1. `std::mutex` locked inside an ISR (Critical)

`OCFRFID.cpp:133`:

```cpp
void OCFRFID::onInterruptDmod() {
    ...
    std::lock_guard<std::mutex> lock(rfid_mutex);
```

`std::mutex` on ESP-IDF is a FreeRTOS mutex; taking it from interrupt context is illegal
and will abort or deadlock. A mutex is also the wrong tool here — the ISR is the only
writer.

**Fix:** delete the mutex. Protect the *task-side* reads with `portENTER_CRITICAL`/
`portEXIT_CRITICAL` (or `taskDISABLE_INTERRUPTS` for the short snapshot), and mark the
shared fields `volatile`.

### B2. ISR code lives in flash and calls non-ISR-safe functions (Critical)

* `OCFRFID::on_clk`, `on_dmod`, `onInterruptClk`, `onInterruptDmod`, `extractBits`,
  `OCFStateMachine::readRFID` are **not** marked `IRAM_ATTR`. When the flash cache is
  disabled (any SPIFFS write — e.g. a config save), an interrupt landing in flash-resident
  code causes an immediate `Cache disabled but cached memory region accessed` panic.
* `OCFRFID.cpp:175` — `Serial.printf("RFID read: %llu\n", id)` inside the ISR. Blocking
  UART at 9600 baud from an interrupt handler.
* `OCFRFID.cpp:16-25` — the antenna-switch logic calls `activate()` from inside the clock
  ISR, which calls `enable()`/`disable()` → `attachInterrupt`, `detachInterrupt`,
  `digitalWrite`, `log_d`, and a `sprintf` into a **dead local** (`OCFRFID.cpp:115-116`).
  Attaching/detaching GPIO interrupts from within an interrupt handler is not supported.

**Fix:** the ISR should do the minimum — decode a bit, and on a complete frame push the
64-bit ID into a `QueueHandle_t` with `xQueueSendFromISR`. Do the antenna switching, the
logging and the `attach/detachInterrupt` calls in `OCFFlapControl::loop` (or a dedicated
RFID task) that drains the queue. Mark whatever remains in the ISR path `IRAM_ATTR`.

### B3. Missing `volatile` / atomicity on ISR-shared state (High)

`clk_counter`, `data_index`, `state`, `expect_short`, `zero_counter`, `data[]`
(`OCFRFID.h:51-56`) and `active_instance` (`OCFRFID.h:28`) are written by the ISR and
by task context (`OCFRFID::enable()`), with no `volatile` and no critical section. The
compiler is free to cache these in registers across the loop.

`OCFStateMachine::rfidTag` is a 64-bit value written by the ISR
(`OCFStateMachine.cpp:166-169`) and read by the flap task (`updateReading`) — a **torn
read** is possible on a 32-bit core: half of one tag and half of another, producing a
plausible-but-wrong ID. Same for `lastRFID`.

**Fix:** queue-based handoff (see B2), or `volatile` + a critical section around the
64-bit read/write pair.

### B4. Shared data structures used from multiple tasks without locking (High)

`OCFState::cats` and `OCFState::config` are mutated by the **webserver** task
(`handle_api_post`, `OCFWebserver.cpp:167`) while the **flap** task iterates/looks them up
(`checkTag`, `OCFStateMachine.cpp:141-151`). A `std::map` rebalancing concurrently with a
`find()` is undefined behaviour. `OCFState::saveCats()` also runs from the webserver task
while `checkTag()` writes `last_seen` from the flap task.

`PubSubClient` is likewise used from two tasks at once: `mqttclient.loop()` in
`monitorMQTT` (`OCFMQTT.cpp:112`) and `mqttclient.publish()` from the flap task via
`OCFMQTT::sendMessage`. PubSubClient is explicitly not thread-safe — this corrupts the TX
buffer and drops the connection.

**Fix:** one `SemaphoreHandle_t` guarding `OCFState`, and a FreeRTOS queue in front of
`sendMessage()` so that only the MQTT task ever touches `mqttclient`.

### B5. Busy-wait loops starve other tasks and trip the watchdog (High)

* `OCFWebserver::loop` (`OCFWebserver.cpp:176-184`) calls `server.handleClient()` in a tight
  loop with no delay. At priority 2 this starves everything at or below it and feeds the
  task watchdog. Add `vTaskDelay(pdMS_TO_TICKS(2))`.
* `OCFWifi::connectWifi` (`OCFWifi.cpp:66-72`) spins on `WiFi.status()` for up to 10 s with
  no yield, and it runs inside `setup()`. Add a `delay(100)` in the loop.

### B6. `sleep()` used as a task delay (Medium)

`sleep()` is the POSIX whole-second call. It is used in `main.cpp:12` (a leftover
**20-second boot delay**), the `OCFStateMachine` constructor (`:10`), `OCFMQTT::configure`
(`:41`), `saveConfiguration` (`:67`) and both monitor loops. Consequences:

* `OCFMQTT::monitorMQTT` (`:110-115`) calls `mqttclient.loop()` only every **10 s**, while
  PubSubClient's default keepalive is 15 s — the connection is permanently marginal and
  inbound messages are delayed by up to 10 s.
* `sleep()` inside `configure()` blocks the webserver task for a second per config request.

**Fix:** use `vTaskDelay(pdMS_TO_TICKS(...))` with sensible values (MQTT `loop()` every
100 ms), and delete the `sleep(20)` in `setup()`.

### B7. 134 kHz GPIO interrupt as a clock counter (Medium, design)

`OCFRFID::enable` attaches a RISING interrupt to the EM4095 clock output — ~134,000
interrupts per second, each doing an indirect call plus a counter increment
(`OCFRFID.cpp:14-27`). That is a very large fraction of one core spent in interrupt
context, and it directly causes the Wi-Fi/MQTT instability the code works around
elsewhere.

**Fix:** use the ESP32 **PCNT** (pulse counter) peripheral to count clock edges in
hardware, and keep only the DMOD-change interrupt in software — read and clear the PCNT
value there. Same accuracy, ~1/1000th the CPU cost. The antenna round-robin
(`switch_counter >= 134000`, `:18`) becomes a plain 1 Hz software timer.

Related: `switch_counter` is never reset when one reader is disabled
(`OCFRFID::disable`, `:67-83`), so the first switch after re-enabling both happens at an
arbitrary time.

---

## C. Logic bugs

### C1. `unlock(BOTH)` locks both directions (High) — ✅ FIXED (with A1)

`OCFLock.cpp:91-95`:

```cpp
}else if (direction == OCFDirection::BOTH) {
    lock(OCFDirection::IN);      // should be unlock()
    lock(OCFDirection::OUT);     // should be unlock()
}
```

Copy-paste from `lock()`. Not reachable today (nothing calls `unlock(BOTH)`), but it is a
landmine.

### C2. Missing braces make the flap re-lock every tick (High)

`OCFStateMachine.cpp:87-89`:

```cpp
if (lastRFID == 0 && lastRFID <= millis() - 5000)
    log_d("%s Lock due to no tag", directionName);
    transitionUnlockedToReading();   // NOT part of the if — runs unconditionally
```

`transitionUnlockedToReading()` therefore runs on **every** `updateUnlocked()` call, i.e.
every 10 ms, so the UNLOCKED state exits immediately after it is entered. The cat gets a
~10 ms unlock window.

The condition itself is also wrong: `lastRFID == 0 && lastRFID <= millis() - 5000` reads
"lastRFID is zero AND zero is at least 5 s in the past", which is not the documented
"no valid RFID for 5 s". It should be something like:

```cpp
if (lastRFID != 0 && millis() - lastRFID >= 5000) {
    log_d("%s Lock due to no tag", directionName);
    transitionUnlockedToReading();
}
```

**Always brace conditional bodies**; enabling `-Wall -Wextra` would have caught this
(`-Wmisleading-indentation`).

### C3. Unsigned underflow in every timeout comparison (High)

`OCFStateMachine.cpp:58`, `:73`, `:79`, `:87` all use the form:

```cpp
if (lastMotion <= millis() - 10000)
```

Two problems:

1. During the first 10 s after boot, `millis() - 10000` underflows to ≈ 4.29e9, so the
   comparison is always true — the machine transitions to IDLE immediately, repeatedly,
   for the first 10 seconds of every boot.
2. `lastMotion` is `uint64_t` but `millis()` returns `unsigned long` (32-bit); the
   subtraction happens in 32-bit and is *then* widened, so the wrap is preserved rather
   than cancelled out.

**Fix:** always subtract in the same direction, with matching 32-bit types:

```cpp
uint32_t lastMotion;      // was uint64_t
if (millis() - lastMotion >= 10000) { ... }
```

This form is also immune to the 49-day `millis()` rollover.

### C4. MQTT flood: one publish per 10 ms tick (High)

`updateReading` (`:60-63`) and `updateUnlocked` (`:79-84`) call `checkTag()` whenever
`lastRFID` is within 2 s, and `checkTag()` unconditionally publishes an MQTT activity
message (`:146`, `:155`) — from a loop that runs every 10 ms. One cat read produces
~200 MQTT publishes. The `TODO` at `:140` acknowledges this.

**Fix:** publish on *transition*, not on evaluation. Split `checkTag()` into a pure
predicate and a separate "report" call made once from the transition functions, and
consume each RFID read once (clear `rfidTag` after evaluating it).

### C5. `loadCats` reads the wrong object (Medium)

`OCFConfig.cpp:45` and `:50` use `doc["last_seen"]` / `doc["location"]` inside a loop over
`elem` — the root array has no such keys, so `last_seen` and `location` are always 0/NONE
regardless of what was saved. Should be `elem["last_seen"]` / `elem["location"]`.

### C6. `setAllowState` with `both` only sets `allow_in` (Medium)

`OCFWebserver.cpp:134-138` — the `else if` chain means `BOTH` matches the first branch and
never reaches the second:

```cpp
if (direction == IN || direction == BOTH)  config.allow_in  = ...;
else if (direction == OUT || direction == BOTH) config.allow_out = ...;  // unreachable for BOTH
```

Use two independent `if`s.

### C7. Double `server.send()` on most POST paths (Medium)

`handle_api_post` sends 202/200 in the `wifiConfig`, `mqttConfig`, `setAllowState` and
`cat` branches, then falls through to `server.send(200, "text/html", "api post")` at
`:171`. Only the `status` branch returns early. Two responses on one connection confuses
clients and wastes a buffer.

**Fix:** `return` after each `server.send()`, or restructure so there is exactly one send.

### C8. String comparison via pointer equality (Medium)

`OCFMQTT.cpp:44` and `:48`:

```cpp
if (config.server == "" || config.port == 0)   // char[20] == const char* → pointer compare
if (config.ssl && config.ca == "")
```

`config.server` decays to a pointer; comparing it with a literal is always false. The
"not configured" validation therefore never triggers, and MQTT is marked configured even
with an empty server. Use `config.server[0] == '\0'` / `strlen()`.

### C9. `String` passed through varargs to `log_d` (Medium, UB)

`log_d` is `printf`-style. Passing a `String` object for `%s` is undefined behaviour.
Occurrences: `OCFWifi.cpp:30, 47, 56, 59, 69, 73, 76` (several also pass
`WiFi.localIP().toString()` — an `IPAddress`-derived temporary) and `OCFHelper.cpp:8`.

**Fix:** `.c_str()` on every one. `-Wformat` flags these if warnings are enabled.

### C10. NTP is effectively non-functional (Medium)

* `OCF_DEFAULT_NTP_SERVER` (`definitions.h:34`) is defined but never used. If the config
  has no `ntpServer`, `config.ntpServer` stays empty and
  `timeClient.setPoolServerName("")` (`OCFWifi.cpp:74`) is meaningless.
* `timeClient.update()` is called twice at connect time (`:75`, `:77`) and never again —
  `NTPClient::getEpochTime()` extrapolates from `millis()` and drifts, and after a
  reconnect it is never re-synced.
* Consequently `OCFCat::last_seen` (`checkTag`, `:150`) is meaningless when NTP fails.

**Fix:** default to `OCF_DEFAULT_NTP_SERVER`, call `timeClient.update()` periodically from
`monitorWifi`, and check `getEpochTime()` for plausibility before storing it.

### C11. `configure()` clears its caller's document (Medium)

`OCFWifi.cpp:33` and `OCFMQTT.cpp:42` call `doc.clear()` on a document they received by
reference from `handle_api_post`, which still owns it. Surprising side effect; a
configuration function should not destroy its input.

Relatedly, `OCFWifi::configure` (`:32`) writes the **entire received request** to
`WIFI_CONFIG_FILE`, including the `"command"` field — the config file is whatever the
client posted rather than a normalised structure.

### C12. `/debug` leaves the servo energised and unclamped (Medium) — ✅ FIXED (with A1)

`OCFWebserver.cpp:56-64`: `val` comes straight from a query parameter and is passed to
`Servo::write()` with no range check, and `enableServo()` is called without a matching
`disableServo()`. Beyond the LEDC leak (A1), this can drive a DSM44 into a mechanical stop
and stall it. Clamp to `[OCF_SERVO_*_LOCKED, OCF_SERVO_*_UNLOCKED]` and restore the
previous armed state afterwards.

### C13. `data/index.html`: the initial status/cat fetches always fail (Medium)

`update_cats()` and `update_status()` are invoked at `data/index.html:130` and `:147`,
inside a `<script>` in `<head>` (`:20`–`:149`), i.e. **before `<body>` is parsed**.
`document.getElementById("cats")` returns `null` and both calls throw
`TypeError: Cannot set properties of null`. The page permanently shows "Requesting...".

**Fix:** move the script to the end of `<body>`, or add `defer`, or wrap the calls in
`window.addEventListener('DOMContentLoaded', ...)`. While there, add a `setInterval` so
the status actually refreshes.

### C14. Filesystem mount failure is not handled (Low)

`OCFFilesystem::initSPIFFS` (`:5-16`) uses `SPIFFS.begin(false)` — no format-on-fail — and
only logs on failure. `readJsonFile`/`getFileSize`/`readStringFile` never check `mounted`
(only the write paths do). A first boot on a blank flash silently degrades into
"everything unconfigured" with no diagnostic.

---

## D. Security

### D1. No authentication anywhere (High)

Every endpoint is unauthenticated: `/api` (wifi config, MQTT config, allow-state, cat
management), `/api/certs` (upload of the MQTT **client private key**), `/api/cats`, and
`/debug` (direct servo control). The setup access point is open —
`WiFi.softAP("OpenCatFlap", NULL)` (`OCFWifi.cpp:41`) — so anyone in radio range can:

* unlock the flap (`setAllowState`, or `/debug` directly),
* register their own RFID chip,
* overwrite the Wi-Fi credentials and take the device off the network,
* replace the MQTT client certificate and key,
* trigger the memory-corruption bugs in A2/A4/A6.

**Fix, in rough priority order:**
1. Password-protect the setup AP (WPA2, password printed on the device or derived from the
   MAC) and time-box it — only start the AP for N minutes after boot or after a button press.
2. Require an API token / HTTP basic auth on all `/api*` and `/debug` routes
   (`WebServer::authenticate()`); generate the token on first boot and show it over serial.
3. Drop `/debug` from production builds (`#ifdef OCF_DEBUG_ENDPOINTS`).

### D2. Secrets stored and echoed in plaintext (Medium)

Wi-Fi passphrase, MQTT password, and the MQTT client private key live unencrypted in
SPIFFS. `OCFMQTT::saveConfiguration` (`:57-69`) rewrites the password on every
configuration. Nothing exposes them over HTTP today (`serveStatic` only maps
`/index.html`), but there is no wipe/factory-reset path either.

Consider NVS with flash encryption enabled for the key material, and never log
credentials. Note `CORE_DEBUG_LEVEL=5` currently prints the MQTT server/user to serial.

### D3. Everything is cleartext HTTP (Low/By design)

Config, including the Wi-Fi passphrase, is posted over plain HTTP on the LAN. Acceptable
for a hobby device but worth documenting in the README.

---

## E. Build configuration and hygiene

### E1. Unpinned dependencies; the code targets ArduinoJson 6 but resolves 7.4.2 (High)

`platformio.ini` pins only `ArxContainer` and `Dictionary`. `bblanchon/ArduinoJson`
currently resolves to **7.4.2**, while the source uses the v6 API:
`StaticJsonDocument<N>`, `DynamicJsonDocument(n)`, `doc.containsKey(...)`. In v7 these are
deprecated compatibility shims, and — importantly — **`StaticJsonDocument<N>` no longer
allocates on the stack and `N` is ignored**: `OCF_MAX_JSON_SIZE` no longer bounds anything,
so `saveCats()` will happily grow past 1500 bytes on the heap. They are removed in v8, so
the next clean checkout can fail to build.

**Fix:** pin every dependency (`bblanchon/ArduinoJson@^7.4.2`, `knolleary/PubSubClient@^2.8`,
`arduino-libraries/NTPClient@^3.2.1`, `roboticsbrno/ServoESP32@^1.1.1`), migrate to
`JsonDocument` + `doc["k"].is<T>()`, and check `doc.overflowed()` / serialization return
values after `saveCats()`.

### E2. Debug build settings shipped as the default (Medium)

`build_flags = ... -DCORE_DEBUG_LEVEL=5` with `monitor_speed = 9600`. Every `log_d` blocks
the calling task for milliseconds at 9600 baud — inside the 10 ms state-machine loop and
near the RFID ISR path this materially changes timing. Move debug logging to a
`[env:...-debug]` variant, raise the baud rate to 115200, and default the release build to
`CORE_DEBUG_LEVEL=1`.

### E3. Missing warnings (Medium)

Add `build_flags = -Wall -Wextra -Wformat -Wmisleading-indentation`. That alone surfaces
C2 (missing braces), C9 (`String` through varargs), C8 (pointer comparison), the
write-only local in `OCFRFID::enable`, and the unused `file_read` variables.

### E4. Dead code and inconsistencies (Low)

* `definitions.h` has **no include guard** (it is included from several headers).
* `braydenanderson2014/Dictionary` is a declared dependency but is not used anywhere.
* `env:esp32dev` and `env:arduino_nano_esp32` have diverged (different deps, no
  partition table, no C++17 flag) — the non-default env almost certainly no longer builds.
* Declared-but-never-defined: `OCFLock::toString` (`OCFLock.h:24`),
  `OCFFilesystem::filesystem` (`OCFFilesystem.h:11`).
* Empty stubs: `OCFRFID::printBitstring`, `OCFFlapControl::deinit`.
* Unused members: `OCFStateMachine::pin_rfid_power/clk/dmod`, `servoAttached`, `lastRead`,
  `rfidReading`; unused locals `file_read` (`OCFWifi.cpp:12`, `OCFMQTT.cpp:13`).
* Typo: `OFC_RFID_STATE` → `OCF_RFID_STATE` (`OCFRFID.h:16`).
* `handle_api_post` repeats `for (int i = 0; i <= OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0;`
  **six times**; `data_buf` is a global mutated by a single task. Replace the global with a
  local `String`/`std::array` and let scope handle the cleanup.
* Stray `debug rfid.txt` at the repo root (with a space in the filename) — move it under
  `docs/`.
* `OCFRFID::init` never sets `pinMode(pin_clk/pin_dmod, INPUT)`, relying on the default
  state; `attachInterrupt` should be passed `digitalPinToInterrupt(pin)` for portability.
* No unit tests and no CI. Even a `pio check` (cppcheck) job on push would catch several
  items in section A.

---

## Suggested order of work

1. **Stop the crashes and corruption:** ~~A5 (`end()` deref)~~ done, ~~A4 (`snprintf`)~~ done,
   A6 (`strlcpy`), A3 (off-by-one), B1 (mutex in ISR).
2. **Stop the leaks:** ~~A1 (LEDC channels)~~ done, A2 (cert buffers), A7 (file handles).
3. **Make the state machine actually work:** C2 (braces), C3 (millis underflow), C4 (MQTT
   flood), C1.
4. **Rework the ISR path** to queue-based handoff with `IRAM_ATTR` (B2/B3), then consider
   PCNT (B7).
5. **Lock down the API** (D1) and add task synchronisation (B4).
6. **Build hygiene:** pin deps + migrate to ArduinoJson 7 (E1), enable `-Wall -Wextra` (E3),
   split debug/release builds (E2).
