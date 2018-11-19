typedef struct SourcePos
{
    u32 colNumber;
    u32 lineNumber;
    String filename;
} SourcePos;

typedef enum TokenKind
{
    TOKEN_NULL,
    
    TOKEN_MUL = '*',
    TOKEN_DIV = '/',
    // TODO(michiel): etc..
    
    TOKEN_ADD = '+',
    TOKEN_SUB = '-',
    
    TOKEN_ADDR = '@',
    TOKEN_DEREF = '$',
    
    TOKEN_ASSIGN = '=',
    
    TOKEN_NUMBER = 128,
    TOKEN_ID,
    TOKEN_INC,
    TOKEN_DEC,

    TOKEN_SLL,
    TOKEN_SRL,
    TOKEN_SRA,
    
    TOKEN_POW, // TODO(michiel): Implement (what to use?)
    // maybe ** and do something else for addresses and dereferences
    // like @ for address and $ for dereference

TOKEN_EOF,

    NUM_TOKENS, // NOTE(michiel): Always at the end of the enum to indicate how many tokens we have.
} TokenKind;
typedef struct Token
{
    TokenKind kind;
    String    value;

    SourcePos origin;
    
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
