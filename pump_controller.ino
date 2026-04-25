#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

#define pinPressostato 18
#define pinBomba 35

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Timing configuration
const unsigned long tempoMinimoLow = 2000;      // pressostat must stay LOW for 2s before starting pump
const unsigned long tempoMinimoHigh = 2000;     // pressostat must stay HIGH for 2s before stopping pump
const unsigned long tempoMaximoLigado = 240000; // safety timeout (240 seconds)
const unsigned long cooldown = 3000;            // minimum delay between pump cycles

bool falhaSeguranca = false;
bool bombaLigada = false;

unsigned long tempoEstadoPressostato = 0;
unsigned long tempoBombaLigada = 0;
unsigned long tempoUltimoDesligamento = 0;

void setup()
{

  // Attempt WiFi connection for OTA
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
  {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    ArduinoOTA.setPassword(nullptr);
    ArduinoOTA.begin();
  }

  pinMode(pinPressostato, INPUT_PULLUP);
  pinMode(pinBomba, OUTPUT);

  digitalWrite(pinBomba, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED)
  {
    ArduinoOTA.handle();
  }

  if (falhaSeguranca)
  {
    desligarBomba();
    return;
  }

  static int estadoAnterior = digitalRead(pinPressostato);
  int estadoAtual = digitalRead(pinPressostato);
  unsigned long agora = millis();

  if (estadoAtual != estadoAnterior)
  {
    tempoEstadoPressostato = agora;
    estadoAnterior = estadoAtual;
  }

  // Safety timeout
  if (bombaLigada && (agora - tempoBombaLigada >= tempoMaximoLigado))
  {
    desligarBomba();
    falhaSeguranca = true;
    return;
  }

  // Pump stop condition
  if (bombaLigada)
  {

    if (estadoAtual == HIGH &&
        (agora - tempoEstadoPressostato >= tempoMinimoHigh))
    {

      desligarBomba();
    }
  }
  else
  {

    // Pump start condition
    if (estadoAtual == LOW &&
        (agora - tempoEstadoPressostato >= tempoMinimoLow) &&
        (agora - tempoUltimoDesligamento >= cooldown))
    {

      ligarBomba();
    }
  }
}

void ligarBomba()
{

  digitalWrite(pinBomba, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);

  tempoBombaLigada = millis();
  bombaLigada = true;
}

void desligarBomba()
{

  digitalWrite(pinBomba, LOW);
  digitalWrite(LED_BUILTIN, LOW);

  tempoUltimoDesligamento = millis();
  bombaLigada = false;
}
