/*
  Author: Leonardo Laguna Ruiz (leonardo@vult-dsp.com)

  License: CC BY-NC-SA

*/

#include <stdint.h>
#include <map>
#include <queue>
#include <tuple>
#include <set>

#include <Bounce2.h>
#include <Encoder.h>

static const uint8_t COL_PINS[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
static const uint8_t TOP_PINS[] = {15, 14, 39, 38, 37, 36, 35, 34, 33};
static const uint8_t BOT_PINS[] = {10, 11, 26, 27, 28, 29, 30, 31, 32};
static const uint16_t MAX_TICK = 0x7FFF;

static const int ENCODER1_PUSH = 17;
static const int ENCODER2_PUSH = 16;
static const int ENCODER1_PIN1 = 23;
static const int ENCODER1_PIN2 = 22;
static const int ENCODER2_PIN1 = 21;
static const int ENCODER2_PIN2 = 20;

class KeyScan
{
public:
    KeyScan()
    {
        // Initialize the grid
        for (uint8_t col = 0; col < 10; col++)
        {
            for (uint8_t row = 0; row < 9; row++)
            {
                top_grid[col][row] = -1;
                bot_grid[col][row] = -1;
            }
        }
        col = 0;
        debug_mode = false;
        tick = 0;
        encoder1_pre = 0;
        encoder2_pre = 0;
    };

    void Setup()
    {
        for (uint8_t col = 0; col < 10; col++)
            pinMode(COL_PINS[col], OUTPUT);

        for (uint8_t row = 0; row < 9; row++)
        {
            pinMode(TOP_PINS[row], INPUT_PULLDOWN);
            pinMode(BOT_PINS[row], INPUT_PULLDOWN);
        }

        encoder1_push.attach(ENCODER1_PUSH, INPUT_PULLUP);
        encoder2_push.attach(ENCODER2_PUSH, INPUT_PULLUP);
    }

    bool isKeyPressed()
    {
        return !note_queue.empty();
    }

    std::tuple<int, int, int> GetKey()
    {
        auto front = note_queue.front();
        note_queue.pop();
        return front;
    }

    bool isButton1Pressed()
    {
        encoder1_push.update();
        bool value = encoder1_push.changed() && encoder1_push.read() == LOW;
        if (debug_mode && value)
            Serial.println("Button 1 pressed");
        return value;
    }

    bool isButton2Pressed()
    {
        encoder2_push.update();
        bool value = encoder2_push.changed() && encoder2_push.read() == LOW;
        if (debug_mode && value)
            Serial.println("Button 2 pressed");
        return value;
    }

    int GetEncoder1Delta()
    {
        int delta = 0;
        int current = encoder1.read();
        if (abs(current - encoder1_pre) > 1)
        {
            delta = (encoder1_pre - current);
            encoder1.write(0);
            if (debug_mode)
            {
                Serial.print("Encoder 1 delta = ");
                Serial.println(delta);
            }
        }
        return delta;
    }

    int GetEncoder2Delta()
    {
        int delta = 0;
        int current = encoder2.read();
        if (abs(current - encoder2_pre) > 1)
        {
            delta = (encoder2_pre - current);
            encoder2.write(0);
            if (debug_mode)
            {
                Serial.print("Encoder 2 delta = ");
                Serial.println(delta);
            }
        }
        return delta;
    }

    void Update()
    {
        for (uint8_t row = 0; row < 9; row++)
        {
            bool trigger_note_on = false;
            bool trigger_note_off = false;
            int top_read = digitalRead(TOP_PINS[row]);
            int bot_read = digitalRead(BOT_PINS[row]);

            // Here we check if the button state has changed.
            // A negative number represents the OFF state
            // A positive number holds the time (tick value) at which it was pressed ON

            if (top_read && top_grid[col][row] < 0)
            { // edge up
                top_grid[col][row] = tick;
            }
            else if (!top_read && top_grid[col][row] >= 0)
            { // edge down
                top_grid[col][row] = -1;
                trigger_note_off = true;
            }

            if (bot_read && bot_grid[col][row] < 0)
            { // edge up
                bot_grid[col][row] = tick;
                trigger_note_on = true;
            }
            else if (!bot_read && bot_grid[col][row] >= 0)
            { // edge down
                bot_grid[col][row] = -1;
            }

            // Checks for faulty notes on the top layer
            if (top_grid[col][row] == -1 && trigger_note_on)
            {
                Serial.print("Failure to detect notes on the top layer. Location: ");
                Serial.print(row);
                Serial.println(col);
            }

            // If the top switch has been pressed for a while and the bottom has not been pressed
            // trigger a soft note
            if (top_grid[col][row] > 0 && bot_grid[col][row] < 0 && calculateDelta(top_grid[col][row], tick) == 127)
            {
                trigger_note_on = true;
                bot_grid[col][row] = tick;
            }

            if (trigger_note_on)
            {
                int16_t delta = calculateDelta(top_grid[col][row], bot_grid[col][row]);
                if (delta < 128)
                {
                    auto element = std::make_tuple(row, col);
                    auto search = active_notes.find(element);
                    if (search == active_notes.end())
                    {
                        int16_t velocity = 127 - delta;
                        if (velocity < 1)
                            velocity = 1;
                        if (debug_mode)
                        {
                            Serial.print("Note ON: ");
                            Serial.print(row);
                            Serial.print(" : ");
                            Serial.print(col);
                            Serial.print(" : ");
                            Serial.println(velocity);
                        }
                        active_notes.insert(element);
                        note_queue.push(std::make_tuple(row, col, velocity));
                    }
                }
            }

            if (trigger_note_off)
            {
                auto element = std::make_tuple(row, col);
                auto search = active_notes.find(element);
                if (search != active_notes.end())
                {
                    if (debug_mode)
                    {
                        Serial.print("Note OFF: ");
                        Serial.print(row);
                        Serial.print(" : ");
                        Serial.println(col);
                    }
                    active_notes.erase(element);
                    note_queue.push(std::make_tuple(row, col, 0));
                }
            }
        }
        col = (col + 1) % 10;
        if (col == 0)
        {
            // reset the tick
            tick = (tick + 1) % MAX_TICK;
        }
        // We select the next column in order to give it time to stabilize
        selectColumn(col);
    }

    void DebugMode(bool active)
    {
        debug_mode = active;
    }

private:
    void selectColumn(uint8_t n)
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            digitalWriteFast(COL_PINS[i], i == n);
        }
    }

    int16_t calculateDelta(int16_t t1, int16_t t2)
    {
        if (t2 >= t1)
        {
            return t2 - t1;
        }
        else
        {
            return t2 - (t1 - MAX_TICK);
        }
    }

    bool debug_mode;
    int16_t tick;
    int16_t top_grid[10][9];
    int16_t bot_grid[10][9];
    int col;
    std::queue<std::tuple<int, int, int>> note_queue;
    std::set<std::tuple<int, int>> active_notes;

    // Encoders
    Bounce encoder1_push = Bounce();
    Bounce encoder2_push = Bounce();
    Encoder encoder1 = Encoder(ENCODER1_PIN1, ENCODER1_PIN2);
    Encoder encoder2 = Encoder(ENCODER2_PIN1, ENCODER2_PIN2);

    int encoder1_pre;
    int encoder2_pre;
};
