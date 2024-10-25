#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    //TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,

    // Comparison tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,

    // Literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER, TOKEN_NIL,

    // Keywords
    TOKEN_AND, TOKEN_OR, TOKEN_TRUE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_WHILE,
    TOKEN_IF, TOKEN_ELSE,
    TOKEN_FUN, TOKEN_RETURN,
    TOKEN_CLASS, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_VAR, TOKEN_PRINT,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif //CLOX_SCANNER_H
