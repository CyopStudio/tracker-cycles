#include "Arduino.h"

extern "C" void cserial_println(const char* text);
extern "C" void cserial_print(const char* text);
extern "C" void cserial_print_val(int n);

void cserial_print(const char* text)
{
    Serial.print(text);
}

void cserial_println(const char* text)
{
    Serial.println(text);
}

void cserial_print_val(int n)
{
    Serial.print(n);
}

void cppserial_print(const char* text)
{
    Serial.print(text);
}
void cppserial_println(const char* text)
{
    Serial.println(text);
}
void cppserial_print_val(int n)
{
    Serial.print(n);
}
