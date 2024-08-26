#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType((value), OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjectType;

struct Obj {
    ObjectType type;
    struct Obj* next;
};

struct ObjString {
    Obj Obj;
    int length;
    char* chars;
    uint32_t hash;
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);

static inline bool isObjType(Value value, ObjectType type){
//    return IS_OBJ(value) && AS_OBJ(value)->type == type;
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

void printObject(Value value);

#endif //CLOX_OBJECT_H
