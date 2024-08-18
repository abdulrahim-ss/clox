#include <stdarg.h>
#include <stdio.h>

#include "debug.h"
#include "compiler.h"
#include "common.h"
#include "vm.h"

VM vm;

static void resetStack(){
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...){
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
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

static Value peek(int distance){
    return vm.stackTop[-1 - distance];
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do {\
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)) ) { \
            runtimeError("Operand must be a number.");\
            return INTERPRET_RUNTIME_ERROR;\
        }\
        double r = AS_NUMBER(pop());\
        double l = AS_NUMBER(pop());\
        push(valueType(l op r));\
    } while (false)

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
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
        /* uint_8 instruction = READ_BYTE();
         switch (instruction) {*/
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            /*Binary operations on constants*/
            case OP_ADD:      BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm.stackTop[-1] = NUMBER_VAL(-AS_NUMBER(vm.stackTop[-1]));
                break;
//            case OP_NEGATE:   push(-pop()); break;

            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;

            case OP_NOT: vm.stackTop[-1] = BOOL_VAL(isFalsey(vm.stackTop[-1])); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;

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


//InterpretResult interpret(Chunk* chunk){
//    vm.chunk = chunk;
//    vm.ip = vm.chunk->code;
//    return run();
//}
InterpretResult interpret(const char* source){
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}
