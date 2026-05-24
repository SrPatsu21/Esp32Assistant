#include <Arduino.h>
#include "esp_sleep.h"

const int PIN_PIR    = 27;
const int PIN_BUTTON = 14;
const int PIN_LED    = 26;
const int PIN_LIGHT  = 34;

enum SystemState
{
    SLEEPING,
    ARMED,
    OCCUPIED
};
SystemState state = ARMED;

unsigned long lastMovement = 0;

unsigned long ledTurnOffTime = 0;
// const unsigned long OCCUPIED_TIMEOUT = 5000UL;
const unsigned long OCCUPIED_TIMEOUT = 10UL  * 60UL * 100UL; // 10 minutos
bool lastButtonState = HIGH;

void goToSleep()
{
    state = SLEEPING;

    analogWrite(PIN_LED, 0);

    Serial.println("Estado: SLEEPING");
}

void goToSleep()
{
    Serial.println("Light sleep");

    digitalWrite(PIN_LED, LOW);

    esp_sleep_enable_ext0_wakeup(
        GPIO_NUM_14,
        0
    );

    delay(100);

    esp_light_sleep_start();

    Serial.println("Acordou");
}


void wakeSystem()
{
    state = ARMED;
    analogWrite(PIN_LED, 255);
    ledTurnOffTime = millis() + 100;

    Serial.println("Estado: ARMED");
}

void enterOccupied()
{
    state = OCCUPIED;

    lastMovement = millis();

    Serial.println("Estado: OCCUPIED");

    // Wake-on-LAN


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

void setup()
{
    Serial.begin(115200);

    pinMode(PIN_PIR, INPUT);
    pinMode(PIN_LIGHT, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);

    digitalWrite(PIN_LED, LOW);

    Serial.println("Sistema iniciado");
}

void loop()
{
    //* BUTTON

    bool buttonState = digitalRead(PIN_BUTTON);

    // debounce
    if (lastButtonState == HIGH && buttonState == LOW)
    {
        delay(20);

        // sleep or wake
        if (digitalRead(PIN_BUTTON) == LOW)
        {
            if (state == SLEEPING)
            {
                wakeSystem();
            }
            else
            {
                goToSleep();
            }
        }
    }

    lastButtonState = buttonState;

    // SLEEP
    if (state == SLEEPING)
    {
        delay(200);
        return;
    }

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
            state = ARMED;
            analogWrite(PIN_LED, 255);
            ledTurnOffTime += 100;

            Serial.println("Estado: ARMED");
        }
    }

    // Trun off LED
    if (time > ledTurnOffTime)
    {
        analogWrite(PIN_LED, 0);
        ledTurnOffTime = time;
    }

    delay(10);
}