#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "secrets.h"

// ============================================================
// ESP32-S3 ALARM
// Wi-Fi STA + Access Point
// ============================================================


// ============================================================
// LED
// ============================================================

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

const int LED_PIN = LED_BUILTIN;


// ============================================================
// ACCESS POINT
// ============================================================

const char* AP_SSID = "ESP32-ALARM";
const char* AP_PASSWORD = "alarm1234";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);


// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);


// ============================================================
// ALARM STATE
// ============================================================

bool alarmRunning = false;
bool alarmTriggered = false;

bool ledState = false;

unsigned long alarmStartMillis = 0;
unsigned long alarmDurationMillis = 0;

unsigned long lastBlinkMillis = 0;

const unsigned long BLINK_INTERVAL = 300;


// ============================================================
// HTML PAGE
// ============================================================

String makePage()
{
    String html = R"rawliteral(
<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta
    name="viewport"
    content="width=device-width, initial-scale=1"
>

<meta
    name="theme-color"
    content="#080808"
>

<title>ESP32 // ALARM</title>


<style>

* {
    box-sizing: border-box;
}


:root {

    --bg: #080808;

    --panel: #101010;

    --border: #292929;

    --border-light: #3a3a3a;

    --text: #eeeeee;

    --muted: #777777;

    --danger: #ff3b30;

}


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


.container {

    width: 100%;

    max-width: 540px;
}


.header {

    display: flex;

    justify-content: space-between;

    align-items: center;

    margin-bottom: 12px;

    color: var(--muted);

    font-size: 11px;

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


.card {

    background: var(--panel);

    border: 1px solid var(--border);

    border-radius: 4px;

    overflow: hidden;

    box-shadow:
        0 20px 60px rgba(0,0,0,.5);
}


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


.status.alarm {

    color: #fff;

    border-color: var(--danger);

    background: rgba(255,59,48,.08);

    animation:
        alarmPulse .8s infinite alternate;
}


.status.alarm .status-dot {

    background: var(--danger);
}


@keyframes alarmPulse {

    from {
        opacity: 1;
    }

    to {
        opacity: .45;
    }

}


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
}


input:focus,
select:focus {

    border-color: var(--border-light);

    background: #101010;
}


select {

    cursor: pointer;
}


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
}


.start {

    background: #fff;

    color: #000;

    border: 1px solid #fff;
}


.start:hover {

    background: #ddd;
}


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


.network {

    padding: 18px 24px;

    border-top: 1px solid var(--border);

    font-size: 10px;

    letter-spacing: 1px;
}


.network-title {

    color: var(--muted);

    margin-bottom: 12px;

    text-transform: uppercase;
}


.net-row {

    display: flex;

    justify-content: space-between;

    padding: 5px 0;
}


.net-label {

    color: #555;
}


.net-value {

    color: #aaa;

    text-align: right;
}


.footer {

    padding: 13px 24px;

    border-top: 1px solid var(--border);

    color: #444;

    font-size: 9px;

    letter-spacing: 1px;

    display: flex;

    justify-content: space-between;
}


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


<div class="header">

    <div>
        ESP32 // ALARM
    </div>

    <div class="online">

        <span class="dot"></span>

        ONLINE

    </div>

</div>


<div class="card">


<div class="title">

    TIMER

    <span>/ LOCAL</span>

</div>


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


<div class="controls">


<label class="label">
    duration
</label>


<div class="row">


<input
    id="duration"
    type="number"
    min="1"
    max="4294967"
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


<div class="network">

    <div class="network-title">
        network
    </div>


    <div class="net-row">

        <span class="net-label">
            WIFI
        </span>

        <span
            class="net-value"
            id="wifiStatus"
        >
            CONNECTING
        </span>

    </div>


    <div class="net-row">

        <span class="net-label">
            WIFI IP
        </span>

        <span
            class="net-value"
            id="wifiIP"
        >
            -
        </span>

    </div>


    <div class="net-row">

        <span class="net-label">
            ACCESS POINT
        </span>

        <span class="net-value">
            ESP32-ALARM
        </span>

    </div>


    <div class="net-row">

        <span class="net-label">
            AP IP
        </span>

        <span class="net-value">
            192.168.4.1
        </span>

    </div>

</div>


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


    if (
        !duration ||
        duration <= 0
    )
    {

        alert(
            "Enter duration"
        );

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


async function stopAlarm()
{

    try
    {

        await fetch(
            "/stop"
        );


        updateStatus();

    }
    catch (error)
    {

        console.log(error);

    }

}


async function updateStatus()
{

    try
    {

        const response =
            await fetch(
                "/status"
            );


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


        if (data.triggered)
        {

            timer.innerText =
                "00:00";


            statusText.innerText =
                "ALARM";


            status.classList.remove(
                "active"
            );


            status.classList.add(
                "alarm"
            );

        }

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

                ":"

                +

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


        document.getElementById(
            "wifiStatus"
        ).innerText =
            data.wifiConnected
                ? "CONNECTED"
                : "OFFLINE";


        document.getElementById(
            "wifiIP"
        ).innerText =
            data.wifiIP;

    }

    catch (error)
    {

        console.log(error);

    }

}


setInterval(
    updateStatus,
    500
);


updateStatus();


</script>


</body>

</html>

)rawliteral";


    return html;
}


// ============================================================
// ROOT
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
        server.arg(
            "duration"
        ).toInt();


    String unit =
        server.arg(
            "unit"
        );


    if (
        duration <= 0
    )
    {

        server.send(
            400,
            "text/plain",
            "Invalid duration"
        );

        return;

    }


    unsigned long durationMs;


    if (
        unit == "minutes"
    )
    {

        durationMs =
            (unsigned long)duration *
            60UL *
            1000UL;

    }

    else
    {

        durationMs =
            (unsigned long)duration *
            1000UL;

    }


    alarmStartMillis =
        millis();


    alarmDurationMillis =
        durationMs;


    alarmRunning =
        true;


    alarmTriggered =
        false;


    ledState =
        false;


    digitalWrite(
        LED_PIN,
        LOW
    );


    Serial.print(
        "Alarm set: "
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
        "Alarm stopped"
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


    if (
        alarmRunning
    )
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


    String json =
        "{";


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
        ",";


    json +=
        "\"wifiConnected\":";


    json +=
        WiFi.status() ==
        WL_CONNECTED
            ? "true"
            : "false";


    json +=
        ",";


    json +=
        "\"wifiIP\":\"";


    json +=
        WiFi.localIP().toString();


    json +=
        "\"";


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

    Serial.begin(
        115200
    );


    delay(
        500
    );


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "       ESP32-S3 LOCAL ALARM"
    );

    Serial.println(
        "========================================"
    );


    // --------------------------------------------------------
    // LED
    // --------------------------------------------------------

    pinMode(
        LED_PIN,
        OUTPUT
    );


    digitalWrite(
        LED_PIN,
        LOW
    );


    // ========================================================
    // WIFI AP + STA
    // ========================================================

    WiFi.mode(
        WIFI_AP_STA
    );


    // ========================================================
    // ACCESS POINT
    // ========================================================

    WiFi.softAPConfig(
        AP_IP,
        AP_GATEWAY,
        AP_SUBNET
    );


    bool apStarted =
        WiFi.softAP(
            AP_SSID,
            AP_PASSWORD
        );


    if (
        apStarted
    )
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


    // ========================================================
    // HOME WIFI
    // ========================================================

    Serial.println();


    Serial.print(
        "Connecting to Wi-Fi: "
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
        millis() - wifiStart < 10000
    )
    {

        delay(
            250
        );


        Serial.print(
            "."
        );

    }


    Serial.println();


    if (
        WiFi.status() ==
        WL_CONNECTED
    )
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

    }

    else
    {

        Serial.println(
            "Wi-Fi unavailable."
        );


        Serial.println(
            "Portable AP is still active."
        );

    }


    // ========================================================
    // WEB ROUTES
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


    Serial.println();


    Serial.println(
        "HTTP server started."
    );


    Serial.print(
        "Portable URL: http://"
    );


    Serial.println(
        WiFi.softAPIP()
    );


    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {

        Serial.print(
            "Home URL: http://"
        );


        Serial.println(
            WiFi.localIP()
        );

    }


    Serial.println(
        "========================================"
    );

}


// ============================================================
// LOOP
// ============================================================

void loop()
{

    // --------------------------------------------------------
    // WEB SERVER
    // --------------------------------------------------------

    server.handleClient();


    // --------------------------------------------------------
    // CHECK ALARM
    // --------------------------------------------------------

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


            ledState =
                true;


            digitalWrite(
                LED_PIN,
                HIGH
            );


            lastBlinkMillis =
                millis();

        }

    }


    // --------------------------------------------------------
    // BLINK LED
    // --------------------------------------------------------

    if (
        alarmTriggered
    )
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
