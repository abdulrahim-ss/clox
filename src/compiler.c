#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

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

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk(){
    return compilingChunk;
}

static void advance();

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

static void synchronize(){
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_WHILE:
            case TOKEN_IF:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
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

static bool check(TokenType type){
    return parser.current.type == type;
}

static bool match(TokenType type){
    if (!check(type)) return false;

    advance();
    return true;
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

static void emitLoop(int loopStart){
    emitByte(OP_LOOP);
    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large!");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction){
    emitByte(instruction);
    emitBytes(0xff, 0xff);
    return currentChunk()->count - 2;
}

static void patchJump(int offset){
    int jump = currentChunk()->count -  offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over!");
//        return; THINK IT SHOULD RETURN OTHERWISE IT'LL STILL TRY AND FAIL
//        TO JUMP OVER CODE LONGER THAN IT COULD HANDLE?
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static uint8_t identifierConstant(Token* name){
    for (int i=0; i<currentChunk()->constants.count; i++){
        Value* constant = &currentChunk()->constants.values[i];
        if (constant->type == VAL_OBJ
            && constant->as.obj->type == OBJ_STRING
            && memcmp(AS_CSTRING(*constant), name->start, name->length) == 0)
            return i;
    }
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/**************************************/

/*******     Compiler State     *******/

static void initCompiler(Compiler* compiler){
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void beginScope(){
    current->scopeDepth++;
}

static void endScope(){
    current->scopeDepth--;

    while (current->localCount > 0
           && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

/**************************************/

/*********     Variables      *********/

static int resolveLocal(Compiler* compiler, Token* name){
    int i=compiler->localCount-1;
    for (; i>=0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(&local->name, name)) {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            break;
//            return i;
        }
    }
    return i; // should be return -1;
}

static void addLocal(Token name){
    if (current->localCount > UINT8_COUNT) {
        error("Too many local variables in scope.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static void declareVariable(){
    Token* name = &parser.previous;

    for (int i=current->localCount-1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
            break;

        if (identifiersEqual(name, &local->name))
            error("Variable with this name already declared in scope.");
    }

    addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage){
    consume(TOKEN_IDENTIFIER, errorMessage);

    if (current->scopeDepth > 0) {
        declareVariable();
        return 0;
    }

    return identifierConstant(&parser.previous);
}

static void markInitialized(){
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global){
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

/**************************************/

/*********     Expressions    *********/

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void number(bool canAssign){
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign){
    emitConstant(OBJ_VAL(copyString(
                parser.previous.start + 1,
                parser.previous.length -2))
            );
}

static void namedVariable(Token name, bool canAssign){
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
        emitBytes(getOp, (uint8_t)arg);
}

static void variable(bool canAssign){
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign){
    TokenType operatorType = parser.previous.type;

    // Parse/compile the operand.
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;

        default: return; // Unreachable.
    }
}

static void grouping(bool canAssign){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void binary(bool canAssign){
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS:  emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:  emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;

        case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
        case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_GREATER: emitByte(OP_GREATER); break;
        case TOKEN_LESS: emitByte(OP_LESS); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
        default: return; // Unreachable
    }
}

static void and_(bool canAssign){
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}
static void or_(bool canAssign){
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void literal(bool canAssign){
    switch (parser.previous.type) {
        case TOKEN_NIL:     emitByte(OP_NIL); break;
        case TOKEN_TRUE:    emitByte(OP_TRUE); break;
        case TOKEN_FALSE:   emitByte(OP_FALSE); break;
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
        [TOKEN_BANG]            = {unary,   NULL,   PREC_NONE},
        [TOKEN_BANG_EQUAL]      = {NULL,    binary, PREC_EQUALITY},
        [TOKEN_EQUAL]           = {NULL,    NULL,   PREC_NONE},
        [TOKEN_EQUAL_EQUAL]     = {NULL,    binary, PREC_EQUALITY},
        [TOKEN_GREATER]         = {NULL,    binary, PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL]   = {NULL,    binary, PREC_COMPARISON},
        [TOKEN_LESS]            = {NULL,    binary, PREC_COMPARISON},
        [TOKEN_LESS_EQUAL]      = {NULL,    binary, PREC_COMPARISON},
        [TOKEN_IDENTIFIER]      = {variable,NULL,   PREC_NONE},
        [TOKEN_STRING]          = {string,  NULL,   PREC_NONE},
        [TOKEN_NUMBER]          = {number,  NULL,   PREC_NONE},
        [TOKEN_NIL]             = {literal, NULL,   PREC_NONE},
        [TOKEN_AND]             = {NULL,    and_,   PREC_AND},
        [TOKEN_OR]              = {NULL,    or_,   PREC_OR},
        [TOKEN_TRUE]            = {literal, NULL,   PREC_NONE},
        [TOKEN_FALSE]           = {literal, NULL,   PREC_NONE},
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

    bool canAssign = prec <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (prec <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
        error("Invalid assignment target.");
}

static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block(){
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        declaration();

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

/**************************************/

/*********     Statements     *********/

static void varDeclaration(){
    uint8_t global = parseVariable("Expected variable name.");
    if (match(TOKEN_EQUAL))
        expression();
    else
        emitByte(OP_NIL);
    consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    defineVariable(global);
}

static void printStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after value.");
    emitByte(OP_PRINT);
}

static void expressionStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
    emitByte(OP_POP);
}

static void ifStatement(){
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void whileStatement(){
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void forStatement(){
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'.");
    // Check if you could replace all entire bit with just: declaration();
    if (match(TOKEN_SEMICOLON)) {
        // Do nothing.
    }
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement();
    // unitl here!

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after condition.");
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); //condition val
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expected ')' after 'for' clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);
    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); //condition val
    }
    endScope();
}

static void statement() {
    if (match(TOKEN_PRINT))
        printStatement();

    else if (match(TOKEN_IF))
        ifStatement();

    else if (match(TOKEN_WHILE))
        whileStatement();

    else if (match(TOKEN_FOR))
        forStatement();

    else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    }

    else
        expressionStatement();
}

static void declaration(){
    if (match(TOKEN_VAR))
        varDeclaration();
    else
        statement();

    if (parser.panicMode) synchronize();
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
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}