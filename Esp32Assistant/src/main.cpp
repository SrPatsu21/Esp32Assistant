#include <Arduino.h>

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
const unsigned long OCCUPIED_TIMEOUT = 500UL;
// const unsigned long OCCUPIED_TIMEOUT = 10UL  * 60UL * 1000UL; // 10 minutos
bool lastButtonState = HIGH;

void goToSleep()
{
    state = SLEEPING;

    digitalWrite(PIN_LED, LOW);

    Serial.println("Estado: SLEEPING");
}

void wakeSystem()
{
    state = ARMED;

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
        Serial.println("Quarto escuro");

        analogWrite(PIN_LED, 200);
    }
    else
    {
        Serial.println("Quarto claro");

        analogWrite(PIN_LED, 0);
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
    if (state == OCCUPIED)
    {
        if (millis() - lastMovement > OCCUPIED_TIMEOUT)
        {
            state = ARMED;

            Serial.println("Estado: ARMED");
        }
    }

    // ===== LED DEBUG =====

    // switch (state)
    // {
    //     case SLEEPING:
    //         digitalWrite(PIN_LED, LOW);
    //         break;

    //     case ARMED:
    //         digitalWrite(PIN_LED, HIGH);
    //         break;

    //     case OCCUPIED:
    //         digitalWrite(PIN_LED, (millis() / 500) % 2);
    //         break;
    // }

    delay(10);
}