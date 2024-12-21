#ifndef __OCFHelper_H__
#define __OCFHelper_H__

#include <Arduino.h>

enum OCFDirection { IN, OUT, NONE, BOTH };
OCFDirection parseDirection(String direction);
const char* writeDirection(OCFDirection dir);
#endif