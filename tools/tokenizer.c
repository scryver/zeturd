internal void
advance(TokenEater *eater)
{
    ++eater->scanner;
    ++eater->columnNumber;
}

#define CASE1(t1) case t1: { \
    token = next_token(result, tokenIndex++); \
    token->kind = t1; \
    token->value = str_internalize((String){.size=1, .data= (u8 *)eater.scanner}); \
    token->origin.colNumber = eater.columnNumber; \
    advance(&eater); \
} break

#define CASE2(t1, t2, k2) case t1: { \
    token = next_token(result, tokenIndex++); \
    String value = { .size = 1, .data = (u8 *)eater.scanner }; \
    token->kind = t1; \
    token->origin.colNumber = eater.columnNumber; \
    advance(&eater); \
    if (eater.scanner[0] && (eater.scanner[0] == t2)) \
    { \
        ++value.size; \
        token->kind = k2; \
        advance(&eater); \
    } \
    token->value = str_internalize(value); \
} break

#define CASE3(t1, t2, k2, t3, k3) case t1: { \
    token = next_token(result, tokenIndex++); \
    String value = { .size = 1, .data = (u8 *)eater.scanner }; \
    token->kind = t1; \
    token->origin.colNumber = eater.columnNumber; \
    advance(&eater); \
    if (eater.scanner[0] && (eater.scanner[0] == t2)) \
    { \
        ++value.size; \
        token->kind = k2; \
        advance(&eater); \
        if (eater.scanner[0] && (eater.scanner[0] == t3)) \
        { \
            ++value.size; \
            token->kind = k3; \
            advance(&eater); \
        } \
    } \
    token->value = str_internalize(value); \
} break

internal Token *
tokenize(Buffer buffer, String filename)
{
    // NOTE(michiel): Example of allocating token memory in chunks until we run out of chunks
    // TODO(michiel): Run out of chunks
    Token *result = allocate_array(MAX_TOKEN_MEM_CHUNK, Token, 0);
    Token *prevToken = NULL;
    u32 tokenIndex = 0;

    TokenEater eater = {1, 1, (char *)buffer.data};
    while (*eater.scanner)
    {
        Token *token = NULL;
        switch (eater.scanner[0])
        {
            case ' ':
            case '\r':
            case '\t':
        {
            // NOTE(michiel): Skip these
            advance(&eater);
            } break;
            
            CASE1('\n');
            CASE1(';');
            CASE1('=');
            CASE1('(');
            CASE1(')');
            CASE1('!');
            CASE1('~');
            CASE1('&');
            CASE1('|');
            CASE1('^');
            CASE1('@');
            CASE1('$');
            
            CASE2('*', '*', TOKEN_POW);
            CASE2('-', '-', TOKEN_DEC);
            CASE2('+', '+', TOKEN_INC);
            
            CASE2('<', '<', TOKEN_SLL);
            CASE3('>', '>', TOKEN_SRA, '>', TOKEN_SRL);
            
            case '/':
            {
                token = next_token(result, tokenIndex++);
                    String value = { .size = 1, .data = (u8 *)eater.scanner };
                    token->kind = TOKEN_DIV;
                    token->origin.colNumber = eater.columnNumber;
                    advance(&eater);
                    if (eater.scanner[0] && (eater.scanner[0] == '/'))
                {
                        token->kind = TOKEN_LINE_COMMENT;
                    while (eater.scanner[0] != '\n')
                    {
                        ++value.size;
                        advance(&eater);
                    }
                        }
                    token->value = str_internalize(value);
            } break;
            
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
        {
            token = next_token(result, tokenIndex++);
            token->kind = TOKEN_NUMBER;
            String value = {
                .size = 1,
                .data = (u8 *)eater.scanner,
            };
                token->origin.colNumber = eater.columnNumber;
            if ((eater.scanner[0] == '0') &&
                ((eater.scanner[1] == 'x') ||
                 (eater.scanner[1] == 'X') ||
                 (eater.scanner[1] == 'b') ||
                 (eater.scanner[1] == 'B')))
            {
                ++value.size;
                advance(&eater);
            }
            advance(&eater);

            while (('0' <= eater.scanner[0]) && (eater.scanner[0] <= '9'))
            {
                ++value.size;
                advance(&eater);
            }
            token->value = str_internalize(value);
            } break;
            
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case '_':
        {
            token = next_token(result, tokenIndex++);
            token->kind = TOKEN_ID;
            String value = {
                .size = 1,
                .data = (u8 *)eater.scanner,
            };
                token->origin.colNumber = eater.columnNumber;
            advance(&eater);
            while ((eater.scanner[0] == '_') ||
                   (('A' <= eater.scanner[0]) && (eater.scanner[0] <= 'Z')) ||
                   (('a' <= eater.scanner[0]) && (eater.scanner[0] <= 'z')) ||
                   (('0' <= eater.scanner[0]) && (eater.scanner[0] <= '9')))
            {
                ++value.size;
                advance(&eater);
            }
            token->value = str_internalize(value);
            } break;
            
            default:
        {
            fprintf(stderr, "Unhandled scan item: %c\n", eater.scanner[0]);
            advance(&eater);
            } break;
    }
        
        if (token)
        {
            token->origin.lineNumber = eater.lineNumber;
            token->origin.filename = filename;
            if (token->kind == '\n')
            {
                ++eater.lineNumber;
                eater.columnNumber = 1;
            }
            
            if (prevToken)
            {
                prevToken->nextToken = token;
            }
            prevToken = token;
        }
    }

    if ((prevToken->kind != '\n') &&
        (prevToken->kind != ';'))
    {
        //fprintf(stderr, "The Tokenizer expects the token stream to end with a newline or semi-colon, but you're forgiven for now...\n");
        Token *token = next_token(result, tokenIndex++);
        token->kind = TOKEN_EOF;
        token->value = str_internalize_cstring("");
        token->origin.colNumber = 0;
        token->origin.lineNumber = eater.lineNumber;
        token->origin.filename = filename;
        prevToken->nextToken = token;
    }

    return result;
}

#undef CASE3
#undef CASE2
#undef CASE1

internal Token *
tokenize_string(String tokenString)
{
    String anonymous = str_internalize_cstring("<anonymous>");
    return tokenize(*(Buffer *)&tokenString, anonymous);
}

internal Token *
tokenize_file(char *filename)
{
    Token *result = 0;
    String fileName = str_internalize_cstring(filename);
    // TODO(michiel): read_entire_file(String) ??
    Buffer fileBuffer = read_entire_file(filename);
    if (fileBuffer.size)
    {
     result = tokenize(fileBuffer, fileName);
    }
    return result;
}

#define CASE(name) case TOKEN_##name: { fprintf(fileStream.file, #name); } break
#define CASEc(name) case name: { fprintf(fileStream.file, "%c", name); } break
internal void
print_token_kind(FileStream fileStream, TokenKind kind)
{
    switch ((u32)kind)
    {
        CASE(NULL);
        CASE(NUMBER);
        CASE(ID);
        CASE(INC);
        CASE(DEC);

        CASEc('~');
        CASEc('!');
        CASEc('*');
        CASEc('/');
        CASEc('&');
        
        CASE(SLL);
CASE(SRL);
        CASE(SRA);

        CASEc('-');
        CASEc('+');
        CASEc('|');
        CASEc('^');
        CASEc('=');
        CASEc(';');
        CASEc('\n');

        default: {
            fprintf(stdout, "unknown");
        } break;
    }
}
#undef CASE

internal void
print_token(FileStream fileStream, Token *token)
{
    fprintf(fileStream.file, "%.*s:%d:%d < ", token->origin.filename.size,
            token->origin.filename.data, token->origin.lineNumber, token->origin.colNumber);
    print_token_kind(fileStream, token->kind);
    if (token->kind == '\n')
    {
        fprintf(fileStream.file, ",\\n");
    }
    else if (token->value.size)
    {
        fprintf(fileStream.file, ",%.*s", token->value.size, token->value.data);
    }
    fprintf(fileStream.file, " >");
}

internal void
print_tokens(Token *tokens)
{
    for (Token *it = tokens; it; it = it->nextToken)
    {
        print_token((FileStream){.file=stdout}, it);
        fprintf(stdout, "\n");
    }
}
