#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "secrets.h"

// ============================================================
// НАСТРОЙКИ
// ============================================================

// Если встроенный LED на твоей плате находится не на GPIO 48,
// измени это значение.
#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

const int LED_PIN = LED_BUILTIN;

// Веб-сервер
WebServer server(80);

// ============================================================
// СОСТОЯНИЕ БУДИЛЬНИКА
// ============================================================

bool alarmRunning = false;
bool alarmTriggered = false;

bool ledState = false;

unsigned long alarmStartMillis = 0;
unsigned long alarmDurationMillis = 0;

unsigned long lastBlinkMillis = 0;

// Скорость мигания LED
const unsigned long BLINK_INTERVAL = 300;

// ============================================================
// HTML
// ============================================================

String makePage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">

<head>

    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1"
    >

    <meta
        name="theme-color"
        content="#0a0a0a"
    >

    <title>ESP32 // ALARM</title>

    <style>

        * {
            box-sizing: border-box;
        }

        :root {
            --bg: #0a0a0a;
            --panel: #111111;
            --panel2: #151515;

            --border: #292929;

            --text: #eeeeee;
            --muted: #777777;

            --accent: #ffffff;

            --danger: #ff3b30;
        }

        /* ====================================================
           BODY
           ==================================================== */

        body {
            margin: 0;

            min-height: 100vh;

            background: var(--bg);

            color: var(--text);

            font-family:
                "SFMono-Regular",
                "Cascadia Code",
                "Roboto Mono",
                Consolas,
                monospace;

            display: flex;

            justify-content: center;

            align-items: center;

            padding: 20px;
        }

        /* ====================================================
           CONTAINER
           ==================================================== */

        .container {
            width: 100%;

            max-width: 520px;
        }

        /* ====================================================
           HEADER
           ==================================================== */

        .header {
            display: flex;

            justify-content: space-between;

            align-items: center;

            margin-bottom: 12px;

            color: var(--muted);

            font-size: 12px;

            letter-spacing: 2px;

            text-transform: uppercase;
        }

        .online {
            display: flex;

            align-items: center;

            gap: 7px;
        }

        .dot {
            width: 6px;
            height: 6px;

            border-radius: 50%;

            background: #aaa;
        }

        /* ====================================================
           CARD
           ==================================================== */

        .card {
            background: var(--panel);

            border: 1px solid var(--border);

            border-radius: 4px;

            overflow: hidden;

            box-shadow:
                0 20px 60px rgba(0, 0, 0, 0.5);
        }

        /* ====================================================
           TITLE
           ==================================================== */

        .title {
            padding: 22px 24px;

            border-bottom: 1px solid var(--border);

            font-size: 14px;

            letter-spacing: 2px;

            font-weight: bold;
        }

        .title span {
            color: var(--muted);
        }

        /* ====================================================
           DISPLAY
           ==================================================== */

        .display {
            padding: 42px 24px 38px;

            text-align: center;

            border-bottom: 1px solid var(--border);
        }

        .display-label {
            color: var(--muted);

            font-size: 10px;

            letter-spacing: 3px;

            margin-bottom: 12px;

            text-transform: uppercase;
        }

        .time {
            font-size: clamp(52px, 15vw, 82px);

            line-height: 1;

            font-weight: 300;

            letter-spacing: -4px;

            font-variant-numeric: tabular-nums;
        }

        /* ====================================================
           STATUS
           ==================================================== */

        .status {
            margin-top: 22px;

            display: inline-flex;

            align-items: center;

            gap: 9px;

            padding: 7px 11px;

            border: 1px solid var(--border);

            color: var(--muted);

            font-size: 10px;

            letter-spacing: 1.5px;

            text-transform: uppercase;
        }

        .status-dot {
            width: 5px;
            height: 5px;

            background: #555;
        }

        .status.active {
            color: var(--text);
        }

        .status.active .status-dot {
            background: #aaa;
        }

        /* ====================================================
           ALARM STATUS
           ==================================================== */

        .status.alarm {
            color: #fff;

            border-color: var(--danger);

            background: rgba(255, 59, 48, 0.08);

            animation: alarmPulse 0.8s infinite alternate;
        }

        .status.alarm .status-dot {
            background: var(--danger);
        }

        @keyframes alarmPulse {

            from {
                opacity: 1;
            }

            to {
                opacity: 0.45;
            }

        }

        /* ====================================================
           CONTROLS
           ==================================================== */

        .controls {
            padding: 24px;
        }

        .label {
            display: block;

            margin-bottom: 8px;

            color: var(--muted);

            font-size: 10px;

            letter-spacing: 2px;

            text-transform: uppercase;
        }

        .row {
            display: grid;

            grid-template-columns: 1fr 1fr;

            gap: 8px;

            margin-bottom: 16px;
        }

        input,
        select {

            width: 100%;

            padding: 14px 15px;

            background: #0c0c0c;

            border: 1px solid var(--border);

            border-radius: 3px;

            outline: none;

            color: var(--text);

            font-family: inherit;

            font-size: 15px;

            transition:
                border-color 0.15s,
                background 0.15s;
        }

        input:focus,
        select:focus {

            border-color: #555;

            background: #101010;
        }

        select {
            cursor: pointer;
        }

        /* ====================================================
           BUTTONS
           ==================================================== */

        button {

            width: 100%;

            padding: 15px;

            border-radius: 3px;

            font-family: inherit;

            font-size: 12px;

            font-weight: bold;

            letter-spacing: 1.5px;

            text-transform: uppercase;

            cursor: pointer;

            transition:
                background 0.15s,
                border-color 0.15s,
                transform 0.08s;
        }

        button:active {
            transform: translateY(1px);
        }

        /* START */

        .start {

            background: var(--accent);

            color: #000;

            border: 1px solid var(--accent);
        }

        .start:hover {
            background: #dcdcdc;
        }

        /* STOP */

        .stop {

            margin-top: 8px;

            background: transparent;

            color: #888;

            border: 1px solid var(--border);
        }

        .stop:hover {

            color: #fff;

            border-color: #555;

            background: #171717;
        }

        /* ====================================================
           FOOTER
           ==================================================== */

        .footer {

            padding: 13px 24px;

            border-top: 1px solid var(--border);

            color: #444;

            font-size: 9px;

            letter-spacing: 1px;

            display: flex;

            justify-content: space-between;
        }

        /* ====================================================
           MOBILE
           ==================================================== */

        @media (max-width: 400px) {

            body {
                padding: 12px;
            }

            .controls,
            .display {

                padding-left: 18px;

                padding-right: 18px;
            }

            .time {
                font-size: 55px;
            }
        }

    </style>

</head>

<body>

<div class="container">

    <!-- ====================================================
         HEADER
         ==================================================== -->

    <div class="header">

        <div>
            ESP32 // ALARM
        </div>

        <div class="online">

            <span class="dot"></span>

            ONLINE

        </div>

    </div>


    <!-- ====================================================
         CARD
         ==================================================== -->

    <div class="card">


        <!-- TITLE -->

        <div class="title">

            TIMER
            <span>/ LOCAL</span>

        </div>


        <!-- ==================================================
             DISPLAY
             ================================================== -->

        <div class="display">

            <div class="display-label">

                remaining

            </div>


            <div
                class="time"
                id="timer"
            >
                --:--
            </div>


            <div
                class="status"
                id="status"
            >

                <span class="status-dot"></span>

                <span id="statusText">
                    READY
                </span>

            </div>

        </div>


        <!-- ==================================================
             CONTROLS
             ================================================== -->

        <div class="controls">

            <label class="label">

                duration

            </label>


            <div class="row">

                <input
                    id="duration"
                    type="number"
                    min="1"
                    value="30"
                    placeholder="30"
                >


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
                class="start"
                onclick="startAlarm()"
            >

                START TIMER

            </button>


            <button
                class="stop"
                onclick="stopAlarm()"
            >

                STOP ALARM

            </button>

        </div>


        <!-- ==================================================
             FOOTER
             ================================================== -->

        <div class="footer">

            <span>
                LOCAL NETWORK
            </span>

            <span>
                ESP32-S3
            </span>

        </div>

    </div>

</div>


<script>

    // ========================================================
    // START
    // ========================================================

    async function startAlarm()
    {
        const duration =
            document.getElementById(
                "duration"
            ).value;

        const unit =
            document.getElementById(
                "unit"
            ).value;


        if (!duration || duration <= 0)
        {
            alert("Enter duration");

            return;
        }


        try
        {
            await fetch(
                "/start?duration=" +
                encodeURIComponent(duration) +
                "&unit=" +
                encodeURIComponent(unit)
            );

            updateStatus();
        }

        catch (error)
        {
            console.log(error);
        }
    }


    // ========================================================
    // STOP
    // ========================================================

    async function stopAlarm()
    {
        try
        {
            await fetch("/stop");

            updateStatus();
        }

        catch (error)
        {
            console.log(error);
        }
    }


    // ========================================================
    // STATUS
    // ========================================================

    async function updateStatus()
    {
        try
        {
            const response =
                await fetch("/status");


            const data =
                await response.json();


            const timer =
                document.getElementById(
                    "timer"
                );


            const status =
                document.getElementById(
                    "status"
                );


            const statusText =
                document.getElementById(
                    "statusText"
                );


            // =================================================
            // ALARM
            // =================================================

            if (data.triggered)
            {
                timer.innerText = "00:00";

                statusText.innerText =
                    "ALARM";

                status.classList.remove(
                    "active"
                );

                status.classList.add(
                    "alarm"
                );
            }


            // =================================================
            // TIMER RUNNING
            // =================================================

            else if (data.running)
            {
                let seconds =
                    Math.ceil(
                        data.remaining / 1000
                    );


                let minutes =
                    Math.floor(
                        seconds / 60
                    );


                seconds =
                    seconds % 60;


                timer.innerText =
                    String(minutes)
                        .padStart(2, "0")
                    +
                    ":" +
                    String(seconds)
                        .padStart(2, "0");


                statusText.innerText =
                    "RUNNING";


                status.classList.remove(
                    "alarm"
                );

                status.classList.add(
                    "active"
                );
            }


            // =================================================
            // READY
            // =================================================

            else
            {
                timer.innerText =
                    "--:--";


                statusText.innerText =
                    "READY";


                status.classList.remove(
                    "alarm"
                );

                status.classList.remove(
                    "active"
                );
            }

        }

        catch (error)
        {
            console.log(error);
        }
    }


    // ========================================================
    // UPDATE EVERY 500ms
    // ========================================================

    setInterval(
        updateStatus,
        500
    );


    // ========================================================
    // INITIAL STATUS
    // ========================================================

    updateStatus();

</script>

</body>

</html>
)rawliteral";

    return html;
}


// ============================================================
// ГЛАВНАЯ СТРАНИЦА
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
// START ALARM
// ============================================================

void handleStart()
{
    // Проверяем параметры

    if (
        !server.hasArg("duration") ||
        !server.hasArg("unit")
    )
    {
        server.send(
            400,
            "text/plain",
            "Missing parameters"
        );

        return;
    }


    long duration =
        server.arg("duration").toInt();


    String unit =
        server.arg("unit");


    // Проверка времени

    if (duration <= 0)
    {
        server.send(
            400,
            "text/plain",
            "Invalid duration"
        );

        return;
    }


    unsigned long durationMs;


    // ========================================================
    // MINUTES
    // ========================================================

    if (unit == "minutes")
    {
        durationMs =
            (unsigned long)duration *
            60UL *
            1000UL;
    }

    // ========================================================
    // SECONDS
    // ========================================================

    else
    {
        durationMs =
            (unsigned long)duration *
            1000UL;
    }


    // ========================================================
    // УСТАНАВЛИВАЕМ БУДИЛЬНИК
    // ========================================================

    alarmStartMillis =
        millis();


    alarmDurationMillis =
        durationMs;


    alarmRunning =
        true;


    alarmTriggered =
        false;


    // LED выключен

    ledState =
        false;


    digitalWrite(
        LED_PIN,
        LOW
    );


    // ========================================================
    // SERIAL
    // ========================================================

    Serial.print(
        "Будильник установлен: "
    );

    Serial.print(
        duration
    );

    Serial.print(
        " "
    );

    Serial.println(
        unit
    );


    server.send(
        200,
        "text/plain",
        "OK"
    );
}


// ============================================================
// STOP ALARM
// ============================================================

void handleStop()
{
    alarmRunning =
        false;


    alarmTriggered =
        false;


    ledState =
        false;


    digitalWrite(
        LED_PIN,
        LOW
    );


    Serial.println(
        "Будильник выключен"
    );


    server.send(
        200,
        "text/plain",
        "OK"
    );
}


// ============================================================
// STATUS
// ============================================================

void handleStatus()
{
    unsigned long remaining =
        0;


    // ========================================================
    // TIMER RUNNING
    // ========================================================

    if (alarmRunning)
    {
        unsigned long elapsed =
            millis() -
            alarmStartMillis;


        if (
            elapsed <
            alarmDurationMillis
        )
        {
            remaining =
                alarmDurationMillis -
                elapsed;
        }
    }


    // ========================================================
    // JSON
    // ========================================================

    String json = "{";


    json +=
        "\"running\":";


    json +=
        alarmRunning
            ? "true"
            : "false";


    json +=
        ",";


    json +=
        "\"triggered\":";


    json +=
        alarmTriggered
            ? "true"
            : "false";


    json +=
        ",";


    json +=
        "\"remaining\":";


    json +=
        String(
            remaining
        );


    json +=
        "}";


    server.send(
        200,
        "application/json",
        json
    );
}


// ============================================================
// 404
// ============================================================

void handleNotFound()
{
    server.send(
        404,
        "text/plain",
        "404 Not Found"
    );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // ========================================================
    // SERIAL
    // ========================================================

    Serial.begin(
        115200
    );


    delay(
        500
    );


    Serial.println();
    Serial.println(
        "================================"
    );
    Serial.println(
        " ESP32-S3 LOCAL ALARM"
    );
    Serial.println(
        "================================"
    );


    // ========================================================
    // LED
    // ========================================================

    pinMode(
        LED_PIN,
        OUTPUT
    );


    digitalWrite(
        LED_PIN,
        LOW
    );


    // ========================================================
    // WIFI
    // ========================================================

    WiFi.mode(
        WIFI_STA
    );


    Serial.print(
        "Connecting to Wi-Fi"
    );


    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );


    while (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        delay(
            500
        );

        Serial.print(
            "."
        );
    }


    // ========================================================
    // WIFI CONNECTED
    // ========================================================

    Serial.println();

    Serial.println(
        "Wi-Fi connected!"
    );


    Serial.print(
        "SSID: "
    );

    Serial.println(
        WIFI_SSID
    );


    Serial.print(
        "IP: "
    );

    Serial.println(
        WiFi.localIP()
    );


    // ========================================================
    // HTTP ROUTES
    // ========================================================

    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );


    server.on(
        "/start",
        HTTP_GET,
        handleStart
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


    server.onNotFound(
        handleNotFound
    );


    // ========================================================
    // START SERVER
    // ========================================================

    server.begin();


    Serial.println(
        "HTTP server started."
    );


    Serial.print(
        "Open: http://"
    );


    Serial.println(
        WiFi.localIP()
    );


    Serial.println(
        "================================"
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ========================================================
    // HTTP
    // ========================================================

    server.handleClient();


    // ========================================================
    // CHECK ALARM
    // ========================================================

    if (
        alarmRunning &&
        !alarmTriggered
    )
    {
        unsigned long elapsed =
            millis() -
            alarmStartMillis;


        if (
            elapsed >=
            alarmDurationMillis
        )
        {
            alarmTriggered =
                true;


            Serial.println();
            Serial.println(
                "!!! ALARM !!!"
            );


            // Начинаем мигать сразу

            lastBlinkMillis =
                millis();


            ledState =
                true;


            digitalWrite(
                LED_PIN,
                HIGH
            );
        }
    }


    // ========================================================
    // BLINK LED
    // ========================================================

    if (alarmTriggered)
    {
        unsigned long now =
            millis();


        if (
            now -
            lastBlinkMillis >=
            BLINK_INTERVAL
        )
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
}
