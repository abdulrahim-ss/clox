#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk(){
    return compilingChunk;
}

/*********   Error Handling   *********/

static void errorAt(Token* token, const char* msg){
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d]", token->line);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type == TOKEN_ERROR){
        //nothing
    }
    else
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    fprintf(stderr, ": %s\n", msg);
    parser.hadError = true;
}

static void error(const char* msg){
    errorAt(&parser.previous, msg);
}

static void errorAtCurrent(const char * msg){
    errorAt(&parser.current, msg);
}

/**************************************/

/*********     Conversion     *********/

static uint8_t makeConstant(Value value){
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t) constant;
}

/**************************************/

/*********      Parssing      *********/

static void advance(){
    parser.previous = parser.current;

    while (true) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* msg){
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(msg);
}

static void emitByte(uint8_t byte){
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn(){
    emitByte(OP_RETURN);
}

static void emitConstant(Value value){
    emitBytes(OP_CONSTANT, makeConstant(value));
}
/**************************************/

/*********     Expresions     *********/
static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void number(){
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

static void unary(){
    TokenType operatorType = parser.previous.type;

    // Parse/compile the operand.
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;

        default: return; // Unreachable.
    }
}

static void grouping(){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void binary(){
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS:  emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:  emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default: return; // Unreachable
    }
}

ParseRule rules[] = {
        [TOKEN_LEFT_PAREN]      = {grouping,NULL,   PREC_NONE},
        [TOKEN_RIGHT_PAREN]     = {NULL,    NULL,   PREC_NONE},
        [TOKEN_LEFT_BRACE]      = {NULL,    NULL,   PREC_NONE},
        [TOKEN_RIGHT_BRACE]     = {NULL,    NULL,   PREC_NONE},
        [TOKEN_COMMA]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_DOT]             = {NULL,    NULL,   PREC_NONE},
        [TOKEN_SEMICOLON]       = {NULL,    NULL,   PREC_NONE},
        [TOKEN_PLUS]            = {NULL,    binary, PREC_TERM},
        [TOKEN_MINUS]           = {unary,   binary, PREC_TERM},
        [TOKEN_STAR]            = {NULL,    binary, PREC_FACTOR},
        [TOKEN_SLASH]           = {NULL,    binary, PREC_FACTOR},
        [TOKEN_BANG]            = {NULL,    NULL,   PREC_NONE},
        [TOKEN_BANG_EQUAL]      = {NULL,    NULL,   PREC_NONE},
        [TOKEN_EQUAL]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_EQUAL_EQUAL]     = {NULL,    NULL,   PREC_NONE},
        [TOKEN_GREATER]         = {NULL,    NULL,   PREC_NONE},
        [TOKEN_GREATER_EQUAL]   = {NULL,    NULL,   PREC_NONE},
        [TOKEN_LESS]            = {NULL,    NULL,   PREC_NONE},
        [TOKEN_LESS_EQUAL]      = {NULL,    NULL,   PREC_NONE},
        [TOKEN_IDENTIFIER]      = {NULL,    NULL,   PREC_NONE},
        [TOKEN_STRING]          = {NULL,    NULL,   PREC_NONE},
        [TOKEN_NUMBER]          = {number,  NULL,   PREC_NONE},
        [TOKEN_NIL]             = {NULL,    NULL,   PREC_NONE},
        [TOKEN_AND]             = {NULL,    NULL,   PREC_NONE},
        [TOKEN_OR]              = {NULL,    NULL,   PREC_NONE},
        [TOKEN_TRUE]            = {NULL,    NULL,   PREC_NONE},
        [TOKEN_FALSE]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_FOR]             = {NULL,    NULL,   PREC_NONE},
        [TOKEN_WHILE]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_IF]              = {NULL,    NULL,   PREC_NONE},
        [TOKEN_ELSE]            = {NULL,    NULL,   PREC_NONE},
        [TOKEN_CLASS]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_SUPER]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_THIS]            = {NULL,    NULL,   PREC_NONE},
        [TOKEN_VAR]             = {NULL,    NULL,   PREC_NONE},
        [TOKEN_PRINT]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_ERROR]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_EOF]             = {NULL,    NULL,   PREC_NONE}
};

static ParseRule* getRule(TokenType type){
    return &rules[type];
}

static void parsePrecedence(Precedence prec){
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expected expression.");
        return;
    }

    prefixRule();

    while (prec <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}

/**************************************/

/*********     Compiling      *********/

static void endCompiler(){
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if(!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

bool compile(const char* source, Chunk* chunk){
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
//    consume(TOKEN_EOF, "Expected end of expression.");
    endCompiler();
    return !parser.hadError;
}