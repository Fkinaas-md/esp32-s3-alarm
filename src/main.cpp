#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

#include "secrets.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

const int LED_PIN = LED_BUILTIN;

const char* AP_SSID = "ESP32-ALARM";
const char* AP_PASSWORD = "alarm1234";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);
Preferences prefs;

// Барнаул: UTC+7, без перехода на летнее время
const char* TZ_INFO = "CST-7";

// ============================================================
// TIMER
// ============================================================

bool timerRunning = false;
bool timerRinging = false;

unsigned long timerStartMillis = 0;
unsigned long timerDurationMillis = 0;

time_t timerEndEpoch = 0;

// ============================================================
// DAILY ALARM
// ============================================================

bool alarmEnabled = false;
bool alarmRinging = false;

int alarmHour = 7;
int alarmMinute = 30;

// bit 0 = Sunday
// bit 1 = Monday
// ...
// bit 6 = Saturday
//
// 62 = 01111110 = Monday-Friday
uint8_t alarmDays = 62;

int lastAlarmYDay = -1;
int lastAlarmYear = -1;

// ============================================================
// CLOCK / LED
// ============================================================

bool timeKnown = false;

bool ledState = false;
unsigned long lastBlinkMillis = 0;

const unsigned long BLINK_INTERVAL = 300;

// ============================================================
// HELPERS
// ============================================================

String twoDigits(int value)
{
    if (value < 10)
        return "0" + String(value);

    return String(value);
}

bool getLocalTimeSafe(struct tm& t)
{
    time_t now = time(nullptr);

    if (now < 1000000000)
        return false;

    localtime_r(&now, &t);

    return true;
}

String localTimeString()
{
    struct tm t;

    if (!getLocalTimeSafe(t))
        return "--:--:--";

    return twoDigits(t.tm_hour) + ":" +
           twoDigits(t.tm_min) + ":" +
           twoDigits(t.tm_sec);
}

// ============================================================
// ALARM STORAGE
// ============================================================

void saveAlarm()
{
    prefs.begin("alarm", false);

    prefs.putBool("enabled", alarmEnabled);
    prefs.putUChar("days", alarmDays);
    prefs.putUChar("hour", alarmHour);
    prefs.putUChar("minute", alarmMinute);

    prefs.end();
}

void loadAlarm()
{
    prefs.begin("alarm", true);

    alarmEnabled = prefs.getBool("enabled", false);
    alarmDays = prefs.getUChar("days", 62);
    alarmHour = prefs.getUChar("hour", 7);
    alarmMinute = prefs.getUChar("minute", 30);

    prefs.end();
}

// ============================================================
// TIMER STORAGE
// ============================================================

void saveTimer()
{
    prefs.begin("timer", false);

    prefs.putBool("active", timerRunning);
    prefs.putULong("duration", timerDurationMillis);
    prefs.putLong64("endEpoch", (int64_t)timerEndEpoch);

    prefs.end();
}

void clearSavedTimer()
{
    prefs.begin("timer", false);

    prefs.putBool("active", false);
    prefs.putULong("duration", 0);
    prefs.putLong64("endEpoch", 0);

    prefs.end();
}

void loadTimer()
{
    prefs.begin("timer", true);

    bool active = prefs.getBool("active", false);

    unsigned long duration =
        prefs.getULong("duration", 0);

    int64_t endEpoch =
        prefs.getLong64("endEpoch", 0);

    prefs.end();

    if (!active || duration == 0)
        return;

    timerDurationMillis = duration;
    timerEndEpoch = (time_t)endEpoch;

    timerStartMillis = millis();

    timerRunning = true;
}

// ============================================================
// RINGING
// ============================================================

void startTimerRinging()
{
    timerRinging = true;
    alarmRinging = false;

    ledState = true;

    digitalWrite(LED_PIN, HIGH);

    lastBlinkMillis = millis();
}

void startAlarmRinging()
{
    alarmRinging = true;
    timerRinging = false;

    ledState = true;

    digitalWrite(LED_PIN, HIGH);

    lastBlinkMillis = millis();
}

void stopRinging()
{
    timerRinging = false;
    alarmRinging = false;

    ledState = false;

    digitalWrite(LED_PIN, LOW);

    clearSavedTimer();
}

// ============================================================
// HTML
// ============================================================

String makePage()
{
    return R"HTML(
<!doctype html>

<html lang="ru">

<head>

<meta charset="utf-8">

<meta name="viewport"
      content="width=device-width,initial-scale=1">

<meta name="theme-color"
      content="#080808">

<title>ESP32 // ALARM</title>

<style>

* {
    box-sizing: border-box;
}

:root {
    --bg: #080808;
    --panel: #101010;
    --line: #292929;
    --line2: #444;
    --text: #eee;
    --muted: #777;
    --red: #ff3b30;
}

body {
    margin: 0;
    min-height: 100vh;

    background: var(--bg);
    color: var(--text);

    font-family:
        Consolas,
        "SFMono-Regular",
        "Roboto Mono",
        monospace;

    display: flex;
    justify-content: center;

    padding: 18px;
}

.wrap {
    width: 100%;
    max-width: 560px;
}

.top {
    display: flex;
    justify-content: space-between;

    margin: 5px 0 12px;

    color: #777;

    font-size: 10px;

    letter-spacing: 2px;
}

.dot {
    display: inline-block;

    width: 6px;
    height: 6px;

    border-radius: 50%;

    background: #aaa;

    margin-right: 7px;
}

.card {
    background: var(--panel);

    border: 1px solid var(--line);

    border-radius: 5px;

    overflow: hidden;

    box-shadow: 0 20px 60px #0008;
}

.tabs {
    display: grid;

    grid-template-columns: 1fr 1fr;

    border-bottom: 1px solid var(--line);
}

.tab {
    border: 0;

    border-right: 1px solid var(--line);

    padding: 16px;

    background: #0c0c0c;

    color: #666;

    font: 11px inherit;

    letter-spacing: 2px;

    cursor: pointer;
}

.tab:last-child {
    border-right: 0;
}

.tab.active {
    background: #151515;
    color: #fff;
}

.panel {
    display: none;
}

.panel.active {
    display: block;
}

.title {
    padding: 20px 22px;

    border-bottom: 1px solid var(--line);

    font-size: 13px;

    letter-spacing: 2px;

    font-weight: bold;
}

.title span {
    color: #666;
}

.display {
    text-align: center;

    padding: 34px 20px;

    border-bottom: 1px solid var(--line);
}

.label {
    color: #777;

    font-size: 10px;

    letter-spacing: 2px;

    text-transform: uppercase;

    margin-bottom: 10px;
}

.big {
    font-size: clamp(55px, 15vw, 82px);

    line-height: 1;

    letter-spacing: -4px;

    font-weight: 300;

    font-variant-numeric: tabular-nums;
}

.status {
    display: inline-flex;

    align-items: center;

    gap: 8px;

    margin-top: 20px;

    padding: 7px 10px;

    border: 1px solid var(--line);

    color: #777;

    font-size: 10px;

    letter-spacing: 1.5px;
}

.status .sd {
    width: 5px;
    height: 5px;

    background: #555;
}

.status.on {
    color: #eee;
}

.status.on .sd {
    background: #aaa;
}

.status.ring {
    color: #fff;

    border-color: var(--red);

    background: #ff3b3014;
}

.status.ring .sd {
    background: var(--red);
}

.controls {
    padding: 22px;
}

.fl {
    display: block;

    color: #777;

    font-size: 10px;

    letter-spacing: 2px;

    text-transform: uppercase;

    margin: 0 0 8px;
}

.row {
    display: grid;

    grid-template-columns: 1fr 1fr;

    gap: 8px;

    margin-bottom: 15px;
}

input,
select {
    width: 100%;

    padding: 14px;

    background: #0c0c0c;

    color: #eee;

    border: 1px solid var(--line);

    border-radius: 3px;

    outline: 0;

    font: 15px inherit;
}

input:focus,
select:focus {
    border-color: var(--line2);
}

button {
    width: 100%;

    padding: 14px;

    border-radius: 3px;

    font: 11px inherit;

    font-weight: bold;

    letter-spacing: 1.5px;

    cursor: pointer;
}

.primary {
    background: #fff;

    color: #000;

    border: 1px solid #fff;
}

.secondary {
    background: transparent;

    color: #888;

    border: 1px solid var(--line);

    margin-top: 8px;
}

.days {
    display: grid;

    grid-template-columns:
        repeat(7, 1fr);

    gap: 5px;

    margin-bottom: 16px;
}

.day {
    padding: 10px 2px;

    text-align: center;

    border: 1px solid var(--line);

    color: #666;

    background: #0c0c0c;

    font-size: 9px;

    cursor: pointer;
}

.day.on {
    background: #eee;

    color: #000;

    border-color: #eee;
}

.network {
    padding: 17px 22px;

    border-top: 1px solid var(--line);

    font-size: 10px;
}

.nt {
    color: #777;

    margin-bottom: 8px;

    letter-spacing: 2px;
}

.nr {
    display: flex;

    justify-content: space-between;

    padding: 4px 0;
}

.nl {
    color: #555;
}

.nv {
    color: #aaa;

    text-align: right;
}

.note {
    color: #555;

    font-size: 9px;

    line-height: 1.6;

    margin-top: 12px;
}

.footer {
    padding: 12px 22px;

    border-top: 1px solid var(--line);

    color: #444;

    font-size: 9px;

    letter-spacing: 1px;

    display: flex;

    justify-content: space-between;
}

@media(max-width:400px) {

    body {
        padding: 10px;
    }

    .controls,
    .display {
        padding-left: 17px;
        padding-right: 17px;
    }

}

</style>

</head>

<body>

<div class="wrap">

<div class="top">

<span>
ESP32 // ALARM
</span>

<span>
<i class="dot"></i>

<span id="online">
ONLINE
</span>

</span>

</div>

<div class="card">

<div class="tabs">

<button
class="tab active"
onclick="tab('timer',this)">
TIMER
</button>

<button
class="tab"
onclick="tab('alarm',this)">
ALARM
</button>

</div>

<!-- ======================================================
     TIMER
====================================================== -->

<section
id="timer"
class="panel active">

<div class="title">
TIMER
<span>/ COUNTDOWN</span>
</div>

<div class="display">

<div class="label">
remaining
</div>

<div
class="big"
id="timerDisplay">
--:--
</div>

<div
class="status"
id="timerStatus">

<span class="sd"></span>

<span id="timerText">
READY
</span>

</div>

</div>

<div class="controls">

<label class="fl">
duration
</label>

<div class="row">

<input
id="duration"
type="number"
min="1"
max="4294967"
value="30">

<select id="unit">

<option value="seconds">
SECONDS
</option>

<option value="minutes">
MINUTES
</option>

</select>

</div>

<button
class="primary"
onclick="startTimer()">
START TIMER
</button>

<button
class="secondary"
onclick="stopAlarm()">
STOP ALARM
</button>

</div>

</section>

<!-- ======================================================
     ALARM
====================================================== -->

<section
id="alarm"
class="panel">

<div class="title">

ALARM

<span>
/ BARNAUL UTC+7
</span>

</div>

<div class="display">

<div class="label">
current local time
</div>

<div
class="big"
id="clock">
--:--
</div>

<div
class="status"
id="alarmStatus">

<span class="sd"></span>

<span id="alarmText">
OFF
</span>

</div>

</div>

<div class="controls">

<label class="fl">
alarm time
</label>

<div class="row">

<input
id="alarmHour"
type="number"
min="0"
max="23"
value="07">

<input
id="alarmMinute"
type="number"
min="0"
max="59"
value="30">

</div>

<label class="fl">
repeat
</label>

<div class="days">

<div
class="day"
data-day="1"
onclick="dayClick(this)">
MON
</div>

<div
class="day"
data-day="2"
onclick="dayClick(this)">
TUE
</div>

<div
class="day"
data-day="3"
onclick="dayClick(this)">
WED
</div>

<div
class="day"
data-day="4"
onclick="dayClick(this)">
THU
</div>

<div
class="day"
data-day="5"
onclick="dayClick(this)">
FRI
</div>

<div
class="day"
data-day="6"
onclick="dayClick(this)">
SAT
</div>

<div
class="day"
data-day="0"
onclick="dayClick(this)">
SUN
</div>

</div>

<button
class="primary"
onclick="saveAlarm()">
SAVE ALARM
</button>

<button
class="secondary"
onclick="disableAlarm()">
DISABLE ALARM
</button>

</div>

</section>

<!-- ======================================================
     NETWORK
====================================================== -->

<div class="network">

<div class="nt">
NETWORK
</div>

<div class="nr">

<span class="nl">
WIFI
</span>

<span
class="nv"
id="wifi">
...
</span>

</div>

<div class="nr">

<span class="nl">
WIFI IP
</span>

<span
class="nv"
id="wifiIP">
-
</span>

</div>

<div class="nr">

<span class="nl">
ACCESS POINT
</span>

<span class="nv">
ESP32-ALARM
</span>

</div>

<div class="nr">

<span class="nl">
AP IP
</span>

<span class="nv">
192.168.4.1
</span>

</div>

</div>

<div class="footer">

<span>
BARNAUL / UTC+7
</span>

<span>
ESP32-S3
</span>

</div>

</div>

</div>

<script>

let daysMask = 62;

/*
 * ВАЖНО:
 *
 * updateStatus() больше НЕ меняет:
 *
 * alarmHour
 * alarmMinute
 * daysMask
 *
 * Поэтому пользователь может спокойно
 * редактировать форму.
 */

function tab(id, el)
{
    document
        .querySelectorAll('.panel')
        .forEach(x => x.classList.remove('active'));

    document
        .getElementById(id)
        .classList.add('active');

    document
        .querySelectorAll('.tab')
        .forEach(x => x.classList.remove('active'));

    el.classList.add('active');
}

function dayClick(el)
{
    const d = Number(el.dataset.day);

    daysMask ^= (1 << d);

    el.classList.toggle(
        'on'
    );
}

function setDays(mask)
{
    daysMask = mask;

    document
        .querySelectorAll('.day')
        .forEach(el =>
        {
            const d =
                Number(el.dataset.day);

            el.classList.toggle(
                'on',
                (mask & (1 << d)) !== 0
            );
        });
}

async function syncBrowserTime()
{
    try
    {
        await fetch(
            '/settime?epoch=' +
            Math.floor(Date.now() / 1000),
            {
                cache: 'no-store'
            }
        );
    }
    catch(e)
    {
    }
}

async function loadAlarmForm()
{
    try
    {
        const r =
            await fetch(
                '/status',
                {
                    cache: 'no-store'
                }
            );

        const d =
            await r.json();

        document
            .getElementById('alarmHour')
            .value =
            String(d.alarmHour)
            .padStart(2, '0');

        document
            .getElementById('alarmMinute')
            .value =
            String(d.alarmMinute)
            .padStart(2, '0');

        setDays(
            d.alarmDays
        );
    }
    catch(e)
    {
    }
}

async function startTimer()
{
    const value =
        Number(
            document
                .getElementById('duration')
                .value
        );

    const unit =
        document
            .getElementById('unit')
            .value;

    if (!Number.isFinite(value) ||
        value <= 0)
    {
        return;
    }

    await fetch(
        '/start?duration=' +
        encodeURIComponent(value) +
        '&unit=' +
        unit,
        {
            cache: 'no-store'
        }
    );

    updateStatus();
}

async function stopAlarm()
{
    await fetch(
        '/stop',
        {
            cache: 'no-store'
        }
    );

    updateStatus();
}

async function saveAlarm()
{
    const h =
        Number(
            document
                .getElementById('alarmHour')
                .value
        );

    const m =
        Number(
            document
                .getElementById('alarmMinute')
                .value
        );

    if (h < 0 || h > 23 ||
        m < 0 || m > 59)
    {
        return;
    }

    if (daysMask === 0)
    {
        alert(
            'Выбери хотя бы один день недели.'
        );

        return;
    }

    /*
     * Сначала передаём правильное
     * текущее время ESP32.
     */

    await syncBrowserTime();

    await fetch(
        '/alarm?enabled=1' +
        '&hour=' + h +
        '&minute=' + m +
        '&days=' + daysMask,
        {
            cache: 'no-store'
        }
    );

    updateStatus();
}

async function disableAlarm()
{
    await fetch(
        '/alarm?enabled=0',
        {
            cache: 'no-store'
        }
    );

    updateStatus();
}

function formatTimer(ms)
{
    let seconds =
        Math.ceil(
            Math.max(0, ms) / 1000
        );

    let minutes =
        Math.floor(seconds / 60);

    seconds %= 60;

    return String(minutes)
        .padStart(2, '0')
        + ':'
        +
        String(seconds)
        .padStart(2, '0');
}

async function updateStatus()
{
    try
    {
        const r =
            await fetch(
                '/status',
                {
                    cache: 'no-store'
                }
            );

        const d =
            await r.json();

        document
            .getElementById('online')
            .textContent =
            'ONLINE';

        document
            .getElementById('wifi')
            .textContent =
            d.wifiConnected
            ? 'CONNECTED'
            : 'OFFLINE';

        document
            .getElementById('wifiIP')
            .textContent =
            d.wifiIP;

        document
            .getElementById('clock')
            .textContent =
            d.localTime;

        /*
         * TIMER
         */

        if (d.timerRinging ||
            d.alarmRinging)
        {
            document
                .getElementById('timerDisplay')
                .textContent =
                '00:00';

            document
                .getElementById('timerText')
                .textContent =
                'ALARM';

            document
                .getElementById('timerStatus')
                .className =
                'status ring';
        }
        else if (d.timerRunning)
        {
            document
                .getElementById('timerDisplay')
                .textContent =
                formatTimer(
                    d.timerRemaining
                );

            document
                .getElementById('timerText')
                .textContent =
                'RUNNING';

            document
                .getElementById('timerStatus')
                .className =
                'status on';
        }
        else
        {
            document
                .getElementById('timerDisplay')
                .textContent =
                '--:--';

            document
                .getElementById('timerText')
                .textContent =
                'READY';

            document
                .getElementById('timerStatus')
                .className =
                'status';
        }

        /*
         * DAILY ALARM STATUS
         *
         * Здесь специально НЕ трогаем
         * alarmHour / alarmMinute / daysMask.
         */

        if (d.alarmRinging)
        {
            document
                .getElementById('alarmText')
                .textContent =
                'ALARM';

            document
                .getElementById('alarmStatus')
                .className =
                'status ring';
        }
        else if (d.alarmEnabled)
        {
            document
                .getElementById('alarmText')
                .textContent =
                'ON';

            document
                .getElementById('alarmStatus')
                .className =
                'status on';
        }
        else
        {
            document
                .getElementById('alarmText')
                .textContent =
                'OFF';

            document
                .getElementById('alarmStatus')
                .className =
                'status';
        }
    }
    catch(e)
    {
        document
            .getElementById('online')
            .textContent =
            'OFFLINE';
    }
}

/*
 * Загружаем сохранённые настройки
 * только один раз.
 */

syncBrowserTime()
    .finally(
        loadAlarmForm
    );

/*
 * Статус обновляем раз в секунду.
 *
 * ФОРМА ПРИ ЭТОМ НЕ ПЕРЕЗАПИСЫВАЕТСЯ.
 */

setInterval(
    updateStatus,
    1000
);

updateStatus();

</script>

</body>

</html>
)HTML";
}

// ============================================================
// HTTP: ROOT
// ============================================================

void handleRoot()
{
    server.send(
        200,
        "text/html; charset=utf-8",
        makePage()
    );
}

// ============================================================
// HTTP: SET TIME
// ============================================================

void handleSetTime()
{
    if (!server.hasArg("epoch"))
    {
        server.send(
            400,
            "text/plain",
            "missing epoch"
        );

        return;
    }

    time_t epoch =
        (time_t)strtoll(
            server.arg("epoch").c_str(),
            nullptr,
            10
        );

    if (epoch < 1000000000)
    {
        server.send(
            400,
            "text/plain",
            "invalid epoch"
        );

        return;
    }

    struct timeval tv;

    tv.tv_sec = epoch;
    tv.tv_usec = 0;

    settimeofday(
        &tv,
        nullptr
    );

    timeKnown = true;

    /*
     * Если таймер был сохранён с абсолютным
     * временем окончания — восстанавливаем его.
     */

    if (timerRunning &&
        timerEndEpoch > 1000000000)
    {
        time_t now =
            time(nullptr);

        if (now >= timerEndEpoch)
        {
            timerRunning = false;

            clearSavedTimer();

            startTimerRinging();
        }
        else
        {
            timerStartMillis =
                millis();
        }
    }

    server.send(
        200,
        "text/plain",
        "OK"
    );
}

// ============================================================
// HTTP: START TIMER
// ============================================================

void handleStartTimer()
{
    if (!server.hasArg("duration") ||
        !server.hasArg("unit"))
    {
        server.send(
            400,
            "text/plain",
            "missing parameters"
        );

        return;
    }

    uint64_t value =
        strtoull(
            server
                .arg("duration")
                .c_str(),
            nullptr,
            10
        );

    if (value == 0)
    {
        server.send(
            400,
            "text/plain",
            "invalid duration"
        );

        return;
    }

    uint64_t ms64;

    if (server.arg("unit") == "minutes")
    {
        ms64 =
            value * 60000ULL;
    }
    else
    {
        ms64 =
            value * 1000ULL;
    }

    if (ms64 > 0xFFFFFFFFULL)
    {
        ms64 = 0xFFFFFFFFULL;
    }

    timerDurationMillis =
        (unsigned long)ms64;

    timerStartMillis =
        millis();

    timerRunning = true;

    timerRinging = false;

    alarmRinging = false;

    /*
     * Если часы известны —
     * сохраняем абсолютное время окончания.
     */

    time_t now =
        time(nullptr);

    if (now >= 1000000000)
    {
        timeKnown = true;

        timerEndEpoch =
            now +
            (time_t)(ms64 / 1000ULL);
    }
    else
    {
        timerEndEpoch = 0;
    }

    ledState = false;

    digitalWrite(
        LED_PIN,
        LOW
    );

    saveTimer();

    server.send(
        200,
        "text/plain",
        "OK"
    );
}

// ============================================================
// HTTP: STOP
// ============================================================

void handleStop()
{
    timerRunning = false;

    stopRinging();

    server.send(
        200,
        "text/plain",
        "OK"
    );
}

// ============================================================
// HTTP: DAILY ALARM
// ============================================================

void handleAlarm()
{
    if (!server.hasArg("enabled"))
    {
        server.send(
            400,
            "text/plain",
            "missing enabled"
        );

        return;
    }

    bool enabled =
        server.arg("enabled") == "1";

    /*
     * DISABLE
     *
     * Время НЕ удаляем.
     */

    if (!enabled)
    {
        alarmEnabled = false;

        saveAlarm();

        server.send(
            200,
            "text/plain",
            "OK"
        );

        return;
    }

    if (!server.hasArg("hour") ||
        !server.hasArg("minute") ||
        !server.hasArg("days"))
    {
        server.send(
            400,
            "text/plain",
            "missing alarm data"
        );

        return;
    }

    alarmHour =
        constrain(
            server
                .arg("hour")
                .toInt(),
            0,
            23
        );

    alarmMinute =
        constrain(
            server
                .arg("minute")
                .toInt(),
            0,
            59
        );

    alarmDays =
        (uint8_t)
        server
            .arg("days")
            .toInt();

    if (alarmDays == 0)
    {
        server.send(
            400,
            "text/plain",
            "select at least one day"
        );

        return;
    }

    alarmEnabled = true;

    /*
     * Разрешаем будильнику сработать
     * в следующий подходящий день/минуту.
     */

    lastAlarmYDay = -1;
    lastAlarmYear = -1;

    saveAlarm();

    Serial.printf(
        "Alarm saved: %02d:%02d days=0x%02X\n",
        alarmHour,
        alarmMinute,
        alarmDays
    );

    server.send(
        200,
        "text/plain",
        "OK"
    );
}

// ============================================================
// HTTP: STATUS
// ============================================================

void handleStatus()
{
    unsigned long remaining = 0;

    if (timerRunning)
    {
        /*
         * Если есть абсолютный deadline
         * и часы известны — используем его.
         */

        if (timerEndEpoch > 1000000000 &&
            time(nullptr) >= 1000000000)
        {
            time_t now =
                time(nullptr);

            if (now < timerEndEpoch)
            {
                remaining =
                    (unsigned long)
                    (timerEndEpoch - now)
                    * 1000UL;
            }
        }
        else
        {
            /*
             * Fallback на millis().
             */

            unsigned long elapsed =
                millis() -
                timerStartMillis;

            if (elapsed <
                timerDurationMillis)
            {
                remaining =
                    timerDurationMillis -
                    elapsed;
            }
        }
    }

    String json = "{";

    json +=
        "\"timerRunning\":" +
        String(
            timerRunning
            ? "true"
            : "false"
        );

    json +=
        ",\"timerRinging\":" +
        String(
            timerRinging
            ? "true"
            : "false"
        );

    json +=
        ",\"alarmRinging\":" +
        String(
            alarmRinging
            ? "true"
            : "false"
        );

    json +=
        ",\"timerRemaining\":" +
        String(remaining);

    json +=
        ",\"alarmEnabled\":" +
        String(
            alarmEnabled
            ? "true"
            : "false"
        );

    json +=
        ",\"alarmHour\":" +
        String(alarmHour);

    json +=
        ",\"alarmMinute\":" +
        String(alarmMinute);

    json +=
        ",\"alarmDays\":" +
        String(alarmDays);

    json +=
        ",\"timeKnown\":" +
        String(
            timeKnown
            ? "true"
            : "false"
        );

    json +=
        ",\"localTime\":\"" +
        localTimeString() +
        "\"";

    json +=
        ",\"wifiConnected\":" +
        String(
            WiFi.status() == WL_CONNECTED
            ? "true"
            : "false"
        );

    json +=
        ",\"wifiIP\":\"" +
        WiFi.localIP().toString() +
        "\"";

    json += "}";

    server.send(
        200,
        "application/json",
        json
    );
}

// ============================================================
// TIMER CHECK
// ============================================================

void checkTimer()
{
    if (!timerRunning)
        return;

    bool expired = false;

    /*
     * При наличии реального времени
     * проверяем абсолютный deadline.
     */

    if (timerEndEpoch > 1000000000 &&
        time(nullptr) >= 1000000000)
    {
        expired =
            time(nullptr) >=
            timerEndEpoch;
    }
    else
    {
        /*
         * Fallback.
         */

        expired =
            (unsigned long)
            (millis() -
             timerStartMillis)
            >=
            timerDurationMillis;
    }

    if (expired)
    {
        timerRunning = false;

        clearSavedTimer();

        startTimerRinging();

        Serial.println(
            "!!! TIMER ALARM !!!"
        );
    }
}

// ============================================================
// DAILY ALARM CHECK
// ============================================================

void checkDailyAlarm()
{
    if (!alarmEnabled)
        return;

    if (alarmRinging ||
        timerRinging)
        return;

    if (!timeKnown)
        return;

    struct tm t;

    if (!getLocalTimeSafe(t))
        return;

    /*
     * tm_wday:
     *
     * 0 = Sunday
     * 1 = Monday
     * ...
     * 6 = Saturday
     */

    uint8_t bit =
        (1 << t.tm_wday);

    if ((alarmDays & bit) == 0)
        return;

    int year =
        t.tm_year + 1900;

    /*
     * Не запускаем один и тот же
     * будильник повторно в течение
     * одной даты.
     */

    if (lastAlarmYDay == t.tm_yday &&
        lastAlarmYear == year)
    {
        return;
    }

    if (t.tm_hour == alarmHour &&
        t.tm_min == alarmMinute)
    {
        lastAlarmYDay =
            t.tm_yday;

        lastAlarmYear =
            year;

        startAlarmRinging();

        Serial.printf(
            "!!! DAILY ALARM %02d:%02d !!!\n",
            alarmHour,
            alarmMinute
        );
    }
}

// ============================================================
// LED BLINK
// ============================================================

void blinkAlarm()
{
    if (!timerRinging &&
        !alarmRinging)
    {
        return;
    }

    unsigned long now =
        millis();

    if (now - lastBlinkMillis >=
        BLINK_INTERVAL)
    {
        lastBlinkMillis =
            now;

        ledState =
            !ledState;

        digitalWrite(
            LED_PIN,
            ledState
            ? HIGH
            : LOW
        );
    }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(500);

    pinMode(
        LED_PIN,
        OUTPUT
    );

    digitalWrite(
        LED_PIN,
        LOW
    );

    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        " ESP32-S3 ALARM / BARNAUL UTC+7"
    );

    Serial.println(
        "================================"
    );

    /*
     * Загружаем сохранённый будильник
     * и таймер.
     */

    loadAlarm();

    loadTimer();

    /*
     * STA + AP одновременно.
     */

    WiFi.mode(
        WIFI_AP_STA
    );

    // --------------------------------------------------------
    // ACCESS POINT
    // --------------------------------------------------------

    WiFi.softAPConfig(
        AP_IP,
        AP_GATEWAY,
        AP_SUBNET
    );

    if (WiFi.softAP(
            AP_SSID,
            AP_PASSWORD))
    {
        Serial.println(
            "AP started!"
        );

        Serial.print(
            "AP SSID: "
        );

        Serial.println(
            AP_SSID
        );

        Serial.print(
            "AP IP: "
        );

        Serial.println(
            WiFi.softAPIP()
        );
    }
    else
    {
        Serial.println(
            "ERROR: AP failed!"
        );
    }

    // --------------------------------------------------------
    // HOME WI-FI
    // --------------------------------------------------------

    Serial.print(
        "Connecting to: "
    );

    Serial.println(
        WIFI_SSID
    );

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    unsigned long wifiStart =
        millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - wifiStart < 10000)
    {
        delay(250);

        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() ==
        WL_CONNECTED)
    {
        Serial.println(
            "Wi-Fi connected!"
        );

        Serial.print(
            "Wi-Fi IP: "
        );

        Serial.println(
            WiFi.localIP()
        );

        /*
         * NTP:
         * Барнаул UTC+7.
         */

        configTzTime(
            TZ_INFO,
            "pool.ntp.org",
            "time.nist.gov",
            "time.google.com"
        );

        Serial.println(
            "NTP synchronization started."
        );
    }
    else
    {
        Serial.println(
            "Home Wi-Fi unavailable."
        );

        Serial.println(
            "Portable AP is still active."
        );
    }

    // --------------------------------------------------------
    // WEB SERVER
    // --------------------------------------------------------

    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/start",
        HTTP_GET,
        handleStartTimer
    );

    server.on(
        "/stop",
        HTTP_GET,
        handleStop
    );

    server.on(
        "/status",
        HTTP_GET,
        handleStatus
    );

    server.on(
        "/settime",
        HTTP_GET,
        handleSetTime
    );

    server.on(
        "/alarm",
        HTTP_GET,
        handleAlarm
    );

    server.begin();

    Serial.println(
        "HTTP server started."
    );

    Serial.print(
        "Portable URL: http://"
    );

    Serial.println(
        WiFi.softAPIP()
    );

    if (WiFi.status() ==
        WL_CONNECTED)
    {
        Serial.print(
            "Home URL: http://"
        );

        Serial.println(
            WiFi.localIP()
        );
    }

    Serial.println(
        "================================"
    );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    server.handleClient();

    /*
     * Проверяем часы и будильники
     * примерно раз в секунду.
     */

    static unsigned long lastSecond =
        0;

    if (millis() - lastSecond >=
        1000)
    {
        lastSecond =
            millis();

        struct tm t;

        if (getLocalTimeSafe(t))
        {
            timeKnown = true;
        }

        checkTimer();

        checkDailyAlarm();
    }

    /*
     * Мигание LED.
     */

    blinkAlarm();

    /*
     * Если домашний Wi-Fi пропал —
     * пытаемся подключиться снова.
     *
     * AP при этом продолжает работать.
     */

    static unsigned long lastReconnect =
        0;

    if (WiFi.status() != WL_CONNECTED &&
        millis() - lastReconnect >= 15000)
    {
        lastReconnect =
            millis();

        Serial.println(
            "Trying to reconnect to home Wi-Fi..."
        );

        WiFi.begin(
            WIFI_SSID,
            WIFI_PASSWORD
        );
    }
}