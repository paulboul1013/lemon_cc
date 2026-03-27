#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//define Token type
typedef  enum{
    TOK_INT, TOK_VOID, TOK_RETURN,

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

    TOK_IDENTIFIER, TOK_CONSTANT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMICOLON, 
    
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
    EXP_CONSTANT,
    EXP_UNARY,
    EXP_BINARY,
    EXP_VAR,
    EXP_ASSIGN,
    EXP_COMPOUND_ASSIGN, // += -= *= /= %=
    EXP_PREFIX_OP, //++a,--a
    EXP_POSTFIX_OP, //a++,a--
    EXP_CONDITIONAL,
}ExpType;

typedef enum {
    BINARY_ADD, BINARY_SUB, BINARY_MULT, BINARY_DIV, BINARY_REM,
    BINARY_LSHIFT, BINARY_RSHIFT,
    BINARY_BITWISE_AND, BINARY_BITWISE_OR, BINARY_BITWISE_XOR, // bit operation
    BINARY_LOGICAL_AND, BINARY_LOGICAL_OR, // logical operation
    BINARY_EQ, BINARY_NEQ, BINARY_LT, BINARY_LTE, BINARY_GT, BINARY_GTE,

}BinaryOpType;

typedef enum {
    UNARY_NEGATION, // -
    UNARY_COMPLEMENT, // ~
    UNARY_NOT // !
}UnaryOpType;

typedef struct Exp {
    ExpType type;
    union {
        int int_value; //for EXP_CONSTANT
        char *var_name; //for EXP_VAR
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
    };
} Exp;

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

typedef struct {
    char *name;
    Exp *init; //optional
} Declaration;

typedef enum {
    FOR_INIT_DECL,
    FOR_INIT_EXP
}ForInitType;

typedef struct {
    ForInitType type;
    Declaration *decl;// when type is FOR_INIT_DECL
    Exp *exp;// when type is FOR_INIT_EXP , NULL means empty
} ForInit;

typedef enum {
    BI_STMT,
    BI_DECL
} BlockItemType;

typedef struct SwitchCaseInfo{
    int value;
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
        int value;      // semantic pass calculate constant value
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

typedef struct {
    char *name; 
    BlockItem **body; 
    int body_count;
} Function;

typedef struct {Function *fn;} Program;




//Exp helper function
//build constant node
Exp *make_int_exp(int i){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_CONSTANT;
    e->int_value=i;
    return e;
}

//build unary node
Exp *make_unary_exp(UnaryOpType op,Exp *inner){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_UNARY;
    e->unary.op=op;
    e->unary.exp=inner;
    return e;
}

Exp *make_binary_exp(BinaryOpType op,Exp *left,Exp *right){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_BINARY;
    e->binary.op=op;
    e->binary.left=left;
    e->binary.right=right;
    return e;
}

//var AST node
Exp *make_var_exp(char *name){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_VAR;
    e->var_name=strdup(name);
    return e;
}

Exp *make_assign_exp(Exp *left,Exp *right){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_ASSIGN;
    e->assign.lvalue=left;
    e->assign.rvalue=right;
    return e;
}

Exp *make_compound_assign_exp(BinaryOpType op,Exp *left,Exp *right){
    Exp *e=malloc(sizeof(Exp));
    e->type=EXP_COMPOUND_ASSIGN;
    e->compound.op=op;
    e->compound.lvalue=left;
    e->compound.rvalue=right;
    return e;
}

Exp *make_inc_dec_exp(ExpType type,int is_increment,Exp *lvalue){
    Exp *e=malloc(sizeof(Exp));
    e->type=type;
    e->inc_dec.lvalue=lvalue;
    e->inc_dec.is_increment=is_increment;
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
    COND_A,COND_AE,COND_B,COND_BE
} CondCode;

//Asm AST Node
typedef enum {
    AS_MOV,
    AS_NEG,
    AS_NOT,
    AS_RET,
    AS_ALLOCATE_STACK,
    AS_BINARY, //for add,sub,mult
    AS_IDIV, //div and get remainder
    AS_CDQ, //sign extend EAX to EDX
    AS_CMP, //cmpl
    AS_JMP, // jmp
    AS_JMP_CC, // setCC
    AS_SET_CC, // label:
    AS_LABEL
} AsmInstType;



typedef enum {
    AS_IMM, //immediate value
    AS_REG, //  register like %eax,%r10d
    AS_PSEUDO, //virutal register (tmp.0)
    AS_STACK // stack address -4(%rbp)
} AsmOpType;

typedef enum{
    REG_EAX,
    REG_R10D, //for mem to mem source fix
    REG_EDX, //for idiv
    REG_R11D, //for imul fix
    REG_ECX //for shl,sar
} AsmReg;

typedef struct {
    AsmOpType type;
    union {
        int imm;
        AsmReg reg;
        char *pseudo;
        int stack_offset;
    };
} AsmOperand;

typedef struct AsmInst {
    AsmInstType type;
    BinaryOpType binary_op;
    CondCode cond;
    AsmOperand src;
    AsmOperand dst;
    char *label; //for JMP,JmpCC, LABEL
    struct AsmInst *next;
} AsmInst;

typedef struct {
    char *name;
    AsmInst *instructions;
} AsmFunc;

typedef struct {
    AsmFunc *fn;
} AsmProg;


//TACKY IR ddefine
typedef enum {
    TACKY_VAL_CONSTANT,
    TACKY_VAL_VAR
} TackyValType;

typedef struct {
    TackyValType type;
    union {
        int int_value;
        char *var_name;
    };
} TackyVal;

typedef enum {
    TACKY_INST_RETURN,
    TACKY_INST_UNARY,
    TACKY_INST_BINARY,
    TACKY_INST_COPY,
    TACKY_INST_JUMP,
    TACKY_INST_JZ,
    TACKY_INST_JNZ,
    TACKY_INST_LABEL
} TackyInstType;

typedef struct TackyInstruction{
    TackyInstType type;
    UnaryOpType unary_op;
    BinaryOpType binary_op;
    TackyVal *src;
    TackyVal *src2;
    TackyVal *dst;
    char *label_name;
    struct TackyInstruction *next;
} TackyInstruction;

typedef struct {
    char *function_name;
    TackyInstruction *instructions;
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

TackyVal *tacky_val_constant(int val){
    TackyVal *v=malloc(sizeof(TackyVal));
    v->type=TACKY_VAL_CONSTANT;
    v->int_value=val;
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
    if (strcmp(s,"int")==0) return TOK_INT;
    if (strcmp(s,"void")==0) return TOK_VOID;
    if (strcmp(s,"return")==0) return TOK_RETURN;

    if (strcmp(s,"if")==0) return TOK_IF;
    if (strcmp(s,"else")==0) return TOK_ELSE;
    if (strcmp(s,"goto")==0) return TOK_GOTO;

    if (strcmp(s,"do")==0) return TOK_DO;
    if (strcmp(s,"while")==0) return TOK_WHILE;
    if (strcmp(s,"for")==0) return TOK_FOR;
    if (strcmp(s,"break")==0) return TOK_BREAK;
    if (strcmp(s,"continue")==0) return TOK_CONTINUE;

    if (strcmp(s,"switch")) return TOK_SWITCH;
    if (strcmp(s,"case")) return TOK_CASE;
    if (strcmp(s,"default")) return TOK_DEFAULT;

    return TOK_IDENTIFIER;
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
        if (isdigit(*p)){
            const char *start=p;
            while (isdigit(*p)) p++;

            //123abc can't pass，need to 123_abc will pass
            if (isalpha(*p) || *p=='_'){
                fprintf(stderr, "Lexer Error: Invalid identifier starting with digit at '%c'\n", *p);
                exit(1);
            }

            int len=p-start;
            char *buf=malloc(len+1);
            strncpy(buf,start,len);
            buf[len]='\0';
            add_token(TOK_CONSTANT,buf);
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
            case '{': 
                add_token(TOK_LBRACE,NULL);
                break;
            case '}': 
                add_token(TOK_RBRACE,NULL);
                break;
            case ';':
                add_token(TOK_SEMICOLON,NULL);
                break;
            case '?':
                add_token(TOK_QUESTION,NULL);
                p++;
                break;
            case ':':
                add_token(TOK_COLON,NULL);
                p++;
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
Exp *parse_factor();
Statement *parse_statement();
Declaration *parse_declaration();

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
    int from_current_block;
    struct IdentifierMap *next;
} IdentifierMap;

void resolve_for_init(ForInit *init,IdentifierMap **map);
void label_statement(Statement *stmt,char *current_break_label,char *current_continue_label);
void label_block_items(BlockItem **items,int count,char *current_break_label,char *current_continue_label);
void resolve_declaration(Declaration *decl,IdentifierMap **map);
void resolve_expression(Exp *e,IdentifierMap *map);

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
        resolve_declaration(init->decl,map);
    }else{
        if(init->exp){
            resolve_expression(init->exp,*map);
        }
    }
}

Statement *parse_block(){
    expect(TOK_LBRACE);
    
    Statement *s=malloc(sizeof(Statement));
    s->type=STMT_COMPOUND;

    s->block_items=malloc(sizeof(BlockItem*)*100);
    s->block_count=0;

    while(peek().type!=TOK_RBRACE){
        BlockItem *bi=malloc(sizeof(BlockItem));

        if (peek().type==TOK_INT){
            bi->type=BI_DECL;
            bi->decl=parse_declaration();
        }else{
            bi->type=BI_STMT;
            bi->stmt=parse_statement();
        }

        s->block_items[s->block_count++]=bi;
    }

    expect(TOK_RBRACE);
    return s;
}

Exp *parse_optional_exp(TokenType end_token){
    if (peek().type==end_token){
        return NULL;
    }
    return parse_exp(0);
}

ForInit *parse_for_init(){
    ForInit *init=malloc(sizeof(ForInit));

    if (peek().type==TOK_INT){
        init->type=FOR_INIT_DECL;
        init->decl=parse_declaration(); //remove after int ';'
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
   Exp *left=parse_factor();
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

            Exp *right=parse_exp(prec+1);

            Exp *node=malloc(sizeof(Exp));
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

//deal with number and parentheses and unary operator
//note:unary operator make sure higher precedence than binary operator
// <factor> ::= <constant>
//            | <identifier>
//            | "(" <expression> ")"
//            | <unary_op> <factor>
//            | <factor> "++"
//            | <factor> "--"
//            | "++" <factor>
//            | "--" <factor>
Exp *parse_factor(){
    Token t=peek();
    Exp *inner = NULL;

    if (t.type==TOK_CONSTANT){
        take();
        return make_int_exp(atoi(t.value));
    }
    // prefix ++ / --
    else if (t.type==TOK_INCREMENT || t.type==TOK_DECREMENT){
        take();
        int is_inc=(t.type==TOK_INCREMENT);
        Exp *operand=parse_factor(); //right-associative
        return make_inc_dec_exp(EXP_PREFIX_OP,is_inc,operand);
    }
    else if (t.type==TOK_IDENTIFIER){
        take();
        inner=make_var_exp(t.value);
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

        Exp *inner=parse_factor(); //unary after factor
        return make_unary_exp(op,inner);
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
    while (next.type==TOK_INCREMENT || next.type==TOK_DECREMENT){
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
        return parse_while_statement();
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
        s->exp=parse_exp(0);
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

// <declaration> ::= "int" <identifier> [ "=" <expression> ] ";"
Declaration *parse_declaration(){
    expect(TOK_INT);
    Token id=take(); //get variable name

    if (id.type!=TOK_IDENTIFIER){
        fprintf(stderr, "Parse Error: Expected identifier as variable name, but got token type %d\n", id.type);
        exit(1); // 必須退出，這樣測試腳本才能捕捉到失敗
    }

    Declaration *d=malloc(sizeof(Declaration));
    d->name=strdup(id.value);

    if (peek().type==TOK_ASSIGN){
        take();
        d->init=parse_exp(0);
    }else{
        d->init=NULL;
    }
    expect(TOK_SEMICOLON);
    return d;
}

// <function> ::= "int" <identifier> "(" "void" ")" "{" { <block_item> } "}"
//
// <block_item> ::= <declaration> | <statement>
Function *parse_function() {
    expect(TOK_INT);
    Token id=take();
    if (id.type!=TOK_IDENTIFIER) {
        fprintf(stderr, "Parse Error: Expected function name\n");
        exit(1); 
    }
    expect(TOK_LPAREN);
    expect(TOK_VOID);
    expect(TOK_RPAREN);
    expect(TOK_LBRACE);
    Function *f=malloc(sizeof(Function));
    f->name=strdup(id.value);
    f->body=malloc(sizeof(BlockItem*)*100);
    f->body_count=0;

    while(peek().type!=TOK_RBRACE){
        BlockItem *bi=malloc(sizeof(BlockItem));
        if (peek().type==TOK_INT){
            bi->type=BI_DECL;
            bi->decl=parse_declaration();
        }else{
            bi->type=BI_STMT;
            bi->stmt=parse_statement();
        }
        f->body[f->body_count++]=bi;
    }
    
    expect(TOK_RBRACE);
    return f;
}

// <program> ::= <function> EOF
Program *parse_program() {
    Program *prog=malloc(sizeof(Program));
    prog->fn=parse_function();
    if (peek().type!=TOK_EOF) exit(1); //make sure not extra tokens
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

//find vairable in  the mapping table 
char *map_get(IdentifierMap *map,const char *name){
    while(map){
        if (strcmp(map->user_name,name)==0){
            return map->unique_name;
        }
        map=map->next;
    }
    return NULL;
}

IdentifierMap *copy_identifier_map(IdentifierMap *map){
    IdentifierMap *new_head=NULL;
    IdentifierMap **tail=&new_head;

    while(map){
        IdentifierMap *node=malloc(sizeof(IdentifierMap));
        node->user_name=strdup(map->user_name);
        node->unique_name=strdup(map->unique_name);
        node->from_current_block=0;
        node->next=NULL;

        *tail=node;
        tail=&node->next;
        map=map->next;
    }

    return new_head;
}

void check_goto_statement(Statement *stmt,LabelMap *labels,IdentifierMap *vars){
    if (stmt->type==STMT_GOTO){
        //check label exist
        if (!label_exists(labels,stmt->label)){
            fprintf(stderr, "Semantic Error: goto to undefined label '%s'\n", stmt->label);
            exit(1);
        }

    }

    else if (stmt->type==STMT_LABEL){
        check_goto_statement(stmt->if_stmt.then_branch,labels,vars);
    }

    else if (stmt->type==STMT_IF){
        check_goto_statement(stmt->if_stmt.then_branch,labels,vars);

        if (stmt->if_stmt.else_branch){
            check_goto_statement(stmt->if_stmt.else_branch,labels,vars);
        }
    }

    else if (stmt->type==STMT_COMPOUND){
        for(int i=0;i<stmt->block_count;i++){
            BlockItem *bi=stmt->block_items[i];
            if (bi->type==BI_STMT){
                check_goto_statement(bi->stmt,labels,vars);
            }
        }
    }

    else if (stmt->type==STMT_WHILE){
        check_goto_statement(stmt->while_stmt.body,labels,vars);
    }
    else if (stmt->type==STMT_DO_WHILE){
        check_goto_statement(stmt->do_while_stmt.body,labels,vars);
    }
    else if (stmt->type==STMT_FOR){
        check_goto_statement(stmt->for_stmt.body,labels,vars);
    }
    else if (stmt->type==STMT_SWITCH){
        check_goto_statement(stmt->switch_stmt.body,labels,vars);
    }
    else if (stmt->type==STMT_CASE){
        check_goto_statement(stmt->case_stmt.body,labels,vars);
    }
    else if (stmt->type==STMT_DEFAULT){
        check_goto_statement(stmt->default_stmt.body,labels,vars);
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

        case EXP_CONSTANT:
            break;

        case EXP_COMPOUND_ASSIGN:
            if (e->compound.lvalue->type!=EXP_VAR){
                fprintf(stderr, "Semantic Error: Invalid lvalue in compound assignment\n");
                exit(1);
            }
            resolve_expression(e->compound.lvalue,map);
            resolve_expression(e->compound.rvalue,map);
            break;
        case EXP_PREFIX_OP:
        case EXP_POSTFIX_OP:
            if (e->inc_dec.lvalue->type!=EXP_VAR){
                fprintf(stderr, "Semantic Error: Invalid lvalue in increment/decrement\n");
                exit(1);
            }
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

        case EXP_ASSIGN:
            //lvalue check
            if (e->assign.lvalue->type!=EXP_VAR){
                fprintf(stderr, "Semantic Error: Invalid lvalue in assignment\n");
                exit(1);
            }
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
    }
}

void resolve_declaration(Declaration *decl,IdentifierMap **map){

    //same name is illegal only if an existing declaration is from current block 
    IdentifierMap *curr=*map;
    while(curr){
        if (strcmp(curr->user_name,decl->name)==0){
            if (curr->from_current_block){
                fprintf(stderr,"Semantic Error: Duplicate declaration of variable '%s'\n",decl->name);
                exit(1);
            }
            break;
        }
        curr=curr->next;
    }
    

    char *unique=make_temporary(); // make only name

    //let new mapping adding link list
    IdentifierMap *new_entry=malloc(sizeof(IdentifierMap));
    new_entry->user_name=strdup(decl->name);
    new_entry->unique_name=strdup(unique);
    new_entry->from_current_block=1;
    new_entry->next=*map;
    *map=new_entry;


    //initializer can see the newly declared variable itself
    if (decl->init){
        resolve_expression(decl->init,*map);
    }

    //make AST node name to unique name
    free(decl->name);
    decl->name=strdup(unique);
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
        resolve_statement(stmt->switch_stmt.body,map);
    }
    else if (stmt->type==STMT_CASE){
        resolve_statement(stmt->case_stmt.body,map);
    }
    else if (stmt->type==STMT_DEFAULT){
        resolve_statement(stmt->default_stmt.body,map);
    }

    //don't need deal with STMT_NULL
}

void resolve_block_items(BlockItem **items,int count,IdentifierMap **map){
    for (int i=0;i<count;i++){
        BlockItem *bi=items[i];
        if (bi->type==BI_DECL){
            resolve_declaration(bi->decl,map);
        }else{
            resolve_statement(bi->stmt,*map);
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

//collect case/default 
static int eval_const_expr(Exp *e,int *out){
    if (!e) {
        return 0;
    }

    switch(e->type){
        case EXP_CONSTANT:
            *out=e->int_value;
            return 1;

        case EXP_UNARY: {
            int v;
            if (!eval_const_expr(e->unary.exp,&v)) return 0;

            switch (e->unary.op){
                case UNARY_NEGATION: *out=-v; return 1;
                case UNARY_COMPLEMENT: *out=~v; return 1;
                case UNARY_NOT: *out=!v; return 1;
            }
            
            return 0;
        }

        case EXP_BINARY:{
            int l,r;
            if (!eval_const_expr(e->binary.left,&l)) return 0;
            if (!eval_const_expr(e->binary.right,&r)) return 0;

            switch (e->binary.op){
                case BINARY_ADD : *out=l+r; return 1;
                case BINARY_SUB: *out=l-r; return 1;
                case BINARY_MULT: *out=l*r; return 1;
                case BINARY_DIV: if (r==0) return 0; *out=l/r; return 1;
                case BINARY_REM: if (r==0) return 0; *out=l%r; return 1;

                case BINARY_LSHIFT: *out=l<<r; return 1;
                case BINARY_RSHIFT: *out=l>>r; return 1;
                
                case BINARY_BITWISE_AND: *out=l&r; return 1;
                case BINARY_BITWISE_OR: *out=l|r; return 1;
                case BINARY_BITWISE_XOR: *out=l^r; return 1;

                case BINARY_LOGICAL_AND: *out=(l&&r); return 1;
                case BINARY_LOGICAL_OR: *out=(l||r); return 1;

                case BINARY_EQ: *out=(l==r); return 1;
                case BINARY_NEQ: *out=(l!=r); return 1;
                case BINARY_LT: *out=(l<r); return 1;
                case BINARY_LTE: *out = (l <= r); return 1;
                case BINARY_GT: *out = (l > r); return 1;
                case BINARY_GTE: *out = (l >= r); return 1;
            }

            return 0;
        }

        case EXP_CONDITIONAL:{
            int cond;
            if (!eval_const_expr(e->conditional.condition,&cond)) return 0;
            if (cond) return eval_const_expr(e->conditional.true_expr,out);
            return eval_const_expr(e->conditional.false_expr,out);
        }

        default:
            return 0;
    }
}

void resolve_program(Program *prog){
    IdentifierMap *map=NULL;
    LabelMap *labels=NULL;

    // pass1 :collect labels
    for(int i=0;i<prog->fn->body_count;i++){
        BlockItem *bi=prog->fn->body[i];

        if (bi->type==BI_STMT){
            check_labels_statement(bi->stmt,&labels);
        }
    }
        
    // pass2 : resolve function body as one top-level block
    resolve_block_items(prog->fn->body,prog->fn->body_count,&map);

    // pass3 : check goto
    for(int i=0;i<prog->fn->body_count;i++){
        BlockItem *bi=prog->fn->body[i];
        if (bi->type==BI_STMT){
            check_goto_statement(bi->stmt,labels,map);
        }
    }

    //pass 4 : label goto
    label_block_items(prog->fn->body, prog->fn->body_count, NULL,NULL);
}

// //assemble generation
// AsmProgram *generate_asm(Program *prog){
//     AsmProgram *asm_prog=malloc(sizeof(AsmProgram));
//     asm_prog->fn=malloc(sizeof(AsmFunction));
//     asm_prog->fn->name=strdup(prog->fn->name);

//     //one return statement to mov and ret two instructions
//     asm_prog->fn->inst_count=2;
//     asm_prog->fn->instructions=malloc(sizeof(AsmInstruction)*2);

//     asm_prog->fn->instructions[0].type=ASM_MOV;

//     if (prog->fn->body->exp->type==EXP_CONSTANT){
//         asm_prog->fn->instructions[0].imm=prog->fn->body->exp->int_value;

//     }else{
//         asm_prog->fn->instructions[0].imm=0;
//     }


//     asm_prog->fn->instructions[1].type=ASM_RET;

//     return asm_prog;
// }

//support build operator
AsmOperand as_imm(int i){
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

AsmOperand convert_tacky_val(TackyVal *v){
    if (v->type==TACKY_VAL_CONSTANT){
        return as_imm(v->int_value);
    }

    return as_pseudo(v->var_name);
}

//helper function: let TACKY  operator  turn it to Asm condition code
CondCode binary_to_cond(BinaryOpType op){
    switch(op){
        case BINARY_EQ: return COND_E;
        case BINARY_NEQ: return COND_NE;
        case BINARY_LT: return COND_L;
        case BINARY_LTE: return COND_LE;
        case BINARY_GT:  return COND_G;
        case BINARY_GTE: return COND_GE;
        default: exit(1);

    }
}

void append_asm(AsmInst **head,AsmInstType type,BinaryOpType op,CondCode cond,AsmOperand src,AsmOperand dst,char *label){
    AsmInst *new_inst= calloc(1, sizeof(AsmInst));
    new_inst->type=type;
    new_inst->binary_op=op;
    new_inst->cond=cond;
    new_inst->src=src;
    new_inst->dst=dst;
    new_inst->label=label ? strdup(label):NULL;
    new_inst->next=NULL;

    if (*head==NULL){
        *head=new_inst;
    }else{
        AsmInst *curr=*head;
        while (curr->next!=NULL){
            curr=curr->next;
        }
        curr->next=new_inst;
    }
    
}

AsmProg *tacky_to_asm(TackyProgram *tacky){
    AsmProg *asmp=malloc(sizeof(AsmProg));
    asmp->fn=calloc(1, sizeof(AsmFunc));
    asmp->fn->name=strdup(tacky->function_name);
    AsmInst **head=&asmp->fn->instructions;
    

    TackyInstruction *curr=tacky->instructions;
    while (curr!=NULL){

        switch (curr->type){
            case TACKY_INST_RETURN:
                append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),as_reg(REG_EAX),NULL);
                append_asm(head,AS_RET,0,0,(AsmOperand){0},(AsmOperand){0},NULL);
                break;

            case TACKY_INST_COPY:
                append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                break;

            case TACKY_INST_JUMP:
                append_asm(head,AS_JMP,0,0,(AsmOperand){0},(AsmOperand){0},curr->label_name);
                break;

            case TACKY_INST_LABEL:
                append_asm(head,AS_LABEL,0,0,(AsmOperand){0},(AsmOperand){0},curr->label_name);
                break;
                
            case TACKY_INST_JZ: //jumpifzero: cmp $0,src; je label
                append_asm(head,AS_CMP,0,0,as_imm(0),convert_tacky_val(curr->src),NULL);
                append_asm(head,AS_JMP_CC,0,COND_E,(AsmOperand){0},(AsmOperand){0},curr->label_name);
                break;

            case TACKY_INST_JNZ: //jumpifnotzero: cmp $0, src, jne label
                append_asm(head,AS_CMP,0,0,as_imm(0),convert_tacky_val(curr->src),NULL);
                append_asm(head,AS_JMP_CC,0,COND_NE,(AsmOperand){0},(AsmOperand){0},curr->label_name);
                break;
            

            case TACKY_INST_UNARY:{
                if (curr->unary_op==UNARY_NOT){ //!x convert to cmp $0; mov $0 ,dst ,sete dst
                    append_asm(head,AS_CMP,0,0,as_imm(0),convert_tacky_val(curr->src),NULL);
                    append_asm(head,AS_MOV,0,0,as_imm(0),convert_tacky_val(curr->dst),NULL);
                    append_asm(head,AS_SET_CC,0,COND_E,(AsmOperand){0},convert_tacky_val(curr->dst),NULL);

                }else{
                    append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                    AsmInstType type=(curr->unary_op==UNARY_NEGATION)?AS_NEG:AS_NOT;
                    append_asm(head,type,0,0,(AsmOperand){0},convert_tacky_val(curr->dst),NULL);
                }
            }

                break;


            case TACKY_INST_BINARY:{
                if (curr->binary_op==BINARY_DIV || curr->binary_op==BINARY_REM) {
                    //1. move src1,eax
                    //2. cdq
                    //3. idiv src2
                    //4. mov eax/edx ,dst
                    append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),as_reg(REG_EAX),NULL);
                    append_asm(head,AS_CDQ,0,0,(AsmOperand){0},(AsmOperand){0},NULL);
                    append_asm(head,AS_IDIV,0,0,convert_tacky_val(curr->src2),(AsmOperand){0},NULL);

                    AsmReg res_reg=(curr->binary_op==BINARY_DIV) ? REG_EAX: REG_EDX;
                    append_asm(head,AS_MOV,0,0,as_reg(res_reg),convert_tacky_val(curr->dst),NULL);
                }
                // deal with shift (<<,>>)
                //x86 rule: shift offset must in the %cl(it's mean in the %ecx)
                else if (curr->binary_op==BINARY_LSHIFT || curr->binary_op==BINARY_RSHIFT){
                    
                    if (curr->src2->type==TACKY_VAL_CONSTANT){//if offset is constant
                        append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                        append_asm(head, AS_BINARY, curr->binary_op, 0, convert_tacky_val(curr->src2), convert_tacky_val(curr->dst), NULL);
                    }else{//if offset is variable
                        // mov scr2 to ecx
                        append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src2),as_reg(REG_ECX),NULL);
                        // mov src1 to dst
                        append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                        //execute shift ，src use REG_ECX marked
                        append_asm(head,AS_BINARY,curr->binary_op,0,as_reg(REG_ECX),convert_tacky_val(curr->dst),NULL);
                    }
                  
                }
                else if (curr->binary_op >= BINARY_EQ && curr->binary_op <= BINARY_GTE){
                    // logic operator : cmp src2 ,src1 ,mov $0 ,dst ,setCC dst
                    append_asm(head,AS_CMP,0,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->src),NULL);
                    append_asm(head,AS_MOV,0,0,as_imm(0),convert_tacky_val(curr->dst),NULL);
                    append_asm(head,AS_SET_CC,0,binary_to_cond(curr->binary_op),(AsmOperand){0},convert_tacky_val(curr->dst),NULL);
                }
                else{
                    // op: add , sub , mult
                    // mov src1 ,dst
                    // op src2, dst
                    append_asm(head,AS_MOV,0,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst),NULL);
                    append_asm(head,AS_BINARY,curr->binary_op,0,convert_tacky_val(curr->src2),convert_tacky_val(curr->dst),NULL);
                }
            }
            break;

        }

        curr=curr->next;
    }

    return asmp;
}

//stack allocation and instruction fix
typedef struct {
    char *name;
    int offset;
} MapEntry;

void fix_and_allocate(AsmProg *asmp){
    MapEntry map[1000];
    int map_size=0;
    int current_stack=-4;
    
    AsmInst *curr=asmp->fn->instructions;
    while (curr){
        AsmOperand *ops[2]={
            &curr->src,
            &curr->dst
        };

        for(int i=0;i<2;i++){
            if (ops[i]->type==AS_PSEUDO){
                int found_offset=0;
                int already_exists=0;
                
                //check is it in the map
                for(int j=0;j<map_size;j++){
                    if (strcmp(map[j].name,ops[i]->pseudo)==0){
                        found_offset=map[j].offset;
                        already_exists=1;
                        break;
                    }
                }

                //if is new variable, add map
                if (!already_exists){
                    map[map_size].name=strdup(ops[i]->pseudo);
                    map[map_size].offset=current_stack;
                    found_offset=current_stack;
                    current_stack-=4;
                    map_size++;
                }

                //convert stack operator 
                ops[i]->type=AS_STACK;
                ops[i]->stack_offset=found_offset;
               
            }
        }
        curr=curr->next;
    }

    //cal total needed stack space
    int total_stack_size=-(current_stack+4);

    //in the begining insert allocate stack instruction
    if (total_stack_size>0){
        AsmInst *allocate=calloc(1, sizeof(AsmInst)); 
        allocate->type=AS_ALLOCATE_STACK;
        allocate->src=as_imm(total_stack_size);
        allocate->next=asmp->fn->instructions;
        asmp->fn->instructions=allocate;
    }


    //fix up
    curr=asmp->fn->instructions;
    while (curr){
        //x64 limit: mov instruction source and destination can't be memory address
        if ((curr->type == AS_MOV || (curr->type == AS_BINARY && curr->binary_op != BINARY_MULT)) 
            && curr->src.type == AS_STACK && curr->dst.type == AS_STACK){

            
            AsmInst *new_op=malloc(sizeof(AsmInst));
            new_op->type=curr->type;
            new_op->binary_op=curr->binary_op;
            new_op->src=as_reg(REG_R10D);
            new_op->dst=curr->dst;
            new_op->next=curr->next;


            curr->type=AS_MOV;
            curr->dst=as_reg(REG_R10D);
            curr->next=new_op;
            curr=new_op;
            
            
        }
        // fix cmp instruction
        // 1. can't cmpl mem,mem
        // 2. second operator can't be imm: cmpl reg/mem imm (x64 is cmpl src,dst)
        else if (curr->type==AS_CMP){
            // both src ,dst is mem
            if (curr->src.type==AS_STACK && curr->dst.type==AS_STACK){
                AsmInst *new_cmp=calloc(1,sizeof(AsmInst));
                new_cmp->type=AS_CMP;
                new_cmp->src=as_reg(REG_R10D);
                new_cmp->dst=curr->dst;
                new_cmp->next=curr->next;
            
                curr->type=AS_MOV;
                curr->dst=as_reg(REG_R10D);
                curr->next=new_cmp;
                curr=new_cmp;
            }
            // cmp %eax, $5
            else if (curr->dst.type==AS_IMM){
                AsmOperand old_src=curr->src;
                AsmOperand old_dst=curr->dst;
                
                AsmInst *new_cmp=calloc(1,sizeof(AsmInst));
                new_cmp->type=AS_CMP;
                new_cmp->src=old_src;
                new_cmp->dst=as_reg(REG_R11D);
                new_cmp->next=curr->next;

                curr->type=AS_MOV;
                curr->src=old_dst;
                curr->dst=as_reg(REG_R11D);
                curr->next=new_cmp;
                curr=new_cmp;
            }
        }
        // idiv can't use immediate value 
        else if (curr->type==AS_IDIV && curr->src.type==AS_IMM){
            AsmInst *new_idiv=malloc(sizeof(AsmInst));
            new_idiv->type=AS_IDIV;
            new_idiv->src=as_reg(REG_R10D);
            new_idiv->next=curr->next;
            
            curr->type=AS_MOV;
            curr->dst=as_reg(REG_R10D);
            curr->next=new_idiv;
            curr=new_idiv;
        }
        else if (curr->type==AS_BINARY && curr->binary_op==BINARY_MULT && curr->dst.type==AS_STACK){
            //imull $3, -4(%rbp) mult constant with memory address content
           
            //movl -4(%rbp), %r11d
            //imull $3, %r11d
            //movl %r11d, -4(%rbp)

            //movl -4(%rbp), %r11d
            AsmInst *mov_back=malloc(sizeof(AsmInst));
            mov_back->type=AS_MOV;
            mov_back->src=as_reg(REG_R11D);
            mov_back->dst=curr->dst; //store back origin stack space
            mov_back->next=curr->next;

            //imull $3, %r11d
            AsmInst *imul_op=malloc(sizeof(AsmInst));
            imul_op->type=AS_BINARY;
            imul_op->binary_op=BINARY_MULT;
            imul_op->src=curr->src;
            imul_op->dst=as_reg(REG_R11D);
            imul_op->next=mov_back;

            //movl %r11d, -4(%rbp)
            AsmOperand original_dst=curr->dst;
            curr->type=AS_MOV;
            curr->src=original_dst;
            curr->dst=as_reg(REG_R11D);
            curr->next=imul_op;


            curr=mov_back;
        }
        curr=curr->next;
    }
}

//Tacky generation
void gen_tacky_block_items(BlockItem **items,int count,TackyInstruction **inst_list);
void gen_tacky_decl(Declaration *decl,TackyInstruction **inst_list);
void gen_tacky_statement(Statement *stmt,TackyInstruction **inst_list);
static char *make_loop_target(const char *kind,const char *loop_id);
static void emit_tacky_label(TackyInstruction **inst_list,const char *label);
static void emit_tacky_jump(TackyInstruction **inst_list,const char *label);
static void emit_tacky_jz(TackyInstruction **inst_list,TackyVal *cond,const char *label);
static void emit_tacky_jnz(TackyInstruction **inst_list,TackyVal *cond,const char *label);
static void gen_tacky_for_init(ForInit *init,TackyInstruction **inst_list);


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
//   short-circuit logical operators (&&, ||)
TackyVal *gen_tacky_exp(Exp *e,TackyInstruction **inst_list){
    if (e->type==EXP_CONSTANT){
        return tacky_val_constant(e->int_value);
    }
    else if (e->type==EXP_CONDITIONAL){
        char *false_label=make_label("cond_false");  
        char *end_label=make_label("cond_end");
        TackyVal *dst=tacky_val_var(make_temporary());

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
        TackyInstruction *cp1=calloc(1,sizeof(TackyInstruction));
        cp1->type=TACKY_INST_COPY;
        cp1->src=v1;
        cp1->dst=dst;
        append_tacky_inst(inst_list,cp1);

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
        TackyInstruction *cp2=calloc(1,sizeof(TackyInstruction));
        cp2->type=TACKY_INST_COPY;
        cp2->src=v2;
        cp2->dst=dst;
        append_tacky_inst(inst_list,cp2);

        //7. label(end_label)
        TackyInstruction *l_end=calloc(1,sizeof(TackyInstruction));
        l_end->type=TACKY_INST_LABEL;
        l_end->label_name=strdup(end_label);
        append_tacky_inst(inst_list,l_end);

        return dst;

    }
    else if (e->type==EXP_COMPOUND_ASSIGN){
        TackyVal *rhs_val=gen_tacky_exp(e->compound.rvalue,inst_list);
        TackyVal *lhs_val=tacky_val_var(e->compound.lvalue->var_name);

        //calculate a+b
        TackyVal *tmp=tacky_val_var(make_temporary());
        TackyInstruction *bin=calloc(1,sizeof(TackyInstruction));
        bin->type=TACKY_INST_BINARY;
        bin->binary_op=e->compound.op;
        bin->src=lhs_val;
        bin->src2=rhs_val;
        bin->dst=tmp;
        append_tacky_inst(inst_list,bin);

        //store resultd into a
        TackyInstruction *cp=calloc(1,sizeof(TackyInstruction));
        cp->type=TACKY_INST_COPY;
        cp->src=tmp;
        cp->dst=lhs_val;
        append_tacky_inst(inst_list,cp);
        
        return lhs_val;
    }
    //  ++ / --
    else if (e->type==EXP_PREFIX_OP || e->type==EXP_POSTFIX_OP){
        TackyVal *lhs_val=tacky_val_var(e->inc_dec.lvalue->var_name);
        TackyVal *one_val=tacky_val_constant(1);
        BinaryOpType op=e->inc_dec.is_increment ? BINARY_ADD :BINARY_SUB;

        TackyVal *old_val=NULL;
        
        //if is a++ (postfix)，need to backup old value，because the expression want old value
        if (e->type==EXP_POSTFIX_OP){
            old_val=tacky_val_var(make_temporary());
            TackyInstruction *save_old=calloc(1,sizeof(TackyInstruction));
            save_old->type=TACKY_INST_COPY;
            save_old->src=lhs_val;
            save_old->dst=old_val;
            append_tacky_inst(inst_list,save_old);
        }

        // calculate a+1 or a-1
        TackyVal *tmp=tacky_val_var(make_temporary());
        TackyInstruction *bin=calloc(1,sizeof(TackyInstruction));
        bin->type=TACKY_INST_BINARY;
        bin->binary_op=op;
        bin->src=lhs_val;
        bin->src2=one_val;
        bin->dst=tmp;
        append_tacky_inst(inst_list,bin);

        //soter new value to a
        TackyInstruction *cp=calloc(1,sizeof(TackyInstruction));
        cp->type=TACKY_INST_COPY;
        cp->src=tmp;
        cp->dst=lhs_val;
        append_tacky_inst(inst_list,cp);

        return (e->type==EXP_POSTFIX_OP) ? old_val : tmp;
    }
    else if (e->type==EXP_VAR){
        return tacky_val_var(e->var_name);
    }
    else if (e->type==EXP_ASSIGN){
        //calculate right side value
        TackyVal *rhs_val=gen_tacky_exp(e->assign.rvalue,inst_list);
        //left side must be variable (already pass semantic anaylsis)
        TackyVal *lhs_val=tacky_val_var(e->assign.lvalue->var_name);

        TackyInstruction *inst=calloc(1,sizeof(TackyInstruction));
        inst->type=TACKY_INST_COPY;
        inst->src=rhs_val;
        inst->dst=lhs_val;
        inst->next=NULL;
        append_tacky_inst(inst_list,inst);
        
        return lhs_val;
    }
    else if (e->type==EXP_UNARY){
        //deal with inner expression first,get sorce operator
        TackyVal *src=gen_tacky_exp(e->unary.exp,inst_list);
        
        //build a temporal variable for dest
        char *t_name=make_temporary();
        TackyVal *dst=tacky_val_var(t_name);

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
            TackyVal *dst=tacky_val_var(make_temporary());

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
            cp1->src=tacky_val_constant(1);
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
            cp0->src=tacky_val_constant(0);
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
            TackyVal *dst=tacky_val_var(make_temporary());
            
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
            cp0->src=tacky_val_constant(0);
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
            cp1->src=tacky_val_constant(1);
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

        //build temporal variable to save result
        char *tmp=make_temporary();
        TackyVal *dst=tacky_val_var(tmp);

        //build and emit binary instruction
        TackyInstruction *inst=calloc(1, sizeof(TackyInstruction));
        inst->type=TACKY_INST_BINARY;
        inst->binary_op=e->binary.op;
        inst->src=v1;
        inst->src2=v2;
        inst->dst=dst;
        inst->next=NULL;
        append_tacky_inst(inst_list,inst);

        return dst;
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
        TackyVal *val=gen_tacky_exp(stmt->exp,inst_list);
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
}

// let Declaration turn to TACKY (only have init value need to generate)
void gen_tacky_decl(Declaration *decl,TackyInstruction **inst_list){
    if (decl->init){
        TackyVal *src=gen_tacky_exp(decl->init,inst_list);
        TackyInstruction *copy=calloc(1,sizeof(TackyInstruction));
        copy->type=TACKY_INST_COPY;
        copy->src=src;
        copy->dst=tacky_val_var(decl->name); // let init value copy to variable name
        append_tacky_inst(inst_list,copy);
    }
}

void gen_tacky_block_items(BlockItem **items,int count,TackyInstruction **inst_list){
    for (int i=0;i<count;i++){
        BlockItem *bi=items[i];
        if (bi->type==BI_STMT){
            gen_tacky_statement(bi->stmt,inst_list);
        }else{
            gen_tacky_decl(bi->decl,inst_list);
        }
    }
}

TackyProgram *generate_tacky(Program *prog){
    TackyProgram *t_prog=malloc(sizeof(TackyProgram));
    t_prog->function_name=strdup(prog->fn->name);
    t_prog->instructions=NULL;

    //travseral all BlockItem
    gen_tacky_block_items(prog->fn->body,prog->fn->body_count,&t_prog->instructions);

    //if functio have no return，add return 0
    TackyInstruction *final_inst=calloc(1,sizeof(TackyInstruction));
    final_inst->type=TACKY_INST_RETURN;
    final_inst->src=tacky_val_constant(0);
    append_tacky_inst(&t_prog->instructions,final_inst);

    return t_prog;
    
}

void emit_op(AsmOperand op,FILE *out){
    if (op.type==AS_IMM){
        fprintf(out,"$%d",op.imm);
    }
    else if (op.type==AS_REG){
        const char *reg_names[]={"%eax","%r10d","%edx","%r11d","%ecx"};
        fprintf(out,"%s",reg_names[op.reg]);
    }
    else if (op.type==AS_STACK){
        fprintf(out, "%d(%%rbp)", op.stack_offset);
    }
}

void emit_op_8bit(AsmOperand op,FILE *out){
    if (op.type==AS_REG){
        const char *reg_name_8[]={"%al","%r10b","%dl","%r11b","%cl"};
        fprintf(out,"%s",reg_name_8[op.reg]);
    }else if (op.type==AS_STACK){
        fprintf(out,"%d(%%rbp)",op.stack_offset);
    }
}

const char *cond_to_str(CondCode c){
    switch (c){
        case COND_E: return "e";
        case COND_NE: return "ne";
        case COND_G: return "g";
        case COND_GE: return "ge";
        case COND_L: return "l";
        case COND_LE: return "le";
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

void emit_asm_new(AsmProg * asmp,FILE *out){
    char *name = asmp->fn->name;
#ifdef __APPLE__
    fprintf(out, ".globl _%s\n_%s:\n", name, name);
#else
    fprintf(out, ".globl %s\n%s:\n", name, name);
#endif


    // Function Prologue
    fprintf(out, "    pushq   %%rbp\n");
    fprintf(out, "    movq    %%rsp, %%rbp\n");
    
    AsmInst *curr = asmp->fn->instructions;
    while (curr) {
        switch (curr->type) {
            case AS_ALLOCATE_STACK: fprintf(out, "    subq    $%d, %%rsp\n", curr->src.imm); break;
            case AS_MOV: 
                fprintf(out, "    movl    ");
                emit_op(curr->src, out); fprintf(out, ", "); emit_op(curr->dst, out);
                fprintf(out, "\n"); break;
            case AS_NEG: fprintf(out, "    negl    "); emit_op(curr->dst, out); fprintf(out, "\n"); break;
            case AS_NOT: fprintf(out, "    notl    "); emit_op(curr->dst, out); fprintf(out, "\n"); break;
            case AS_RET:
                //Function Epilogue
                fprintf(out, "    movq    %%rbp, %%rsp\n");
                fprintf(out, "    popq    %%rbp\n");
                fprintf(out, "    ret\n"); break;
            case AS_BINARY:
                {
                    const char *op_str=NULL;
                    switch (curr->binary_op){
                        case BINARY_ADD: op_str="addl"; break;
                        case BINARY_SUB: op_str = "subl"; break;
                        case BINARY_MULT: op_str = "imull"; break;
                         // logical operation (&&, ||) will not reach here, here processing bitwise operation (&, |)
                        case BINARY_BITWISE_AND: op_str = "andl"; break;
                        case BINARY_BITWISE_OR:  op_str = "orl"; break;
                        case BINARY_BITWISE_XOR: op_str = "xorl"; break;
                        case BINARY_LSHIFT: op_str = "sall"; break; // arithmetic shift left
                        case BINARY_RSHIFT: op_str = "sarl"; break; // arithmetic shift right
                        default: break;
                    }

                    if (op_str){
                        
                        //shift speical deal with:if it's shift，and source is ECX ，output %cl
                        if (curr->binary_op==BINARY_LSHIFT || curr->binary_op==BINARY_RSHIFT){
                            fprintf(out, "    %s    ", op_str);
                            //check offset (src)
                            if (curr->src.type==AS_IMM){
                                //if is immediate，directly output
                                emit_op(curr->src,out);
                            }else{
                                //if not，msut be %cl
                                //tacky_to_asm phase make sure src2 move into ECX
                                //only output %cl
                                fprintf(out, "%%cl");
                            }

                            fprintf(out, ", ");
                            emit_op(curr->dst, out);
                            fprintf(out, "\n");
                        }else{ //other binary operators
                            fprintf(out, "    %s    ", op_str);
                            emit_op(curr->src,out);
                            fprintf(out,", ");
                            emit_op(curr->dst,out);
                            fprintf(out,"\n");
                        }


                    }

                }
                break;
            
            case AS_IDIV:
                fprintf(out, "    idivl   ");
                emit_op(curr->src, out); 
                fprintf(out, "\n");
                break;
            
            case AS_CDQ:
                fprintf(out, "    cdq\n");
                break;
            
            case AS_CMP:
                fprintf(out, "    cmpl    ");
                emit_op(curr->src, out); fprintf(out, ", "); emit_op(curr->dst, out);
                fprintf(out, "\n"); break;
            case AS_JMP:
                fprintf(out, "    jmp     .L%s\n", curr->label); break;
            case AS_JMP_CC:
                // 減少多餘空格
                fprintf(out, "    j%s .L%s\n", cond_to_str(curr->cond), curr->label); break;
            case AS_SET_CC:
                fprintf(out, "    set%s ", cond_to_str(curr->cond)); // 減少多餘空格
                emit_op_8bit(curr->dst, out);
                fprintf(out, "\n"); 
                break;
            case AS_LABEL:
                fprintf(out, ".L%s:\n", curr->label); break;
        }
        curr = curr->next;
    }
    // Linux safe stack marking
#ifndef __APPLE__
    fprintf(out, ".section .note.GNU-stack,\"\",@progbits\n");
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
    resolve_program(prog);
    if (stage==3){ //--validate
        return 0;
    }

    //TACKY Generation
    TackyProgram *tacky_prog=generate_tacky(prog);
    if (stage==4){
        return 0;
    }

    //codegen
    AsmProg *asmp=tacky_to_asm(tacky_prog);
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

    //use gcc make final execute
    char cmd[1024];
    char out_path[256];
    strncpy(out_path, input_path, sizeof(out_path) - 1);
    out_path[sizeof(out_path) - 1] = '\0';
    dot = strrchr(out_path, '.');
    if (dot) {
        *dot = '\0';
    }

    snprintf(cmd, sizeof(cmd), "gcc %s -o %s", asm_path, out_path);
    int res = system(cmd);

    remove(asm_path);

    return res;
}
