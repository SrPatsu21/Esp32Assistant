#include <Arduino.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>

// PIN
const int PIN_PIR = 27;
const int PIN_BUTTON = 14;
const int PIN_LED = 26;
const int PIN_LIGHT = 34;

// State control
enum SystemState
{
    ARMED,
    OCCUPIED
};
SystemState state = ARMED;

unsigned long lastMovement = 0;

unsigned long ledTurnOffTime = 0;
const unsigned long OCCUPIED_TIMEOUT = 10UL  * 60UL * 1000UL; // 10 minutos
bool lastButtonState = HIGH;
int pass = 0;

// wifi
const char* ssid = "wifiname";
const char* password = "wifipass";
const byte targetMAC[6] =
{
    0xAA,
    0xAA,
    0xAA,
    0xAA,
    0xAA,
    0xAA
};
WiFiUDP udp;
WebServer server(80);

void connectWifi(){
    WiFi.begin(ssid, password);

    Serial.print("Conectando WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi conectado");\
    Serial.println(WiFi.localIP());

    udp.begin(9);

}

void disconnectWifi(){
    udp.stop();

    WiFi.disconnect(true);

    WiFi.mode(WIFI_OFF);

    Serial.println("WiFi desligado");

}

// webserver
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>ESP32 Logs</title>
<style>
body{
    font-family: Arial;
    background:#111;
    color:#eee;
    padding:20px;
}
.card{
    background:#222;
    padding:15px;
    margin-bottom:10px;
    border-radius:10px;
}
</style>
</head>
<body>

<h1>ESP32 Presence Logs</h1>

<div id="status"></div>

<div id="logs"></div>

<script>

async function loadLogs()
{
    const response = await fetch('/logs');

    const data = await response.json();

    const nowBrowser = Date.now();

    const bootTime = nowBrowser - data.uptime;

    document.getElementById('status').innerHTML =
        '<div class="card">' +
        'Estado: ' + data.state +
        '<br>Uptime: ' +
        Math.floor(data.uptime / 1000) +
        's</div>';

    let html = '';

    for(const log of data.logs)
    {
        const realTime =
            new Date(bootTime + log.time);

        html +=
            '<div class="card">' +
            '<b>' + log.type + '</b><br>' +
            realTime.toLocaleString() +
            '</div>';
    }

    document.getElementById('logs').innerHTML =
        html;
}

loadLogs();

setInterval(loadLogs, 5000);

</script>

</body>
</html>
)rawliteral";

void handleRoot()
{
    server.send(200, "text/html", webpage);
}

// wake on lan
void sendWOL(const byte* mac)
{
    byte packet[102];

    // 6 bytes FF
    for (int i = 0; i < 6; i++)
    {
        packet[i] = 0xFF;
    }

    // MAC repetido 16 vezes
    for (int i = 1; i <= 16; i++)
    {
        memcpy(&packet[i * 6], mac, 6);
    }

    udp.beginPacket(
        IPAddress(255,255,255,255),
        9
    );

    udp.write(packet, sizeof(packet));

    udp.endPacket();

    Serial.println("Wake-on-LAN enviado");
}

// log
enum EventType
{
    EVENT_ENTER,
    EVENT_TIMEOUT
};
struct EventLog
{
    EventType type;
    unsigned long time;
};
const int MAX_LOGS = 50;
EventLog logs[MAX_LOGS];
int logIndex = 0;

void addLog(EventType type)
{
    logs[logIndex].type = type;

    logs[logIndex].time = millis();

    logIndex++;

    if (logIndex >= MAX_LOGS)
    {
        logIndex = 0;
    }
}

// change state
void goToSleep()
{
    Serial.println("Entrando em deep sleep");
    disconnectWifi();

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, 0);

    delay(100);

    esp_deep_sleep_start();
}

void enterOccupied()
{
    state = OCCUPIED;
    addLog(EVENT_ENTER);

    lastMovement = millis();

    Serial.println("Estado: OCCUPIED");

    // Wake-on-LAN
    sendWOL(&targetMAC[0]);

    // LIGHT
    int lightLevel = analogRead(PIN_LIGHT);

    Serial.print("Luminosidade: ");
    Serial.println(lightLevel);
    if (lightLevel < 1500)
    {
        ledTurnOffTime += 10000;
        analogWrite(PIN_LED, 230);
    }
}

void enterArmed()
{
    addLog(EVENT_TIMEOUT);
    state = ARMED;
    analogWrite(PIN_LED, 255);
    ledTurnOffTime += 100;

    Serial.println("Estado: ARMED");
}

void handleLogs()
{
    String json = "{";

    json += "\"uptime\":";
    json += millis();

    json += ",";

    json += "\"state\":\"";

    if(state == ARMED)
    {
        json += "ARMED";
    }
    else
    {
        json += "OCCUPIED";
    }

    json += "\",";

    json += "\"logs\":[";

    bool first = true;

    for(int i = 0; i < MAX_LOGS; i++)
    {
        if(logs[i].time == 0)
        {
            continue;
        }

        if(!first)
        {
            json += ",";
        }

        first = false;

        json += "{";

        json += "\"type\":\"";

        if(logs[i].type == EVENT_ENTER)
        {
            json += "ENTER";
        }
        else
        {
            json += "TIMEOUT";
        }

        json += "\",";

        json += "\"time\":";
        json += logs[i].time;

        json += "}";
    }

    json += "]}";

    server.send(
        200,
        "application/json",
        json
    );
}

// main

void setup()
{
    Serial.begin(115200);

    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_LIGHT, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);

    digitalWrite(PIN_LED, LOW);

    delay(100);

    Serial.println("Sistema iniciado");

    connectWifi();

    // server
    server.on("/", handleRoot);

    server.on("/logs", handleLogs);

    server.begin();

    Serial.println("HTTP server iniciado");
}

void loop()
{
    //* BUTTON

    bool buttonState = digitalRead(PIN_BUTTON);

    // debounce
    if (lastButtonState == HIGH && buttonState == LOW)
    {
        delay(100);

        // sleep
        if (digitalRead(PIN_BUTTON) == LOW)
        {
            goToSleep();
        }
    }

    lastButtonState = buttonState;


    // PIR
    bool movement = digitalRead(PIN_PIR);

    if (movement)
    {
        lastMovement = millis();

        if (state == ARMED)
        {
            enterOccupied();
        }
    }

    // TIMEOUT PRESENÇA
    int time = millis();
    if (state == OCCUPIED)
    {
        if (time - lastMovement > OCCUPIED_TIMEOUT)
        {
            enterArmed();
        } else {
            delay(100);
        }
    }

    // Trun off LED
    if (time > ledTurnOffTime)
    {
        analogWrite(PIN_LED, 0);
        ledTurnOffTime = time;
    }
    server.handleClient();
    delay(100);
}