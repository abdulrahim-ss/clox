#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"

typedef struct {
    Chunk* chunk;
} VM;

void initVM();
void freeVM();

#endif
