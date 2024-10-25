#include <stdio.h>
#include <string.h>

#include "value.h"
#include "memory.h"
#include "object.h"

bool isFalsey(Value value){
    if (value.type == VAL_NUMBER) return AS_NUMBER(value) == 0;
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}


bool valuesEqual(Value a, Value b){
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
//        case VAL_OBJ: {
//            ObjString* aString = AS_STRING(a);
//            ObjString* bString = AS_STRING(b);
//            return aString->length == bString->length
//                && memcmp(aString->chars, bString->chars, aString->length) == 0;
//        }
        default: false; // Unreachable.
    }
}

void initValueArray(ValueArray* array){
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void writeValueArray(ValueArray* array, Value value){
    if (array->capacity < array->count + 1){
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value,
                                   array->values,
                                   oldCapacity,
                                   array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array){
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value){
    switch (value.type) {
        case VAL_BOOL:
            printf("%s", AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("%s", "nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            printObject(value);
            break;
    }
}