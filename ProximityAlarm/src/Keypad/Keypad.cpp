#include "Keypad.h"

Keypad::Keypad(const int numRows, const int numCols, const int *rowPins, const int *colPins, const char **keys)
    : numRows(numRows), numCols(numCols), rowPins(rowPins), colPins(colPins), keys(keys), lastKeyPressed('Z')
{
}

int Keypad::getLastKeyPressed()
{
    return lastKeyPressed;
}

void Keypad::setLastKeyPressed(char key)
{
    lastKeyPressed = key;
}

void Keypad::initialize()
{
    // Set row pins as inputs and enable pull-up resistorss
    for (int i = 0; i < numRows; i++)
    {
        pinMode(rowPins[i], INPUT_PULLUP);
    }

    // Set column pins as outputs
    for (int i = 0; i < numCols; i++)
    {
        pinMode(colPins[i], OUTPUT);
        digitalWrite(colPins[i], HIGH); // Set columns to high to disable
    }
}

char Keypad::getKey()
{
    for (int col = 0; col < numCols; col++)
    {
        // Enable the current column
        digitalWrite(colPins[col], LOW);

        // Check each row in this column
        for (int row = 0; row < numRows; row++)
        {
            // If a key is pressed (LOW), return its value
            if (digitalRead(rowPins[row]) == LOW)
            {
                // Disable the current column
                digitalWrite(colPins[col], HIGH);
                return keys[row][col];
            }
        }

        // Disable the current column
        digitalWrite(colPins[col], HIGH);
    }

    // No key pressed, clear the flag
    lastKeyPressed = 'Z';
    return '\0'; // Return null if no key is pressed
}

void Keypad::printPressedKey(char key)
{
    if (key != '\0')
    {
        Serial.println("Key: " + String(key));
        delay(250);
    }
}