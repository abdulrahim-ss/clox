#include <stdio.h>

#include "memory.h"
#include "debug.h"
#include "common.h"
#include "vm.h"

VM vm;

static void resetStack(){
    vm.stackTop = vm.stack;
}

void initVM(){
    resetStack();
}

void freeVM(){

}

void push(Value value){
    *vm.stackTop = value;
    vm.stackTop++;
//    if (vm.stackTop == &vm.stack[STACK_MAX]){
//        vm.stack = GROW_ARRAY(uint8_t, &vm.stack, STACK_MAX, STACK_MAX*2);
//    }
}

Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do {\
        double r = pop();\
        double l = pop();\
        push(l op r);\
    } while (false)

    while (true) {
#ifdef DeBUG_TRACE_EXECUTION
        printf("[");
        bool first = true;
        for (Value* slot=vm.stack; slot < vm.stackTop; slot++) {
            if (!first) printf(", ");
            first = false;
            printValue(*slot);
        }
        printf("]\n");
        disassembleInstruction(vm.chunk,
                               (int) (vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        /* uint8 instruction = READ_BYTE();
         switch (instruction)*/
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            /*Binary operations on constants*/
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_NEGATE:   *(vm.stackTop-1) = -*(vm.stackTop-1); break;
//            case OP_NEGATE:   push(-pop()); break;

            case OP_RETURN: {
                printValue(pop());
                printf("\n\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk* chunk){
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}
