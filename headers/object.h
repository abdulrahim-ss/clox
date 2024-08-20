#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType((value), OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_Obj(value))
#define AS_CSTRING(value) (((ObjString*)AS_Obj(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjectType;

struct Obj {
    ObjectType type;
};

struct ObjString {
    Obj Obj;
    int length;
    char* chars;
};

ObjString* copyString(const char* chars, int length);

static inline bool isObjType(Value value, ObjectType type){
//    return IS_OBJ(value) && AS_OBJ(value)->type == type;
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif //CLOX_OBJECT_H
