#include <Arduino.h>
#include <MicrophoneController.h>
#include <Ticker.h>

// Connect to ESP32:
// INMP441 >> ESP32:
// SCK >> GPIO14
// SD >> GPIO32
// WS >> GPIO15
// GND >> GND
// VDD >> VDD3.3

// TENTO JE GOOOD https://hackaday.io/project/166867-esp32-i2s-slm & https://github.com/ikostoski/esp32-i2s-slm

// dalsi...:
// nevim : https://community.hiveeyes.org/t/first-steps-digital-sound-recording/306
// vzor https://github.com/maspetsberger/esp32-i2s-mems

MicrophoneController microphoneController;

void checkMicrophones();

Ticker timerMicrophoneController(checkMicrophones, 20000); // 4 sec

void setup()
{
  Serial.begin(112500);
  timerMicrophoneController.start();

  Serial.println("Setup DONE\n\n");
}

void loop()
{
  timerMicrophoneController.update();
}

void checkMicrophones()
{
  microphoneController.setData();
}