typedef enum TokenKind
{
    TOKEN_NULL,
    TOKEN_NUMBER,
    TOKEN_ID,
    TOKEN_INC,
    TOKEN_DEC,
    TOKEN_NOT,
    TOKEN_INV,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_AND,
    TOKEN_SLL,
    TOKEN_SRL,
    TOKEN_SRA,
    TOKEN_SUB,
    TOKEN_ADD,
    TOKEN_OR,
    TOKEN_XOR,
    TOKEN_PAREN_OPEN,
    TOKEN_PAREN_CLOSE,
    TOKEN_ASSIGN,
    TOKEN_SEMI,
    TOKEN_EOL,
    TOKEN_EOF,

    NUM_TOKENS, // NOTE(michiel): Always at the end of the enum to indicate how many tokens we have.
} TokenKind;
typedef struct Token
{
    TokenKind kind;
    String    value;

    u32 colNumber;
    u32 lineNumber;
    String filename;

    struct Token *nextToken;
} Token;

typedef struct TokenEater
{
    u32 columnNumber;
    u32 lineNumber;

    char *scanner;
} TokenEater;

#define MAX_TOKEN_MEM_CHUNK 2048
internal inline Token *
next_token(Token *tokens, u32 index)
{
    i_expect(index < MAX_TOKEN_MEM_CHUNK);
    return tokens + index;
}
