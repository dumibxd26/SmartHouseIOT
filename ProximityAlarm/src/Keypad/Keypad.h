#pragma once

#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>

class Keypad
{
private:
    const int numRows;
    const int numCols;
    const int *rowPins;
    const int *colPins;
    const char **keys;
    int lastKeyPressed;

public:
    Keypad(const int numRows, const int numCols, const int *rowPins, const int *colPins, const char **keys);
    void initialize();
    char getKey();
    void printPressedKey(char key);
    int getLastKeyPressed();
    void setLastKeyPressed(char key);
};

#endif