#include <Arduino.h>

extern "C" unsigned int sleep(unsigned int seconds)
{
    mock::advance(seconds * 1000);
    return 0;
}
