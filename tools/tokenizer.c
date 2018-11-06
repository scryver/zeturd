internal void
advance(TokenEater *eater)
{
    ++eater->scanner;
    ++eater->columnNumber;
}

#define SIMPLE_TOKEN(type) token = next_token(result, tokenIndex++); \
    token->kind = TOKEN_##type; \
    token->value = str_internalize((String){.size=1, .data= (u8 *)eater.scanner}); \
    token->colNumber = eater.columnNumber; \
    advance(&eater)

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
        if ((eater.scanner[0] == ' ') ||
            (eater.scanner[0] == '\r') ||
            (eater.scanner[0] == '\t'))
        {
            // NOTE(michiel): Skip these
            advance(&eater);
        }
        else if (eater.scanner[0] == '\n')
        {
            SIMPLE_TOKEN(EOL);
        }
        else if (eater.scanner[0] == ';')
        {
            SIMPLE_TOKEN(SEMI);
        }
        else if (eater.scanner[0] == '=')
        {
            SIMPLE_TOKEN(ASSIGN);
        }
        else if (eater.scanner[0] == '(')
        {
            SIMPLE_TOKEN(PAREN_OPEN);
        }
        else if (eater.scanner[0] == ')')
        {
            SIMPLE_TOKEN(PAREN_CLOSE);
        }
        else if (eater.scanner[0] == '!')
        {
            SIMPLE_TOKEN(NOT);
        }
        else if (eater.scanner[0] == '~')
        {
            SIMPLE_TOKEN(INV);
        }
        else if (eater.scanner[0] == '*')
        {
            SIMPLE_TOKEN(MUL);
        }
        else if (eater.scanner[0] == '/')
        {
            SIMPLE_TOKEN(DIV);
        }
        else if (eater.scanner[0] == '&')
        {
            SIMPLE_TOKEN(AND);
        }
        else if (eater.scanner[0] == '<')
        {
            if (eater.scanner[1] && (eater.scanner[1] == '<'))
            {
                // NOTE(michiel): >> == Logical right shift
                token = next_token(result, tokenIndex++);
                token->value = str_internalize((String){.size=2, .data=(u8 *)eater.scanner});
                token->kind = TOKEN_SLL;
                token->colNumber = eater.columnNumber;
                advance(&eater);
                advance(&eater);
            }
            else
            {
                fprintf(stderr, "Incomplete shift left operator\n");
                advance(&eater);
            }
        }
        else if (eater.scanner[0] == '>')
        {
            if (eater.scanner[1] && (eater.scanner[1] == '>'))
            {
                // NOTE(michiel): >> == Logical right shift
                token = next_token(result, tokenIndex++);
                String value = {
                    .size = 2,
                    .data = (u8 *)eater.scanner,
                };
                token->kind = TOKEN_SRA;
                token->colNumber = eater.columnNumber;
                if (eater.scanner[2] && (eater.scanner[2] == '>'))
                {
                    // NOTE(michiel): >>> == Logical right shift
                    ++value.size;
                    token->kind = TOKEN_SRL;
                    advance(&eater);
                }
                token->value = str_internalize(value);
                advance(&eater);
                advance(&eater);
            }
            else
            {
                fprintf(stderr, "Incomplete shift right operator\n");
                advance(&eater);
            }
        }
        else if (eater.scanner[0] == '-')
        {
            if (eater.scanner[1] && (eater.scanner[1] == '-'))
            {
                token = next_token(result, tokenIndex++);
                token->value = str_internalize((String){.size=2, .data=(u8 *)eater.scanner});
                token->kind = TOKEN_DEC;
                token->colNumber = eater.columnNumber;
                advance(&eater);
                advance(&eater);
            }
            else
            {
                SIMPLE_TOKEN(SUB);
            }
        }
        else if (eater.scanner[0] == '+')
        {
            if (eater.scanner[1] && (eater.scanner[1] == '+'))
            {
                token = next_token(result, tokenIndex++);
                token->value = str_internalize((String){.size=2, .data=(u8 *)eater.scanner});
                token->kind = TOKEN_INC;
                token->colNumber = eater.columnNumber;
                advance(&eater);
                advance(&eater);
            }
            else
            {
                SIMPLE_TOKEN(ADD);
            }
        }
        else if (eater.scanner[0] == '|')
        {
            SIMPLE_TOKEN(OR);
        }
        else if (eater.scanner[0] == '^')
        {
            SIMPLE_TOKEN(XOR);
        }
        else if (('0' <= eater.scanner[0]) && (eater.scanner[0] <= '9'))
        {
            token = next_token(result, tokenIndex++);
            token->kind = TOKEN_NUMBER;
            String value = {
                .size = 1,
                .data = (u8 *)eater.scanner,
            };
            token->colNumber = eater.columnNumber;
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
        }
        else if ((eater.scanner[0] == '_') ||
                 (('A' <= eater.scanner[0]) && (eater.scanner[0] <= 'Z')) ||
                 (('a' <= eater.scanner[0]) && (eater.scanner[0] <= 'z')))
        {
            token = next_token(result, tokenIndex++);
            token->kind = TOKEN_ID;
            String value = {
                .size = 1,
                .data = (u8 *)eater.scanner,
            };
            token->colNumber = eater.columnNumber;
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
        }
        else
        {
            fprintf(stderr, "Unhandled scan item: %c\n", eater.scanner[0]);
            advance(&eater);
        }

        if (token)
        {
            token->lineNumber = eater.lineNumber;
            token->filename = filename;
            if (token->kind == TOKEN_EOL)
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

    if ((prevToken->kind != TOKEN_EOL) &&
        (prevToken->kind != TOKEN_SEMI))
    {
        //fprintf(stderr, "The Tokenizer expects the token stream to end with a newline or semi-colon, but you're forgiven for now...\n");
        Token *token = next_token(result, tokenIndex++);
        token->kind = TOKEN_EOF;
        token->value = str_internalize_cstring("");
        token->colNumber = 0;
        token->lineNumber = eater.lineNumber;
        token->filename = filename;
        prevToken->nextToken = token;
    }

    return result;
}

#undef SIMPLE_TOKEN

internal Token *
tokenize_string(String tokenString)
{
    String anonymous = str_internalize_cstring("<anonymous>");
    return tokenize(*(Buffer *)&tokenString, anonymous);
}

internal Token *
tokenize_file(char *filename)
{
    String fileName = str_internalize_cstring(filename);
    // TODO(michiel): read_entire_file(String) ??
    Buffer fileBuffer = read_entire_file(filename);
    return tokenize(fileBuffer, fileName);
}

#define CASE(name) case TOKEN_##name: { fprintf(fileStream.file, #name); } break
internal void
print_token_kind(FileStream fileStream, Token *token)
{
    switch (token->kind)
    {
        CASE(NULL);
        CASE(NUMBER);
        CASE(ID);
        CASE(INC);
        CASE(DEC);
        CASE(INV);
        CASE(NOT);
        CASE(MUL);
        CASE(DIV);
        CASE(AND);
        CASE(SLL);
        CASE(SRL);
        CASE(SRA);
        CASE(SUB);
        CASE(ADD);
        CASE(OR);
        CASE(XOR);
        CASE(ASSIGN);
        CASE(SEMI);
        CASE(EOL);

        default: {
            fprintf(stdout, "unknown");
        } break;
    }
}
#undef CASE

internal void
print_token(FileStream fileStream, Token *token)
{
    fprintf(fileStream.file, "%.*s:%d:%d < ", token->filename.size, token->filename.data,
            token->lineNumber, token->colNumber);
    print_token_kind(fileStream, token);
    if (token->kind == TOKEN_EOL)
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
