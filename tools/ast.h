typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum ExprKind
{
    Expr_None,
    Expr_Paren,
    Expr_Int,
    Expr_Id,
    Expr_Unary,
    Expr_Binary,
} ExprKind;

struct Expr
{
    SourcePos origin;
    
    ExprKind kind;
    
    union {
        struct
        {
            Expr *expr;
        } paren;
        s64 intConst;
        String name;
        struct
        {
            TokenKind op;
            Expr *expr;
        } unary;
        struct
        {
            TokenKind op;
            Expr *left;
            Expr *right;
        } binary;
        Expr *nextFree;
    };
};

typedef enum StmtKind
{
    Stmt_None,
    Stmt_Assign,
    Stmt_Hint,
} StmtKind;

struct Stmt
{
    SourcePos origin;
    StmtKind kind;
    
    union
    {
        Expr *expr;
        struct
        {
            TokenKind op;
            Expr *left;
            Expr *right;
        } assign;
        Stmt *nextFree;
    };
};

typedef struct StmtList
{
    u64  stmtCount;
    Stmt **stmts;
} StmtList;

typedef struct AstParser
{
    Token *tokens;
    Token *current;
} AstParser;

typedef struct AstOptimizer
{
    StmtList statements;
    Expr *exprFreeList;
    Stmt *stmtFreeList;
} AstOptimizer;
