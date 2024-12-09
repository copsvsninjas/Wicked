/*
  Author: Leonardo Laguna Ruiz (leonardo@vult-dsp.com)

  License: CC BY-NC-SA

*/

#include <stdint.h>
#include <map>
#include <MIDI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "KeyScan.h"

// Serial MIDI
#ifdef ARDUINO_TEENSY36
MIDI_CREATE_INSTANCE(HardwareSerial, Serial6, MIDI);
#endif
#ifdef ARDUINO_TEENSY41
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
#endif

// OLED Screen
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Keys and Encoder scanning
KeyScan key_scan;

IntervalTimer update_timer;

uint8_t decodeNote(uint8_t row, uint8_t col)
{
  switch (row)
  {
  case 8:
    return 42 + 2 * col;
  case 7:
    return 47 + 2 * col;
  case 6:
    return 54 + 2 * col;
  case 5:
    return 59 + 2 * col;
  case 4:
    return 66 + 2 * col;
  case 3:
    return 71 + 2 * col;
  case 2:
    return 78 + 2 * col;
  case 1:
    return 83 + 2 * col;
  case 0:
    return 90 + 2 * col;
  }
  return 0;
}

void UpdateKeys()
{
  key_scan.Update();
}

void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing...");

  Serial.println(" - Starting MIDI");
  MIDI.begin(MIDI_CHANNEL_OMNI);

  key_scan.Setup();

#ifdef ARDUINO_TEENSY41
  Serial1.setRX(52);
  Serial1.setTX(53);
#endif

  Serial.println(" - Starting screen");
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F(" ! SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Start the key scanning
  update_timer.begin(UpdateKeys, 100); // tis is equivalent to scanning the keys every 1 ms

  display.clearDisplay();

  display.setTextSize(2);   // Normal 1:1 pixel scale
  display.setTextColor(1);  // Draw white text
  display.setCursor(32, 8); // Start at top-left corner
  display.cp437(true);      // Use full 256 char 'Code Page 437' font

  display.write('W');
  display.write('I');
  display.write('C');
  display.write('K');
  display.write('E');
  display.write('D');

  display.display();
  Serial.println("Done!");
}

void loop()
{
  key_scan.GetEncoder1Delta();
  key_scan.GetEncoder2Delta();
  key_scan.isButton1Pressed();
  key_scan.isButton2Pressed();

  if (key_scan.isKeyPressed())
  {
    auto key = key_scan.GetKey();
    int row = std::get<0>(key);
    int col = std::get<1>(key);
    int vel = std::get<2>(key);
    int note = decodeNote(row, col);
    if (vel != 0)
    {
      usbMIDI.sendNoteOn(note, vel, 1);
    }
    else
    {
      usbMIDI.sendNoteOff(note, vel, 1);
    }
  }

  while (usbMIDI.read())
  {
  }

  while (MIDI.read())
  {
  }
  // checkMidiInput();
}
