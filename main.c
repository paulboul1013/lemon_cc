#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

//define Token type
typedef  enum{
    TOK_CHAR, TOK_CHAR_CONSTANT, TOK_STRING, TOK_INT, TOK_LONG, TOK_SIGNED, TOK_UNSIGNED, TOK_DOUBLE, TOK_VOID, TOK_RETURN,
    TOK_STATIC, TOK_EXTERN,

    TOK_IF,
    TOK_ELSE,
    TOK_GOTO,

    TOK_DO,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,

    TOK_SWITCH,
    TOK_CASE,
    TOK_DEFAULT,
    TOK_SIZEOF,

    TOK_IDENTIFIER, TOK_CONSTANT, TOK_LONG_CONSTANT,
    TOK_UNSIGNED_CONSTANT, TOK_UNSIGNED_LONG_CONSTANT, TOK_DOUBLE_CONSTANT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMICOLON, 
    TOK_COMMA,
    
    TOK_QUESTION, //?
    TOK_COLON, //:
    
    TOK_EOF, 
    TOK_INVALID,

    TOK_NEGATION, // -
    TOK_COMPLEMENT, // ~
    TOK_DECREMENT, // --

    TOK_PLUS, //+
    TOK_MULT, //*
    TOK_DIV, // /
    TOK_REMAINDER, //%

    TOK_LOGICAL_NOT, //!
    TOK_AND, // &&
    TOK_OR, // ||
    TOK_EQ, // ==
    TOK_NEQ, // !=
    TOK_LT, // <
    TOK_GT, // >
    TOK_LTE, // <=
    TOK_GTE, // >=
    TOK_ASSIGN, // =

    TOK_INCREMENT, // ++
    TOK_XOR,// ^

     TOK_BITWISE_AND, // &
    TOK_BITWISE_OR,  // |

    TOK_LSHIFT, // <<
    TOK_RSHIFT, // >>

    TOK_PLUS_ASSIGN, // +=
    TOK_MINUS_ASSIGN, // -=
    TOK_MULT_ASSIGN, // *=
    TOK_DIV_ASSIGN, // /=
    TOK_REM_ASSIGN, // %=
    TOK_AND_ASSIGN, // &=
    TOK_OR_ASSIGN, // |=
    
    TOK_XOR_ASSIGN, // ^=
    TOK_LSHIFT_ASSIGN, // <<=
    TOK_RSHIFT_ASSIGN, // >>=

}TokenType;

typedef struct {
    TokenType type;
    char *value;
}Token;

typedef enum {
    TYPE_CHAR, TYPE_SCHAR, TYPE_UCHAR,
    TYPE_INT,
    TYPE_LONG,
    TYPE_UINT,
    TYPE_ULONG,
    TYPE_DOUBLE,
    TYPE_VOID,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION
} TypeKind;

typedef struct Type {
    TypeKind kind;
    struct Type **params; //for TYPE_FUNCTION
    int param_count;
    struct Type *ret;
    struct Type *referenced; //pointer referenced type or array element
    unsigned long count; //TYPE_ARRAY
} Type;

Type *type_int(void){
    static Type t={.kind=TYPE_INT};
    return &t;
}

Type *type_long(void){
    static Type t={.kind=TYPE_LONG};
    return &t;
}

Type *type_uint(void){
    static Type t={.kind=TYPE_UINT};
    return &t;
}

Type *type_ulong(void){
    static Type t={.kind=TYPE_ULONG};
    return &t;
}

Type *type_double(void){
    static Type t={.kind=TYPE_DOUBLE};
    return &t;
}

Type *type_char(void){ static Type t={.kind=TYPE_CHAR}; return &t; }
Type *type_schar(void){ static Type t={.kind=TYPE_SCHAR}; return &t; }
Type *type_uchar(void){ static Type t={.kind=TYPE_UCHAR}; return &t; }
Type *type_void(void){ static Type t={.kind=TYPE_VOID}; return &t; }

int is_character_type(Type *t){
    return t->kind==TYPE_CHAR || t->kind==TYPE_SCHAR || t->kind==TYPE_UCHAR;
}

Type *integer_promotion(Type *t){ return is_character_type(t) ? type_int() : t; }

int type_size(Type *t){
    if (is_character_type(t)) return 1;
    if (t->kind==TYPE_ARRAY){
        int element_size=type_size(t->referenced);
        if (t->count<=0 || t->count>(unsigned long)(INT_MAX/element_size)){
            fprintf(stderr,"Type Error: array object is too large\n");
            exit(1);
        }
        return (int)t->count*element_size;
    }
    return (t->kind==TYPE_LONG || t->kind==TYPE_ULONG || t->kind==TYPE_DOUBLE || t->kind==TYPE_POINTER) ? 8 : 4;
}

//sizeof may name a type far larger than any object type_size accepts
unsigned long type_bytes(Type *t){
    if (t->kind==TYPE_ARRAY) return t->count*type_bytes(t->referenced);
    return (unsigned long)type_size(t);
}

int type_is_signed(Type *t){
    return t->kind==TYPE_CHAR || t->kind==TYPE_SCHAR || t->kind==TYPE_INT || t->kind==TYPE_LONG;
}

//wrap v to the range of t, keeping the bit pattern in a long
long convert_value(long v,Type *t){
    switch (t->kind){
        case TYPE_CHAR: case TYPE_SCHAR: return (signed char)v;
        case TYPE_UCHAR: return (unsigned char)v;
        case TYPE_INT: return (int)v;
        case TYPE_UINT: return (unsigned int)v;
        default: return v;
    }
}

Type *make_function_type(Type **params,int param_count,Type *ret){
    Type *t=calloc(1,sizeof(Type));
    t->kind=TYPE_FUNCTION;
    t->params=params;
    t->param_count=param_count;
    t->ret=ret;
    return t;
}

Type *make_pointer_type(Type *referenced){
    Type *t=calloc(1,sizeof(Type));
    t->kind=TYPE_POINTER;
    t->referenced=referenced;
    return t;
}

Type *make_array_type(Type *element,unsigned long count){
    Type *t=calloc(1,sizeof(Type));
    t->kind=TYPE_ARRAY; t->referenced=element; t->count=count;
    return t;
}

int type_equal(Type *a,Type *b){
    if (a->kind!=b->kind) return 0;
    if (a->kind==TYPE_ARRAY) return a->count==b->count && type_equal(a->referenced,b->referenced);
    if (a->kind==TYPE_POINTER) return type_equal(a->referenced,b->referenced);
    if (a->kind!=TYPE_FUNCTION) return 1;

    if (a->param_count!=b->param_count) return 0;
    if (!type_equal(a->ret,b->ret)) return 0;
    for (int i=0;i<a->param_count;i++){
        if (!type_equal(a->params[i],b->params[i])) return 0;
    }
    return 1;
}

//Void, arrays and functions are the only non-scalar types
int is_scalar(Type *t){
    return t->kind!=TYPE_VOID && t->kind!=TYPE_ARRAY && t->kind!=TYPE_FUNCTION;
}

int is_complete(Type *t){
    return t->kind!=TYPE_VOID;
}

int is_pointer_to_complete(Type *t){
    return t->kind==TYPE_POINTER && is_complete(t->referenced);
}

//an array of an incomplete type is illegal however deep it is nested
void validate_type_specifier(Type *t){
    switch (t->kind){
        case TYPE_ARRAY:
            if (!is_complete(t->referenced)){ fprintf(stderr,"Type Error: illegal array of incomplete type\n"); exit(1); }
            validate_type_specifier(t->referenced);
            return;
        case TYPE_POINTER:
            validate_type_specifier(t->referenced);
            return;
        case TYPE_FUNCTION:
            for (int i=0;i<t->param_count;i++) validate_type_specifier(t->params[i]);
            validate_type_specifier(t->ret);
            return;
        default:
            return;
    }
}

typedef enum {
    CONST_CHAR, CONST_UCHAR,
    CONST_INT,
    CONST_LONG,
    CONST_UINT,
    CONST_ULONG,
    CONST_DOUBLE
} ConstType;

typedef struct {
    ConstType type;
    union {
        long value; //bit pattern, already wrapped by convert_value
        double dvalue; //CONST_DOUBLE
    };
} Const;

Type *const_exp_type(Const c){
    switch (c.type){
        case CONST_CHAR: return type_schar();
        case CONST_UCHAR: return type_uchar();
        case CONST_LONG: return type_long();
        case CONST_UINT: return type_uint();
        case CONST_ULONG: return type_ulong();
        case CONST_DOUBLE: return type_double();
        default: return type_int();
    }
}

typedef enum {
    INIT_STRING, INIT_POINTER,
    INIT_CHAR, INIT_UCHAR,
    INIT_INT,
    INIT_LONG,
    INIT_UINT,
    INIT_ULONG,
    INIT_DOUBLE,
    INIT_ZERO
} StaticInitType;

typedef struct { char *data; int length; } ByteString;

typedef struct {
    StaticInitType type;
    union {
        long value;
        double dvalue; //INIT_DOUBLE
        struct { ByteString bytes; int null_terminated; } string;
        char *pointer_name;
    };
} StaticInit;

typedef struct { StaticInit *items; int count; } StaticInitList;

typedef enum {
    EXP_STRING,
    EXP_CONSTANT,
    EXP_CAST,
    EXP_UNARY,
    EXP_BINARY,
    EXP_VAR,
    EXP_ASSIGN,
    EXP_COMPOUND_ASSIGN, // += -= *= /= %=
    EXP_PREFIX_OP, //++a,--a
    EXP_POSTFIX_OP, //a++,a--
    EXP_CONDITIONAL,
    EXP_FUNCTION_CALL,
    EXP_DEREFERENCE, // *e
    EXP_ADDR_OF, // &e
    EXP_SUBSCRIPT,
    EXP_SIZEOF, // sizeof e
    EXP_SIZEOF_TYPE, // sizeof (type-name)
}ExpType;

typedef enum {
    BINARY_ADD, BINARY_SUB, BINARY_MULT, BINARY_DIV, BINARY_REM,
    BINARY_LSHIFT, BINARY_RSHIFT,
    BINARY_BITWISE_AND, BINARY_BITWISE_OR, BINARY_BITWISE_XOR, // bit operation
    BINARY_LOGICAL_AND, BINARY_LOGICAL_OR, // logical operation
    BINARY_EQ, BINARY_NEQ, BINARY_LT, BINARY_LTE, BINARY_GT, BINARY_GTE,
    BINARY_SHR, // asm only: logical right shift of an unsigned operand

}BinaryOpType;

typedef enum {
    UNARY_NEGATION, // -
    UNARY_COMPLEMENT, // ~
    UNARY_NOT // !
}UnaryOpType;

typedef struct Exp {
    ExpType type;
    Type *exp_type; //set by the type checker
    union {
        Const constant; //for EXP_CONSTANT
        struct { ByteString bytes; char *name; } string;
        char *var_name; //for EXP_VAR
        struct { //for EXP_CAST
            Type *target_type;
            struct Exp *exp;
        } cast;

        struct { //for EXP_UNARY
            UnaryOpType op;
            struct Exp *exp;
        }unary;

        struct {
            BinaryOpType op;
            struct Exp *left;
            struct Exp *right;
        }binary;

        struct { //for EXP_ASSIGN
            struct Exp *lvalue;
            struct Exp *rvalue;
        } assign;

        struct {
            BinaryOpType op; //record += or -= ...
            struct Exp *lvalue;
            struct Exp *rvalue;
        } compound;

        struct {
           struct Exp *lvalue; //use Exp node replace pure string
           int is_increment; // 1 for ++ ,0 for --
        } inc_dec;      

        struct {
            struct Exp *condition;
            struct Exp *true_expr;
            struct Exp *false_expr;
        } conditional;

        struct {
            char *name;
            struct Exp **args;
            int arg_count;
        } function_call;

        struct { struct Exp *left,*right; } subscript;

        struct { //for EXP_DEREFERENCE
            struct Exp *exp;
        } deref;

        struct { //for EXP_ADDR_OF
            struct Exp *exp;
        } addr_of;

        struct Exp *sizeof_exp; //for EXP_SIZEOF
        Type *sizeof_type; //for EXP_SIZEOF_TYPE
    };
} Exp;

typedef struct Initializer {
    int compound;
    Type *type;
    Exp *exp;
    struct Initializer **items;
    int count;
} Initializer;

// for save lexer make all tokens
Token * token_list=NULL;
int token_count=0;
int token_pos=0;

// c AST Node
typedef enum {
    STMT_RETURN,
    STMT_EXPRESSION,
    STMT_IF,
    STMT_NULL,
    STMT_GOTO,
    STMT_LABEL,
    STMT_COMPOUND,

    STMT_BREAK,
    STMT_CONTINUE,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_FOR,   

    STMT_SWITCH,
    STMT_CASE,
    STMT_DEFAULT,

}StmtType;

typedef enum {
    STORAGE_NONE,
    STORAGE_STATIC,
    STORAGE_EXTERN
} StorageClass;

typedef struct {
    char *name;
    Initializer *init; //optional
    Type *var_type;
    StorageClass storage_class;
} VariableDeclaration;

typedef struct Function Function;

typedef enum {
    DECL_VAR,
    DECL_FUN
} DeclarationType;

typedef struct Declaration {
    DeclarationType type;
    union {
        VariableDeclaration *var;
        Function *fun;
    };
} Declaration;

typedef enum {
    FOR_INIT_DECL,
    FOR_INIT_EXP
}ForInitType;

typedef struct {
    ForInitType type;
    VariableDeclaration *decl;// when type is FOR_INIT_DECL
    Exp *exp;// when type is FOR_INIT_EXP , NULL means empty
} ForInit;

typedef enum {
    BI_STMT,
    BI_DECL
} BlockItemType;

typedef struct SwitchCaseInfo{
    long value;
    char *label;
    struct SwitchCaseInfo *next;
}SwitchCaseInfo;

typedef struct BlockItem BlockItem;
typedef struct Statement{
    StmtType type;
    Exp *exp;// return / expression

    struct {
        Exp *condition;
        struct Statement *then_branch;
        struct Statement *else_branch;
    } if_stmt;

    struct {
        Exp *condition;
        struct Statement *body;
    }while_stmt;

    struct {
        struct Statement *body;
        Exp *condition;
    } do_while_stmt;

    struct {
        ForInit *init;
        Exp *condition; //optional
        Exp *post; //optional
        struct Statement *body;
    } for_stmt;

    BlockItem **block_items;
    int block_count;

    char *label;
    char *loop_label;
    /*for goto label
                label:        */

    struct {
        Exp *control;
        struct Statement *body;
        SwitchCaseInfo *cases;
        char *default_label;
    } switch_stmt;

    struct {
        Exp *value_exp; //origin case expresion
        long value;     // converted to the switch control's type after type checking
        struct Statement *body;
        char *case_label; // TACKY or asm use inner label
    } case_stmt;

    struct {
        struct Statement *body;
        char *default_label; // TACKY or asm use inner label
    } default_stmt;


} Statement;


typedef struct BlockItem{
    BlockItemType type;
    union {
        Statement *stmt;
        Declaration *decl;
    };
} BlockItem;

typedef struct Function {
    char *name; 

    char **params;
    int param_count;

    BlockItem **body; 
    int body_count;

    int has_body; //1= function definition, 0 = function declaration
    Type *fun_type;
    StorageClass storage_class;
} Function;

typedef struct {
    Declaration **declarations;
    int declaration_count;
} Program;




//Exp helper function
//build constant node
Exp *make_constant_exp(Const c){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_CONSTANT;
    e->constant=c;
    return e;
}

Exp *make_cast_exp(Type *target_type,Exp *inner){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_CAST;
    e->cast.target_type=target_type;
    e->cast.exp=inner;
    return e;
}

//build unary node
Exp *make_unary_exp(UnaryOpType op,Exp *inner){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_UNARY;
    e->unary.op=op;
    e->unary.exp=inner;
    return e;
}

Exp *make_binary_exp(BinaryOpType op,Exp *left,Exp *right){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_BINARY;
    e->binary.op=op;
    e->binary.left=left;
    e->binary.right=right;
    return e;
}

//var AST node
Exp *make_var_exp(char *name){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_VAR;
    e->var_name=strdup(name);
    return e;
}

Exp *make_assign_exp(Exp *left,Exp *right){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_ASSIGN;
    e->assign.lvalue=left;
    e->assign.rvalue=right;
    return e;
}

Exp *make_compound_assign_exp(BinaryOpType op,Exp *left,Exp *right){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_COMPOUND_ASSIGN;
    e->compound.op=op;
    e->compound.lvalue=left;
    e->compound.rvalue=right;
    return e;
}

Exp *make_inc_dec_exp(ExpType type,int is_increment,Exp *lvalue){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=type;
    e->inc_dec.lvalue=lvalue;
    e->inc_dec.is_increment=is_increment;
    return e;
}

Exp *make_dereference_exp(Exp *inner){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_DEREFERENCE;
    e->deref.exp=inner;
    return e;
}

Exp *make_addr_of_exp(Exp *inner){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_ADDR_OF;
    e->addr_of.exp=inner;
    return e;
}

Exp *make_function_call_exp(char *name, Exp **args,int arg_count){
    Exp *e=calloc(1,sizeof(Exp));
    e->type=EXP_FUNCTION_CALL;
    e->function_call.name=strdup(name);
    e->function_call.args=args;
    e->function_call.arg_count=arg_count;
    return e;
}

//precedence section

//build precedence table
int get_precedence(TokenType op){
    switch (op) {
        case TOK_MULT:
        case TOK_DIV:
        case TOK_REMAINDER:
            return 50;
        case TOK_PLUS:
        case TOK_NEGATION: //it's minus
            return 45;
        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return 40;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE:
            return 35;
        case TOK_EQ:
        case TOK_NEQ:
            return 30;
        case TOK_BITWISE_AND:
            return 25;
        case TOK_XOR:
            return 20;
        case TOK_BITWISE_OR:
            return 15;
        case TOK_AND:
            return 10;
        case TOK_OR:
            return 5;
        case TOK_QUESTION:
            return 3;
        case TOK_ASSIGN:
        case TOK_PLUS_ASSIGN:
        case TOK_MINUS_ASSIGN:
        case TOK_MULT_ASSIGN:
        case TOK_DIV_ASSIGN:
        case TOK_REM_ASSIGN:
        case TOK_AND_ASSIGN:
        case TOK_OR_ASSIGN:
        case TOK_XOR_ASSIGN:
        case TOK_LSHIFT_ASSIGN:
        case TOK_RSHIFT_ASSIGN:
            return 1;
        default: //not binary operator
            return -1;
    }
}

BinaryOpType token_to_binary_op(TokenType type){
    switch (type){
        case TOK_PLUS: return BINARY_ADD;
        case TOK_NEGATION: return BINARY_SUB;
        case TOK_MULT: return BINARY_MULT;
        case TOK_DIV: return BINARY_DIV;
        case TOK_REMAINDER: return BINARY_REM;


        case TOK_AND: return BINARY_LOGICAL_AND;
        case TOK_OR: return BINARY_LOGICAL_OR;

        case TOK_XOR: return BINARY_BITWISE_XOR;
        case TOK_BITWISE_AND: return BINARY_BITWISE_AND;
        case TOK_BITWISE_OR: return BINARY_BITWISE_OR;

        case TOK_LSHIFT: return BINARY_LSHIFT;
        case TOK_RSHIFT: return BINARY_RSHIFT;


        case TOK_EQ: return BINARY_EQ;
        case TOK_NEQ: return BINARY_NEQ;

        case TOK_LT: return BINARY_LT;
        case TOK_LTE: return BINARY_LTE;
        case TOK_GT: return BINARY_GT;
        case TOK_GTE: return BINARY_GTE;
        default: exit(1);
    }
}


BinaryOpType compound_token_to_binary_op(TokenType type){
    switch (type){
        case TOK_PLUS_ASSIGN: return BINARY_ADD;
        case TOK_MINUS_ASSIGN: return BINARY_SUB;
        case TOK_MULT_ASSIGN: return BINARY_MULT;
        case TOK_DIV_ASSIGN: return BINARY_DIV;
        case TOK_REM_ASSIGN: return BINARY_REM;
        case TOK_AND_ASSIGN: return BINARY_BITWISE_AND;
        case TOK_OR_ASSIGN: return BINARY_BITWISE_OR;
        case TOK_XOR_ASSIGN: return BINARY_BITWISE_XOR;
        case TOK_LSHIFT_ASSIGN: return BINARY_LSHIFT;
        case TOK_RSHIFT_ASSIGN: return BINARY_RSHIFT;
        default: exit(1);
    }
}

//condition code
typedef enum {
    COND_E,COND_NE,COND_G,COND_GE,COND_L,COND_LE,
    COND_A,COND_AE,COND_B,COND_BE,
    COND_P,COND_NP //parity: set when comisd sees a NaN
} CondCode;

//Asm AST Node
typedef enum {
    AS_MOV,
    AS_MOVSX, // sign extend src_type to asm_type
    AS_MOVZX, // zero extend src_type to asm_type
    AS_NEG,
    AS_NOT,
    AS_RET,
    AS_PUSH,             //  push an argument
    AS_CALL,             //  call function
    AS_BINARY, //for add,sub,mult
    AS_IDIV, //div and get remainder
    AS_DIV, //unsigned div
    AS_CDQ, //sign extend EAX to EDX
    AS_CMP, //cmpl
    AS_CVTTSD2SI, //double to signed integer; asm_type is the destination's
    AS_CVTSI2SD, //signed integer to double; asm_type is the source's
    AS_JMP, // jmp
    AS_JMP_CC, // setCC
    AS_SET_CC, // label:
    AS_LABEL,
    AS_LEA // load effective address
} AsmInstType;



typedef enum {
    AS_IMM, //immediate value
    AS_REG, // register
    AS_PSEUDO,
    AS_PSEUDO_MEM,
    AS_INDEXED, //virtual register (tmp.0)
    AS_MEMORY, // base register plus offset: -4(%rbp), 16(%rbp), (%rax)
    AS_DATA // static storage, foo(%rip)
} AsmOpType;

//emission picks the 8/32/64-bit name from the operand size
typedef enum{
    REG_AX,
    REG_CX,  //for shift and fourth argument
    REG_DX,  //for idiv and third argument
    REG_DI,  //  first argument
    REG_SI,  //  second argument
    REG_R8,  //  fifth argument
    REG_R9,  //  sixth argument
    REG_R10, //for source operand fix-up
    REG_R11, //for destination operand fix-up
    REG_SP,
    REG_BP,
    REG_XMM0, REG_XMM1, REG_XMM2, REG_XMM3, //double arguments and return value
    REG_XMM4, REG_XMM5, REG_XMM6, REG_XMM7,
    REG_XMM14, //for double source operand fix-up
    REG_XMM15  //for double destination operand fix-up
} AsmReg;

typedef enum {
    ASM_BYTE,
    ASM_LONGWORD,
    ASM_QUADWORD,
    ASM_DOUBLE
} AsmType;

typedef struct {
    AsmOpType type;
    union {
        long imm;
        AsmReg reg;
        char *pseudo;
        struct { char *name; int offset; } pseudo_mem;
        struct { AsmReg base,index; int scale; } indexed;
        struct {
            AsmReg base;
            int offset;
        } memory;
        char *data_name;
    };
} AsmOperand;

typedef struct AsmInst {
    AsmInstType type;
    AsmType src_type; //extension source size
    AsmType asm_type; //operand size of Mov/Unary/Binary/Cmp/Idiv/Cdq
    BinaryOpType binary_op;
    CondCode cond;
    AsmOperand src;
    AsmOperand dst;
    char *label; //for JMP/JmpCC/LABEL/CALL
    struct AsmInst *next;
} AsmInst;

typedef struct {
    char *name;
    int global;
    AsmInst *instructions;
    int stack_size; //  stack size belongs to each function
} AsmFunc;

typedef struct {
    char *name;
    int global;
    int alignment;
    StaticInitList init;
} AsmStaticVariable;

//read-only constant, emitted under a local label
typedef struct {
    char *name;
    int alignment;
    StaticInit init;
} AsmStaticConstant;

typedef enum {
    ASM_TOP_FUNCTION,
    ASM_TOP_STATIC_VARIABLE,
    ASM_TOP_STATIC_CONSTANT
} AsmTopLevelType;

typedef struct {
    AsmTopLevelType type;
    union {
        AsmFunc *function;
        AsmStaticVariable *static_variable;
        AsmStaticConstant *static_constant;
    };
} AsmTopLevel;

//backend symbol table entry: ObjEntry(asm_type, is_static, is_constant) | FunEntry(defined)
typedef enum {
    ASM_SYMBOL_OBJ,
    ASM_SYMBOL_FUN
} AsmSymbolKind;

typedef struct AsmSymbol {
    char *name;
    AsmSymbolKind kind;
    AsmType asm_type; //ObjEntry
    int byte_size,alignment,is_aggregate;
    int is_static;    //ObjEntry
    int is_constant;  //ObjEntry: a StaticConstant, emitted with a local label
    int defined;      //FunEntry
    struct AsmSymbol *next;
} AsmSymbol;

typedef struct {
    AsmTopLevel **top_levels;
    int top_level_count;
    AsmSymbol *symbols;
} AsmProg;


//TACKY IR ddefine
typedef enum {
    TACKY_VAL_CONSTANT,
    TACKY_VAL_VAR
} TackyValType;

typedef struct {
    TackyValType type;
    union {
        Const constant;
        char *var_name;
    };
} TackyVal;

typedef enum {
    TACKY_INST_RETURN,
    TACKY_INST_SIGN_EXTEND,
    TACKY_INST_ZERO_EXTEND,
    TACKY_INST_TRUNCATE,
    TACKY_INST_DOUBLE_TO_INT,
    TACKY_INST_DOUBLE_TO_UINT,
    TACKY_INST_INT_TO_DOUBLE,
    TACKY_INST_UINT_TO_DOUBLE,
    TACKY_INST_UNARY,
    TACKY_INST_BINARY,
    TACKY_INST_COPY,
    TACKY_INST_JUMP,
    TACKY_INST_JZ,
    TACKY_INST_JNZ,
    TACKY_INST_LABEL,
    TACKY_INST_FUN_CALL,
    TACKY_INST_LOAD, // dst = *src
    TACKY_INST_STORE, // *dst = src
    TACKY_INST_GET_ADDRESS, // dst = &src
    TACKY_INST_ADD_PTR,
    TACKY_INST_COPY_TO_OFFSET
} TackyInstType;

typedef struct TackyInstruction{
    TackyInstType type;
    UnaryOpType unary_op;
    BinaryOpType binary_op;
    TackyVal *src;
    TackyVal *src2;
    TackyVal *dst;
    int scale,offset;
    char *label_name;

    char *fun_name;
    TackyVal **args;
    int arg_count;
    
    struct TackyInstruction *next;
} TackyInstruction;

typedef struct {
    char *name;
    int global;
    char **params;
    int param_count;
    TackyInstruction *instructions;
} TackyFunction;

typedef struct {
    char *name;
    int global;
    Type *type;
    StaticInitList init;
} TackyStaticVariable;

typedef enum {
    TACKY_TOP_FUNCTION,
    TACKY_TOP_STATIC_VARIABLE,
    TACKY_TOP_STATIC_CONSTANT
} TackyTopLevelType;

typedef struct {
    TackyTopLevelType type;
    union {
        TackyFunction *function;
        TackyStaticVariable *static_variable;
    };
} TackyTopLevel;

typedef struct {
    TackyTopLevel **top_levels;
    int top_level_count;
} TackyProgram;

int label_counter=0;

char *make_label(const char *prefix){
    char *name=malloc(32);
    sprintf(name,"%s_%d",prefix,label_counter++);
    return name;
}

//temporal variable
int temp_counter=0;

char *make_temporary(){
    char *name=malloc(16);
    sprintf(name,"tmp.%d",temp_counter++);
    return name;
}

//casting a double outside the target's range (or NaN) is UB in this compiler's own C
static int double_fits(double d,Type *t){
    switch (t->kind){
        case TYPE_CHAR: case TYPE_SCHAR: return d>-129.0 && d<128.0;
        case TYPE_UCHAR: return d>-1.0 && d<256.0;
        case TYPE_LONG: return d>=-9223372036854775808.0 && d<9223372036854775808.0;
        case TYPE_UINT: return d>-1.0 && d<4294967296.0;
        case TYPE_ULONG: return d>-1.0 && d<18446744073709551616.0;
        default: return d>-2147483649.0 && d<2147483648.0;
    }
}

//every compile-time conversion: integer wrap, int<->double, double->int truncates toward zero
Const convert_const(Const c,Type *t){
    Const r;
    switch (t->kind){
        case TYPE_CHAR: case TYPE_SCHAR: r.type=CONST_CHAR; break;
        case TYPE_UCHAR: r.type=CONST_UCHAR; break;
        case TYPE_LONG: r.type=CONST_LONG; break;
        case TYPE_UINT: r.type=CONST_UINT; break;
        case TYPE_ULONG:
        case TYPE_POINTER: r.type=CONST_ULONG; break;
        case TYPE_DOUBLE: r.type=CONST_DOUBLE; break;
        default: r.type=CONST_INT; break;
    }

    if (c.type==CONST_DOUBLE){
        if (t->kind!=TYPE_DOUBLE && !double_fits(c.dvalue,t)){
            fprintf(stderr,"Type Error: constant %g is out of range of its integer type\n",c.dvalue);
            exit(1);
        }
        switch (t->kind){
            case TYPE_DOUBLE: r.dvalue=c.dvalue; break;
            case TYPE_CHAR: case TYPE_SCHAR: r.value=(signed char)c.dvalue; break;
            case TYPE_UCHAR: r.value=(unsigned char)c.dvalue; break;
            case TYPE_LONG: r.value=(long)c.dvalue; break;
            case TYPE_UINT: r.value=(unsigned int)c.dvalue; break;
            case TYPE_ULONG: r.value=(long)(unsigned long)c.dvalue; break;
            default: r.value=(int)c.dvalue; break;
        }
    }else if (t->kind==TYPE_DOUBLE){
        r.dvalue=c.type==CONST_ULONG ? (double)(unsigned long)c.value : (double)c.value;
    }else{
        r.value=convert_value(c.value,t);
    }
    return r;
}

Const make_const(Type *t,long value){
    Const c={CONST_LONG,{value}};
    return convert_const(c,t);
}

TackyVal *tacky_val_constant(Const c){
    TackyVal *v=malloc(sizeof(TackyVal));
    v->type=TACKY_VAL_CONSTANT;
    v->constant=c;
    return v;
}

TackyVal *tacky_val_var(char *name){
    TackyVal *v=malloc(sizeof(TackyVal));
    v->type=TACKY_VAL_VAR;
    v->var_name=strdup(name);
    return v;
}
//add instruction to the end of list
void append_tacky_inst(TackyInstruction **head,TackyInstruction *new_list){
    if (*head==NULL){
        *head=new_list;
    }else{
        TackyInstruction *curr=*head;
        while (curr->next!=NULL){
            curr=curr->next;
        }
        curr->next=new_list;
    }
}

//lexer save Token
void add_token(TokenType type,const char *val){
    token_list =realloc(token_list,sizeof(Token)*(token_count+1));
    token_list[token_count].type=type;
    token_list[token_count].value=val? strdup(val):NULL;
    token_count++;
}

TokenType check_keyword(const char *s){
    if (strcmp(s,"char")==0) return TOK_CHAR;
    if (strcmp(s,"int")==0) return TOK_INT;
    if (strcmp(s,"long")==0) return TOK_LONG;
    if (strcmp(s,"signed")==0) return TOK_SIGNED;
    if (strcmp(s,"unsigned")==0) return TOK_UNSIGNED;
    if (strcmp(s,"double")==0) return TOK_DOUBLE;
    if (strcmp(s,"void")==0) return TOK_VOID;
    if (strcmp(s,"return")==0) return TOK_RETURN;
    if (strcmp(s,"static")==0) return TOK_STATIC;
    if (strcmp(s,"extern")==0) return TOK_EXTERN;

    if (strcmp(s,"if")==0) return TOK_IF;
    if (strcmp(s,"else")==0) return TOK_ELSE;
    if (strcmp(s,"goto")==0) return TOK_GOTO;

    if (strcmp(s,"do")==0) return TOK_DO;
    if (strcmp(s,"while")==0) return TOK_WHILE;
    if (strcmp(s,"for")==0) return TOK_FOR;
    if (strcmp(s,"break")==0) return TOK_BREAK;
    if (strcmp(s,"continue")==0) return TOK_CONTINUE;

    if (strcmp(s,"switch")==0) return TOK_SWITCH;
    if (strcmp(s,"case")==0) return TOK_CASE;
    if (strcmp(s,"default")==0) return TOK_DEFAULT;
    if (strcmp(s,"sizeof")==0) return TOK_SIZEOF;

    return TOK_IDENTIFIER;
}

static char decode_escape(char c){
    const char *escapes="'\"?\\abfnrtv";
    const char values[]={'\'', '\"', '?', '\\', '\a', '\b', '\f', '\n', '\r', '\t', '\v'};
    const char *position=strchr(escapes,c);
    if (!c || !position){ fprintf(stderr,"Lexer Error: invalid escape sequence\n"); exit(1); }
    return values[position-escapes];
}

void lex(const char *input){
    const char *p=input;
    while (*p!='\0'){
        // skip space
        if (isspace(*p)){
            p++;
            continue;
        }

        //skip #
        if (*p=='#'){
            while (*p!='\0' && *p!='\n') {
                p++;
            }
            continue;
        }

        //deal with comments
        if (*p=='/' && (*(p+1)=='/' || *(p+1)=='*')){
            if (*(p+1)=='/'){ //single  comments
                while (*p!='\0' && *p!='\n') {
                    p++;
                }
            }else{ //multiple comments
                p+=2;
                while (*p!='\0' && !(*p=='*' && *(p+1)=='/')){
                    p++;
                }
                if (*p!='\0') p+=2;
            }
            continue;
        }
        
        if (*p=='\'' || *p=='"'){
            char quote=*p++;
            char *bytes=malloc(strlen(p)+1);
            int length=0;
            while (*p && *p!=quote){
                if (*p=='\n' || *p=='\r'){ fprintf(stderr,"Lexer Error: newline in literal\n"); exit(1); }
                char value=*p++;
                if (value=='\\') value=decode_escape(*p++);
                bytes[length++]=value;
            }
            if (*p!=quote || (quote=='\'' && length!=1)){
                fprintf(stderr,"Lexer Error: invalid or unterminated literal\n"); exit(1);
            }
            p++; bytes[length]=0;
            add_token(quote=='\'' ? TOK_CHAR_CONSTANT : TOK_STRING,bytes);
            free(bytes); continue;
        }

        //identifier and keyword
        if (isalpha(*p) || *p=='_'){
            const char *start=p;
            while (isalnum(*p) || *p=='_') p++;
            int len=p-start;
            char *buf=malloc(len+1);
            strncpy(buf,start,len);
            buf[len]='\0';
            add_token(check_keyword(buf),buf);
            
            // printf("Token: %s\n",buf);
            free(buf);
            continue;
        }

        //constant number
        if (isdigit(*p) || (*p=='.' && isdigit(*(p+1)))){
            const char *start=p;
            int is_double=0;
            while (isdigit(*p)) p++;
            if (*p=='.'){
                is_double=1;
                p++;
                while (isdigit(*p)) p++;
            }
            //an exponent needs digits; otherwise the e is left to fail the boundary check
            if (*p=='e' || *p=='E'){
                const char *q=p+1;
                if (*q=='+' || *q=='-') q++;
                if (isdigit(*q)){
                    is_double=1;
                    while (isdigit(*q)) q++;
                    p=q;
                }
            }
            int len=p-start;

            //integer suffix: l, u, ul or lu in any case
            int is_long=0,is_unsigned=0;
            if (!is_double && (*p=='l' || *p=='L')){
                is_long=1;
                p++;
                if (*p=='u' || *p=='U'){
                    is_unsigned=1;
                    p++;
                }
            }else if (!is_double && (*p=='u' || *p=='U')){
                is_unsigned=1;
                p++;
                if (*p=='l' || *p=='L'){
                    is_long=1;
                    p++;
                }
            }
            TokenType const_type=is_double ? TOK_DOUBLE_CONSTANT
                : is_unsigned
                ? (is_long ? TOK_UNSIGNED_LONG_CONSTANT : TOK_UNSIGNED_CONSTANT)
                : (is_long ? TOK_LONG_CONSTANT : TOK_CONSTANT);

            //123abc can't pass，need to 123_abc will pass
            if (isalnum(*p) || *p=='_' || *p=='.'){
                fprintf(stderr, "Lexer Error: Invalid identifier starting with digit at '%c'\n", *p);
                exit(1);
            }

            char *buf=malloc(len+1);
            strncpy(buf,start,len);
            buf[len]='\0';
            add_token(const_type,buf);
            free(buf);
            continue;
        }
        
        switch (*p){
            case '(' : 
                add_token(TOK_LPAREN,NULL);
                break;
            case ')': 
                add_token(TOK_RPAREN,NULL);
                break;
            case '[': add_token(TOK_LBRACKET,NULL); break;
            case ']': add_token(TOK_RBRACKET,NULL); break;
            case '{': 
                add_token(TOK_LBRACE,NULL);
                break;
            case '}': 
                add_token(TOK_RBRACE,NULL);
                break;
            case ';':
                add_token(TOK_SEMICOLON,NULL);
                break;
            case ',':
                add_token(TOK_COMMA,NULL);
                break;
            case '?':
                add_token(TOK_QUESTION,NULL);
                break;
            case ':':
                add_token(TOK_COLON,NULL);
                break;
            case '~':
                add_token(TOK_COMPLEMENT,NULL);
                break;
            case '+':
                if (*(p+1)=='+') {add_token(TOK_INCREMENT,NULL);p++;}
                else if (*(p+1)=='=') {add_token(TOK_PLUS_ASSIGN,NULL),p++;}
                else{add_token(TOK_PLUS,NULL);}
                break;
            case '*':
                if (*(p+1)=='=') {add_token(TOK_MULT_ASSIGN,NULL),p++;}
                else{add_token(TOK_MULT,NULL);}
                break;
            case '/':
                if (*(p+1)=='=') {add_token(TOK_DIV_ASSIGN,NULL),p++;}
                else{add_token(TOK_DIV,NULL);}
                break;
            case '%':
                if (*(p+1)=='=') {add_token(TOK_REM_ASSIGN,NULL),p++;}
                else{add_token(TOK_REMAINDER,NULL);}
                break;
            case '-':
                if (*(p+1)=='-'){ //deal with --
                    add_token(TOK_DECREMENT,NULL);
                    p++;
                }else if (*(p+1)=='='){
                    add_token(TOK_MINUS_ASSIGN,NULL);
                    p++;
                }
                else{ //deal with -
                    add_token(TOK_NEGATION,NULL);
                }
                break;
            case '!':
                if (*(p+1)=='='){
                    add_token(TOK_NEQ,NULL);
                    p++;
                }else{
                    add_token(TOK_LOGICAL_NOT,NULL);
                }
                break;
            
            case '=':
                if (*(p+1)=='='){
                    add_token(TOK_EQ,NULL);
                    p++;
                }else{
                    add_token(TOK_ASSIGN,NULL);
                }
                break;
            case '<':
                if (*(p+1)=='<'){
                    if (*(p+2)=='='){
                        add_token(TOK_LSHIFT_ASSIGN,NULL);
                        p+=2;
                    }else{
                        add_token(TOK_LSHIFT,NULL);
                        p++;
                    }
                }
                else if (*(p+1)=='='){
                    add_token(TOK_LTE,NULL);
                    p++;
                }else{
                    add_token(TOK_LT,NULL);
                }
                break;
            case '>':
                if (*(p+1)=='>'){
                    if (*(p+2)=='='){
                        add_token(TOK_RSHIFT_ASSIGN,NULL);
                        p+=2;
                    }else{
                        add_token(TOK_RSHIFT,NULL);
                        p++;
                    }
                }
                else if (*(p+1)=='='){
                    add_token(TOK_GTE,NULL);
                    p++;
                }else{
                    add_token(TOK_GT,NULL);
                }
                break;
            case '&':
                if (*(p+1)=='&'){
                    add_token(TOK_AND,NULL);
                    p++;
                }else if (*(p+1)=='='){
                    add_token(TOK_AND_ASSIGN,NULL);
                    p++;
                }else{
                    add_token(TOK_BITWISE_AND,NULL);
                }
                break;
            case '|':
                if (*(p+1)=='|'){
                    add_token(TOK_OR,NULL);
                    p++;
                }else if (*(p+1)=='='){
                    add_token(TOK_OR_ASSIGN,NULL);
                    p++;
                }else{
                    add_token(TOK_BITWISE_OR,NULL);
                }
                break;

            case '^':
                if (*(p+1)=='='){
                    add_token(TOK_XOR_ASSIGN,NULL);
                    p++;
                }else{
                    add_token(TOK_XOR,NULL);
                }
                break;

            default:
                fprintf(stderr,"Lexer Error: Invalid character '%c'\n",*p);
                exit(1);
        }
        p++;
    }
    add_token(TOK_EOF,NULL);
}

//Parser section
Token peek() {return token_list[token_pos];}
Token take() {return token_list[token_pos++];}
Exp *parse_exp(int min_prec);
Exp *parse_cast_exp(void);
Exp *parse_unary_exp(void);
Statement *parse_statement();

Declaration *parse_declaration();
BlockItem *parse_block_item();


Exp *parse_optional_exp(TokenType end_token);
ForInit *parse_for_init();
Statement *parse_for_statement();
Statement *parse_do_while_statement();
Statement *parse_while_statement();

Statement *parse_switch_statement();
Statement *parse_case_statement();
Statement *parse_default_statement();


typedef struct IdentifierMap{
    char *user_name;
    char *unique_name;
    //from_current_block is represnet current scope
    int from_current_block;

    // 1= have linkage for function, 0 = no linkage for local variable / parameter
    int has_linkage;

    struct IdentifierMap *next;
} IdentifierMap;

void resolve_for_init(ForInit *init,IdentifierMap **map);
void label_statement(Statement *stmt,char *current_break_label,char *current_continue_label);
void label_block_items(BlockItem **items,int count,char *current_break_label,char *current_continue_label);
void resolve_file_scope_variable_declaration(VariableDeclaration *decl,IdentifierMap **map);
void resolve_local_variable_declaration(VariableDeclaration *decl,IdentifierMap **map);
void resolve_expression(Exp *e,IdentifierMap *map);
void resolve_initializer(Initializer *init,IdentifierMap *map){
    if (!init) return;
    if (!init->compound) resolve_expression(init->exp,map);
    for (int i=0;i<init->count;i++) resolve_initializer(init->items[i],map);
}

void expect(TokenType type){
    Token t=take();
    if (t.type!=type){
        fprintf(stderr, "Parse Error: Expected type %d but got %d\n", type, t.type);
        exit(1);//pass invalid_parse test
    }
}

void resolve_for_init(ForInit *init,IdentifierMap **map){
    if (!init){
        return;
    }

    if (init->type==FOR_INIT_DECL){
        resolve_local_variable_declaration(init->decl,map);
    }else{
        if(init->exp){
            resolve_expression(init->exp,*map);
        }
    }
}

int is_type_specifier(TokenType type){
    return type==TOK_CHAR || type==TOK_INT || type==TOK_LONG || type==TOK_SIGNED || type==TOK_UNSIGNED || type==TOK_DOUBLE || type==TOK_VOID;
}

int is_declaration_start(void) {
    TokenType type=peek().type;
    return is_type_specifier(type) || type==TOK_STATIC || type==TOK_EXTERN;
}

// specifiers in any order, none repeated, not both signed and unsigned; double and void stand alone
Type *parse_type(TokenType *types,int count){
    int has_char=0,has_int=0,has_long=0,has_signed=0,has_unsigned=0,has_double=0,has_void=0;

    for (int i=0;i<count;i++){
        int *seen=types[i]==TOK_CHAR ? &has_char :
                  types[i]==TOK_DOUBLE ? &has_double :
                  types[i]==TOK_VOID ? &has_void :
                  types[i]==TOK_INT ? &has_int :
                  types[i]==TOK_LONG ? &has_long :
                  types[i]==TOK_SIGNED ? &has_signed : &has_unsigned;
        if (*seen){
            fprintf(stderr,"Parse Error: invalid type specifier\n");
            exit(1);
        }
        *seen=1;
    }

    if (count==0 || (has_signed && has_unsigned) || (has_double && count>1) || (has_void && count>1) || (has_char && (has_int || has_long || has_double))){
        fprintf(stderr,"Parse Error: invalid type specifier\n");
        exit(1);
    }

    if (has_char) return has_unsigned ? type_uchar() : has_signed ? type_schar() : type_char();
    if (has_double) return type_double();
    if (has_void) return type_void();

    if (has_unsigned) return has_long ? type_ulong() : type_uint();
    return has_long ? type_long() : type_int();
}

// { <type-specifier> }+
Type *parse_type_specifiers(void){
    TokenType types[4];
    int count=0;

    while (is_type_specifier(peek().type)){
        if (count==4){
            fprintf(stderr,"Parse Error: invalid type specifier\n");
            exit(1);
        }
        types[count++]=take().type;
    }

    return parse_type(types,count);
}

BlockItem *parse_block_item() {
    BlockItem *bi=calloc(1,sizeof(BlockItem));

    if (is_declaration_start()) {
        bi->type=BI_DECL;
        bi->decl=parse_declaration();
    } else {
        bi->type=BI_STMT;
        bi->stmt=parse_statement();
    }

    return bi;
}

Statement *parse_block(){
    expect(TOK_LBRACE);
    
    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_COMPOUND;

    s->block_items=NULL;
    s->block_count=0;

    while(peek().type!=TOK_RBRACE){
        BlockItem *bi= parse_block_item();

        s->block_items=realloc(
            s->block_items,
            sizeof(BlockItem*) * (s->block_count+1)
        );

        s->block_items[s->block_count++]=bi;
    }

    expect(TOK_RBRACE);
    return s;
}

/*
Grammar order is the reverse of type-derivation order, so a declarator is
parsed into this tree first and turned into a type by process_declarator.
*/
typedef enum {DECLARATOR_IDENT,DECLARATOR_POINTER,DECLARATOR_FUN,DECLARATOR_ARRAY} DeclaratorKind;

typedef struct Declarator Declarator;

typedef struct {
    Type *base;
    Declarator *declarator;
} ParamInfo;

struct Declarator {
    DeclaratorKind kind;
    char *name; //DECLARATOR_IDENT; NULL at the bottom of an abstract declarator
    Declarator *inner; //DECLARATOR_POINTER, DECLARATOR_FUN
    ParamInfo *params; //DECLARATOR_FUN
    int param_count;
    unsigned long array_count;
};

typedef struct {
    char *name;
    Type *type;
    char **param_names;
} DeclInfo;

static Declarator *make_declarator(DeclaratorKind kind,Declarator *inner){
    Declarator *d=calloc(1,sizeof(Declarator));
    d->kind=kind;
    d->inner=inner;
    return d;
}

ParamInfo *parse_param_list(int *count);
Const parse_constant(Token t);

// <declarator>        ::= "*" <declarator> | <direct-declarator>
// <direct-declarator> ::= <simple-declarator> [ <param-list> ]
// <simple-declarator> ::= <identifier> | "(" <declarator> ")"
//
// abstract: no identifier allowed, no function suffix
// <abstract-declarator>        ::= "*" [ <abstract-declarator> ] | <direct-abstract-declarator>
// <direct-abstract-declarator> ::= "(" <abstract-declarator> ")"
Declarator *parse_declarator(int abstract){
    if (peek().type==TOK_MULT){
        take();
        return make_declarator(DECLARATOR_POINTER,parse_declarator(abstract));
    }

    Declarator *d;
    if (peek().type==TOK_LPAREN){
        take();
        if (abstract && peek().type!=TOK_MULT && peek().type!=TOK_LPAREN && peek().type!=TOK_LBRACKET){
            fprintf(stderr,"Parse Error: malformed abstract declarator\n");
            exit(1);
        }
        d=parse_declarator(abstract);
        expect(TOK_RPAREN);
    }else if (abstract){
        d=make_declarator(DECLARATOR_IDENT,NULL);
    }else{
        Token id=take();
        if (id.type!=TOK_IDENTIFIER){
            fprintf(stderr,"Parse Error: expected identifier in declarator\n");
            exit(1);
        }
        d=make_declarator(DECLARATOR_IDENT,NULL);
        d->name=strdup(id.value);
    }

    int function_suffix=0;
    while (peek().type==TOK_LBRACKET || (!abstract && peek().type==TOK_LPAREN)){
        if (peek().type==TOK_LBRACKET){
            if (function_suffix){ fprintf(stderr,"Parse Error: array suffix after function suffix\n"); exit(1); }
            take();
            Token count=take();
            if (count.type!=TOK_CHAR_CONSTANT && count.type!=TOK_CONSTANT && count.type!=TOK_LONG_CONSTANT && count.type!=TOK_UNSIGNED_CONSTANT && count.type!=TOK_UNSIGNED_LONG_CONSTANT){
                fprintf(stderr,"Parse Error: array length must be an integer literal\n"); exit(1);
            }
            unsigned long n=(unsigned long)parse_constant(count).value;
            if (!n){ fprintf(stderr,"Parse Error: unsupported array length\n"); exit(1); }
            expect(TOK_RBRACKET);
            Declarator *array=make_declarator(DECLARATOR_ARRAY,d);
            array->array_count=n; d=array; continue;
        }
        take();
        Declarator *fun=make_declarator(DECLARATOR_FUN,d);
        fun->params=parse_param_list(&fun->param_count);
        expect(TOK_RPAREN);
        d=fun; function_suffix=1;
    }

    return d;
}

// <param-list> without its parentheses: "void" | <param> { "," <param> }
// <param> ::= { <type-specifier> }+ <declarator>
ParamInfo *parse_param_list(int *count) {
    ParamInfo *params=NULL;
    *count=0;

    if (peek().type==TOK_VOID && token_list[token_pos+1].type==TOK_RPAREN) {
        take();
        return NULL;
    }

    if (!is_type_specifier(peek().type)) {
        fprintf(stderr,"Parse Error: expected parameter list\n");
        exit(1);
    }

    while (1) {
        params=realloc(params,sizeof(ParamInfo) * (*count+1));
        params[*count].base=parse_type_specifiers();
        params[*count].declarator=parse_declarator(0);
        (*count)++;

        if (peek().type!=TOK_COMMA) {
            return params;
        }
        take();
    }
}

DeclInfo process_declarator(Declarator *d,Type *base){
    switch (d->kind){
        case DECLARATOR_ARRAY:
            if (base->kind==TYPE_FUNCTION){ fprintf(stderr,"Type Error: array of functions\n"); exit(1); }
            return process_declarator(d->inner,make_array_type(base,d->array_count));
        case DECLARATOR_POINTER:
            return process_declarator(d->inner,make_pointer_type(base));

        case DECLARATOR_FUN: {
            if (d->inner->kind!=DECLARATOR_IDENT){
                fprintf(stderr,"Parse Error: function pointers and functions returning functions are not supported\n");
                exit(1);
            }

            Type **types=d->param_count ? calloc(d->param_count,sizeof(Type*)) : NULL;
            char **names=d->param_count ? calloc(d->param_count,sizeof(char*)) : NULL;

            for (int i=0;i<d->param_count;i++){
                DeclInfo param=process_declarator(d->params[i].declarator,d->params[i].base);
                if (param.type->kind==TYPE_FUNCTION){
                    fprintf(stderr,"Parse Error: function-typed parameter '%s' is not supported\n",param.name);
                    exit(1);
                }
                //validate before the decay hides an array of void behind a pointer
                validate_type_specifier(param.type);
                types[i]=param.type->kind==TYPE_ARRAY ? make_pointer_type(param.type->referenced) : param.type;
                names[i]=param.name;
            }

            return (DeclInfo){d->inner->name,make_function_type(types,d->param_count,base),names};
        }

        case DECLARATOR_IDENT:
            break;
    }
    return (DeclInfo){d->name,base,NULL};
}

Exp *parse_optional_exp(TokenType end_token){
    if (peek().type==end_token){
        return NULL;
    }
    return parse_exp(0);
}

ForInit *parse_for_init(){
    ForInit *init=calloc(1,sizeof(ForInit));

    if (is_declaration_start()){
        Declaration *decl=parse_declaration();

        if (decl->type!=DECL_VAR){
            fprintf(stderr,"Parse Error: function declaration is not allowed in for initializer\n");
            exit(1);
        }

        init->type=FOR_INIT_DECL;
        init->decl=decl->var;
        init->exp=NULL;
    }else{
        init->type=FOR_INIT_EXP;
        init->decl=NULL;
        init->exp=parse_optional_exp(TOK_SEMICOLON);
        expect(TOK_SEMICOLON);
    }

    return init;
}

Statement *parse_while_statement(){
    expect(TOK_WHILE);
    expect(TOK_LPAREN);
    Exp *cond=parse_exp(0);
    expect(TOK_RPAREN);

    Statement *body=parse_statement();

    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_WHILE;
    s->while_stmt.condition=cond;
    s->while_stmt.body=body;

    return s;
}

Statement *parse_do_while_statement(){
    expect(TOK_DO);

    Statement *body=parse_statement();

    expect(TOK_WHILE);
    expect(TOK_LPAREN);
    Exp *cond=parse_exp(0);
    expect(TOK_RPAREN);
    expect(TOK_SEMICOLON);
    

    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_DO_WHILE;
    s->do_while_stmt.body=body;
    s->do_while_stmt.condition=cond;
    return s;
}

Statement *parse_for_statement(){
    expect(TOK_FOR);
    expect(TOK_LPAREN);

    ForInit *init=parse_for_init();

    Exp *cond=parse_optional_exp(TOK_SEMICOLON);
    expect(TOK_SEMICOLON);

    Exp *post=parse_optional_exp(TOK_RPAREN);
    expect(TOK_RPAREN);

    Statement *body=parse_statement();

    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_FOR;
    s->for_stmt.init=init;
    s->for_stmt.condition=cond;
    s->for_stmt.post=post;
    s->for_stmt.body=body;

    return s;
}

Statement *parse_switch_statement(){
    expect(TOK_SWITCH);
    expect(TOK_LPAREN);
    Exp *cond=parse_exp(0);
    expect(TOK_RPAREN);

    Statement *body=parse_statement();

    Statement *s=calloc(1,sizeof(Statement));
    s->type=STMT_SWITCH;
    s->switch_stmt.control=cond;
    s->switch_stmt.body=body;
    s->switch_stmt.cases=NULL;
    s->switch_stmt.default_label=NULL;

    return s;
}

Statement *parse_case_statement(){
    expect(TOK_CASE);

    Exp *value_exp=parse_exp(0);
    expect(TOK_COLON);

    Statement *body=parse_statement();

    Statement *s=calloc(1,sizeof(Statement));
    s->type=STMT_CASE;
    s->case_stmt.value_exp=value_exp;
    s->case_stmt.body=body;
    s->case_stmt.case_label=NULL;

    return s;
}

Statement *parse_default_statement(){
    expect(TOK_DEFAULT);
    expect(TOK_COLON);

    Statement *body=parse_statement();

    Statement *s=calloc(1,sizeof(Statement));
    s->type=STMT_DEFAULT;
    s->default_stmt.body=body;
    s->default_stmt.default_label=NULL;

    return s;
}

// binary operators (precedence handled by parse_exp)
//
// assignment
//   = += -= *= /= %= &= |= ^= <<= >>=
//
// logical
//   || &&
//
// bitwise
//   | ^ &
//
// comparison
//   == != < <= > >=
//
// shift
//   << >>
//
// arithmetic
//   + - * / %
Exp *parse_exp(int min_prec){
   Exp *left=parse_cast_exp();
   Token next=peek();

   while (get_precedence(next.type)>=min_prec){
        TokenType op_type=next.type;
        int prec=get_precedence(op_type);

        if (op_type==TOK_ASSIGN){
            //right-associative: a=b=c -> a=(b=c)
            take();
            Exp *right=parse_exp(prec);
            left=make_assign_exp(left,right);
        }else if (op_type==TOK_QUESTION){
            take(); //consume ?

            Exp *middle=parse_exp(0);

            expect(TOK_COLON);

            Exp *right=parse_exp(prec);

            Exp *node=calloc(1,sizeof(Exp));
            node->type=EXP_CONDITIONAL;
            node->conditional.condition=left;
            node->conditional.true_expr=middle;
            node->conditional.false_expr=right;

            left=node;
        }
        else if (op_type>=TOK_PLUS_ASSIGN && op_type <=TOK_RSHIFT_ASSIGN){
            take();
            Exp *right=parse_exp(prec); //right-associative(like =)
            left=make_compound_assign_exp(compound_token_to_binary_op(op_type),left,right);
        }
        else{
            //left-associative, pass prec+1
            take();
            Exp *right=parse_exp(prec+1);
            left=make_binary_exp(token_to_binary_op(op_type),left,right);
        }

        next=peek();
   }

   return left;
} 

Exp **parse_argument_list(int *out_count) {
    Exp **args=NULL;
    int count=0;

    if (peek().type==TOK_RPAREN) {
        *out_count=0;
        return NULL;
    }

    while (1) {
        Exp *arg=parse_exp(0);
        
        args=realloc(args,sizeof(Exp*)*(count+1));
        args[count++]=arg;

        if (peek().type==TOK_COMMA) {
            take();
            continue;
        }

        break;
    }

    
    *out_count=count;
    return args;
}

// <int> fits in int -> ConstInt, otherwise ConstLong; <long> -> ConstLong
// <uint> fits in unsigned int -> ConstUInt, otherwise ConstULong; <ulong> -> ConstULong
Const parse_constant(Token t){
    if (t.type==TOK_CHAR_CONSTANT) return make_const(type_int(),(unsigned char)t.value[0]);
    if (t.type==TOK_DOUBLE_CONSTANT){
        Const c;
        c.type=CONST_DOUBLE;
        c.dvalue=strtod(t.value,NULL);
        return c;
    }

    int is_unsigned=(t.type==TOK_UNSIGNED_CONSTANT || t.type==TOK_UNSIGNED_LONG_CONSTANT);
    const unsigned long max=is_unsigned ? ULONG_MAX : LONG_MAX;
    unsigned long v=0;

    for (const char *p=t.value;*p;p++){
        unsigned long digit=(unsigned long)(*p-'0');
        if (v > (max-digit)/10){
            fprintf(stderr,"Parse Error: constant %s is too large to represent\n",t.value);
            exit(1);
        }
        v=v*10+digit;
    }

    Const c;
    c.value=(long)v;
    if (is_unsigned){
        c.type=(t.type==TOK_UNSIGNED_CONSTANT && v<=UINT_MAX) ? CONST_UINT : CONST_ULONG;
    }else{
        c.type=(t.type==TOK_CONSTANT && v<=INT_MAX) ? CONST_INT : CONST_LONG;
    }
    return c;
}

// <type-name> ::= { <type-specifier> }+ [ <abstract-declarator> ]
Type *parse_type_name(void){
    Type *base=parse_type_specifiers();
    return process_declarator(parse_declarator(1),base).type;
}

// <cast-exp> ::= "(" <type-name> ")" <cast-exp> | <unary-exp>
Exp *parse_cast_exp(void){
    if (peek().type==TOK_LPAREN && is_type_specifier(token_list[token_pos+1].type)){
        take();
        Type *target_type=parse_type_name();
        expect(TOK_RPAREN);
        return make_cast_exp(target_type,parse_cast_exp());
    }
    return parse_unary_exp();
}

// <unary-exp> ::= <unop> <cast-exp> | "sizeof" <unary-exp> | "sizeof" "(" <type-name> ")" | <postfix-exp>
Exp *parse_unary_exp(void){
    Token t=peek();
    Exp *inner = NULL;

    if (t.type==TOK_CHAR_CONSTANT){
        take(); inner=make_constant_exp(parse_constant(t));
    }
    else if (t.type==TOK_STRING){
        inner=calloc(1,sizeof(Exp)); inner->type=EXP_STRING;
        inner->string.bytes.data=calloc(1,1);
        while (peek().type==TOK_STRING){
            Token part=take(); int length=(int)strlen(part.value);
            ByteString *bytes=&inner->string.bytes;
            bytes->data=realloc(bytes->data,(size_t)bytes->length+length+1);
            memcpy(bytes->data+bytes->length,part.value,(size_t)length+1);
            bytes->length+=length;
        }
    }
    else if (t.type==TOK_CONSTANT || t.type==TOK_LONG_CONSTANT ||
        t.type==TOK_UNSIGNED_CONSTANT || t.type==TOK_UNSIGNED_LONG_CONSTANT ||
        t.type==TOK_DOUBLE_CONSTANT){
        take();
        inner=make_constant_exp(parse_constant(t));
    }
    else if (t.type==TOK_SIZEOF){
        take();
        Exp *e=calloc(1,sizeof(Exp));
        if (peek().type==TOK_LPAREN && is_type_specifier(token_list[token_pos+1].type)){
            take();
            e->type=EXP_SIZEOF_TYPE; e->sizeof_type=parse_type_name();
            expect(TOK_RPAREN);
        }else{
            e->type=EXP_SIZEOF; e->sizeof_exp=parse_unary_exp();
        }
        return e;
    }
    // prefix ++ / --
    else if (t.type==TOK_INCREMENT || t.type==TOK_DECREMENT){
        take();
        int is_inc=(t.type==TOK_INCREMENT);
        Exp *operand=parse_cast_exp(); //right-associative
        return make_inc_dec_exp(EXP_PREFIX_OP,is_inc,operand);
    }
    else if (t.type==TOK_IDENTIFIER){
        take();

        if (peek().type==TOK_LPAREN){
            take(); //comsume '('
            
            int arg_count=0;
            Exp **args=parse_argument_list(&arg_count);

            expect(TOK_RPAREN);

            inner=make_function_call_exp(t.value,args,arg_count);
            
        } else{
            inner=make_var_exp(t.value);
        }

    }
    else if (t.type==TOK_NEGATION || t.type==TOK_COMPLEMENT || t.type==TOK_LOGICAL_NOT){
        take();
        UnaryOpType op;
        if (t.type==TOK_NEGATION) {
            op=UNARY_NEGATION;
        }
        else if (t.type==TOK_COMPLEMENT) {
            op=UNARY_COMPLEMENT;
        }
        else{
            op=UNARY_NOT;
        }

        Exp *inner=parse_cast_exp();
        return make_unary_exp(op,inner);
    }
    else if (t.type==TOK_MULT){
        take();
        return make_dereference_exp(parse_cast_exp());
    }
    else if (t.type==TOK_BITWISE_AND){
        take();
        return make_addr_of_exp(parse_cast_exp());
    }
    else if (t.type==TOK_LPAREN){
        take();
        inner =parse_exp(0); // PAREN inter predence reset to 0
        expect(TOK_RPAREN);
        
    }else{
        fprintf(stderr, "Parse Error: Expected factor but got %d\n", t.type);
        exit(1);
    }

    //handle postfix ++/--
    // now  IDENTIFIER and LPAREN can continue parse ++ or --
    Token next=peek();
    while (next.type==TOK_INCREMENT || next.type==TOK_DECREMENT || next.type==TOK_LBRACKET){
        if (next.type==TOK_LBRACKET){
            take();
            Exp *sub=calloc(1,sizeof(Exp)); sub->type=EXP_SUBSCRIPT;
            sub->subscript.left=inner; sub->subscript.right=parse_exp(0);
            expect(TOK_RBRACKET); inner=sub; next=peek(); continue;
        }
        take();
        int is_inc=(next.type==TOK_INCREMENT);
        inner = make_inc_dec_exp(EXP_POSTFIX_OP, is_inc, inner);
        next=peek();
    }

    return inner;
}

Statement *parse_if_statment();
Statement *parse_statement();


// <statement> ::= "return" <expression> ";"
//               | <expression> ";"
//               | ";"
//               | "if" "(" <expression> ")" <statement>
//               | "if" "(" <expression> ")" <statement> "else" <statement>
//               | "goto" <identifier> ";"
//               | <identifier> ":" <statement>
Statement *parse_statement() {

    if (peek().type==TOK_IF){
        return parse_if_statment();
    }

    if (peek().type==TOK_LBRACE){
        return parse_block();
    }

    if (peek().type==TOK_WHILE){
        return parse_while_statement();
    }

    if (peek().type==TOK_DO){
        return parse_do_while_statement();
    }

    if (peek().type==TOK_FOR){
        return parse_for_statement();
    }

    if (peek().type==TOK_SWITCH){
        return parse_switch_statement();
    }

    if (peek().type==TOK_CASE){
        return parse_case_statement();
    }

    if (peek().type==TOK_DEFAULT){
        return parse_default_statement();
    }

    Statement *s=malloc(sizeof(Statement));

    if (peek().type==TOK_RETURN){
        take();
        s->type=STMT_RETURN;
        s->exp=parse_optional_exp(TOK_SEMICOLON);
        expect(TOK_SEMICOLON);
    }
    else if (peek().type==TOK_BREAK){
        take();
        expect(TOK_SEMICOLON);
        s->type=STMT_BREAK;
        s->exp=NULL;
    }
    else if (peek().type==TOK_CONTINUE){
        take();
        expect(TOK_SEMICOLON);
        s->type=STMT_CONTINUE;
        s->exp=NULL;
    }
    else if (peek().type==TOK_GOTO){
        take();
        Token id=take();

        if (id.type!=TOK_IDENTIFIER){
            fprintf(stderr,"Parse Error: expected label after goto\n");
            exit(1);
        }

        expect(TOK_SEMICOLON);

        Statement *s=malloc(sizeof(Statement));
        s->type=STMT_GOTO;
        s->label=strdup(id.value);
        return s;
    }
    else if (peek().type==TOK_IDENTIFIER && token_list[token_pos+1].type==TOK_COLON){
        Token id=take();
        take(); //consume :

        Statement *inner=parse_statement();

        Statement *s=malloc(sizeof(Statement));
        s->type=STMT_LABEL;
        s->label=strdup(id.value);
        s->exp=NULL;
        s->if_stmt.then_branch=inner;

        return s;
    }
    else if (peek().type==TOK_SEMICOLON){
        take();
        s->type=STMT_NULL;
        s->exp=NULL;
    }else{
        s->type=STMT_EXPRESSION;
        s->exp=parse_exp(0);
        expect(TOK_SEMICOLON);
    }
    return s;
}
// <if_statement> ::= "if" "(" <expression> ")" <statement>
//                  | "if" "(" <expression> ")" <statement> "else" <statement>
Statement *parse_if_statment(){
    expect(TOK_IF);
    expect(TOK_LPAREN);

    Exp *cond=parse_exp(0);

    expect(TOK_RPAREN);

    Statement *then_stmt=parse_statement();

    Statement *else_stmt=NULL;

    if (peek().type==TOK_ELSE){
        take();
        else_stmt=parse_statement();
    }

    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_IF;

    s->if_stmt.condition=cond;
    s->if_stmt.then_branch=then_stmt;
    s->if_stmt.else_branch=else_stmt;

    return s;
}

typedef struct {
    Type *type;
    StorageClass storage_class;
} ParsedSpecifiers;

int is_specifier_token(TokenType type){
    return is_type_specifier(type) || type==TOK_STATIC || type==TOK_EXTERN;
}

ParsedSpecifiers parse_type_and_storage_class(){
    TokenType *specifiers=NULL;
    int specifier_count=0;

    while(is_specifier_token(peek().type)){
        Token tok=take();

        specifiers=realloc(
            specifiers,
            sizeof(TokenType)*(specifier_count+1)
        );
        specifiers[specifier_count++]=tok.type;
    }

    TokenType *types=malloc(sizeof(TokenType)*(specifier_count+1));
    int type_count=0;
    int storage_count=0;
    StorageClass storage=STORAGE_NONE;

    for(int i=0;i<specifier_count;i++){
        if (is_type_specifier(specifiers[i])){
            types[type_count++]=specifiers[i];
        }else if (specifiers[i]==TOK_STATIC){
            storage_count++;
            storage=STORAGE_STATIC;
        }else if (specifiers[i]==TOK_EXTERN){
            storage_count++;
            storage=STORAGE_EXTERN;
        }
    }

    free(specifiers);

    Type *type=parse_type(types,type_count);
    free(types);

    if (storage_count>1){
        fprintf(stderr,"Parse Error: invalid storage class\n");
        exit(1);
    }

    ParsedSpecifiers result;
    result.type=type;
    result.storage_class=storage;
    return result;
}

// the declarator is already consumed; only ';' or the body remain
Function *parse_function_rest(DeclInfo info,StorageClass storage_class){
    Function *f=calloc(1,sizeof(Function));
    f->name=strdup(info.name);
    f->params=info.param_names;
    f->param_count=info.type->param_count;
    f->body=NULL;
    f->body_count=0;
    f->has_body=0;
    f->fun_type=info.type;
    f->storage_class=storage_class;

    if (peek().type==TOK_SEMICOLON){
        take();
        return f;
    }

    if (peek().type!=TOK_LBRACE){
        fprintf(stderr,"Parse Error: expected function body or ';'\n");
        exit(1);
    }

    take();

    f->has_body=1;

    while(peek().type!=TOK_RBRACE){
        if (peek().type==TOK_EOF){
            fprintf(stderr,"Parse Error: expected '}' before end of file\n");
            exit(1);
        }

        BlockItem *bi=parse_block_item();

        f->body=realloc(
            f->body,
            sizeof(BlockItem*)*(f->body_count+1)
        );
        f->body[f->body_count++]=bi;
    }

    expect(TOK_RBRACE);
    return f;
}

Initializer *parse_initializer(void){
    Initializer *init=calloc(1,sizeof(Initializer));
    if (peek().type!=TOK_LBRACE){ init->exp=parse_exp(0); return init; }
    take(); init->compound=1;
    do {
        init->items=realloc(init->items,sizeof(Initializer*)*(init->count+1));
        init->items[init->count++]=parse_initializer();
        if (peek().type!=TOK_COMMA) break;
        take();
    } while (peek().type!=TOK_RBRACE);
    expect(TOK_RBRACE); return init;
}

Declaration *parse_declaration(){
    ParsedSpecifiers specs=parse_type_and_storage_class();

    DeclInfo info=process_declarator(parse_declarator(0),specs.type);

    Declaration *decl=calloc(1,sizeof(Declaration));

    if (info.type->kind==TYPE_FUNCTION){
        decl->type=DECL_FUN;
        decl->fun=parse_function_rest(info,specs.storage_class);
        return decl;
    }

    VariableDeclaration *var=calloc(1,sizeof(VariableDeclaration));
    var->name=strdup(info.name);
    var->var_type=info.type;
    var->storage_class=specs.storage_class;

    if (peek().type==TOK_ASSIGN){
        take();
        var->init=parse_initializer();
    }else{
        var->init=NULL;
    }

    expect(TOK_SEMICOLON);

    decl->type=DECL_VAR;
    decl->var=var;
    return decl;
}

Program *parse_program() {
    Program *prog=calloc(1,sizeof(Program));

    while(peek().type!=TOK_EOF){
        Declaration *decl=parse_declaration();

        prog->declarations=realloc(
            prog->declarations,
            sizeof(Declaration*)*(prog->declaration_count+1)
        );
        prog->declarations[prog->declaration_count++]=decl;
    }

    return prog;
}

//Semantic analysis
typedef struct LabelMap{
    char *name;
    struct LabelMap *next;
}LabelMap;

int label_exists(LabelMap *map,const char *name){
    while(map){
        if (strcmp(map->name,name)==0) {
            return 1;
        }
        map=map->next;
    }
    return 0;
}


typedef enum {
    SYMBOL_ATTR_CONSTANT,
    SYMBOL_ATTR_FUNCTION,
    SYMBOL_ATTR_STATIC,
    SYMBOL_ATTR_LOCAL
} SymbolAttrType;

typedef enum {
    INITIAL_TENTATIVE,
    INITIAL_VALUE,
    INITIAL_NONE
} InitialValueType;

typedef struct {
    InitialValueType type;
    StaticInitList value;
} InitialValue;

typedef struct Symbol {
    char *name;
    Type *type;

    SymbolAttrType attr_type;

    int defined;
    int global;

    InitialValue init;

    struct Symbol *next;
} Symbol;

Symbol *typecheck_program(Program *prog);
void typecheck_function_declaration(Function *fn,Symbol **symbols);
void typecheck_block_items(BlockItem **items,int count,Symbol **symbols);
void typecheck_file_scope_variable_declaration(VariableDeclaration *decl,Symbol **symbols);
void typecheck_local_variable_declaration(VariableDeclaration *decl,Symbol **symbols);
void typecheck_statement(Statement *stmt,Symbol **symbols);
void typecheck_expression(Exp *exp,Symbol **symbols);
void typecheck_and_convert(Exp **slot,Symbol **symbols){
    if (!*slot) return;
    typecheck_expression(*slot,symbols);
    if ((*slot)->exp_type->kind==TYPE_ARRAY){
        Exp *decay=make_addr_of_exp(*slot);
        decay->exp_type=make_pointer_type((*slot)->exp_type->referenced);
        *slot=decay;
    }
}
void typecheck_for_init(ForInit *init,Symbol **symbols);
static int eval_const_expr(Exp *e,Const *out);

//return type of the function whose body is being type checked
static Type *current_return_type=NULL;

Symbol *symbol_find(Symbol *symbols,const char *name) {
    while(symbols) {
        if (strcmp(symbols->name,name)==0){
            return symbols;
        }
        symbols=symbols->next;
    }

    return NULL;
}

InitialValue make_initial_value(InitialValueType type,StaticInitList value) {
    InitialValue init;
    init.type=type;
    init.value=value;
    return init;
}

//convert a constant to a static initializer of type t, truncating like a C cast
StaticInit const_to_static_init(Const c,Type *t) {
    Const converted=convert_const(c,t);
    StaticInit init;
    switch (converted.type){
        case CONST_CHAR: init.type=INIT_CHAR; break;
        case CONST_UCHAR: init.type=INIT_UCHAR; break;
        case CONST_LONG: init.type=INIT_LONG; break;
        case CONST_UINT: init.type=INIT_UINT; break;
        case CONST_ULONG: init.type=INIT_ULONG; break;
        case CONST_DOUBLE: init.type=INIT_DOUBLE; break;
        default: init.type=INIT_INT; break;
    }
    if (converted.type==CONST_DOUBLE){
        init.dvalue=converted.dvalue;
    }else{
        init.value=converted.value;
    }
    return init;
}

Symbol *symbol_add(Symbol **symbols,const char *name,Type *type) {
    Symbol *entry=calloc(1,sizeof(Symbol));
    if (!entry){
        perror("calloc Symbol");
        exit(1);
    }

    entry->name=strdup(name);
    entry->type=type;
    entry->next=*symbols;
    *symbols=entry;

    return entry;
}

void symbol_add_local(Symbol **symbols,const char *name,Type *type) {
    Symbol *entry=symbol_add(symbols,name,type);
    entry->attr_type=SYMBOL_ATTR_LOCAL;
}

void symbol_add_static(
    Symbol **symbols,
    const char *name,
    Type *type,
    InitialValue init,
    int global
) {
    Symbol *entry=symbol_add(symbols,name,type);
    entry->attr_type=SYMBOL_ATTR_STATIC;
    entry->init=init;
    entry->global=global;
}

void symbol_add_function(
    Symbol **symbols,
    const char *name,
    Type *type,
    int defined,
    int global
) {
    Symbol *entry=symbol_add(symbols,name,type);
    entry->attr_type=SYMBOL_ATTR_FUNCTION;
    entry->defined=defined;
    entry->global=global;
}

static char *register_string_constant(Symbol **symbols,ByteString bytes){
    char *name=make_label("string.");
    Symbol *entry=symbol_add(symbols,name,make_array_type(type_char(),(unsigned long)bytes.length+1));
    entry->attr_type=SYMBOL_ATTR_CONSTANT;
    entry->init.type=INITIAL_VALUE;
    entry->init.value.count=1;
    entry->init.value.items=calloc(1,sizeof(StaticInit));
    entry->init.value.items[0].type=INIT_STRING;
    entry->init.value.items[0].string.bytes=bytes;
    entry->init.value.items[0].string.null_terminated=1;
    return name;
}

Type *get_common_type(Type *t1,Type *t2) {
    t1=integer_promotion(t1); t2=integer_promotion(t2);
    if (type_equal(t1,t2)) {
        return t1;
    }
    if (t1->kind==TYPE_DOUBLE || t2->kind==TYPE_DOUBLE) {
        return type_double();
    }
    if (type_size(t1)==type_size(t2)) {
        return type_is_signed(t1) ? t2 : t1;
    }
    return type_size(t1)>type_size(t2) ? t1 : t2;
}

//wrap *slot in a cast to t unless it already has type t
void convert_to(Exp **slot,Type *t) {
    if (type_equal((*slot)->exp_type,t)) {
        return;
    }

    Exp *cast=make_cast_exp(t,*slot);
    cast->exp_type=t;
    *slot=cast;
}

static void check_variable_redeclaration(
    Symbol *old_symbol,
    VariableDeclaration *decl
) {
    if (old_symbol->type->kind==TYPE_FUNCTION) {
        fprintf(
            stderr,
            "Type Error: function '%s' redeclared as variable\n",
            decl->name
        );
        exit(1);
    }

    if (!type_equal(old_symbol->type,decl->var_type)) {
        fprintf(
            stderr,
            "Type Error: conflicting types for variable '%s'\n",
            decl->name
        );
        exit(1);
    }
}

static int is_shift_op(BinaryOpType op) {
    return op==BINARY_LSHIFT || op==BINARY_RSHIFT || op==BINARY_SHR;
}

static int is_comparison_op(BinaryOpType op) {
    return op>=BINARY_EQ && op<=BINARY_GTE;
}

//%, bitwise and shift operators only take integer operands
static int is_integer_only_op(BinaryOpType op) {
    return op==BINARY_REM || is_shift_op(op) ||
           op==BINARY_BITWISE_AND || op==BINARY_BITWISE_OR || op==BINARY_BITWISE_XOR;
}

static int is_double(Type *t) {
    return t->kind==TYPE_DOUBLE;
}

static int is_pointer(Type *t) {
    return t->kind==TYPE_POINTER;
}

static int is_integer_type(Type *t) {
    return is_character_type(t) || t->kind==TYPE_INT || t->kind==TYPE_UINT || t->kind==TYPE_LONG || t->kind==TYPE_ULONG;
}

static int is_arithmetic(Type *t) {
    return is_integer_type(t) || is_double(t);
}

static int is_void_pointer(Type *t) {
    return is_pointer(t) && t->referenced->kind==TYPE_VOID;
}

static void typecheck_scalar(Exp **slot,Symbol **symbols,const char *context) {
    typecheck_and_convert(slot,symbols);
    if (!is_scalar((*slot)->exp_type)) {
        fprintf(stderr,"Type Error: %s must be scalar\n",context);
        exit(1);
    }
}

int is_lvalue(Exp *e) {
    return e->type==EXP_STRING || e->type==EXP_VAR || e->type==EXP_DEREFERENCE || e->type==EXP_SUBSCRIPT;
}

static void check_lvalue(Exp *e,const char *context) {
    if (!is_lvalue(e)) {
        fprintf(stderr,"Type Error: invalid lvalue in %s\n",context);
        exit(1);
    }
}

//only an integer literal 0 qualifies: 0.0, (long)0 and 10-10 do not
int is_null_pointer_constant(Exp *e) {
    return e->type==EXP_CONSTANT && e->constant.type!=CONST_DOUBLE && e->constant.value==0;
}

Type *get_common_pointer_type(Exp *a,Exp *b) {
    if (type_equal(a->exp_type,b->exp_type)) {
        return a->exp_type;
    }
    if (is_null_pointer_constant(a)) {
        return b->exp_type;
    }
    if (is_null_pointer_constant(b)) {
        return a->exp_type;
    }
    if ((is_void_pointer(a->exp_type) && is_pointer(b->exp_type)) ||
        (is_void_pointer(b->exp_type) && is_pointer(a->exp_type))) {
        return make_pointer_type(type_void());
    }
    fprintf(stderr,"Type Error: expressions have incompatible pointer types\n");
    exit(1);
}

Type *common_operand_type(Exp *a,Exp *b) {
    if (is_pointer(a->exp_type) || is_pointer(b->exp_type)) {
        return get_common_pointer_type(a,b);
    }
    return get_common_type(a->exp_type,b->exp_type);
}

//the implicit conversion of assignment, arguments, initializers and return
void convert_by_assignment(Exp **slot,Type *t) {
    Type *from=(*slot)->exp_type;
    int both_arithmetic=is_arithmetic(from) && is_arithmetic(t);
    int void_pointer=(is_void_pointer(t) && is_pointer(from)) || (is_pointer(t) && is_void_pointer(from));

    if (type_equal(from,t) || both_arithmetic || void_pointer ||
        (is_pointer(t) && is_null_pointer_constant(*slot))) {
        convert_to(slot,t);
        return;
    }

    fprintf(stderr,"Type Error: cannot convert type as if by assignment\n");
    exit(1);
}

void check_binary_operands(BinaryOpType op,Type *left,Type *right) {
    if (op==BINARY_LOGICAL_AND || op==BINARY_LOGICAL_OR) {
        if (!is_scalar(left) || !is_scalar(right)) {
            fprintf(stderr,"Type Error: logical operators only apply to scalar expressions\n");
            exit(1);
        }
        return;
    }

    int has_pointer=is_pointer(left) || is_pointer(right);

    //pointer equality is checked by get_common_pointer_type
    if ((op==BINARY_EQ || op==BINARY_NEQ) && has_pointer) {
        return;
    }

    if (is_integer_only_op(op)) {
        if (!is_integer_type(left) || !is_integer_type(right)) {
            fprintf(stderr,"Type Error: operator requires integer operands\n");
            exit(1);
        }
        return;
    }

    if (has_pointer) {
        int valid=0;
        if (op==BINARY_ADD) valid=(is_pointer_to_complete(left) && is_integer_type(right)) || (is_integer_type(left) && is_pointer_to_complete(right));
        else if (op==BINARY_SUB) valid=(is_pointer_to_complete(left) && is_integer_type(right)) || (is_pointer_to_complete(left) && type_equal(left,right));
        else if (is_comparison_op(op)) valid=is_pointer(left) && is_pointer(right) && type_equal(left,right);
        if (!valid){ fprintf(stderr,"Type Error: incompatible pointer operands\n"); exit(1); }
        return;
    }

    if (!is_arithmetic(left) || !is_arithmetic(right)) {
        fprintf(stderr,"Type Error: operator requires arithmetic operands\n");
        exit(1);
    }
}

static Type *sizeof_operand_type(Exp *e) {
    return e->type==EXP_SIZEOF ? e->sizeof_exp->exp_type : e->sizeof_type;
}

static Const sizeof_value(Exp *e) {
    return make_const(type_ulong(),(long)type_bytes(sizeof_operand_type(e)));
}

void typecheck_expression(Exp *exp,Symbol **symbols) {
    if (!exp) {
        return;
    }

    switch(exp->type) {
        case EXP_STRING:
            exp->exp_type=make_array_type(type_char(),(unsigned long)exp->string.bytes.length+1);
            exp->string.name=register_string_constant(symbols,exp->string.bytes);
            return;
        case EXP_CONSTANT:
            exp->exp_type=const_exp_type(exp->constant);
            return;

        case EXP_CAST:
            validate_type_specifier(exp->cast.target_type);
            typecheck_and_convert(&exp->cast.exp,symbols);
            exp->exp_type=exp->cast.target_type;
            if (exp->exp_type->kind==TYPE_VOID) return;
            if (!is_scalar(exp->cast.target_type) || !is_scalar(exp->cast.exp->exp_type)) {
                fprintf(stderr,"Type Error: can only cast scalar expressions to scalar type or void\n");
                exit(1);
            }
            if ((is_pointer(exp->cast.target_type) && is_double(exp->cast.exp->exp_type)) ||
                (is_double(exp->cast.target_type) && is_pointer(exp->cast.exp->exp_type))) {
                fprintf(stderr,"Type Error: can't cast between a pointer and a double\n");
                exit(1);
            }
            return;

        case EXP_VAR: {
            Symbol *entry=symbol_find(*symbols,exp->var_name);

            if (!entry) {
                fprintf(stderr,"Type Error: unknown identifier '%s'\n",exp->var_name);
                exit(1);
            }

            if (entry->type->kind==TYPE_FUNCTION) {
                fprintf(
                    stderr,
                    "Type Error: function '%s' used as variable\n",
                    exp->var_name
                );
                exit(1);
            }

            exp->exp_type=entry->type;
            return;
        }

        case EXP_FUNCTION_CALL: {
            Symbol *entry=symbol_find(*symbols,exp->function_call.name);

            if (!entry) {
                fprintf(
                    stderr,
                    "Type Error: unknown function '%s'\n",
                    exp->function_call.name
                );
                exit(1);
            }

            Type *fun_type=entry->type;

            if (fun_type->kind!=TYPE_FUNCTION) {
                fprintf(
                    stderr,
                    "Type Error: called object '%s' is not a function\n",
                    exp->function_call.name
                );
                exit(1);
            }

            if (fun_type->param_count!=exp->function_call.arg_count) {
                fprintf(
                    stderr,
                    "Type Error: function '%s' expects %d arguments but got %d\n",
                    exp->function_call.name,
                    fun_type->param_count,
                    exp->function_call.arg_count
                );
                exit(1);
            }

            for(int i=0;i<exp->function_call.arg_count;i++){
                typecheck_and_convert(&exp->function_call.args[i],symbols);
                convert_by_assignment(&exp->function_call.args[i],fun_type->params[i]);
            }

            exp->exp_type=fun_type->ret;
            return;
        }

        case EXP_UNARY:
            if (exp->unary.op==UNARY_NOT) {
                typecheck_scalar(&exp->unary.exp,symbols,"operand of !");
            }else{
                typecheck_and_convert(&exp->unary.exp,symbols);
                if (!is_arithmetic(exp->unary.exp->exp_type)) {
                    fprintf(stderr,"Type Error: can only negate or complement arithmetic values\n");
                    exit(1);
                }
            }
            if (exp->unary.op==UNARY_COMPLEMENT && is_double(exp->unary.exp->exp_type)) {
                fprintf(stderr,"Type Error: can't take the bitwise complement of a double\n");
                exit(1);
            }
            if (exp->unary.op!=UNARY_NOT) convert_to(&exp->unary.exp,integer_promotion(exp->unary.exp->exp_type));
            exp->exp_type=(exp->unary.op==UNARY_NOT) ? type_int() : exp->unary.exp->exp_type;
            return;

        case EXP_BINARY: {
            BinaryOpType op=exp->binary.op;
            typecheck_and_convert(&exp->binary.left,symbols);
            typecheck_and_convert(&exp->binary.right,symbols);
            check_binary_operands(op,exp->binary.left->exp_type,exp->binary.right->exp_type);

            if (op==BINARY_LOGICAL_AND || op==BINARY_LOGICAL_OR) {
                exp->exp_type=type_int();
                return;
            }

            Type *left=exp->binary.left->exp_type,*right=exp->binary.right->exp_type;
            if ((op==BINARY_ADD || op==BINARY_SUB) && (is_pointer(left) || is_pointer(right))){
                if (is_pointer(left) && is_pointer(right)) exp->exp_type=type_long();
                else if (is_pointer(left)){ convert_to(&exp->binary.right,type_long()); exp->exp_type=left; }
                else { convert_to(&exp->binary.left,type_long()); exp->exp_type=right; }
                return;
            }

            //a shift has its left operand's type; no usual arithmetic conversions
            if (is_shift_op(op)) {
                Type *left_type=integer_promotion(exp->binary.left->exp_type);
                convert_to(&exp->binary.left,left_type);
                convert_to(&exp->binary.right,left_type);
                exp->exp_type=left_type;
                return;
            }

            Type *common_type=common_operand_type(exp->binary.left,exp->binary.right);
            convert_to(&exp->binary.left,common_type);
            convert_to(&exp->binary.right,common_type);

            exp->exp_type=is_comparison_op(op) ? type_int() : common_type;
            return;
        }

        case EXP_ASSIGN:
            typecheck_and_convert(&exp->assign.lvalue,symbols);
            typecheck_and_convert(&exp->assign.rvalue,symbols);
            check_lvalue(exp->assign.lvalue,"assignment");
            convert_by_assignment(&exp->assign.rvalue,exp->assign.lvalue->exp_type);
            exp->exp_type=exp->assign.lvalue->exp_type;
            return;

        /*
        a op= b computes in the common type of a and b (in a's type for shifts);
        TACKY generation reads that type from the converted rvalue.
        */
        case EXP_COMPOUND_ASSIGN: {
            typecheck_and_convert(&exp->compound.lvalue,symbols);
            typecheck_and_convert(&exp->compound.rvalue,symbols);
            check_lvalue(exp->compound.lvalue,"compound assignment");

            Type *left_type=exp->compound.lvalue->exp_type;
            check_binary_operands(exp->compound.op,left_type,exp->compound.rvalue->exp_type);
            if (is_pointer(left_type)){
                if ((exp->compound.op!=BINARY_ADD && exp->compound.op!=BINARY_SUB) || !is_integer_type(exp->compound.rvalue->exp_type)){
                    fprintf(stderr,"Type Error: invalid pointer compound assignment\n"); exit(1);
                }
                convert_to(&exp->compound.rvalue,type_long()); exp->exp_type=left_type; return;
            }
            if (is_pointer(exp->compound.rvalue->exp_type)){
                fprintf(stderr,"Type Error: pointer cannot be assigned to an arithmetic object\n"); exit(1);
            }
            Type *op_type=is_shift_op(exp->compound.op)
                ? integer_promotion(left_type)
                : get_common_type(left_type,exp->compound.rvalue->exp_type);

            convert_to(&exp->compound.rvalue,op_type);
            exp->exp_type=left_type;
            return;
        }

        case EXP_PREFIX_OP:
        case EXP_POSTFIX_OP:
            typecheck_and_convert(&exp->inc_dec.lvalue,symbols);
            check_lvalue(exp->inc_dec.lvalue,"increment/decrement");
            exp->exp_type=exp->inc_dec.lvalue->exp_type;
            check_binary_operands(
                exp->inc_dec.is_increment ? BINARY_ADD : BINARY_SUB,
                exp->exp_type,
                type_int()
            );
            return;

        case EXP_CONDITIONAL: {
            typecheck_scalar(&exp->conditional.condition,symbols,"condition in conditional operator");
            typecheck_and_convert(&exp->conditional.true_expr,symbols);
            typecheck_and_convert(&exp->conditional.false_expr,symbols);

            Type *t1=exp->conditional.true_expr->exp_type,*t2=exp->conditional.false_expr->exp_type;
            Type *common_type;
            if (t1->kind==TYPE_VOID && t2->kind==TYPE_VOID) common_type=type_void();
            else if (is_arithmetic(t1) && is_arithmetic(t2)) common_type=get_common_type(t1,t2);
            else if (is_pointer(t1) || is_pointer(t2)) common_type=get_common_pointer_type(exp->conditional.true_expr,exp->conditional.false_expr);
            else {
                fprintf(stderr,"Type Error: cannot convert branches of conditional to a common type\n");
                exit(1);
            }
            convert_to(&exp->conditional.true_expr,common_type);
            convert_to(&exp->conditional.false_expr,common_type);
            exp->exp_type=common_type;
            return;
        }

        case EXP_SUBSCRIPT: {
            typecheck_and_convert(&exp->subscript.left,symbols);
            typecheck_and_convert(&exp->subscript.right,symbols);
            Exp **ptr=&exp->subscript.left,**idx=&exp->subscript.right;
            if (!is_pointer((*ptr)->exp_type)){ ptr=&exp->subscript.right; idx=&exp->subscript.left; }
            if (!is_pointer_to_complete((*ptr)->exp_type) || !is_integer_type((*idx)->exp_type)){
                fprintf(stderr,"Type Error: invalid subscript operands\n"); exit(1);
            }
            convert_to(idx,type_long()); exp->exp_type=(*ptr)->exp_type->referenced; return;
        }
        case EXP_DEREFERENCE:
            typecheck_and_convert(&exp->deref.exp,symbols);
            if (!is_pointer(exp->deref.exp->exp_type)) {
                fprintf(stderr,"Type Error: can't dereference a non-pointer\n");
                exit(1);
            }
            if (is_void_pointer(exp->deref.exp->exp_type)) {
                fprintf(stderr,"Type Error: can't dereference a pointer to void\n");
                exit(1);
            }
            exp->exp_type=exp->deref.exp->exp_type->referenced;
            return;

        case EXP_ADDR_OF:
            typecheck_expression(exp->addr_of.exp,symbols);
            check_lvalue(exp->addr_of.exp,"address-of");
            exp->exp_type=make_pointer_type(exp->addr_of.exp->exp_type);
            return;

        //the operand is not decayed: sizeof of an array is the whole array
        case EXP_SIZEOF:
        case EXP_SIZEOF_TYPE:
            if (exp->type==EXP_SIZEOF) typecheck_expression(exp->sizeof_exp,symbols);
            else validate_type_specifier(exp->sizeof_type);
            if (!is_complete(sizeof_operand_type(exp))) {
                fprintf(stderr,"Type Error: can't get the size of an incomplete type\n");
                exit(1);
            }
            exp->exp_type=type_ulong();
            return;
    }
}

static Initializer *zero_initializer(Type *t){
    Initializer *init=calloc(1,sizeof(Initializer)); init->type=t;
    if (t->kind==TYPE_ARRAY){
        init->compound=1; init->count=(int)t->count;
        init->items=calloc(t->count,sizeof(Initializer*));
        for (int i=0;i<(int)t->count;i++) init->items[i]=zero_initializer(t->referenced);
    }else{
        init->exp=make_constant_exp(make_const(t,0)); init->exp->exp_type=t;
    }
    return init;
}

static void check_initializer(Initializer *init,Type *t,Symbol **symbols){
    init->type=t;
    if (t->kind==TYPE_ARRAY && !init->compound && init->exp->type==EXP_STRING){
        if (!is_character_type(t->referenced) || (unsigned long)init->exp->string.bytes.length>t->count){
            fprintf(stderr,"Type Error: string does not fit character array\n"); exit(1);
        }
        init->exp->exp_type=t;
        return;
    }
    if (t->kind!=TYPE_ARRAY){
        if (init->compound){ fprintf(stderr,"Type Error: scalar initializer cannot be a list\n"); exit(1); }
        typecheck_and_convert(&init->exp,symbols); convert_by_assignment(&init->exp,t); return;
    }
    if (!init->compound || init->count>(int)t->count){ fprintf(stderr,"Type Error: invalid array initializer shape\n"); exit(1); }
    for (int i=0;i<init->count;i++) check_initializer(init->items[i],t->referenced,symbols);
    init->items=realloc(init->items,sizeof(Initializer*)*t->count);
    while (init->count<(int)t->count) init->items[init->count++]=zero_initializer(t->referenced);
}

static void append_static_init(StaticInitList *list,StaticInit value){
    if (value.type==INIT_ZERO && list->count && list->items[list->count-1].type==INIT_ZERO){
        list->items[list->count-1].value+=value.value; return;
    }
    list->items=realloc(list->items,sizeof(StaticInit)*(list->count+1));
    list->items[list->count++]=value;
}

static StaticInitList zero_static_list(Type *t){
    StaticInitList list={0}; append_static_init(&list,(StaticInit){INIT_ZERO,{type_size(t)}}); return list;
}

static void flatten_static_initializer(Initializer *init,StaticInitList *list){
    if (init->compound){
        for (int i=0;i<init->count;i++) flatten_static_initializer(init->items[i],list);
    }else if (init->type->kind==TYPE_ARRAY && init->exp->type==EXP_STRING){
        ByteString bytes=init->exp->string.bytes;
        int terminated=(unsigned long)bytes.length<init->type->count;
        StaticInit string={.type=INIT_STRING};
        string.string.bytes=bytes; string.string.null_terminated=terminated;
        append_static_init(list,string);
        long padding=(long)init->type->count-bytes.length-terminated;
        if (padding) append_static_init(list,(StaticInit){INIT_ZERO,{padding}});
    }else if (init->type->kind==TYPE_POINTER && init->exp->type==EXP_ADDR_OF && init->exp->addr_of.exp->type==EXP_STRING){
        StaticInit pointer={.type=INIT_POINTER};
        pointer.pointer_name=init->exp->addr_of.exp->string.name;
        append_static_init(list,pointer);
    }else{
        Const c;
        if (!eval_const_expr(init->exp,&c)){ fprintf(stderr,"Type Error: non-constant static initializer\n"); exit(1); }
        StaticInit v=const_to_static_init(c,init->type);
        unsigned long bits=0;
        if (v.type==INIT_DOUBLE) memcpy(&bits,&v.dvalue,sizeof(bits)); else bits=(unsigned long)v.value;
        if (bits==0) v=(StaticInit){INIT_ZERO,{type_size(init->type)}};
        append_static_init(list,v);
    }
}

static StaticInitList eval_static_initializer(Initializer *init,Type *target,Symbol **symbols){
    check_initializer(init,target,symbols);
    StaticInitList list={0}; flatten_static_initializer(init,&list); return list;
}

static void validate_variable_type(VariableDeclaration *decl) {
    validate_type_specifier(decl->var_type);
    if (decl->var_type->kind==TYPE_VOID) {
        fprintf(stderr,"Type Error: variable '%s' declared void\n",decl->name);
        exit(1);
    }
}

void typecheck_file_scope_variable_declaration(
    VariableDeclaration *decl,
    Symbol **symbols
) {
    InitialValue initial_value;
    validate_variable_type(decl);
    if (decl->init || decl->storage_class!=STORAGE_EXTERN) type_size(decl->var_type);

    if (decl->init) {
        initial_value=make_initial_value(INITIAL_VALUE,eval_static_initializer(decl->init,decl->var_type,symbols));
    }
    else if (decl->storage_class==STORAGE_EXTERN) {
        initial_value=make_initial_value(INITIAL_NONE,(StaticInitList){0});
    }
    else {
        initial_value=make_initial_value(INITIAL_TENTATIVE,zero_static_list(decl->var_type));
    }

    int global=(decl->storage_class!=STORAGE_STATIC);

    Symbol *old_symbol=symbol_find(*symbols,decl->name);

    if (old_symbol) {
        check_variable_redeclaration(old_symbol,decl);

        if (old_symbol->attr_type!=SYMBOL_ATTR_STATIC) {
            fprintf(
                stderr,
                "Type Error: conflicting declaration of variable '%s'\n",
                decl->name
            );
            exit(1);
        }

        if (decl->storage_class==STORAGE_EXTERN) {
            global=old_symbol->global;
        }
        else if (old_symbol->global!=global) {
            fprintf(
                stderr,
                "Type Error: conflicting variable linkage for '%s'\n",
                decl->name
            );
            exit(1);
        }

        if (old_symbol->init.type==INITIAL_VALUE) {
            if (initial_value.type==INITIAL_VALUE) {
                fprintf(
                    stderr,
                    "Type Error: conflicting file-scope definitions of '%s'\n",
                    decl->name
                );
                exit(1);
            }

            initial_value=old_symbol->init;
        }
        else if (
            initial_value.type!=INITIAL_VALUE &&
            old_symbol->init.type==INITIAL_TENTATIVE
        ) {
            initial_value=old_symbol->init;
        }

        old_symbol->attr_type=SYMBOL_ATTR_STATIC;
        old_symbol->init=initial_value;
        old_symbol->global=global;
        return;
    }

    symbol_add_static(
        symbols,
        decl->name,
        decl->var_type,
        initial_value,
        global
    );
}

void typecheck_local_variable_declaration(
    VariableDeclaration *decl,
    Symbol **symbols
) {
    validate_variable_type(decl);
    if (decl->storage_class!=STORAGE_EXTERN) type_size(decl->var_type);
    if (decl->storage_class==STORAGE_EXTERN) {
        if (decl->init) {
            fprintf(
                stderr,
                "Type Error: initializer on local extern variable '%s'\n",
                decl->name
            );
            exit(1);
        }

        Symbol *old_symbol=symbol_find(*symbols,decl->name);

        if (old_symbol) {
            check_variable_redeclaration(old_symbol,decl);

            return;
        }

        symbol_add_static(
            symbols,
            decl->name,
            decl->var_type,
            make_initial_value(INITIAL_NONE,(StaticInitList){0}),
            1
        );
        return;
    }

    if (decl->storage_class==STORAGE_STATIC) {
        InitialValue initial_value;

        if (decl->init) {
            initial_value=make_initial_value(INITIAL_VALUE,eval_static_initializer(decl->init,decl->var_type,symbols));
        }
        else {
            initial_value=make_initial_value(INITIAL_VALUE,zero_static_list(decl->var_type));
        }

        symbol_add_static(
            symbols,
            decl->name,
            decl->var_type,
            initial_value,
            0
        );
        return;
    }

    symbol_add_local(symbols,decl->name,decl->var_type);

    if (decl->init) {
        check_initializer(decl->init,decl->var_type,symbols);
    }
}

void typecheck_for_init(ForInit *init,Symbol **symbols) {
    if (!init) {
        return;
    }

    if (init->type==FOR_INIT_DECL) {
        if (init->decl->storage_class!=STORAGE_NONE) {
            fprintf(
                stderr,
                "Type Error: storage-class specifier is not allowed in for-loop declaration\n"
            );
            exit(1);
        }

        typecheck_local_variable_declaration(
            init->decl,
            symbols
        );
        return;
    }

    if (init->exp) {
        typecheck_and_convert(&init->exp,symbols);
    }
}

void typecheck_statement(Statement *stmt,Symbol **symbols) {
    if (!stmt) {
        return;
    }

    switch(stmt->type) {
        case STMT_RETURN:
            if (current_return_type->kind==TYPE_VOID) {
                if (stmt->exp) {
                    fprintf(stderr,"Type Error: void function cannot return a value\n");
                    exit(1);
                }
                return;
            }
            if (!stmt->exp) {
                fprintf(stderr,"Type Error: non-void function must return a value\n");
                exit(1);
            }
            typecheck_and_convert(&stmt->exp,symbols);
            convert_by_assignment(&stmt->exp,current_return_type);
            return;

        case STMT_EXPRESSION:
            if (stmt->exp) {
                typecheck_and_convert(&stmt->exp,symbols);
            }
            return;

        case STMT_IF:
            typecheck_scalar(&stmt->if_stmt.condition,symbols,"if condition");
            typecheck_statement(stmt->if_stmt.then_branch,symbols);

            if (stmt->if_stmt.else_branch) {
                typecheck_statement(stmt->if_stmt.else_branch,symbols);
            }
            return;

        case STMT_COMPOUND:
            typecheck_block_items(
                stmt->block_items,
                stmt->block_count,
                symbols
            );
            return;

        case STMT_LABEL:
            typecheck_statement(
                stmt->if_stmt.then_branch,
                symbols
            );
            return;

        case STMT_WHILE:
            typecheck_scalar(&stmt->while_stmt.condition,symbols,"loop condition");
            typecheck_statement(
                stmt->while_stmt.body,
                symbols
            );
            return;

        case STMT_DO_WHILE:
            typecheck_statement(
                stmt->do_while_stmt.body,
                symbols
            );
            typecheck_scalar(&stmt->do_while_stmt.condition,symbols,"loop condition");
            return;

        case STMT_FOR:
            typecheck_for_init(
                stmt->for_stmt.init,
                symbols
            );

            if (stmt->for_stmt.condition) {
                typecheck_scalar(&stmt->for_stmt.condition,symbols,"loop condition");
            }

            if (stmt->for_stmt.post) {
                typecheck_and_convert(&stmt->for_stmt.post,symbols);
            }

            typecheck_statement(
                stmt->for_stmt.body,
                symbols
            );
            return;

        case STMT_SWITCH:
            typecheck_and_convert(&stmt->switch_stmt.control,symbols);
            convert_to(&stmt->switch_stmt.control,integer_promotion(stmt->switch_stmt.control->exp_type));
            if (!is_integer_type(stmt->switch_stmt.control->exp_type)) {
                fprintf(stderr,"Type Error: switch controlling expression must be an integer\n");
                exit(1);
            }
            typecheck_statement(
                stmt->switch_stmt.body,
                symbols
            );
            return;

        case STMT_CASE:
            typecheck_and_convert(&stmt->case_stmt.value_exp,symbols);
            if (!is_integer_type(stmt->case_stmt.value_exp->exp_type)) {
                fprintf(stderr,"Type Error: case label must be an integer\n");
                exit(1);
            }
            typecheck_statement(
                stmt->case_stmt.body,
                symbols
            );
            return;

        case STMT_DEFAULT:
            typecheck_statement(
                stmt->default_stmt.body,
                symbols
            );
            return;

        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
        case STMT_NULL:
            return;
    }
}

void typecheck_block_items(
    BlockItem **items,
    int count,
    Symbol **symbols
) {
    for(int i=0;i<count;i++) {
        BlockItem *bi=items[i];

        if (bi->type==BI_DECL) {
            if (bi->decl->type==DECL_VAR) {
                typecheck_local_variable_declaration(
                    bi->decl->var,
                    symbols
                );
            }
            else {
                typecheck_function_declaration(
                    bi->decl->fun,
                    symbols
                );
            }
        }
        else if (bi->type==BI_STMT) {
            typecheck_statement(
                bi->stmt,
                symbols
            );
        }
    }
}

void typecheck_function_declaration(
    Function *fn,
    Symbol **symbols
) {
    if (fn->fun_type->ret->kind==TYPE_ARRAY){ fprintf(stderr,"Type Error: function returns array\n"); exit(1); }
    validate_type_specifier(fn->fun_type);
    for (int i=0;i<fn->fun_type->param_count;i++) {
        if (fn->fun_type->params[i]->kind==TYPE_VOID) {
            fprintf(stderr,"Type Error: parameter of function '%s' declared void\n",fn->name);
            exit(1);
        }
    }

    int has_body=fn->has_body;
    int already_defined=0;
    int global=(fn->storage_class!=STORAGE_STATIC);

    Symbol *old_symbol=symbol_find(*symbols,fn->name);

    if (old_symbol) {
        if (old_symbol->type->kind!=TYPE_FUNCTION) {
            fprintf(
                stderr,
                "Type Error: '%s' redeclared as function\n",
                fn->name
            );
            exit(1);
        }

        if (!type_equal(old_symbol->type,fn->fun_type)) {
            fprintf(
                stderr,
                "Type Error: incompatible declarations of function '%s'\n",
                fn->name
            );
            exit(1);
        }

        already_defined=old_symbol->defined;

        if (already_defined && has_body) {
            fprintf(
                stderr,
                "Type Error: function '%s' defined more than once\n",
                fn->name
            );
            exit(1);
        }

        if (
            old_symbol->global &&
            fn->storage_class==STORAGE_STATIC
        ) {
            fprintf(
                stderr,
                "Type Error: static declaration of function '%s' follows non-static declaration\n",
                fn->name
            );
            exit(1);
        }

        global=old_symbol->global;
        old_symbol->defined=(already_defined || has_body);
        old_symbol->global=global;
        old_symbol->attr_type=SYMBOL_ATTR_FUNCTION;
    }
    else {
        symbol_add_function(
            symbols,
            fn->name,
            fn->fun_type,
            has_body,
            global
        );
    }

    if (!has_body) {
        return;
    }

    for(int i=0;i<fn->param_count;i++) {
        symbol_add_local(
            symbols,
            fn->params[i],
            fn->fun_type->params[i]
        );
    }

    current_return_type=fn->fun_type->ret;

    typecheck_block_items(
        fn->body,
        fn->body_count,
        symbols
    );
}

Symbol *typecheck_program(Program *prog) {
    if (!prog) {
        return NULL;
    }

    Symbol *symbols=NULL;

    for(int i=0;i<prog->declaration_count;i++) {
        Declaration *decl=prog->declarations[i];

        if (decl->type==DECL_FUN) {
            typecheck_function_declaration(
                decl->fun,
                &symbols
            );
        }
        else {
            typecheck_file_scope_variable_declaration(
                decl->var,
                &symbols
            );
        }
    }

    return symbols;
}

void check_labels_statement(Statement *stmt,LabelMap **map){
    if (stmt->type==STMT_LABEL){
        if (label_exists(*map,stmt->label)){
            fprintf(stderr,"Semantic Error: duplicate label '%s'\n",stmt->label);
            exit(1);
        }

        LabelMap *entry=malloc(sizeof(LabelMap));
        entry->name=strdup(stmt->label);
        entry->next=*map;
        *map=entry;

        check_labels_statement(stmt->if_stmt.then_branch,map);
    }

    else if (stmt->type==STMT_IF){
        check_labels_statement(stmt->if_stmt.then_branch,map);

        if (stmt->if_stmt.else_branch){
            check_labels_statement(stmt->if_stmt.else_branch,map);
        }
    }
    else if (stmt->type==STMT_COMPOUND){
        for(int i=0;i<stmt->block_count;i++){
            BlockItem *bi=stmt->block_items[i];
            if (bi->type==BI_STMT){
                check_labels_statement(bi->stmt,map);
            }
        }
    }
    else if (stmt->type==STMT_WHILE){
        check_labels_statement(stmt->while_stmt.body,map);
    }
    else if (stmt->type==STMT_DO_WHILE){
        check_labels_statement(stmt->do_while_stmt.body,map);
    }
    else if (stmt->type==STMT_FOR){
        check_labels_statement(stmt->for_stmt.body,map);
    }
    else if (stmt->type==STMT_SWITCH){
        check_labels_statement(stmt->switch_stmt.body,map);
    }
    else if (stmt->type==STMT_CASE){
        check_labels_statement(stmt->case_stmt.body,map);
    }
    else if (stmt->type==STMT_DEFAULT){
        check_labels_statement(stmt->default_stmt.body,map);
    }
}



void resolve_block_items(BlockItem **items,int count,IdentifierMap **map);

IdentifierMap *map_find(IdentifierMap *map, const char *name){
    while (map) {
        if (strcmp(map->user_name,name)==0) {
            return map;
        }
        map=map->next;
    }
    return NULL;
}

//find vairable in  the mapping table 
char *map_get(IdentifierMap *map,const char *name){
    IdentifierMap *entry=map_find(map,name);
    return entry ? entry->unique_name : NULL;
}

void map_add(IdentifierMap **map,
            const char *user_name,
            const char *unique_name,
            int from_current_block,
            int has_linkage) {
                IdentifierMap *entry=malloc(sizeof(IdentifierMap));
                entry->user_name=strdup(user_name);
                entry->unique_name=strdup(unique_name);
                entry->from_current_block=from_current_block;
                entry->has_linkage=has_linkage;
                entry->next=*map;
                *map=entry;
            }

IdentifierMap *copy_identifier_map(IdentifierMap *map){
    IdentifierMap *new_head=NULL;
    IdentifierMap **tail=&new_head;

    while(map){
        IdentifierMap *node=malloc(sizeof(IdentifierMap));
        node->user_name=strdup(map->user_name);
        node->unique_name=strdup(map->unique_name);
        node->from_current_block=0;
        node->has_linkage=map->has_linkage;
        node->next=NULL;

        *tail=node;
        tail=&node->next;

        map=map->next;
    }

    return new_head;
}

// C labels are function-scoped but .L symbols are file-scoped, so qualify each
// label with its function; '.' can't appear in an identifier, so names stay unique
static char *qualify_label(const char *fun_name,const char *label){
    char *name=malloc(strlen(fun_name)+strlen(label)+2);
    sprintf(name,"%s.%s",fun_name,label);
    return name;
}

void check_goto_statement(Statement *stmt,LabelMap *labels,const char *fun_name){
    if (stmt->type==STMT_GOTO){
        //check label exist
        if (!label_exists(labels,stmt->label)){
            fprintf(stderr, "Semantic Error: goto to undefined label '%s'\n", stmt->label);
            exit(1);
        }

        stmt->label=qualify_label(fun_name,stmt->label);
    }

    else if (stmt->type==STMT_LABEL){
        stmt->label=qualify_label(fun_name,stmt->label);
        check_goto_statement(stmt->if_stmt.then_branch,labels,fun_name);
    }

    else if (stmt->type==STMT_IF){
        check_goto_statement(stmt->if_stmt.then_branch,labels,fun_name);

        if (stmt->if_stmt.else_branch){
            check_goto_statement(stmt->if_stmt.else_branch,labels,fun_name);
        }
    }

    else if (stmt->type==STMT_COMPOUND){
        for(int i=0;i<stmt->block_count;i++){
            BlockItem *bi=stmt->block_items[i];
            if (bi->type==BI_STMT){
                check_goto_statement(bi->stmt,labels,fun_name);
            }
        }
    }

    else if (stmt->type==STMT_WHILE){
        check_goto_statement(stmt->while_stmt.body,labels,fun_name);
    }
    else if (stmt->type==STMT_DO_WHILE){
        check_goto_statement(stmt->do_while_stmt.body,labels,fun_name);
    }
    else if (stmt->type==STMT_FOR){
        check_goto_statement(stmt->for_stmt.body,labels,fun_name);
    }
    else if (stmt->type==STMT_SWITCH){
        check_goto_statement(stmt->switch_stmt.body,labels,fun_name);
    }
    else if (stmt->type==STMT_CASE){
        check_goto_statement(stmt->case_stmt.body,labels,fun_name);
    }
    else if (stmt->type==STMT_DEFAULT){
        check_goto_statement(stmt->default_stmt.body,labels,fun_name);
    }
}

void resolve_expression(Exp *e,IdentifierMap *map){
    if (!e) return;
    switch (e->type){

        case EXP_CONDITIONAL:
            resolve_expression(e->conditional.condition,map);
            resolve_expression(e->conditional.true_expr,map);
            resolve_expression(e->conditional.false_expr,map);
            break;

        case EXP_STRING:
        case EXP_CONSTANT:
            break;

        case EXP_CAST:
            resolve_expression(e->cast.exp,map);
            break;

        case EXP_COMPOUND_ASSIGN:
            resolve_expression(e->compound.lvalue,map);
            resolve_expression(e->compound.rvalue,map);
            break;
        case EXP_PREFIX_OP:
        case EXP_POSTFIX_OP:
            resolve_expression(e->inc_dec.lvalue, map);
            break;
        case EXP_VAR: {
            char *unique=map_get(map,e->var_name);
            if (!unique) {
                fprintf(stderr, "Semantic Error: Undeclared variable '%s'\n", e->var_name);
                exit(1);
            }
            free(e->var_name);
            e->var_name=strdup(unique);
            break;
        }

        case EXP_FUNCTION_CALL: {
            IdentifierMap *entry=map_find(map,e->function_call.name);

            if (!entry) {
                fprintf(stderr,
                        "Semantic Error: undeclared function '%s'\n",
                    e->function_call.name);
                exit(1);
            }

            free(e->function_call.name);
            e->function_call.name=strdup(entry->unique_name);

            for(int i=0;i<e->function_call.arg_count;i++) {
                resolve_expression(e->function_call.args[i],map);
            }
            
            break;
        }

        case EXP_ASSIGN:
            resolve_expression(e->assign.lvalue,map);
            resolve_expression(e->assign.rvalue,map);
            break;
        
        case EXP_UNARY:
            resolve_expression(e->unary.exp,map);
            break;
        
        case EXP_BINARY:
            resolve_expression(e->binary.left,map);
            resolve_expression(e->binary.right,map);
            break;

        case EXP_SUBSCRIPT:
            resolve_expression(e->subscript.left,map);
            resolve_expression(e->subscript.right,map);
            return;
        case EXP_DEREFERENCE:
            resolve_expression(e->deref.exp,map);
            break;

        case EXP_ADDR_OF:
            resolve_expression(e->addr_of.exp,map);
            break;

        case EXP_SIZEOF:
            resolve_expression(e->sizeof_exp,map);
            break;
        case EXP_SIZEOF_TYPE:
            break;
    }
}

char *resolve_local_name(const char *name,IdentifierMap **map){
    IdentifierMap *curr=*map;

    while(curr) {
        if (
            strcmp(curr->user_name,name)==0 &&
            curr->from_current_block
        ) {
            fprintf(
                stderr,
                "Semantic Error: duplicate declaration of '%s'\n",
                name
            );
            exit(1);
        }

        curr=curr->next;
    }

    char *unique=make_temporary();

    map_add(
        map,
        name,
        unique,
        1,
        0
    );

    return unique;
}

void resolve_file_scope_variable_declaration(
    VariableDeclaration *decl,
    IdentifierMap **map
) {
    map_add(
        map,
        decl->name,
        decl->name,
        1,
        1
    );
}

void resolve_local_variable_declaration(
    VariableDeclaration *decl,
    IdentifierMap **map
) {
    IdentifierMap *prev=map_find(*map,decl->name);

    if (prev && prev->from_current_block) {
        if (!(
            prev->has_linkage &&
            decl->storage_class==STORAGE_EXTERN
        )) {
            fprintf(
                stderr,
                "Semantic Error: conflicting local declarations of '%s'\n",
                decl->name
            );
            exit(1);
        }
    }

    if (decl->storage_class==STORAGE_EXTERN) {
        map_add(
            map,
            decl->name,
            decl->name,
            1,
            1
        );

        return;
    }

    char *unique=make_temporary();

    map_add(
        map,
        decl->name,
        unique,
        1,
        0
    );

    if (decl->init) {
        resolve_initializer(decl->init,*map);
    }

    free(decl->name);
    decl->name=strdup(unique);
}

void resolve_function_declaration(
    Function *fn,
    IdentifierMap **map
) {
    IdentifierMap *prev=map_find(*map,fn->name);

    if (
        prev &&
        prev->from_current_block &&
        !prev->has_linkage
    ) {
        fprintf(
            stderr,
            "Semantic Error: conflicting function declaration '%s'\n",
            fn->name
        );
        exit(1);
    }

    map_add(
        map,
        fn->name,
        fn->name,
        1,
        1
    );

    IdentifierMap *inner_map=copy_identifier_map(*map);

    for(int i=0;i<fn->param_count;i++) {
        char *unique=resolve_local_name(
            fn->params[i],
            &inner_map
        );

        free(fn->params[i]);
        fn->params[i]=strdup(unique);
    }

    if (fn->has_body) {
        resolve_block_items(
            fn->body,
            fn->body_count,
            &inner_map
        );
    }
}


void resolve_statement(Statement *stmt,IdentifierMap *map){
    if (stmt->type==STMT_RETURN || stmt->type==STMT_EXPRESSION){
        if (stmt->exp){
            resolve_expression(stmt->exp,map);
        }
    }
    else if (stmt->type==STMT_IF){
        // resolve if condition statement
        resolve_expression(stmt->if_stmt.condition,map);

        //resolve if true statement
        resolve_statement(stmt->if_stmt.then_branch,map);

        //if have else statement, resolve it
        if (stmt->if_stmt.else_branch){
            resolve_statement(stmt->if_stmt.else_branch,map);
        }
    }
    else if (stmt->type==STMT_LABEL){
        resolve_statement(stmt->if_stmt.then_branch,map);
    }
    else if (stmt->type==STMT_COMPOUND){
        //inner block gets a copy of current map
        IdentifierMap *inner_map=copy_identifier_map(map);
        resolve_block_items(stmt->block_items,stmt->block_count,&inner_map);
    }
    else if (stmt->type==STMT_WHILE){
        resolve_expression(stmt->while_stmt.condition,map);
        resolve_statement(stmt->while_stmt.body,map);
    }
    else if (stmt->type==STMT_DO_WHILE){
        resolve_statement(stmt->do_while_stmt.body,map);
        resolve_expression(stmt->do_while_stmt.condition,map);
    }
    else if (stmt->type==STMT_FOR){
        IdentifierMap *inner_map=copy_identifier_map(map);

        resolve_for_init(stmt->for_stmt.init,&inner_map);

        if (stmt->for_stmt.condition){
            resolve_expression(stmt->for_stmt.condition,inner_map);
        }

        if (stmt->for_stmt.post){
            resolve_expression(stmt->for_stmt.post,inner_map);
        }

        resolve_statement(stmt->for_stmt.body,inner_map);
    }
    else if (stmt->type==STMT_BREAK || stmt->type==STMT_CONTINUE){
        return;
    }
    else if (stmt->type==STMT_SWITCH){
        resolve_expression(stmt->switch_stmt.control,map);
        resolve_statement(stmt->switch_stmt.body,map);
    }
    else if (stmt->type==STMT_CASE){
        resolve_expression(stmt->case_stmt.value_exp,map);
        resolve_statement(stmt->case_stmt.body,map);
    }
    else if (stmt->type==STMT_DEFAULT){
        resolve_statement(stmt->default_stmt.body,map);
    }

    //don't need deal with STMT_NULL
}

void resolve_block_items(BlockItem **items,int count,IdentifierMap **map){
    for(int i=0;i<count;i++){
        BlockItem *bi=items[i];

        if (bi->type==BI_DECL){
            if (bi->decl->type==DECL_VAR){
                resolve_local_variable_declaration(
                    bi->decl->var,
                    map
                );
            }
            else {
                Function *fn=bi->decl->fun;

                if (fn->has_body){
                    fprintf(
                        stderr,
                        "Semantic Error: nested function definition '%s'\n",
                        fn->name
                    );
                    exit(1);
                }

                if (fn->storage_class==STORAGE_STATIC){
                    fprintf(
                        stderr,
                        "Semantic Error: block-scope function declaration '%s' cannot be static\n",
                        fn->name
                    );
                    exit(1);
                }

                resolve_function_declaration(
                    fn,
                    map
                );
            }
        }
        else if (bi->type==BI_STMT){
            resolve_statement(
                bi->stmt,
                *map
            );
        }
    }
}

void label_block_items(BlockItem **items,int count ,char *current_break_label,char *current_continue_label){
    for(int i=0;i<count;i++){
        BlockItem *bi=items[i];
        if (bi->type==BI_STMT){
            label_statement(bi->stmt,current_break_label,current_continue_label);
        }
    }
}

void label_statement(Statement *stmt,char *current_break_label,char *current_continue_label){
    if (!stmt) return;

    if (stmt->type==STMT_BREAK){
        if (!current_break_label){
            fprintf(stderr,"Label Error: Break statement outside of loop\n");
            exit(1);
        }
        stmt->loop_label=strdup(current_break_label);
    }

    else if (stmt->type==STMT_CONTINUE){
        if (!current_continue_label){
            fprintf(stderr, "Semantic Error: continue statement outside of loop\n");
            exit(1);
        }
        stmt->loop_label=strdup(current_continue_label);
    }
    else if (stmt->type==STMT_WHILE){
        char *new_label=make_label("loop");
        stmt->loop_label=strdup(new_label);
        label_statement(stmt->while_stmt.body,new_label,new_label);
    }
    else if (stmt->type==STMT_DO_WHILE){
        char *new_label=make_label("loop");
        stmt->loop_label=strdup(new_label);
        label_statement(stmt->do_while_stmt.body,new_label,new_label);
    }
    else if (stmt->type==STMT_FOR){
        char *new_label=make_label("loop");
        stmt->loop_label=strdup(new_label);
        label_statement(stmt->for_stmt.body,new_label,new_label);
    }
    else if (stmt->type==STMT_SWITCH){
        char *new_label=make_label("switch");
        stmt->loop_label=strdup(new_label);
        label_statement(stmt->switch_stmt.body,new_label,current_continue_label);
    }
    else if (stmt->type==STMT_CASE){
        label_statement(stmt->case_stmt.body,current_break_label,current_continue_label);
    }
    else if (stmt->type==STMT_DEFAULT){
        label_statement(stmt->default_stmt.body,current_break_label,current_continue_label);
    }
    else if (stmt->type==STMT_IF){
        label_statement(stmt->if_stmt.then_branch,current_break_label,current_continue_label);
        if (stmt->if_stmt.else_branch){
            label_statement(stmt->if_stmt.else_branch,current_break_label,current_continue_label);
        }
    }
    else if (stmt->type==STMT_LABEL){
        label_statement(stmt->if_stmt.then_branch,current_break_label,current_continue_label);
    }
    else if (stmt->type==STMT_COMPOUND){
        label_block_items(stmt->block_items,stmt->block_count,current_break_label,current_continue_label);
    }
}

//eval case/default expression
/*
Fold an already type-checked constant expression. Integer arithmetic runs in
long (wrapping through unsigned to avoid host overflow), then every node is
wrapped to its own type, so results match what gcc computes, e.g.
(1 << 31) is INT_MIN rather than 2^31. Operands of an unsigned type are
already non-negative or raw bit patterns, so their division, shifts and
comparisons run in unsigned long. Double operands fold with host double ops.
*/
static int const_is_true(Const c){
    return c.type==CONST_DOUBLE ? c.dvalue!=0 : c.value!=0;
}

static int eval_double_binary(BinaryOpType op,double l,double r,Type *t,Const *out){
    switch (op){
        case BINARY_ADD: out->dvalue=l+r; break;
        case BINARY_SUB: out->dvalue=l-r; break;
        case BINARY_MULT: out->dvalue=l*r; break;
        case BINARY_DIV: out->dvalue=l/r; break;
        case BINARY_EQ: *out=make_const(t,l==r); return 1;
        case BINARY_NEQ: *out=make_const(t,l!=r); return 1;
        case BINARY_LT: *out=make_const(t,l<r); return 1;
        case BINARY_LTE: *out=make_const(t,l<=r); return 1;
        case BINARY_GT: *out=make_const(t,l>r); return 1;
        case BINARY_GTE: *out=make_const(t,l>=r); return 1;
        default: return 0;
    }
    out->type=CONST_DOUBLE;
    return 1;
}

static int eval_const_value(Exp *e,Const *out){
    switch(e->type){
        case EXP_CONSTANT:
            *out=e->constant;
            return 1;

        case EXP_CAST:
            return eval_const_expr(e->cast.exp,out);

        case EXP_SIZEOF:
        case EXP_SIZEOF_TYPE:
            *out=sizeof_value(e);
            return 1;

        case EXP_UNARY: {
            Const c;
            if (!eval_const_expr(e->unary.exp,&c)) return 0;
            long v=c.value;

            switch (e->unary.op){
                case UNARY_NEGATION:
                    if (c.type==CONST_DOUBLE){
                        out->type=CONST_DOUBLE;
                        out->dvalue=-c.dvalue;
                        return 1;
                    }
                    *out=make_const(e->exp_type,(long)(0UL-(unsigned long)v));
                    return 1;
                case UNARY_COMPLEMENT: *out=make_const(e->exp_type,~v); return 1;
                case UNARY_NOT: *out=make_const(e->exp_type,!const_is_true(c)); return 1;
            }
            
            return 0;
        }

        case EXP_BINARY:{
            Const lc,rc;
            if (!eval_const_expr(e->binary.left,&lc)) return 0;
            if (!eval_const_expr(e->binary.right,&rc)) return 0;

            Type *t=e->exp_type;
            BinaryOpType op=e->binary.op;

            if (op==BINARY_LOGICAL_AND){
                *out=make_const(t,const_is_true(lc) && const_is_true(rc));
                return 1;
            }
            if (op==BINARY_LOGICAL_OR){
                *out=make_const(t,const_is_true(lc) || const_is_true(rc));
                return 1;
            }

            if (lc.type==CONST_DOUBLE){
                return eval_double_binary(op,lc.dvalue,rc.dvalue,t,out);
            }

            long l=lc.value,r=rc.value;
            unsigned long ul=(unsigned long)l;
            unsigned long ur=(unsigned long)r;

            if (!type_is_signed(e->binary.left->exp_type)){
                switch (op){
                    case BINARY_DIV:
                        if (ur==0) return 0;
                        *out=make_const(t,(long)(ul/ur));
                        return 1;
                    case BINARY_REM:
                        if (ur==0) return 0;
                        *out=make_const(t,(long)(ul%ur));
                        return 1;
                    case BINARY_RSHIFT: *out=make_const(t,(long)(ul>>(r&63))); return 1;
                    case BINARY_LT: *out=make_const(t,ul<ur); return 1;
                    case BINARY_LTE: *out=make_const(t,ul<=ur); return 1;
                    case BINARY_GT: *out=make_const(t,ul>ur); return 1;
                    case BINARY_GTE: *out=make_const(t,ul>=ur); return 1;
                    default: break;
                }
            }

            long v;
            switch (op){
                case BINARY_ADD : v=(long)(ul+ur); break;
                case BINARY_SUB: v=(long)(ul-ur); break;
                case BINARY_MULT: v=(long)(ul*ur); break;
                case BINARY_DIV:
                    if (r==0) return 0;
                    v=(r==-1) ? (long)(0UL-ul) : l/r;
                    break;
                case BINARY_REM:
                    if (r==0) return 0;
                    v=(r==-1) ? 0 : l%r;
                    break;

                case BINARY_LSHIFT: v=(long)(ul<<(r&63)); break;
                case BINARY_RSHIFT: v=l>>(r&63); break;
                case BINARY_SHR: v=(long)(ul>>(r&63)); break;
                
                case BINARY_BITWISE_AND: v=l&r; break;
                case BINARY_BITWISE_OR: v=l|r; break;
                case BINARY_BITWISE_XOR: v=l^r; break;

                case BINARY_EQ: v=(l==r); break;
                case BINARY_NEQ: v=(l!=r); break;
                case BINARY_LT: v=(l<r); break;
                case BINARY_LTE: v=(l<=r); break;
                case BINARY_GT: v=(l>r); break;
                case BINARY_GTE: v=(l>=r); break;
                default: return 0;
            }

            *out=make_const(t,v);
            return 1;
        }

        case EXP_CONDITIONAL:{
            Const cond;
            if (!eval_const_expr(e->conditional.condition,&cond)) return 0;
            if (const_is_true(cond)) return eval_const_expr(e->conditional.true_expr,out);
            return eval_const_expr(e->conditional.false_expr,out);
        }

        default:
            return 0;
    }
}

static int eval_const_expr(Exp *e,Const *out){
    if (!e || !eval_const_value(e,out)) {
        return 0;
    }

    *out=convert_const(*out,e->exp_type);
    return 1;
}

static int switch_case_exists(SwitchCaseInfo *cases,long value){
    while(cases){
        if (cases->value==value) return 1;
        cases=cases->next;
    }
    return 0;
}

static void add_switch_case(Statement *switch_stmt,long value,const char *label){
    SwitchCaseInfo *node=calloc(1,sizeof(SwitchCaseInfo));
    node->value=value;
    node->label=strdup(label);
    node->next=NULL;

    if (!switch_stmt->switch_stmt.cases){
        switch_stmt->switch_stmt.cases=node;
        return;
    }

    SwitchCaseInfo *curr=switch_stmt->switch_stmt.cases;
    while(curr->next) curr=curr->next;
    curr->next=node;
}

void collect_switch_cases_block_items(BlockItem **items,int count,Statement *current_switch);

void collect_switch_cases_statement(Statement *stmt,Statement *current_switch){
    if (!stmt) return;

    switch (stmt->type){
        case STMT_SWITCH:
            stmt->switch_stmt.cases=NULL;
            stmt->switch_stmt.default_label=NULL;
            collect_switch_cases_statement(stmt->switch_stmt.body,stmt);
            return;

        case STMT_CASE: {
            if (!current_switch){
                fprintf(stderr, "Semantic Error: case label outside of switch\n");
                exit(1);
            }

            Const case_value;
            if (!eval_const_expr(stmt->case_stmt.value_exp,&case_value)){
                fprintf(stderr, "Semantic Error: case label is not an integer constant expression\n");
                exit(1);
            }

            long value=convert_const(case_value,current_switch->switch_stmt.control->exp_type).value;

            if (switch_case_exists(current_switch->switch_stmt.cases,value)){
                fprintf(stderr, "Semantic Error: duplicate case value %ld\n",value);
                exit(1);
            }

            stmt->case_stmt.value=value;
            stmt->case_stmt.case_label=make_label("case");
            add_switch_case(current_switch,value,stmt->case_stmt.case_label);

            collect_switch_cases_statement(stmt->case_stmt.body,current_switch);
            return;
        }

        case STMT_DEFAULT:{
            if (!current_switch){
                fprintf(stderr, "Semantic Error: default label outside of switch\n");
                exit(1);
            }

            if (current_switch->switch_stmt.default_label){
                fprintf(stderr, "Semantic Error: multiple default labels in one switch\n");
                exit(1);
            }

            stmt->default_stmt.default_label=make_label("default");
            current_switch->switch_stmt.default_label=strdup(stmt->default_stmt.default_label);

            collect_switch_cases_statement(stmt->default_stmt.body,current_switch);
            return;

        }

        case STMT_IF:
            collect_switch_cases_statement(stmt->if_stmt.then_branch,current_switch);
            if (stmt->if_stmt.else_branch){
                collect_switch_cases_statement(stmt->if_stmt.else_branch,current_switch);
            }
            return;

        case STMT_LABEL:
            collect_switch_cases_statement(stmt->if_stmt.then_branch,current_switch);
            return;

        case STMT_COMPOUND:
            collect_switch_cases_block_items(stmt->block_items,stmt->block_count,current_switch);
            return;

        case STMT_WHILE:
            collect_switch_cases_statement(stmt->while_stmt.body,current_switch);
            return;

        case STMT_FOR:
            collect_switch_cases_statement(stmt->for_stmt.body,current_switch);
            return;

        case STMT_DO_WHILE:
            collect_switch_cases_statement(stmt->do_while_stmt.body,current_switch);
            return;

        default:
            return;
    }
}

void collect_switch_cases_block_items(BlockItem **items,int count ,Statement *current_switch){
    for (int i=0;i<count;i++){
        BlockItem *bi=items[i];
        if (bi->type==BI_STMT){
            collect_switch_cases_statement(bi->stmt,current_switch);
        }
    }
}

void validate_program_pass(Function *fn) {
    if (!fn || !fn->has_body) {
        return;
    }

    LabelMap *labels=NULL;

    for(int i=0;i<fn->body_count;i++){
        BlockItem *bi=fn->body[i];

        if (bi->type==BI_STMT){
            check_labels_statement(
                bi->stmt,
                &labels
            );
        }
    }

    for(int i=0;i<fn->body_count;i++){
        BlockItem *bi=fn->body[i];

        if (bi->type==BI_STMT){
            check_goto_statement(
                bi->stmt,
                labels,
                fn->name
            );
        }
    }

    label_block_items(
        fn->body,
        fn->body_count,
        NULL,
        NULL
    );
}

//case values are converted to the control's type, so this runs after type checking
void collect_program_switch_cases(Program *prog){
    for(int i=0;i<prog->declaration_count;i++) {
        Declaration *decl=prog->declarations[i];

        if (decl->type==DECL_FUN && decl->fun->has_body) {
            collect_switch_cases_block_items(
                decl->fun->body,
                decl->fun->body_count,
                NULL
            );
        }
    }
}

void resolve_program(Program *prog){
    if (!prog) {
        return;
    }

    IdentifierMap *map=NULL;

    for(int i=0;i<prog->declaration_count;i++) {
        Declaration *decl=prog->declarations[i];

        if (decl->type==DECL_VAR) {
            resolve_file_scope_variable_declaration(
                decl->var,
                &map
            );
        }
        else {
            resolve_function_declaration(
                decl->fun,
                &map
            );
        }
    }

    for(int i=0;i<prog->declaration_count;i++) {
        Declaration *decl=prog->declarations[i];

        if (decl->type==DECL_FUN) {
            validate_program_pass(
                decl->fun
            );
        }
    }
}

//support build operator
AsmOperand as_imm(long i){
    return (AsmOperand){
        AS_IMM,
        .imm=i
    };
}

AsmOperand as_reg(AsmReg r){
    return (AsmOperand){
        AS_REG,
        .reg=r
    };
}

AsmOperand as_pseudo(char *s){
    return (AsmOperand){
        AS_PSEUDO,
        .pseudo=strdup(s)
    };
}

AsmOperand as_pseudo_mem(char *name,int offset){
    return (AsmOperand){AS_PSEUDO_MEM,.pseudo_mem={name,offset}};
}

AsmOperand as_indexed(AsmReg base,AsmReg index,int scale){
    return (AsmOperand){AS_INDEXED,.indexed={base,index,scale}};
}

AsmOperand as_memory(AsmReg base,int offset){
    return (AsmOperand){
        AS_MEMORY,
        .memory={base,offset}
    };
}

AsmOperand as_data(char *name){
    return (AsmOperand){
        AS_DATA,
        .data_name=name
    };
}

//StaticConstants collected while converting one program to assembly
static AsmStaticConstant **double_constants=NULL;
static int double_constant_count=0;

//one constant per distinct (bit pattern, alignment), so 0.0 and -0.0 stay apart
static AsmOperand double_constant(double d,int alignment){
    for (int i=0;i<double_constant_count;i++){
        AsmStaticConstant *c=double_constants[i];
        if (c->alignment==alignment && memcmp(&c->init.dvalue,&d,sizeof(double))==0){
            return as_data(c->name);
        }
    }

    AsmStaticConstant *c=calloc(1,sizeof(AsmStaticConstant));
    c->name=malloc(32);
    sprintf(c->name,"double.%d",double_constant_count);
    c->alignment=alignment;
    c->init.type=INIT_DOUBLE;
    c->init.dvalue=d;

    double_constants=realloc(double_constants,sizeof(AsmStaticConstant*)*(double_constant_count+1));
    double_constants[double_constant_count++]=c;
    return as_data(c->name);
}

AsmOperand convert_tacky_val(TackyVal *v){
    if (v->type==TACKY_VAL_CONSTANT){
        if (v->constant.type==CONST_DOUBLE){
            return double_constant(v->constant.dvalue,8);
        }
        return as_imm(v->constant.value);
    }

    return as_pseudo(v->var_name);
}

AsmType type_to_asm_type(Type *t){
    if (is_character_type(t)) return ASM_BYTE;
    if (t->kind==TYPE_DOUBLE) return ASM_DOUBLE;
    return type_size(t)==8 ? ASM_QUADWORD : ASM_LONGWORD;
}

int type_alignment(Type *t){
    if (t->kind==TYPE_ARRAY){
        if (type_size(t)>=16) return 16;
        while (t->kind==TYPE_ARRAY) t=t->referenced;
    }
    return type_size(t);
}

static Type *variable_type(Symbol *symbols,const char *name){
    Symbol *entry=symbol_find(symbols,name);
    if (!entry){
        fprintf(stderr,"Codegen Error: missing symbol '%s'\n",name);
        exit(1);
    }
    return entry->type;
}

static Type *tacky_val_type(TackyVal *v,Symbol *symbols){
    if (v->type==TACKY_VAL_CONSTANT){
        return const_exp_type(v->constant);
    }
    return variable_type(symbols,v->var_name);
}

static AsmType tacky_val_asm_type(TackyVal *v,Symbol *symbols){
    return type_to_asm_type(tacky_val_type(v,symbols));
}

//helper function: let TACKY operator turn it to Asm condition code
CondCode binary_to_cond(BinaryOpType op,int is_signed){
    switch(op){
        case BINARY_EQ: return COND_E;
        case BINARY_NEQ: return COND_NE;
        case BINARY_LT: return is_signed ? COND_L : COND_B;
        case BINARY_LTE: return is_signed ? COND_LE : COND_BE;
        case BINARY_GT:  return is_signed ? COND_G : COND_A;
        case BINARY_GTE: return is_signed ? COND_GE : COND_AE;
        default: exit(1);
    }
}

AsmInst *make_asm(
    AsmInstType type,
    AsmType asm_type,
    BinaryOpType op,
    CondCode cond,
    AsmOperand src,
    AsmOperand dst,
    char *label
){
    AsmInst *new_inst=calloc(1,sizeof(AsmInst));
    new_inst->type=type;
    new_inst->asm_type=asm_type;
    new_inst->src_type=ASM_LONGWORD;
    new_inst->binary_op=op;
    new_inst->cond=cond;
    new_inst->src=src;
    new_inst->dst=dst;
    new_inst->label=label ? strdup(label):NULL;
    new_inst->next=NULL;
    return new_inst;
}

AsmInst *append_asm(
    AsmInst **head,
    AsmInstType type,
    AsmType asm_type,
    BinaryOpType op,
    CondCode cond,
    AsmOperand src,
    AsmOperand dst,
    char *label
){
    AsmInst *new_inst=make_asm(type,asm_type,op,cond,src,dst,label);

    if (*head==NULL){
        *head=new_inst;
    }else{
        AsmInst *curr=*head;
        while (curr->next!=NULL){
            curr=curr->next;
        }
        curr->next=new_inst;
    }
    return new_inst;
}

static const AsmOperand NO_OPERAND={0};

/*
System V integer argument registers:
1 -> DI
2 -> SI
3 -> DX
4 -> CX
5 -> R8
6 -> R9
*/
static const AsmReg ARG_REGISTERS[6]={
    REG_DI,
    REG_SI,
    REG_DX,
    REG_CX,
    REG_R8,
    REG_R9
};

//double arguments go in XMM0-XMM7
static const AsmReg DOUBLE_ARG_REGISTERS[8]={
    REG_XMM0, REG_XMM1, REG_XMM2, REG_XMM3,
    REG_XMM4, REG_XMM5, REG_XMM6, REG_XMM7
};

typedef struct {
    AsmType type;
    AsmOperand operand;
} TypedOperand;

typedef struct {
    TypedOperand int_args[6];
    int int_count;
    AsmOperand double_args[8];
    int double_count;
    TypedOperand *stack_args; //in source order: the first one sits at 16(%rbp)
    int stack_count;
} ClassifiedArgs;

//split arguments (or parameters) into those passed in integer registers, XMM registers and on the stack
static ClassifiedArgs classify_parameters(TackyVal **values,int count,Symbol *symbols){
    ClassifiedArgs c={0};
    c.stack_args=calloc(count>0 ? count : 1,sizeof(TypedOperand));

    for (int i=0;i<count;i++){
        TypedOperand arg={tacky_val_asm_type(values[i],symbols),convert_tacky_val(values[i])};

        if (arg.type==ASM_DOUBLE && c.double_count<8){
            c.double_args[c.double_count++]=arg.operand;
        }else if (arg.type!=ASM_DOUBLE && c.int_count<6){
            c.int_args[c.int_count++]=arg;
        }else{
            c.stack_args[c.stack_count++]=arg;
        }
    }
    return c;
}

/*
Convert:
    FunCall(fun_name, args, dst)

into:
    optional Binary(Sub, Quadword, Imm(8), Reg(SP))
    Mov(int args -> DI ... R9), Mov(double args -> XMM0 ... XMM7)
    Push(stack arguments in reverse order)
    Call(fun_name)
    optional Binary(Add, Quadword, Imm(bytes), Reg(SP))
    Mov(AX or XMM0, dst)
*/
static void convert_function_call(
    TackyInstruction *call,
    AsmInst **head,
    Symbol *symbols
){
    ClassifiedArgs args=classify_parameters(call->args,call->arg_count,symbols);

    //Keep the stack 16-byte aligned before CALL.
    int stack_padding=(args.stack_count % 2) ? 8 : 0;

    if (stack_padding){
        append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_SUB,0,as_imm(stack_padding),as_reg(REG_SP),NULL);
    }

    for (int i=0;i<args.int_count;i++){
        append_asm(head,AS_MOV,args.int_args[i].type,0,0,args.int_args[i].operand,as_reg(ARG_REGISTERS[i]),NULL);
    }

    for (int i=0;i<args.double_count;i++){
        append_asm(head,AS_MOV,ASM_DOUBLE,0,0,args.double_args[i],as_reg(DOUBLE_ARG_REGISTERS[i]),NULL);
    }

    //Stack arguments are pushed right-to-left.
    for (int i=args.stack_count-1;i>=0;i--){
        AsmOperand arg=args.stack_args[i].operand;
        AsmType arg_type=args.stack_args[i].type;

        if (arg.type==AS_IMM || arg.type==AS_REG || (arg_type!=ASM_LONGWORD && arg_type!=ASM_BYTE)){
            append_asm(head,AS_PUSH,ASM_QUADWORD,0,0,arg,NO_OPERAND,NULL);
        }else{
            /*
            pushq would read beyond a byte or longword memory operand,
            so copy its declared width into AX before pushing the register.
            */
            append_asm(head,AS_MOV,arg_type,0,0,arg,as_reg(REG_AX),NULL);
            append_asm(head,AS_PUSH,ASM_QUADWORD,0,0,as_reg(REG_AX),NO_OPERAND,NULL);
        }
    }

    append_asm(head,AS_CALL,0,0,0,NO_OPERAND,NO_OPERAND,call->fun_name);

    int bytes_to_remove=args.stack_count*8 + stack_padding;
    if (bytes_to_remove){
        append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_ADD,0,as_imm(bytes_to_remove),as_reg(REG_SP),NULL);
    }
    free(args.stack_args);

    if (!call->dst) return;
    AsmType dst_type=tacky_val_asm_type(call->dst,symbols);
    append_asm(
        head,
        AS_MOV,
        dst_type,
        0,
        0,
        as_reg(dst_type==ASM_DOUBLE ? REG_XMM0 : REG_AX),
        convert_tacky_val(call->dst),
        NULL
    );
}

//comisd val against 0.0 in XMM0; a NaN val sets PF (and ZF, CF)
static void emit_double_zero_compare(AsmInst **head,AsmOperand val){
    append_asm(head,AS_BINARY,ASM_DOUBLE,BINARY_BITWISE_XOR,0,as_reg(REG_XMM0),as_reg(REG_XMM0),NULL);
    append_asm(head,AS_CMP,ASM_DOUBLE,0,0,val,as_reg(REG_XMM0),NULL);
}

/*
After a comisd, set dst (already zeroed) to 1 when cond holds and the
comparison was ordered. A NaN operand makes == false and != true.
*/
static void emit_nan_aware_setcc(AsmInst **head,CondCode cond,AsmType dst_type,AsmOperand dst){
    char *end=make_label("fp_cmp_end");
    if (cond==COND_E){
        append_asm(head,AS_JMP_CC,0,0,COND_P,NO_OPERAND,NO_OPERAND,end);
        append_asm(head,AS_SET_CC,0,0,COND_E,NO_OPERAND,dst,NULL);
    }else{
        append_asm(head,AS_SET_CC,0,0,COND_NE,NO_OPERAND,dst,NULL);
        append_asm(head,AS_JMP_CC,0,0,COND_NP,NO_OPERAND,NO_OPERAND,end);
        append_asm(head,AS_MOV,dst_type,0,0,as_imm(1),dst,NULL);
    }
    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,end);
}

/*
comisd sets CF on an unordered result, so A/AE are false for NaN while B/BE
would be true: a < b is lowered as b > a by swapping the operands.
*/
static void convert_double_comparison(TackyInstruction *curr,AsmInst **head,Symbol *symbols){
    BinaryOpType op=curr->binary_op;
    AsmOperand a=convert_tacky_val(curr->src);
    AsmOperand b=convert_tacky_val(curr->src2);
    AsmOperand dst=convert_tacky_val(curr->dst);
    AsmType dst_type=tacky_val_asm_type(curr->dst,symbols);

    if (op==BINARY_LT || op==BINARY_LTE){
        append_asm(head,AS_CMP,ASM_DOUBLE,0,0,a,b,NULL);
    }else{
        append_asm(head,AS_CMP,ASM_DOUBLE,0,0,b,a,NULL);
    }
    append_asm(head,AS_MOV,dst_type,0,0,as_imm(0),dst,NULL);

    switch (op){
        case BINARY_EQ: emit_nan_aware_setcc(head,COND_E,dst_type,dst); break;
        case BINARY_NEQ: emit_nan_aware_setcc(head,COND_NE,dst_type,dst); break;
        case BINARY_LT: case BINARY_GT:
            append_asm(head,AS_SET_CC,0,0,COND_A,NO_OPERAND,dst,NULL);
            break;
        default:
            append_asm(head,AS_SET_CC,0,0,COND_AE,NO_OPERAND,dst,NULL);
            break;
    }
}

static void convert_uint_to_double(TackyInstruction *curr,AsmInst **head,Symbol *symbols){
    AsmOperand src=convert_tacky_val(curr->src);
    AsmOperand dst=convert_tacky_val(curr->dst);

    if (tacky_val_asm_type(curr->src,symbols)==ASM_LONGWORD){
        append_asm(head,AS_MOVZX,ASM_QUADWORD,0,0,src,as_reg(REG_AX),NULL);
        append_asm(head,AS_CVTSI2SD,ASM_QUADWORD,0,0,as_reg(REG_AX),dst,NULL);
        return;
    }

    //above LONG_MAX: halve rounding to odd, convert, then double the result
    char *out_of_range=make_label("ulong_to_double_big");
    char *end=make_label("ulong_to_double_end");
    append_asm(head,AS_CMP,ASM_QUADWORD,0,0,as_imm(0),src,NULL);
    append_asm(head,AS_JMP_CC,0,0,COND_L,NO_OPERAND,NO_OPERAND,out_of_range);
    append_asm(head,AS_CVTSI2SD,ASM_QUADWORD,0,0,src,dst,NULL);
    append_asm(head,AS_JMP,0,0,0,NO_OPERAND,NO_OPERAND,end);
    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,out_of_range);
    append_asm(head,AS_MOV,ASM_QUADWORD,0,0,src,as_reg(REG_AX),NULL);
    append_asm(head,AS_MOV,ASM_QUADWORD,0,0,as_reg(REG_AX),as_reg(REG_DX),NULL);
    append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_SHR,0,as_imm(1),as_reg(REG_DX),NULL);
    append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_BITWISE_AND,0,as_imm(1),as_reg(REG_AX),NULL);
    append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_BITWISE_OR,0,as_reg(REG_AX),as_reg(REG_DX),NULL);
    append_asm(head,AS_CVTSI2SD,ASM_QUADWORD,0,0,as_reg(REG_DX),dst,NULL);
    append_asm(head,AS_BINARY,ASM_DOUBLE,BINARY_ADD,0,dst,dst,NULL);
    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,end);
}

static void convert_double_to_uint(TackyInstruction *curr,AsmInst **head,Symbol *symbols){
    AsmOperand src=convert_tacky_val(curr->src);
    AsmOperand dst=convert_tacky_val(curr->dst);

    if (tacky_val_asm_type(curr->dst,symbols)==ASM_LONGWORD){
        append_asm(head,AS_CVTTSD2SI,ASM_QUADWORD,0,0,src,as_reg(REG_AX),NULL);
        append_asm(head,AS_MOV,ASM_LONGWORD,0,0,as_reg(REG_AX),dst,NULL);
        return;
    }

    //at or above 2^63: subtract 2^63 (exactly), convert, then add it back as an integer
    AsmOperand upper_bound=double_constant(9223372036854775808.0,8);
    char *out_of_range=make_label("double_to_ulong_big");
    char *end=make_label("double_to_ulong_end");
    append_asm(head,AS_CMP,ASM_DOUBLE,0,0,upper_bound,src,NULL);
    append_asm(head,AS_JMP_CC,0,0,COND_AE,NO_OPERAND,NO_OPERAND,out_of_range);
    append_asm(head,AS_CVTTSD2SI,ASM_QUADWORD,0,0,src,dst,NULL);
    append_asm(head,AS_JMP,0,0,0,NO_OPERAND,NO_OPERAND,end);
    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,out_of_range);
    append_asm(head,AS_MOV,ASM_DOUBLE,0,0,src,as_reg(REG_XMM1),NULL);
    append_asm(head,AS_BINARY,ASM_DOUBLE,BINARY_SUB,0,upper_bound,as_reg(REG_XMM1),NULL);
    append_asm(head,AS_CVTTSD2SI,ASM_QUADWORD,0,0,as_reg(REG_XMM1),dst,NULL);
    append_asm(head,AS_MOV,ASM_QUADWORD,0,0,as_imm(LONG_MIN),as_reg(REG_DX),NULL);
    append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_ADD,0,as_reg(REG_DX),dst,NULL);
    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,end);
}

static void convert_tacky_instruction(
    TackyInstruction *curr,
    AsmInst **head,
    Symbol *symbols
){
    switch (curr->type){
        case TACKY_INST_RETURN: {
            if (curr->src){
                AsmType t=tacky_val_asm_type(curr->src,symbols);
                append_asm(head,AS_MOV,t,0,0,convert_tacky_val(curr->src),as_reg(t==ASM_DOUBLE ? REG_XMM0 : REG_AX),NULL);
            }
            append_asm(head,AS_RET,0,0,0,NO_OPERAND,NO_OPERAND,NULL);
            break;
        }

        case TACKY_INST_SIGN_EXTEND:
        case TACKY_INST_ZERO_EXTEND: {
            AsmInst *extension=append_asm(head,curr->type==TACKY_INST_SIGN_EXTEND ? AS_MOVSX : AS_MOVZX,
                tacky_val_asm_type(curr->dst,symbols),0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
            extension->src_type=tacky_val_asm_type(curr->src,symbols);
            break;
        }

        case TACKY_INST_TRUNCATE:
            append_asm(head,AS_MOV,tacky_val_asm_type(curr->dst,symbols),0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
            break;

        case TACKY_INST_COPY:
            append_asm(head,AS_MOV,tacky_val_asm_type(curr->src,symbols),0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
            break;

        case TACKY_INST_JUMP:
            append_asm(head,AS_JMP,0,0,0,NO_OPERAND,NO_OPERAND,curr->label_name);
            break;

        case TACKY_INST_LABEL:
            append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,curr->label_name);
            break;

        //NaN is truthy: JumpIfZero must not jump on an unordered compare
        case TACKY_INST_JZ:
        case TACKY_INST_JNZ: {
            if (tacky_val_asm_type(curr->src,symbols)==ASM_DOUBLE){
                emit_double_zero_compare(head,convert_tacky_val(curr->src));
                if (curr->type==TACKY_INST_JZ){
                    char *end=make_label("fp_cmp_end");
                    append_asm(head,AS_JMP_CC,0,0,COND_P,NO_OPERAND,NO_OPERAND,end);
                    append_asm(head,AS_JMP_CC,0,0,COND_E,NO_OPERAND,NO_OPERAND,curr->label_name);
                    append_asm(head,AS_LABEL,0,0,0,NO_OPERAND,NO_OPERAND,end);
                }else{
                    append_asm(head,AS_JMP_CC,0,0,COND_NE,NO_OPERAND,NO_OPERAND,curr->label_name);
                    append_asm(head,AS_JMP_CC,0,0,COND_P,NO_OPERAND,NO_OPERAND,curr->label_name);
                }
                break;
            }

            CondCode cond=(curr->type==TACKY_INST_JZ) ? COND_E : COND_NE;
            append_asm(head,AS_CMP,tacky_val_asm_type(curr->src,symbols),0,0,as_imm(0),convert_tacky_val(curr->src),NULL);
            append_asm(head,AS_JMP_CC,0,0,cond,NO_OPERAND,NO_OPERAND,curr->label_name);
            break;
        }

        case TACKY_INST_UNARY:{
            AsmType src_type=tacky_val_asm_type(curr->src,symbols);

            if (src_type==ASM_DOUBLE && curr->unary_op==UNARY_NOT){
                AsmType dst_type=tacky_val_asm_type(curr->dst,symbols);
                AsmOperand dst=convert_tacky_val(curr->dst);
                emit_double_zero_compare(head,convert_tacky_val(curr->src));
                append_asm(head,AS_MOV,dst_type,0,0,as_imm(0),dst,NULL);
                emit_nan_aware_setcc(head,COND_E,dst_type,dst);
            }
            //flip the sign bit
            else if (src_type==ASM_DOUBLE){
                append_asm(head,AS_MOV,ASM_DOUBLE,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                append_asm(head,AS_BINARY,ASM_DOUBLE,BINARY_BITWISE_XOR,0,double_constant(-0.0,16),convert_tacky_val(curr->dst),NULL);
            }
            else if (curr->unary_op==UNARY_NOT){
                append_asm(head,AS_CMP,src_type,0,0,as_imm(0),convert_tacky_val(curr->src),NULL);
                append_asm(head,AS_MOV,tacky_val_asm_type(curr->dst,symbols),0,0,as_imm(0),convert_tacky_val(curr->dst),NULL);
                append_asm(head,AS_SET_CC,0,0,COND_E,NO_OPERAND,convert_tacky_val(curr->dst),NULL);
            }else{
                append_asm(head,AS_MOV,src_type,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                AsmInstType type=(curr->unary_op==UNARY_NEGATION)?AS_NEG:AS_NOT;
                append_asm(head,type,src_type,0,0,NO_OPERAND,convert_tacky_val(curr->dst),NULL);
            }
            break;
        }

        case TACKY_INST_BINARY:{
            AsmType src_type=tacky_val_asm_type(curr->src,symbols);
            int is_signed=type_is_signed(tacky_val_type(curr->src,symbols));
            BinaryOpType op=(curr->binary_op==BINARY_RSHIFT && !is_signed) ? BINARY_SHR : curr->binary_op;

            if (src_type==ASM_DOUBLE && is_comparison_op(op)){
                convert_double_comparison(curr,head,symbols);
            }
            else if (src_type==ASM_DOUBLE){
                append_asm(head,AS_MOV,ASM_DOUBLE,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                append_asm(head,AS_BINARY,ASM_DOUBLE,op,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->dst),NULL);
            }
            else if (op==BINARY_DIV || op==BINARY_REM){
                append_asm(head,AS_MOV,src_type,0,0,convert_tacky_val(curr->src),as_reg(REG_AX),NULL);
                if (is_signed){
                    append_asm(head,AS_CDQ,src_type,0,0,NO_OPERAND,NO_OPERAND,NULL);
                    append_asm(head,AS_IDIV,src_type,0,0,convert_tacky_val(curr->src2),NO_OPERAND,NULL);
                }else{
                    append_asm(head,AS_MOV,src_type,0,0,as_imm(0),as_reg(REG_DX),NULL);
                    append_asm(head,AS_DIV,src_type,0,0,convert_tacky_val(curr->src2),NO_OPERAND,NULL);
                }

                AsmReg res_reg=(op==BINARY_DIV) ? REG_AX:REG_DX;
                append_asm(head,AS_MOV,src_type,0,0,as_reg(res_reg),convert_tacky_val(curr->dst),NULL);
            }
            else if (is_shift_op(op)){
                if (curr->src2->type==TACKY_VAL_CONSTANT){
                    append_asm(head,AS_MOV,src_type,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                    append_asm(head,AS_BINARY,src_type,op,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->dst),NULL);
                }else{
                    append_asm(head,AS_MOV,tacky_val_asm_type(curr->src2,symbols),0,0,convert_tacky_val(curr->src2),as_reg(REG_CX),NULL);
                    append_asm(head,AS_MOV,src_type,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                    append_asm(head,AS_BINARY,src_type,op,0,as_reg(REG_CX),convert_tacky_val(curr->dst),NULL);
                }
            }
            else if (is_comparison_op(op)){
                append_asm(head,AS_CMP,src_type,0,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->src),NULL);
                append_asm(head,AS_MOV,tacky_val_asm_type(curr->dst,symbols),0,0,as_imm(0),convert_tacky_val(curr->dst),NULL);
                append_asm(head,AS_SET_CC,0,0,binary_to_cond(op,is_signed),NO_OPERAND,convert_tacky_val(curr->dst),NULL);
            }
            else{
                append_asm(head,AS_MOV,src_type,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                append_asm(head,AS_BINARY,src_type,op,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->dst),NULL);
            }
            break;
        }

        case TACKY_INST_FUN_CALL:
            convert_function_call(curr,head,symbols);
            break;

        case TACKY_INST_INT_TO_DOUBLE:
            append_asm(head,AS_CVTSI2SD,tacky_val_asm_type(curr->src,symbols),0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
            break;

        case TACKY_INST_DOUBLE_TO_INT:
            append_asm(head,AS_CVTTSD2SI,tacky_val_asm_type(curr->dst,symbols),0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
            break;

        case TACKY_INST_UINT_TO_DOUBLE:
            convert_uint_to_double(curr,head,symbols);
            break;

        case TACKY_INST_DOUBLE_TO_UINT:
            convert_double_to_uint(curr,head,symbols);
            break;

        //AX holds the pointer: fix-up rewrites these moves with R10, R11 and XMM14
        case TACKY_INST_LOAD:
            append_asm(head,AS_MOV,ASM_QUADWORD,0,0,convert_tacky_val(curr->src),as_reg(REG_AX),NULL);
            append_asm(head,AS_MOV,tacky_val_asm_type(curr->dst,symbols),0,0,as_memory(REG_AX,0),convert_tacky_val(curr->dst),NULL);
            break;

        case TACKY_INST_STORE:
            append_asm(head,AS_MOV,ASM_QUADWORD,0,0,convert_tacky_val(curr->dst),as_reg(REG_AX),NULL);
            append_asm(head,AS_MOV,tacky_val_asm_type(curr->src,symbols),0,0,convert_tacky_val(curr->src),as_memory(REG_AX,0),NULL);
            break;

        case TACKY_INST_ADD_PTR: {
            append_asm(head,AS_MOV,ASM_QUADWORD,0,0,convert_tacky_val(curr->src),as_reg(REG_AX),NULL);
            int scale=curr->scale;
            if (curr->src2->type==TACKY_VAL_CONSTANT){
                long index=curr->src2->constant.value;
                if (index>=INT_MIN/scale && index<=INT_MAX/scale){
                    append_asm(head,AS_LEA,ASM_QUADWORD,0,0,as_memory(REG_AX,(int)(index*scale)),convert_tacky_val(curr->dst),NULL);
                    break;
                }
            }
            append_asm(head,AS_MOV,ASM_QUADWORD,0,0,convert_tacky_val(curr->src2),as_reg(REG_DX),NULL);
            if (scale!=1 && scale!=2 && scale!=4 && scale!=8){
                append_asm(head,AS_BINARY,ASM_QUADWORD,BINARY_MULT,0,as_imm(scale),as_reg(REG_DX),NULL); scale=1;
            }
            append_asm(head,AS_LEA,ASM_QUADWORD,0,0,as_indexed(REG_AX,REG_DX,scale),convert_tacky_val(curr->dst),NULL);
            break;
        }
        case TACKY_INST_COPY_TO_OFFSET:
            append_asm(head,AS_MOV,tacky_val_asm_type(curr->src,symbols),0,0,convert_tacky_val(curr->src),as_pseudo_mem(curr->dst->var_name,curr->offset),NULL);
            break;
        case TACKY_INST_GET_ADDRESS: {
            Symbol *obj=symbol_find(symbols,curr->src->var_name);
            AsmOperand src=obj->type->kind==TYPE_ARRAY ? as_pseudo_mem(curr->src->var_name,0) : convert_tacky_val(curr->src);
            append_asm(head,AS_LEA,ASM_QUADWORD,0,0,src,convert_tacky_val(curr->dst),NULL);
            break;
        }
    }
}

/*
Convert one TACKY function.

At function entry, copy parameters out of their ABI-defined locations
into pseudoregisters.  Pseudoregister replacement will later give them
ordinary stack slots, just like local variables.
*/
static AsmFunc *convert_tacky_function(TackyFunction *tacky_fn,Symbol *symbols){
    AsmFunc *asm_fn=calloc(1,sizeof(AsmFunc));
    if (!asm_fn){
        perror("calloc AsmFunc");
        exit(1);
    }

    asm_fn->name=strdup(tacky_fn->name);
    asm_fn->global=tacky_fn->global;
    asm_fn->instructions=NULL;
    asm_fn->stack_size=0;

    AsmInst **head=&asm_fn->instructions;

    TackyVal **params=calloc(tacky_fn->param_count>0 ? tacky_fn->param_count : 1,sizeof(TackyVal*));
    for (int i=0;i<tacky_fn->param_count;i++){
        params[i]=tacky_val_var(tacky_fn->params[i]);
    }
    ClassifiedArgs args=classify_parameters(params,tacky_fn->param_count,symbols);

    for (int i=0;i<args.int_count;i++){
        append_asm(head,AS_MOV,args.int_args[i].type,0,0,as_reg(ARG_REGISTERS[i]),args.int_args[i].operand,NULL);
    }

    for (int i=0;i<args.double_count;i++){
        append_asm(head,AS_MOV,ASM_DOUBLE,0,0,as_reg(DOUBLE_ARG_REGISTERS[i]),args.double_args[i],NULL);
    }

    for (int i=0;i<args.stack_count;i++){
        append_asm(head,AS_MOV,args.stack_args[i].type,0,0,as_memory(REG_BP,16+8*i),args.stack_args[i].operand,NULL);
    }
    free(args.stack_args);

    for (TackyInstruction *curr=tacky_fn->instructions;
         curr!=NULL;
         curr=curr->next){
        convert_tacky_instruction(curr,head,symbols);
    }

    return asm_fn;
}

static AsmSymbol *build_backend_symbols(Symbol *symbols){
    AsmSymbol *backend=NULL;

    for (Symbol *entry=symbols;entry;entry=entry->next){
        AsmSymbol *asm_entry=calloc(1,sizeof(AsmSymbol));
        if (!asm_entry){
            perror("calloc AsmSymbol");
            exit(1);
        }

        asm_entry->name=strdup(entry->name);
        if (entry->type->kind==TYPE_FUNCTION){
            asm_entry->kind=ASM_SYMBOL_FUN;
            asm_entry->defined=entry->defined;
        }else{
            asm_entry->kind=ASM_SYMBOL_OBJ;
            if (entry->attr_type==SYMBOL_ATTR_STATIC && entry->init.type==INITIAL_NONE){
                asm_entry->asm_type=ASM_LONGWORD; asm_entry->alignment=1;
            }else{
                asm_entry->asm_type=entry->type->kind==TYPE_ARRAY ? ASM_LONGWORD : type_to_asm_type(entry->type);
                asm_entry->byte_size=type_size(entry->type);
                asm_entry->alignment=type_alignment(entry->type);
            }
            asm_entry->is_aggregate=entry->type->kind==TYPE_ARRAY;
            asm_entry->is_static=(entry->attr_type==SYMBOL_ATTR_STATIC || entry->attr_type==SYMBOL_ATTR_CONSTANT);
            asm_entry->is_constant=entry->attr_type==SYMBOL_ATTR_CONSTANT;
        }

        asm_entry->next=backend;
        backend=asm_entry;
    }

    return backend;
}

static AsmSymbol *asm_symbol_find(AsmSymbol *symbols,const char *name){
    while (symbols){
        if (strcmp(symbols->name,name)==0){
            return symbols;
        }
        symbols=symbols->next;
    }
    return NULL;
}

/*
TACKY Program(top_level*) -> Assembly Program(top_level*)
*/
static void append_asm_top_level(AsmProg *asmp,AsmTopLevel *asm_top){
    AsmTopLevel **new_top_levels=realloc(
        asmp->top_levels,
        sizeof(AsmTopLevel*)*(size_t)(asmp->top_level_count+1)
    );
    if (!new_top_levels){
        perror("realloc assembly top levels");
        exit(1);
    }

    asmp->top_levels=new_top_levels;
    asmp->top_levels[asmp->top_level_count++]=asm_top;
}

AsmProg *tacky_to_asm(TackyProgram *tacky,Symbol *symbols){
    AsmProg *asmp=calloc(1,sizeof(AsmProg));
    if (!asmp){
        perror("calloc AsmProg");
        exit(1);
    }

    double_constants=NULL;
    double_constant_count=0;

    for (int i=0;i<tacky->top_level_count;i++){
        TackyTopLevel *top_level=tacky->top_levels[i];

        if (!top_level){
            continue;
        }

        AsmTopLevel *asm_top=calloc(1,sizeof(AsmTopLevel));
        if (!asm_top){
            perror("calloc AsmTopLevel");
            exit(1);
        }

        switch (top_level->type){
            case TACKY_TOP_FUNCTION:
                if (!top_level->function){
                    free(asm_top);
                    continue;
                }
                asm_top->type=ASM_TOP_FUNCTION;
                asm_top->function=convert_tacky_function(top_level->function,symbols);
                break;
            case TACKY_TOP_STATIC_CONSTANT: {
                TackyStaticVariable *value=top_level->static_variable;
                AsmStaticConstant *constant=calloc(1,sizeof(AsmStaticConstant));
                constant->name=strdup(value->name); constant->alignment=type_alignment(value->type);
                constant->init=value->init.items[0];
                asm_top->type=ASM_TOP_STATIC_CONSTANT; asm_top->static_constant=constant;
                break;
            }
            case TACKY_TOP_STATIC_VARIABLE: {
                TackyStaticVariable *tacky_var=top_level->static_variable;
                if (!tacky_var){
                    free(asm_top);
                    continue;
                }

                AsmStaticVariable *asm_var=calloc(1,sizeof(AsmStaticVariable));
                if (!asm_var){
                    perror("calloc AsmStaticVariable");
                    exit(1);
                }
                asm_var->name=strdup(tacky_var->name);
                asm_var->global=tacky_var->global;
                asm_var->alignment=type_alignment(tacky_var->type);
                asm_var->init=tacky_var->init;

                asm_top->type=ASM_TOP_STATIC_VARIABLE;
                asm_top->static_variable=asm_var;
                break;
            }
        }

        append_asm_top_level(asmp,asm_top);
    }

    asmp->symbols=build_backend_symbols(symbols);

    for (int i=0;i<double_constant_count;i++){
        AsmTopLevel *asm_top=calloc(1,sizeof(AsmTopLevel));
        asm_top->type=ASM_TOP_STATIC_CONSTANT;
        asm_top->static_constant=double_constants[i];
        append_asm_top_level(asmp,asm_top);

        AsmSymbol *entry=calloc(1,sizeof(AsmSymbol));
        entry->name=strdup(double_constants[i]->name);
        entry->kind=ASM_SYMBOL_OBJ;
        entry->asm_type=ASM_DOUBLE;
        entry->is_static=1;
        entry->is_constant=1;
        entry->next=asmp->symbols;
        asmp->symbols=entry;
    }

    return asmp;
}

//stack allocation and instruction fix
typedef struct {
    char *name;
    int offset;
} MapEntry;

static int is_memory_operand(AsmOperand op){
    return op.type==AS_MEMORY || op.type==AS_DATA || op.type==AS_INDEXED;
}

static int is_large_imm(AsmOperand op){
    return op.type==AS_IMM && (op.imm < INT_MIN || op.imm > INT_MAX);
}

static void replace_pseudos(AsmFunc *fn,AsmSymbol *symbols){
    MapEntry map[1000];
    int map_size=0;
    int stack_bytes=0;

    /*
    Replace every pseudoregister in this function with its own stack slot,
    unless it names a variable with static storage duration, which becomes
    a Data operand instead.
    */
    for (AsmInst *curr=fn->instructions;curr;curr=curr->next){
        AsmOperand *ops[2]={
            &curr->src,
            &curr->dst
        };

        for (int i=0;i<2;i++){
            if (ops[i]->type!=AS_PSEUDO && ops[i]->type!=AS_PSEUDO_MEM){
                continue;
            }

            int object_offset=ops[i]->type==AS_PSEUDO_MEM ? ops[i]->pseudo_mem.offset : 0;
            char *object_name=ops[i]->type==AS_PSEUDO_MEM ? ops[i]->pseudo_mem.name : ops[i]->pseudo;
            int found_offset=0;
            int already_exists=0;

            for (int j=0;j<map_size;j++){
                if (strcmp(map[j].name,object_name)==0){
                    found_offset=map[j].offset;
                    already_exists=1;
                    break;
                }
            }

            if (!already_exists){
                AsmSymbol *sym=asm_symbol_find(symbols,object_name);
                if (!sym || sym->kind!=ASM_SYMBOL_OBJ){
                    fprintf(stderr,"Codegen Error: unknown pseudoregister '%s'\n",object_name);
                    exit(1);
                }

                if (sym->is_static){
                    ops[i]->type=AS_DATA;
                    ops[i]->data_name=object_name;
                    continue;
                }

                if (map_size >= (int)(sizeof(map)/sizeof(map[0]))){
                    fprintf(stderr,"Codegen Error: too many pseudoregisters in function %s\n",fn->name);
                    exit(1);
                }

                int alignment=sym->alignment;
                if (stack_bytes>INT_MAX-sym->byte_size-alignment-16){ fprintf(stderr,"Codegen Error: stack frame too large\n"); exit(1); }
                stack_bytes=((stack_bytes+sym->byte_size+alignment-1)/alignment)*alignment;

                map[map_size].name=strdup(object_name);
                map[map_size].offset=-stack_bytes;
                found_offset=-stack_bytes;
                map_size++;
            }

            *ops[i]=as_memory(REG_BP,found_offset+object_offset);
        }
    }

    /*
    Each function gets its own stack size, rounded up to a
    multiple of 16 so calls made from this function remain aligned.
    */
    fn->stack_size=((stack_bytes+15)/16)*16;
}

static void push_inst(AsmInst ***tail,AsmInst *inst){
    **tail=inst;
    *tail=&inst->next;
}

/*
Rewrite one instruction into a valid sequence and append it at *tail.
R10 fixes source operands and R11 fixes destination operands;
XMM14 and XMM15 play the same roles for doubles.
*/
static void fix_instruction(AsmInst *inst,AsmInst ***tail){

    AsmType t=inst->asm_type;

    switch (inst->type){
        case AS_MOV:
            if (t==ASM_DOUBLE){
                if (is_memory_operand(inst->src) && is_memory_operand(inst->dst)){
                    push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_XMM14),NULL));
                    inst->src=as_reg(REG_XMM14);
                }
                break;
            }

            //movl would truncate an 8-byte immediate anyway; do it here to avoid assembler warnings
            if (t==ASM_BYTE && inst->src.type==AS_IMM) inst->src.imm=(unsigned char)inst->src.imm;
            if (t==ASM_LONGWORD && inst->src.type==AS_IMM){
                inst->src.imm=(int)inst->src.imm;
            }

            if ((is_memory_operand(inst->src) && is_memory_operand(inst->dst)) ||
                (is_large_imm(inst->src) && is_memory_operand(inst->dst))){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }
            break;

        case AS_MOVSX:
        case AS_MOVZX:
            if (inst->type==AS_MOVZX && inst->src_type==ASM_LONGWORD){
                if (inst->dst.type==AS_REG){
                    inst->type=AS_MOV; inst->asm_type=ASM_LONGWORD;
                }else{
                    push_inst(tail,make_asm(AS_MOV,ASM_LONGWORD,0,0,inst->src,as_reg(REG_R11),NULL));
                    inst->type=AS_MOV; inst->src=as_reg(REG_R11);
                }
                break;
            }
            if (inst->src.type==AS_IMM){
                AsmOperand source=inst->src;
                if (inst->src_type==ASM_BYTE) source.imm=(unsigned char)source.imm;
                push_inst(tail,make_asm(AS_MOV,inst->src_type,0,0,source,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }
            if (is_memory_operand(inst->dst)){
                AsmInst *extension=make_asm(inst->type,t,0,0,inst->src,as_reg(REG_R11),NULL);
                extension->src_type=inst->src_type;
                push_inst(tail,extension);
                inst->type=AS_MOV; inst->src=as_reg(REG_R11);
            }
            break;

        case AS_CVTTSD2SI:
            if (inst->dst.type!=AS_REG){
                AsmOperand dst=inst->dst;
                push_inst(tail,make_asm(AS_CVTTSD2SI,t,0,0,inst->src,as_reg(REG_R11),NULL));
                inst->type=AS_MOV;
                inst->src=as_reg(REG_R11);
                inst->dst=dst;
            }
            break;

        case AS_CVTSI2SD:
            if (inst->src.type==AS_IMM){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }

            if (inst->dst.type!=AS_REG){
                AsmOperand dst=inst->dst;
                push_inst(tail,make_asm(AS_CVTSI2SD,t,0,0,inst->src,as_reg(REG_XMM15),NULL));
                inst->type=AS_MOV;
                inst->asm_type=ASM_DOUBLE;
                inst->src=as_reg(REG_XMM15);
                inst->dst=dst;
            }
            break;

        //addsd, subsd, mulsd, divsd and xorpd write only to a register
        case AS_BINARY:
            if (t==ASM_DOUBLE){
                if (inst->dst.type!=AS_REG){
                    AsmOperand dst=inst->dst;
                    push_inst(tail,make_asm(AS_MOV,t,0,0,dst,as_reg(REG_XMM15),NULL));
                    push_inst(tail,make_asm(AS_BINARY,t,inst->binary_op,0,inst->src,as_reg(REG_XMM15),NULL));
                    inst->type=AS_MOV;
                    inst->src=as_reg(REG_XMM15);
                    inst->dst=dst;
                }
                break;
            }

            //shift count immediates are 8 bits; mask like the CPU does for %cl
            if (is_shift_op(inst->binary_op)){
                if (inst->src.type==AS_IMM){
                    inst->src.imm&=(t==ASM_QUADWORD) ? 63 : 31;
                }
                break;
            }

            if (is_large_imm(inst->src) ||
                (inst->binary_op!=BINARY_MULT &&
                 is_memory_operand(inst->src) &&
                 is_memory_operand(inst->dst))){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }

            //imul cannot write to memory
            if (inst->binary_op==BINARY_MULT && is_memory_operand(inst->dst)){
                AsmOperand dst=inst->dst;
                push_inst(tail,make_asm(AS_MOV,t,0,0,dst,as_reg(REG_R11),NULL));
                push_inst(tail,make_asm(AS_BINARY,t,BINARY_MULT,0,inst->src,as_reg(REG_R11),NULL));
                inst->type=AS_MOV;
                inst->src=as_reg(REG_R11);
                inst->dst=dst;
            }
            break;

        //comisd compares against a register
        case AS_CMP:
            if (t==ASM_DOUBLE){
                if (inst->dst.type!=AS_REG){
                    push_inst(tail,make_asm(AS_MOV,t,0,0,inst->dst,as_reg(REG_XMM15),NULL));
                    inst->dst=as_reg(REG_XMM15);
                }
                break;
            }

            if (is_large_imm(inst->src) ||
                (is_memory_operand(inst->src) && is_memory_operand(inst->dst))){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }

            if (inst->dst.type==AS_IMM){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->dst,as_reg(REG_R11),NULL));
                inst->dst=as_reg(REG_R11);
            }
            break;

        case AS_IDIV:
        case AS_DIV:
            if (inst->src.type==AS_IMM){
                push_inst(tail,make_asm(AS_MOV,t,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }
            break;

        case AS_LEA:
            if (inst->dst.type!=AS_REG){
                push_inst(tail,make_asm(AS_LEA,ASM_QUADWORD,0,0,inst->src,as_reg(REG_R11),NULL));
                inst->type=AS_MOV;
                inst->src=as_reg(REG_R11);
            }
            break;

        case AS_PUSH:
            if (is_large_imm(inst->src)){
                push_inst(tail,make_asm(AS_MOV,ASM_QUADWORD,0,0,inst->src,as_reg(REG_R10),NULL));
                inst->src=as_reg(REG_R10);
            }
            break;

        default:
            break;
    }

    push_inst(tail,inst);
}

static void fix_and_allocate_function(AsmFunc *fn,AsmSymbol *symbols){
    replace_pseudos(fn,symbols);

    AsmInst *instructions=fn->instructions;
    fn->instructions=NULL;
    AsmInst **tail=&fn->instructions;

    if (fn->stack_size>0){
        push_inst(&tail,make_asm(AS_BINARY,ASM_QUADWORD,BINARY_SUB,0,as_imm(fn->stack_size),as_reg(REG_SP),NULL));
    }

    /*
    Apply instruction fix-up independently to every function.
    */
    while (instructions){
        AsmInst *next=instructions->next;
        instructions->next=NULL;
        fix_instruction(instructions,&tail);
        instructions=next;
    }
}

void fix_and_allocate(AsmProg *asmp){
    if (!asmp){
        return;
    }

    for (int i=0;i<asmp->top_level_count;i++){
        AsmTopLevel *top_level=asmp->top_levels[i];
        if (top_level->type==ASM_TOP_FUNCTION){
            fix_and_allocate_function(top_level->function,asmp->symbols);
        }
    }
}

//Tacky generation
void gen_tacky_block_items(BlockItem **items,int count,TackyInstruction **inst_list);
void gen_tacky_decl(VariableDeclaration *decl,TackyInstruction **inst_list);
void gen_tacky_statement(Statement *stmt,TackyInstruction **inst_list);
static TackyFunction *generate_tacky_function(Function *fn,Symbol *symbols);
static char *make_loop_target(const char *kind,const char *loop_id);
static void emit_tacky_label(TackyInstruction **inst_list,const char *label);
static void emit_tacky_jump(TackyInstruction **inst_list,const char *label);
static void emit_tacky_jz(TackyInstruction **inst_list,TackyVal *cond,const char *label);
static void emit_tacky_jnz(TackyInstruction **inst_list,TackyVal *cond,const char *label);
static void gen_tacky_for_init(ForInit *init,TackyInstruction **inst_list);
static void emit_switch_dispatch(
    TackyInstruction **inst_list,
    TackyVal *control,
    Type *control_type,
    SwitchCaseInfo *cases,
    const char *default_label,
    const char *break_label
);

//frontend symbol table; TACKY generation records every temporary's type here
static Symbol **tacky_symbols=NULL;

TackyVal *make_tacky_variable(Type *type){
    char *name=make_temporary();
    symbol_add_local(tacky_symbols,name,type);
    return tacky_val_var(name);
}

static void emit_tacky_src_dst(
    TackyInstruction **inst_list,
    TackyInstType type,
    TackyVal *src,
    TackyVal *dst
){
    TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
    inst->type=type;
    inst->src=src;
    inst->dst=dst;
    append_tacky_inst(inst_list,inst);
}

//convert val from one type to another; no instruction when the types match
static TackyVal *emit_tacky_cast(
    TackyInstruction **inst_list,
    TackyVal *val,
    Type *from,
    Type *to
){
    if (type_equal(from,to)){
        return val;
    }
    if (to->kind==TYPE_DOUBLE && is_character_type(from)){
        val=emit_tacky_cast(inst_list,val,from,type_int());
        return emit_tacky_cast(inst_list,val,type_int(),to);
    }
    if (from->kind==TYPE_DOUBLE && is_character_type(to)){
        val=emit_tacky_cast(inst_list,val,from,type_int());
        return emit_tacky_cast(inst_list,val,type_int(),to);
    }

    TackyVal *dst=make_tacky_variable(to);
    TackyInstType type;
    if (to->kind==TYPE_DOUBLE){
        type=type_is_signed(from) ? TACKY_INST_INT_TO_DOUBLE : TACKY_INST_UINT_TO_DOUBLE;
    }else if (from->kind==TYPE_DOUBLE){
        type=type_is_signed(to) ? TACKY_INST_DOUBLE_TO_INT : TACKY_INST_DOUBLE_TO_UINT;
    }else if (type_size(from)==type_size(to)){
        type=TACKY_INST_COPY;
    }else if (type_size(from)>type_size(to)){
        type=TACKY_INST_TRUNCATE;
    }else{
        type=type_is_signed(from) ? TACKY_INST_SIGN_EXTEND : TACKY_INST_ZERO_EXTEND;
    }
    emit_tacky_src_dst(inst_list,type,val,dst);
    return dst;
}

//val is the variable itself, or the pointer value when is_deref
typedef struct {
    TackyVal *val;
    int is_deref;
} TackyLvalue;

TackyVal *gen_tacky_exp(Exp *e,TackyInstruction **inst_list);

static TackyVal *emit_tacky_binary(
    TackyInstruction **list,BinaryOpType op,TackyVal *left,TackyVal *right,
    Type *lt,Type *rt,Type *result
){
    if ((op==BINARY_ADD || op==BINARY_SUB) && (is_pointer(lt) || is_pointer(rt))){
        if (is_pointer(lt) && is_pointer(rt)){
            TackyVal *bytes=make_tacky_variable(type_long());
            TackyInstruction *sub=calloc(1,sizeof(TackyInstruction));
            sub->type=TACKY_INST_BINARY;
            sub->binary_op=BINARY_SUB;
            sub->src=left;
            sub->src2=right;
            sub->dst=bytes;
            append_tacky_inst(list,sub);
            TackyVal *scale=tacky_val_constant(make_const(type_long(),type_size(lt->referenced)));
            return emit_tacky_binary(list,BINARY_DIV,bytes,scale,type_long(),type_long(),type_long());
        }
        if (!is_pointer(lt)){
            TackyVal *swap=left;
            left=right;
            right=swap;
            lt=rt;
        }
        if (op==BINARY_SUB){
            TackyVal *negative=make_tacky_variable(type_long());
            TackyInstruction *neg=calloc(1,sizeof(TackyInstruction));
            neg->type=TACKY_INST_UNARY;
            neg->unary_op=UNARY_NEGATION;
            neg->src=right;
            neg->dst=negative;
            append_tacky_inst(list,neg);
            right=negative;
        }
        TackyVal *dst=make_tacky_variable(result);
        TackyInstruction *add=calloc(1,sizeof(TackyInstruction));
        add->type=TACKY_INST_ADD_PTR;
        add->src=left;
        add->src2=right;
        add->scale=type_size(lt->referenced);
        add->dst=dst;
        append_tacky_inst(list,add);
        return dst;
    }
    TackyVal *dst=make_tacky_variable(result);
    TackyInstruction *bin=calloc(1,sizeof(TackyInstruction));
    bin->type=TACKY_INST_BINARY;
    bin->binary_op=op;
    bin->src=left;
    bin->src2=right;
    bin->dst=dst;
    append_tacky_inst(list,bin);
    return dst;
}

TackyLvalue gen_tacky_lvalue(Exp *e,TackyInstruction **inst_list){
    if (e->type==EXP_STRING) return (TackyLvalue){tacky_val_var(e->string.name),0};
    if (e->type==EXP_SUBSCRIPT){
        Exp *ptr=e->subscript.left,*idx=e->subscript.right;
        if (!is_pointer(ptr->exp_type)){ Exp *swap=ptr; ptr=idx; idx=swap; }
        TackyVal *p=gen_tacky_exp(ptr,inst_list),*i=gen_tacky_exp(idx,inst_list);
        return (TackyLvalue){emit_tacky_binary(inst_list,BINARY_ADD,p,i,ptr->exp_type,idx->exp_type,ptr->exp_type),1};
    }
    if (e->type==EXP_DEREFERENCE){
        return (TackyLvalue){gen_tacky_exp(e->deref.exp,inst_list),1};
    }
    return (TackyLvalue){tacky_val_var(e->var_name),0};
}

TackyVal *emit_lvalue_load(TackyLvalue lv,Type *t,TackyInstruction **inst_list){
    if (!lv.is_deref){
        return lv.val;
    }

    TackyVal *dst=make_tacky_variable(t);
    emit_tacky_src_dst(inst_list,TACKY_INST_LOAD,lv.val,dst);
    return dst;
}

//returns the value of the assignment expression
TackyVal *emit_lvalue_store(TackyLvalue lv,TackyVal *v,TackyInstruction **inst_list){
    emit_tacky_src_dst(inst_list,lv.is_deref ? TACKY_INST_STORE : TACKY_INST_COPY,v,lv.val);
    return v;
}

// Generate TACKY IR for <expression>
//
// Handles:
//   constants
//   variables
//   unary expressions
//   binary expressions
//   assignment
//   compound assignment
//   ++ / --
//   conditional (?:)
//   dereference and address-of
//   short-circuit logical operators (&&, ||)
TackyVal *gen_tacky_exp(Exp *e,TackyInstruction **inst_list){
    if (e->type==EXP_STRING) return tacky_val_var(e->string.name);
    if (e->type==EXP_CONSTANT){
        return tacky_val_constant(e->constant);
    }
    else if (e->type==EXP_SIZEOF || e->type==EXP_SIZEOF_TYPE){
        return tacky_val_constant(sizeof_value(e));
    }
    else if (e->type==EXP_CAST){
        TackyVal *inner=gen_tacky_exp(e->cast.exp,inst_list);
        if (e->exp_type->kind==TYPE_VOID) return NULL;
        return emit_tacky_cast(inst_list,inner,e->cast.exp->exp_type,e->cast.target_type);
    }
    else if (e->type==EXP_FUNCTION_CALL) {
        int arg_count= e->function_call.arg_count;
        TackyVal **args=NULL;

        if (arg_count > 0) {
            args = calloc(
                (size_t)arg_count,
                sizeof(TackyVal*)
            );
        }

        //orderly calculate arguments
        for(int i=0; i< arg_count;i++){
            args[i]=gen_tacky_exp(
                e->function_call.args[i],
                inst_list
            );
        }

        TackyVal *dst = e->exp_type->kind==TYPE_VOID ? NULL : make_tacky_variable(e->exp_type);

        TackyInstruction *call = calloc(1,sizeof(TackyInstruction));

        call->type = TACKY_INST_FUN_CALL;
        call->fun_name = strdup(e->function_call.name);
        call->args = args;
        call->arg_count = arg_count;
        call->dst = dst;

        append_tacky_inst(inst_list,call);

        return dst;
    }
    else if (e->type==EXP_CONDITIONAL){
        char *false_label=make_label("cond_false");  
        char *end_label=make_label("cond_end");
        //a void conditional keeps its jumps but has no value to copy
        TackyVal *dst=e->exp_type->kind==TYPE_VOID ? NULL : make_tacky_variable(e->exp_type);

        //1. calculate condition
        TackyVal *cond=gen_tacky_exp(e->conditional.condition,inst_list);

        //2. JumpIfZero(cond,false_label)
        TackyInstruction *jz=calloc(1,sizeof(TackyInstruction));
        jz->type=TACKY_INST_JZ;
        jz->src=cond;
        jz->label_name=strdup(false_label);
        append_tacky_inst(inst_list,jz);

        //3. calculate true_expr，and copy to dst
        TackyVal *v1=gen_tacky_exp(e->conditional.true_expr,inst_list);
        if (dst) emit_tacky_src_dst(inst_list,TACKY_INST_COPY,v1,dst);

        //4. jump(end_label) skip false_expr
        TackyInstruction *jmp=calloc(1,sizeof(TackyInstruction));
        jmp->type=TACKY_INST_JUMP;
        jmp->label_name=strdup(end_label);
        append_tacky_inst(inst_list,jmp);

        //5. label(false_label)
        TackyInstruction *l_false=calloc(1,sizeof(TackyInstruction));
        l_false->type=TACKY_INST_LABEL;
        l_false->label_name=strdup(false_label);
        append_tacky_inst(inst_list,l_false);


        //6. calculate false_expr，and copy to dst
        TackyVal *v2=gen_tacky_exp(e->conditional.false_expr,inst_list);
        if (dst) emit_tacky_src_dst(inst_list,TACKY_INST_COPY,v2,dst);

        //7. label(end_label)
        TackyInstruction *l_end=calloc(1,sizeof(TackyInstruction));
        l_end->type=TACKY_INST_LABEL;
        l_end->label_name=strdup(end_label);
        append_tacky_inst(inst_list,l_end);

        return dst;

    }
    else if (e->type==EXP_COMPOUND_ASSIGN){
        TackyLvalue lv=gen_tacky_lvalue(e->compound.lvalue,inst_list);
        TackyVal *rhs_val=gen_tacky_exp(e->compound.rvalue,inst_list);

        //the type checker converted the rvalue to the type the operation runs in
        Type *lhs_type=e->compound.lvalue->exp_type;
        Type *op_type=e->compound.rvalue->exp_type;
        TackyVal *lhs_val=emit_lvalue_load(lv,lhs_type,inst_list);

        Type *calculation_type=is_pointer(lhs_type) ? lhs_type : op_type;
        TackyVal *left=is_pointer(lhs_type) ? lhs_val : emit_tacky_cast(inst_list,lhs_val,lhs_type,op_type);
        TackyVal *tmp=emit_tacky_binary(inst_list,e->compound.op,left,rhs_val,calculation_type,op_type,calculation_type);
        return emit_lvalue_store(lv,emit_tacky_cast(inst_list,tmp,calculation_type,lhs_type),inst_list);
    }
    //  ++ / --
    else if (e->type==EXP_PREFIX_OP || e->type==EXP_POSTFIX_OP){
        TackyLvalue lv=gen_tacky_lvalue(e->inc_dec.lvalue,inst_list);
        Type *lhs_type=e->inc_dec.lvalue->exp_type;
        TackyVal *lhs_val=emit_lvalue_load(lv,lhs_type,inst_list);
        Type *op_type=integer_promotion(lhs_type);
        TackyVal *one_val=tacky_val_constant(make_const(is_pointer(lhs_type) ? type_long() : op_type,1));
        BinaryOpType op=e->inc_dec.is_increment ? BINARY_ADD :BINARY_SUB;

        TackyVal *old_val=NULL;
        
        //if is a++ (postfix)，need to backup old value，because the expression want old value
        if (e->type==EXP_POSTFIX_OP){
            old_val=make_tacky_variable(lhs_type);
            TackyInstruction *save_old=calloc(1,sizeof(TackyInstruction));
            save_old->type=TACKY_INST_COPY;
            save_old->src=lhs_val;
            save_old->dst=old_val;
            append_tacky_inst(inst_list,save_old);
        }

        TackyVal *promoted=emit_tacky_cast(inst_list,lhs_val,lhs_type,op_type);
        TackyVal *tmp=emit_tacky_binary(inst_list,op,promoted,one_val,op_type,is_pointer(lhs_type) ? type_long() : op_type,op_type);
        tmp=emit_tacky_cast(inst_list,tmp,op_type,lhs_type);

        //soter new value to a
        emit_lvalue_store(lv,tmp,inst_list);

        return (e->type==EXP_POSTFIX_OP) ? old_val : tmp;
    }
    else if (e->type==EXP_VAR){
        return tacky_val_var(e->var_name);
    }
    else if (e->type==EXP_ASSIGN){
        TackyLvalue lv=gen_tacky_lvalue(e->assign.lvalue,inst_list);
        //calculate right side value
        TackyVal *rhs_val=gen_tacky_exp(e->assign.rvalue,inst_list);

        //the value is what was stored, and a later call may change a source variable
        if (rhs_val->type==TACKY_VAL_VAR){
            TackyVal *stored=make_tacky_variable(e->exp_type);
            emit_tacky_src_dst(inst_list,TACKY_INST_COPY,rhs_val,stored);
            rhs_val=stored;
        }

        return emit_lvalue_store(lv,rhs_val,inst_list);
    }
    else if (e->type==EXP_DEREFERENCE || e->type==EXP_SUBSCRIPT){
        return emit_lvalue_load(gen_tacky_lvalue(e,inst_list),e->exp_type,inst_list);
    }
    else if (e->type==EXP_ADDR_OF){
        TackyLvalue lv=gen_tacky_lvalue(e->addr_of.exp,inst_list);
        //&*p is p: neither operator is evaluated
        if (lv.is_deref){
            return lv.val;
        }

        TackyVal *dst=make_tacky_variable(e->exp_type);
        emit_tacky_src_dst(inst_list,TACKY_INST_GET_ADDRESS,lv.val,dst);
        return dst;
    }
    else if (e->type==EXP_UNARY){
        //deal with inner expression first,get sorce operator
        TackyVal *src=gen_tacky_exp(e->unary.exp,inst_list);
        
        //build a temporal variable for dest
        TackyVal *dst=make_tacky_variable(e->exp_type);

        //emit unary instruction and add list
        TackyInstruction *inst=calloc(1, sizeof(TackyInstruction));
        inst->type=TACKY_INST_UNARY;
        inst->unary_op=e->unary.op;
        inst->src=src;
        inst->dst=dst;
        inst->next=NULL;
        append_tacky_inst(inst_list,inst);
        
        //return temporal variable，let outter layer function can use it
        return dst;
    }
    else if (e->type==EXP_BINARY){

        //deal with && Short-circuiting  logic
        if (e->binary.op==BINARY_LOGICAL_AND){
            char *false_label=make_label("and_false");
            char *end_label=make_label("and_end");
            TackyVal *dst=make_tacky_variable(type_int());

            // calculate left exp
            TackyVal *v1=gen_tacky_exp(e->binary.left,inst_list);
            // if v1  is zero， jump to false label
            TackyInstruction *jz=calloc(1,sizeof(TackyInstruction));
            jz->type=TACKY_INST_JZ;
            jz->src=v1;
            jz->label_name=strdup(false_label);
            append_tacky_inst(inst_list,jz);

            //calculate right exp
            TackyVal *v2=gen_tacky_exp(e->binary.right,inst_list);
            // if v2 is zero，jump fase label
            TackyInstruction *jz2=calloc(1,sizeof(TackyInstruction));
            jz2->type=TACKY_INST_JZ;
            jz2->src=v2;
            jz2->label_name=strdup(false_label);
            append_tacky_inst(inst_list,jz2);

            // if execute here，it's mean v1,v2 result is non-zero
            TackyInstruction *cp1=calloc(1,sizeof(TackyInstruction));
            cp1->type=TACKY_INST_COPY;
            cp1->src=tacky_val_constant(make_const(type_int(),1));
            cp1->dst=dst;
            append_tacky_inst(inst_list,cp1);
            
            TackyInstruction *jmp=calloc(1,sizeof(TackyInstruction));
            jmp->type=TACKY_INST_JUMP;
            jmp->label_name=strdup(end_label);
            append_tacky_inst(inst_list,jmp);

            // false label : result setting 0
            TackyInstruction *l_false=calloc(1,sizeof(TackyInstruction));
            l_false->type=TACKY_INST_LABEL;
            l_false->label_name=strdup(false_label);
            append_tacky_inst(inst_list,l_false);

            TackyInstruction *cp0=calloc(1,sizeof(TackyInstruction));
            cp0->type=TACKY_INST_COPY;
            cp0->src=tacky_val_constant(make_const(type_int(),0));
            cp0->dst=dst;
            append_tacky_inst(inst_list,cp0);

            //end label
            TackyInstruction *l_end=calloc(1,sizeof(TackyInstruction));
            l_end->type=TACKY_INST_LABEL;
            l_end->label_name=strdup(end_label);
            append_tacky_inst(inst_list,l_end);

            return dst;
        }

        // deal with ||  Short-circuiting  logic
        if (e->binary.op==BINARY_LOGICAL_OR){
            char *true_label=make_label("or_true");
            char *end_label=make_label("or_end");
            TackyVal *dst=make_tacky_variable(type_int());
            
            TackyVal *v1=gen_tacky_exp(e->binary.left,inst_list);
            // if v1 not zero ，jump true label
            TackyInstruction *jnz=calloc(1,sizeof(TackyInstruction));
            jnz->type=TACKY_INST_JNZ;
            jnz->src=v1;
            jnz->label_name=strdup(true_label);
            append_tacky_inst(inst_list,jnz);

            TackyVal *v2=gen_tacky_exp(e->binary.right,inst_list);
            // if v2 not zero ，jump true label
            TackyInstruction *jnz2=calloc(1,sizeof(TackyInstruction));
            jnz2->type=TACKY_INST_JNZ;
            jnz2->src=v2;
            jnz2->label_name=strdup(true_label);
            append_tacky_inst(inst_list,jnz2);

            // both result all 0
            TackyInstruction *cp0=calloc(1,sizeof(TackyInstruction));
            cp0->type=TACKY_INST_COPY;
            cp0->src=tacky_val_constant(make_const(type_int(),0));
            cp0->dst=dst;
            append_tacky_inst(inst_list,cp0);

            TackyInstruction *jmp=calloc(1,sizeof(TackyInstruction));
            jmp->type=TACKY_INST_JUMP;
            jmp->label_name=strdup(end_label);
            append_tacky_inst(inst_list,jmp);

            //true label
            TackyInstruction *l_true=calloc(1,sizeof(TackyInstruction));
            l_true->type=TACKY_INST_LABEL;
            l_true->label_name=strdup(true_label);
            append_tacky_inst(inst_list,l_true);

            TackyInstruction *cp1=calloc(1,sizeof(TackyInstruction));
            cp1->type=TACKY_INST_COPY;
            cp1->src=tacky_val_constant(make_const(type_int(),1));
            cp1->dst=dst;
            append_tacky_inst(inst_list,cp1);

            //end label
            TackyInstruction *l_end=calloc(1,sizeof(TackyInstruction));
            l_end->type=TACKY_INST_LABEL;
            l_end->label_name=strdup(end_label);
            append_tacky_inst(inst_list,l_end);

            return dst;
        }

        //deal with normal binary or logic operator(EQ,NEQ,LT,GT,etc)

        //deal with left side expression，get result position v1
        TackyVal *v1=gen_tacky_exp(e->binary.left,inst_list);

        TackyVal *v2=gen_tacky_exp(e->binary.right,inst_list);

        return emit_tacky_binary(inst_list,e->binary.op,v1,v2,e->binary.left->exp_type,e->binary.right->exp_type,e->exp_type);
    }

    return NULL;
}

// Generate TACKY IR for <statement>
//
// Handles:
//   return
//   expression
//   if / else
//   goto
//   label
void gen_tacky_statement(Statement *stmt,TackyInstruction **inst_list){
    if (stmt->type==STMT_RETURN){
        TackyVal *val=stmt->exp ? gen_tacky_exp(stmt->exp,inst_list) : NULL;
        TackyInstruction *ret=calloc(1,sizeof(TackyInstruction));
        ret->type=TACKY_INST_RETURN;
        ret->src=val;
        append_tacky_inst(inst_list,ret);
    }else if (stmt->type==STMT_LABEL){
        TackyInstruction *lab=calloc(1,sizeof(TackyInstruction));
        lab->type=TACKY_INST_LABEL;
        lab->label_name=strdup(stmt->label);

        append_tacky_inst(inst_list,lab);

        gen_tacky_statement(stmt->if_stmt.then_branch,inst_list);
    }
    else if (stmt->type==STMT_GOTO){
       TackyInstruction *jmp=calloc(1,sizeof(TackyInstruction));
       jmp->type=TACKY_INST_JUMP;
       jmp->label_name=strdup(stmt->label);
       
       append_tacky_inst(inst_list,jmp);
    }
    else if (stmt->type==STMT_EXPRESSION){
        if (stmt->exp){
            gen_tacky_exp(stmt->exp,inst_list);
        } 
    }
    else if (stmt->type==STMT_IF){
        TackyVal *cond=gen_tacky_exp(stmt->if_stmt.condition,inst_list);
        char *end_label=make_label("if_end");

        if (stmt->if_stmt.else_branch){
            //have else branch condition
            char *else_label=make_label("if_else");
            
            //JumpIfZero(cond,else_label)
            TackyInstruction *jz=calloc(1,sizeof(TackyInstruction));
            jz->type=TACKY_INST_JZ;
            jz->src=cond;
            jz->label_name=strdup(else_label);
            append_tacky_inst(inst_list,jz);

            // execute then section
            gen_tacky_statement(stmt->if_stmt.then_branch,inst_list);

            //jump(end_label)(execute then，and skip else section)
            TackyInstruction *jmp=calloc(1,sizeof(TackyInstruction));
            jmp->type=TACKY_INST_JUMP;
            jmp->label_name=strdup(end_label);
            append_tacky_inst(inst_list,jmp);

            //label(else_label)
            TackyInstruction *l_else=calloc(1,sizeof(TackyInstruction));
            l_else->type=TACKY_INST_LABEL;
            l_else->label_name=strdup(else_label);
            append_tacky_inst(inst_list,l_else);

            gen_tacky_statement(stmt->if_stmt.else_branch,inst_list);
        
        }else{
            //no else branch situation，condition if it's false just jump to end_label
            //JumpIfZero(cond,else_label)
            TackyInstruction *jz=calloc(1,sizeof(TackyInstruction));
            jz->type=TACKY_INST_JZ;
            jz->src=cond;
            jz->label_name=strdup(end_label);
            append_tacky_inst(inst_list,jz);

            //execute then section
            gen_tacky_statement(stmt->if_stmt.then_branch,inst_list);
        }

        //label(end_label)
        TackyInstruction *l_end=calloc(1,sizeof(TackyInstruction));
        l_end->type=TACKY_INST_LABEL;
        l_end->label_name=strdup(end_label);
        append_tacky_inst(inst_list,l_end);
    }
    else if(stmt->type==STMT_COMPOUND){
        gen_tacky_block_items(stmt->block_items,stmt->block_count,inst_list);
    }
    else if (stmt->type==STMT_NULL){
        return;
    }
    else if (stmt->type==STMT_BREAK){
        char *break_label=make_loop_target("break",stmt->loop_label);
        emit_tacky_jump(inst_list,break_label);
    }
    else if (stmt->type==STMT_CONTINUE){
        char *continue_label=make_loop_target("continue",stmt->loop_label);
        emit_tacky_jump(inst_list,continue_label);
    }
    else if (stmt->type==STMT_WHILE){
        char *continue_label=make_loop_target("continue",stmt->loop_label);
        char *break_label=make_loop_target("break",stmt->loop_label);

        emit_tacky_label(inst_list,continue_label);
        
        TackyVal *cond=gen_tacky_exp(stmt->while_stmt.condition,inst_list);
        emit_tacky_jz(inst_list,cond,break_label);

        gen_tacky_statement(stmt->while_stmt.body,inst_list);

        emit_tacky_jump(inst_list,continue_label);
        emit_tacky_label(inst_list,break_label);
    }
    else if (stmt->type==STMT_DO_WHILE){
        char *start_label=make_loop_target("start",stmt->loop_label);
        char *continue_label=make_loop_target("continue",stmt->loop_label);
        char *break_label=make_loop_target("break",stmt->loop_label);

        emit_tacky_label(inst_list,start_label);

        gen_tacky_statement(stmt->do_while_stmt.body,inst_list);

        emit_tacky_label(inst_list,continue_label);

        TackyVal *cond=gen_tacky_exp(stmt->do_while_stmt.condition,inst_list);
        emit_tacky_jnz(inst_list,cond,start_label);

        emit_tacky_label(inst_list,break_label);

    }
    else if (stmt->type==STMT_FOR){
        char *start_label=make_loop_target("start",stmt->loop_label);
        char *continue_label=make_loop_target("continue",stmt->loop_label);
        char *break_label=make_loop_target("break",stmt->loop_label);

        gen_tacky_for_init(stmt->for_stmt.init,inst_list);

        emit_tacky_label(inst_list,start_label);

        if (stmt->for_stmt.condition){
            TackyVal *cond=gen_tacky_exp(stmt->for_stmt.condition,inst_list);
            emit_tacky_jz(inst_list,cond,break_label);
        }

        gen_tacky_statement(stmt->for_stmt.body,inst_list);

        emit_tacky_label(inst_list,continue_label);

        if (stmt->for_stmt.post){
            (void)gen_tacky_exp(stmt->for_stmt.post,inst_list);
        }

        emit_tacky_jump(inst_list,start_label);
        emit_tacky_label(inst_list,break_label);
    }
    else if (stmt->type==STMT_SWITCH){
        char *break_label=make_loop_target("break",stmt->loop_label);
        
        TackyVal *control=gen_tacky_exp(stmt->switch_stmt.control,inst_list);

        emit_switch_dispatch(
            inst_list,
            control,
            stmt->switch_stmt.control->exp_type,
            stmt->switch_stmt.cases,
            stmt->switch_stmt.default_label,
            break_label
        );

        gen_tacky_statement(stmt->switch_stmt.body,inst_list);

        emit_tacky_label(inst_list,break_label);
    }
    else if (stmt->type==STMT_CASE){
        emit_tacky_label(inst_list,stmt->case_stmt.case_label);
        gen_tacky_statement(stmt->case_stmt.body,inst_list);
    }
    else if (stmt->type==STMT_DEFAULT){
        emit_tacky_label(inst_list,stmt->default_stmt.default_label);
        gen_tacky_statement(stmt->default_stmt.body,inst_list);
    }
}

static void emit_switch_dispatch(
    TackyInstruction **inst_list,
    TackyVal *control,
    Type *control_type,
    SwitchCaseInfo *cases,
    const char *default_label,
    const char *break_label
){
    for(SwitchCaseInfo *c=cases;c;c=c->next){
        TackyVal *is_match=make_tacky_variable(type_int());
        
        TackyInstruction *cmp=calloc(1,sizeof(TackyInstruction));
        cmp->type=TACKY_INST_BINARY;
        cmp->binary_op=BINARY_EQ;
        cmp->src=control;
        cmp->src2=tacky_val_constant(make_const(control_type,c->value));
        cmp->dst=is_match;
        append_tacky_inst(inst_list,cmp);

        emit_tacky_jnz(inst_list,is_match,c->label);
        
    }

    if (default_label){
        emit_tacky_jump(inst_list,default_label);
    }else{
        emit_tacky_jump(inst_list,break_label);
    }
}

static void gen_tacky_initializer(Initializer *init,char *name,int offset,int aggregate,TackyInstruction **list){
    if (!init->compound && init->type->kind==TYPE_ARRAY && init->exp->type==EXP_STRING){
        ByteString bytes=init->exp->string.bytes;
        for (unsigned long i=0;i<init->type->count;i++){
            long value=i<(unsigned long)bytes.length ? (unsigned char)bytes.data[i] : 0;
            TackyInstruction *copy=calloc(1,sizeof(TackyInstruction));
            copy->type=TACKY_INST_COPY_TO_OFFSET; copy->offset=offset+(int)i;
            copy->src=tacky_val_constant(make_const(init->type->referenced,value));
            copy->dst=tacky_val_var(name); append_tacky_inst(list,copy);
        }
    }else if (init->compound){
        int step=type_size(init->type->referenced);
        for (int i=0;i<init->count;i++) gen_tacky_initializer(init->items[i],name,offset+i*step,1,list);
    }else{
        TackyVal *src=gen_tacky_exp(init->exp,list);
        TackyInstruction *copy=calloc(1,sizeof(TackyInstruction));
        copy->type=aggregate ? TACKY_INST_COPY_TO_OFFSET : TACKY_INST_COPY;
        copy->src=src; copy->dst=tacky_val_var(name); copy->offset=offset; append_tacky_inst(list,copy);
    }
}

void gen_tacky_decl(VariableDeclaration *decl,TackyInstruction **inst_list){
    if (decl && decl->storage_class==STORAGE_NONE && decl->init) gen_tacky_initializer(decl->init,decl->name,0,decl->var_type->kind==TYPE_ARRAY,inst_list);
}

void gen_tacky_block_items(BlockItem **items,int count,TackyInstruction **inst_list){
    for (int i=0;i<count;i++){
        BlockItem *bi=items[i];
        if (bi->type==BI_STMT){
            gen_tacky_statement(bi->stmt,inst_list);
        }else if (bi->type==BI_DECL){
            if (bi->decl->type==DECL_VAR){
                gen_tacky_decl(bi->decl->var,inst_list);
            }else{
                continue;
            }
        }
    }
}

TackyFunction *generate_tacky_function(Function *fn,Symbol *symbols) {
    if (!fn || !fn->has_body) {
        return NULL;
    }

    TackyFunction *t_fn= calloc(1,sizeof(TackyFunction));

    t_fn->name = strdup(fn->name);

    Symbol *entry=symbol_find(symbols,fn->name);
    if (!entry || entry->attr_type!=SYMBOL_ATTR_FUNCTION) {
        fprintf(stderr,"TACKY Error: missing function symbol '%s'\n",fn->name);
        exit(1);
    }

    t_fn->global = entry->global;
    t_fn->param_count = fn->param_count;
    t_fn->instructions = NULL;

    //copy function paramters

    if (fn->param_count > 0){
        t_fn->params = calloc(
            (size_t)fn->param_count,
            sizeof(char*)
        );

        if (!t_fn->params) {
            perror("calloc TACKY parameters fail");
            exit(1);
        }

        for(int i=0;i<fn->param_count;i++) {
            t_fn->params[i] = strdup(fn->params[i]);
        }
    }
    else{
        t_fn->params = NULL;
    }

    gen_tacky_block_items(
        fn->body,
        fn->body_count,
        &t_fn->instructions
    );

    //Add return(0) in the end of function
    TackyInstruction *final_inst = calloc(1,sizeof(TackyInstruction));

    final_inst->type = TACKY_INST_RETURN;
    final_inst->src = tacky_val_constant(make_const(type_int(),0));

    append_tacky_inst(
        &t_fn->instructions,
        final_inst
    );

    return t_fn;
}

static void append_tacky_top_level(
    TackyProgram *program,
    TackyTopLevel *top_level
){
    TackyTopLevel **new_items=realloc(
        program->top_levels,
        sizeof(TackyTopLevel*)*(size_t)(program->top_level_count+1)
    );

    if (!new_items) {
        perror("realloc TACKY top levels");
        exit(1);
    }

    program->top_levels=new_items;
    program->top_levels[program->top_level_count++]=top_level;
}

static void append_tacky_function(
    TackyProgram *program,
    TackyFunction *function
){
    TackyTopLevel *top_level=calloc(1,sizeof(TackyTopLevel));
    if (!top_level) {
        perror("calloc TACKY function top level");
        exit(1);
    }

    top_level->type=TACKY_TOP_FUNCTION;
    top_level->function=function;
    append_tacky_top_level(program,top_level);
}

static void append_tacky_static_variable(
    TackyProgram *program,
    const char *name,
    int global,
    Type *type,
    StaticInitList init,
    int constant
){
    TackyStaticVariable *variable=calloc(1,sizeof(TackyStaticVariable));
    if (!variable) {
        perror("calloc TACKY static variable");
        exit(1);
    }

    variable->name=strdup(name);
    variable->global=global;
    variable->type=type;
    variable->init=init;

    TackyTopLevel *top_level=calloc(1,sizeof(TackyTopLevel));
    if (!top_level) {
        perror("calloc TACKY static top level");
        exit(1);
    }

    top_level->type=constant ? TACKY_TOP_STATIC_CONSTANT : TACKY_TOP_STATIC_VARIABLE;
    top_level->static_variable=variable;
    append_tacky_top_level(program,top_level);
}

static void convert_symbols_to_tacky(
    Symbol *symbols,
    TackyProgram *program
){
    for (Symbol *entry=symbols;entry;entry=entry->next) {
        if (entry->attr_type!=SYMBOL_ATTR_STATIC && entry->attr_type!=SYMBOL_ATTR_CONSTANT) {
            continue;
        }

        if (entry->init.type==INITIAL_NONE) {
            continue;
        }

        StaticInitList init;
        if (entry->init.type==INITIAL_VALUE) {
            init=entry->init.value;
        }
        else {
            init=zero_static_list(entry->type);
        }

        append_tacky_static_variable(
            program,
            entry->name,
            entry->global,
            entry->type,
            init,
            entry->attr_type==SYMBOL_ATTR_CONSTANT
        );
    }
}

TackyProgram *generate_tacky(Program *prog,Symbol **symbols){
    if (!prog){
        return NULL;
    }

    tacky_symbols=symbols;

    TackyProgram *t_prog=calloc(1,sizeof(TackyProgram));
    if (!t_prog){
        perror("calloc TackyProgram");
        exit(1);
    }

    for (int i=0;i<prog->declaration_count;i++){
        Declaration *decl=prog->declarations[i];

        if (decl->type!=DECL_FUN){
            continue;
        }

        Function *fn=decl->fun;

        if (!fn || !fn->has_body){
            continue;
        }

        TackyFunction *t_fn=generate_tacky_function(fn,*symbols);
        if (!t_fn){
            continue;
        }

        append_tacky_function(t_prog,t_fn);
    }

    convert_symbols_to_tacky(*symbols,t_prog);

    return t_prog;
}


static const char *reg_name_64(AsmReg reg){
    static const char *names[]={
        "%rax","%rcx","%rdx","%rdi","%rsi",
        "%r8","%r9","%r10","%r11","%rsp","%rbp"
    };
    return names[reg];
}

static const char *reg_name_32(AsmReg reg){
    static const char *names[]={
        "%eax","%ecx","%edx","%edi","%esi",
        "%r8d","%r9d","%r10d","%r11d","%esp","%ebp"
    };
    return names[reg];
}

static const char *reg_name_8(AsmReg reg){
    static const char *names[]={
        "%al","%cl","%dl","%dil","%sil",
        "%r8b","%r9b","%r10b","%r11b","%spl","%bpl"
    };
    return names[reg];
}

#ifdef __APPLE__
#define SYMBOL_PREFIX "_"
#else
#define SYMBOL_PREFIX ""
#endif

//backend symbols of the program being emitted, to tell constants from variables
static AsmSymbol *emit_symbols=NULL;

static void emit_data_op(AsmOperand op,FILE *out){
    AsmSymbol *entry=asm_symbol_find(emit_symbols,op.data_name);
    if (entry && entry->is_constant){
        fprintf(out,".L%s(%%rip)",op.data_name);
    }else{
        fprintf(out,SYMBOL_PREFIX "%s(%%rip)",op.data_name);
    }
}

static const char *xmm_name(AsmReg reg){
    static const char *names[]={
        "%xmm0","%xmm1","%xmm2","%xmm3",
        "%xmm4","%xmm5","%xmm6","%xmm7",
        "%xmm14","%xmm15"
    };
    return names[reg-REG_XMM0];
}

//register names depend on operand size; 1 byte is for SetCC
static void emit_sized_op(AsmOperand op,int bytes,FILE *out){
    if (op.type==AS_IMM){
        fprintf(out,"$%ld",op.imm);
    }
    else if (op.type==AS_REG && op.reg>=REG_XMM0){
        fprintf(out,"%s",xmm_name(op.reg));
    }
    else if (op.type==AS_REG){
        const char *name=bytes==8 ? reg_name_64(op.reg)
                        : bytes==4 ? reg_name_32(op.reg)
                        : reg_name_8(op.reg);
        fprintf(out,"%s",name);
    }
    //an address is 8 bytes whatever the operand size
    else if (op.type==AS_MEMORY){
        fprintf(out,"%d(%s)",op.memory.offset,reg_name_64(op.memory.base));
    }
    else if (op.type==AS_INDEXED){
        fprintf(out,"(%s,%s,%d)",reg_name_64(op.indexed.base),reg_name_64(op.indexed.index),op.indexed.scale);
    }
    else if (op.type==AS_DATA){
        emit_data_op(op,out);
    }
    else{
        fprintf(stderr,"Emitter Error: unresolved operand\n");
        exit(1);
    }
}

void emit_op(AsmOperand op,AsmType t,FILE *out){
    emit_sized_op(op,t==ASM_BYTE ? 1 : t==ASM_LONGWORD ? 4 : 8,out);
}

static char type_suffix(AsmType t){
    return t==ASM_BYTE ? 'b' : t==ASM_QUADWORD ? 'q' : 'l';
}

const char *cond_to_str(CondCode c){
    switch (c){
        case COND_E: return "e";
        case COND_NE: return "ne";
        case COND_G: return "g";
        case COND_GE: return "ge";
        case COND_L: return "l";
        case COND_LE: return "le";
        case COND_A: return "a";
        case COND_AE: return "ae";
        case COND_B: return "b";
        case COND_BE: return "be";
        case COND_P: return "p";
        case COND_NP: return "np";
        default: return "";
    }
}


static char *make_loop_target(const char *kind,const char *loop_id){
    size_t len=strlen(kind)+1+strlen(loop_id)+1;
    char *name=malloc(len);
    snprintf(name,len,"%s_%s",kind,loop_id);
    return name;
}

static void emit_tacky_label(TackyInstruction **inst_list,const char *label){
    TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
    inst->type=TACKY_INST_LABEL;
    inst->label_name=strdup(label);
    append_tacky_inst(inst_list,inst);
}

static void emit_tacky_jump(TackyInstruction **inst_list,const char *label){
    TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
    inst->type=TACKY_INST_JUMP;
    inst->label_name=strdup(label);
    append_tacky_inst(inst_list,inst);
}

static void emit_tacky_jz(TackyInstruction **inst_list,TackyVal *cond,const char *label){
    TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
    inst->type=TACKY_INST_JZ;
    inst->src=cond;
    inst->label_name=strdup(label);
    append_tacky_inst(inst_list,inst);
}

static void emit_tacky_jnz(TackyInstruction **inst_list,TackyVal *cond,const char *label){
    TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
    inst->type=TACKY_INST_JNZ;
    inst->src=cond;
    inst->label_name=strdup(label);
    append_tacky_inst(inst_list,inst);
}

static void gen_tacky_for_init(ForInit *init,TackyInstruction **inst_list){
    if (!init) return;

    if (init->type==FOR_INIT_DECL){
        gen_tacky_decl(init->decl,inst_list);
    }else{
        if (init->exp){
            (void)gen_tacky_exp(init->exp,inst_list);
        }
    }
}

static void emit_call_target(
    AsmProg *asmp,
    const char *name,
    FILE *out
){
#ifdef __APPLE__
    (void)asmp;
    fprintf(out,"_%s",name);
#else
    fprintf(out,"%s",name);

    AsmSymbol *entry=asm_symbol_find(asmp->symbols,name);
    if (!entry || !entry->defined){
        fprintf(out,"@PLT");
    }
#endif
}

static void emit_instruction(
    AsmProg *asmp,
    AsmInst *curr,
    FILE *out
){
    AsmType t=curr->asm_type;
    char suffix=type_suffix(t);

    switch (curr->type){
        case AS_PUSH:
            fprintf(out,"    pushq   ");
            emit_op(curr->src,ASM_QUADWORD,out);
            fprintf(out,"\n");
            break;

        case AS_CALL:
            fprintf(out,"    call    ");
            emit_call_target(asmp,curr->label,out);
            fprintf(out,"\n");
            break;

        case AS_MOV:
            if (t==ASM_DOUBLE){
                fprintf(out,"    movsd   ");
            }else{
                fprintf(out,"    mov%c    ",suffix);
            }
            emit_op(curr->src,t,out);
            fprintf(out,", ");
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;

        case AS_MOVSX:
        case AS_MOVZX:
            fprintf(out,"    mov%c%c%c  ",curr->type==AS_MOVSX ? 's' : 'z',type_suffix(curr->src_type),suffix);
            emit_op(curr->src,curr->src_type,out);
            fprintf(out,", "); emit_op(curr->dst,t,out); fprintf(out,"\n");
            break;

        case AS_NEG:
            fprintf(out,"    neg%c    ",suffix);
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;

        case AS_NOT:
            fprintf(out,"    not%c    ",suffix);
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;

        case AS_RET:
            fprintf(out,"    movq    %%rbp, %%rsp\n");
            fprintf(out,"    popq    %%rbp\n");
            fprintf(out,"    ret\n");
            break;

        case AS_BINARY:{
            const char *op_str=NULL;

            if (t==ASM_DOUBLE){
                switch (curr->binary_op){
                    case BINARY_ADD: op_str="addsd"; break;
                    case BINARY_SUB: op_str="subsd"; break;
                    case BINARY_MULT: op_str="mulsd"; break;
                    case BINARY_DIV: op_str="divsd"; break;
                    case BINARY_BITWISE_XOR: op_str="xorpd"; break;
                    default:
                        fprintf(stderr,"Emitter Error: unsupported double operator\n");
                        exit(1);
                }
                fprintf(out,"    %-8s",op_str);
                emit_op(curr->src,t,out);
                fprintf(out,", ");
                emit_op(curr->dst,t,out);
                fprintf(out,"\n");
                break;
            }

            switch (curr->binary_op){
                case BINARY_ADD:
                    op_str="add";
                    break;
                case BINARY_SUB:
                    op_str="sub";
                    break;
                case BINARY_MULT:
                    op_str="imul";
                    break;
                case BINARY_BITWISE_AND:
                    op_str="and";
                    break;
                case BINARY_BITWISE_OR:
                    op_str="or";
                    break;
                case BINARY_BITWISE_XOR:
                    op_str="xor";
                    break;
                case BINARY_LSHIFT:
                    op_str="sal";
                    break;
                case BINARY_RSHIFT:
                    op_str="sar";
                    break;
                case BINARY_SHR:
                    op_str="shr";
                    break;
                default:
                    fprintf(stderr,"Emitter Error: unsupported binary operator\n");
                    exit(1);
            }

            fprintf(out,"    %s%c    ",op_str,suffix);

            if (is_shift_op(curr->binary_op)){
                if (curr->src.type==AS_IMM){
                    emit_op(curr->src,t,out);
                }else{
                    fprintf(out,"%%cl");
                }
            }else{
                emit_op(curr->src,t,out);
            }

            fprintf(out,", ");
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;
        }

        case AS_IDIV:
        case AS_DIV:
            fprintf(out,curr->type==AS_IDIV ? "    idiv%c   " : "    div%c    ",suffix);
            emit_op(curr->src,t,out);
            fprintf(out,"\n");
            break;

        case AS_CDQ:
            fprintf(out,t==ASM_QUADWORD ? "    cqo\n" : "    cdq\n");
            break;

        case AS_CMP:
            if (t==ASM_DOUBLE){
                fprintf(out,"    comisd  ");
            }else{
                fprintf(out,"    cmp%c    ",suffix);
            }
            emit_op(curr->src,t,out);
            fprintf(out,", ");
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;

        case AS_CVTSI2SD:
            fprintf(out,"    cvtsi2sd%c ",suffix);
            emit_op(curr->src,t,out);
            fprintf(out,", ");
            emit_op(curr->dst,ASM_DOUBLE,out);
            fprintf(out,"\n");
            break;

        case AS_CVTTSD2SI:
            fprintf(out,"    cvttsd2si%c ",suffix);
            emit_op(curr->src,ASM_DOUBLE,out);
            fprintf(out,", ");
            emit_op(curr->dst,t,out);
            fprintf(out,"\n");
            break;

        case AS_JMP:
            fprintf(out,"    jmp     .L%s\n",curr->label);
            break;

        case AS_JMP_CC:
            fprintf(
                out,
                "    j%s     .L%s\n",
                cond_to_str(curr->cond),
                curr->label
            );
            break;

        case AS_SET_CC:
            fprintf(out,"    set%s   ",cond_to_str(curr->cond));
            emit_sized_op(curr->dst,1,out);
            fprintf(out,"\n");
            break;

        case AS_LABEL:
            fprintf(out,".L%s:\n",curr->label);
            break;

        case AS_LEA:
            fprintf(out,"    leaq    ");
            emit_op(curr->src,ASM_QUADWORD,out);
            fprintf(out,", ");
            emit_op(curr->dst,ASM_QUADWORD,out);
            fprintf(out,"\n");
            break;
    }
}

static void emit_function(
    AsmProg *asmp,
    AsmFunc *fn,
    FILE *out
){
    if (fn->global){
        fprintf(out,".globl " SYMBOL_PREFIX "%s\n",fn->name);
    }
    fprintf(out,".text\n" SYMBOL_PREFIX "%s:\n",fn->name);

    fprintf(out,"    pushq   %%rbp\n");
    fprintf(out,"    movq    %%rsp, %%rbp\n");

    for (AsmInst *curr=fn->instructions;
         curr!=NULL;
         curr=curr->next){
        emit_instruction(asmp,curr,out);
    }

    fprintf(out,"\n");
}

static long double_bits(double d){
    long bits;
    memcpy(&bits,&d,sizeof(bits));
    return bits;
}

static void emit_static_init(StaticInit value,FILE *out){
    switch (value.type){
        case INIT_ZERO: fprintf(out,"    .zero %ld\n",value.value); break;
        case INIT_DOUBLE: fprintf(out,"    .quad %ld\n",double_bits(value.dvalue)); break;
        case INIT_CHAR: case INIT_UCHAR: fprintf(out,"    .byte %ld\n",value.value); break;
        case INIT_LONG: case INIT_ULONG: fprintf(out,"    .quad %ld\n",value.value); break;
        case INIT_INT: case INIT_UINT: fprintf(out,"    .long %ld\n",value.value); break;
        case INIT_POINTER: fprintf(out,"    .quad .L%s\n",value.pointer_name); break;
        case INIT_STRING:
            fprintf(out,"    %s \"",value.string.null_terminated ? ".asciz" : ".ascii");
            for (int i=0;i<value.string.bytes.length;i++) fprintf(out,"\\%03o",(unsigned char)value.string.bytes.data[i]);
            fprintf(out,"\"\n"); break;
    }
}

static void emit_static_variable(AsmStaticVariable *var,FILE *out){
    if (var->global){
        fprintf(out,".globl " SYMBOL_PREFIX "%s\n",var->name);
    }

    int all_zero=1;
    for (int i=0;i<var->init.count;i++) if (var->init.items[i].type!=INIT_ZERO) all_zero=0;
    fprintf(out,"%s\n.balign %d\n" SYMBOL_PREFIX "%s:\n",all_zero ? ".bss" : ".data",var->alignment,var->name);
    for (int i=0;i<var->init.count;i++) emit_static_init(var->init.items[i],out);
    fprintf(out,"\n");
}

static void emit_static_constant(AsmStaticConstant *c,FILE *out){
#ifdef __APPLE__
    const char *section=c->init.type==INIT_STRING ? ".cstring" : c->alignment==16 ? ".literal16" : ".literal8";
    fprintf(out,"%s\n.balign %d\n.L%s:\n",section,c->alignment,c->name);
    emit_static_init(c->init,out);
    //the linker expects 16-byte-aligned literals to be 16 bytes long
    if (c->init.type==INIT_DOUBLE && c->alignment==16) fprintf(out,"    .quad 0\n");
#else
    fprintf(out,".section .rodata\n.balign %d\n.L%s:\n",c->alignment,c->name);
    emit_static_init(c->init,out);
#endif
    fprintf(out,"\n");
}

void emit_asm_new(AsmProg *asmp,FILE *out){
    if (!asmp || !out){
        return;
    }

    emit_symbols=asmp->symbols;

    for (int i=0;i<asmp->top_level_count;i++){
        AsmTopLevel *top_level=asmp->top_levels[i];
        switch (top_level->type){
            case ASM_TOP_FUNCTION:
                emit_function(asmp,top_level->function,out);
                break;
            case ASM_TOP_STATIC_VARIABLE:
                emit_static_variable(top_level->static_variable,out);
                break;
            case ASM_TOP_STATIC_CONSTANT:
                emit_static_constant(top_level->static_constant,out);
                break;
        }
    }

#ifndef __APPLE__
    fprintf(out,".section .note.GNU-stack,\"\",@progbits\n");
#endif
}

char *read_file(const char *filename){
    FILE *f=fopen(filename,"rb");
    if (!f){
        perror(filename);
        return NULL;
    }

    fseek(f,0,SEEK_END);
    long length=ftell(f);
    fseek(f,0,SEEK_SET);

    char *buffer=malloc(length+1);
    if (buffer){
        size_t bytes_read =fread(buffer,1,length,f);
        buffer[bytes_read ]='\0';
    }

    fclose(f);

    return buffer;
}

int main(int argc,char *argv[]){
    char *input_path=NULL;
    int stage=6;
    int s_flag=0;
    int c_flag=0;
    char libs[256]="";

    for(int i=1;i<argc;i++){
        if (strcmp(argv[i],"--lex")==0){
            stage=1;
            continue;
        }
        else if (strcmp(argv[i],"--parse")==0){
            stage=2;
            continue;
        }
        else if (strcmp(argv[i], "--validate") == 0) {
            stage = 3;
            continue;
        }
        else if (strcmp(argv[i],"--tacky")==0){
            stage=4;
            continue;
        }
        else if (strcmp(argv[i],"--codegen")==0){
            stage=5;
            continue;
        }
        else if (strcmp(argv[i],"-S")==0){
            s_flag=1;
            continue;
        }
        else if (strcmp(argv[i],"-c")==0){
            c_flag=1;
            continue;
        }
        else if (strncmp(argv[i],"-l",2)==0){
            if (strlen(libs)+strlen(argv[i])+2 > sizeof(libs)){
                fprintf(stderr,"Error: too many libraries\n");
                return 1;
            }
            strcat(libs," ");
            strcat(libs,argv[i]);
            continue;
        }
        else if (argv[i][0]!='-'){
            input_path =argv[i];
        }
    }

    if (!input_path ){
        fprintf(stderr, "Usage: %s <input_file> [--lex]\n", argv[0]);
        return 1;
    }

    
    char *input=read_file(input_path);
    if (!input){
        fprintf(stderr, "Error: Could not read file %s\n", input_path);
        return 1;
    }

    //lex
    lex(input);
    if (stage==1){
        return 0;
    }

    //parse
    Program *prog = parse_program();
    if (stage==2){
        return 0;
    }

    //semantic analysis

    //identifer resolution
    resolve_program(prog);

    //type checking
    Symbol *symbols=typecheck_program(prog);
    collect_program_switch_cases(prog);

    if (stage==3){ //--validate
        return 0;
    }

    //TACKY Generation
    TackyProgram *tacky_prog=generate_tacky(prog,&symbols);
    if (stage==4){
        return 0;
    }

    //codegen
    AsmProg *asmp=tacky_to_asm(tacky_prog,symbols);
    fix_and_allocate(asmp);
    if (stage==5){
        return 0;
    }


    //emit Asm to .s file
    char asm_path[256];
    strncpy(asm_path, input_path, sizeof(asm_path) - 1);
    asm_path[sizeof(asm_path) - 1] = '\0';
    char *dot = strrchr(asm_path, '.');
    if (dot) strcpy(dot, ".s"); else strcat(asm_path, ".s");

    FILE *out=fopen(asm_path,"w");
    if (!out) {perror("faile to open asm file"); return 1;}
    emit_asm_new(asmp,out);
    fclose(out);

    
    if (stage < 5){
        remove(asm_path);
        return 0;
    }

    if (s_flag){
        return 0;
    }

    char cmd[1024];

    if (c_flag){
        char obj_path[256];
        strncpy(obj_path, input_path, sizeof(obj_path) - 1);
        obj_path[sizeof(obj_path) - 1] = '\0';

        dot = strrchr(obj_path, '.');
        if (dot){
            strcpy(dot, ".o");
        }else{
            strcat(obj_path, ".o");
        }

        snprintf(
            cmd,
            sizeof(cmd),
            "gcc -c %s -o %s",
            asm_path,
            obj_path
        );

        int res=system(cmd);
        remove(asm_path);
        return res==0 ? 0 : 1;
    }

    char out_path[256];
    strncpy(out_path, input_path, sizeof(out_path) - 1);
    out_path[sizeof(out_path) - 1] = '\0';
    dot = strrchr(out_path, '.');
    if (dot) {
        *dot = '\0';
    }

    snprintf(cmd, sizeof(cmd), "gcc %s -o %s%s", asm_path, out_path, libs);
    int res = system(cmd);

    remove(asm_path);

    return res==0 ? 0 : 1;
}
