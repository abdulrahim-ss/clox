#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H int x = 5;

#include "common.h"

typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
