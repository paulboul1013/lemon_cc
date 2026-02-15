#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//define Token type
typedef  enum{
    TOK_INT, TOK_VOID, TOK_RETURN,
    TOK_IDENTIFIER, TOK_CONSTANT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMICOLON, TOK_EOF, TOK_INVALID,

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
    TOK_ASSIGN // =
}TokenType;

typedef struct {
    TokenType type;
    char *value;
}Token;

typedef enum {
    EXP_CONSTANT,
    EXP_UNARY,
    EXP_BINARY
}ExpType;

typedef enum {
    BINARY_ADD,
    BINARY_SUB,
    BINARY_MULT,
    BINARY_DIV,
    BINARY_REM
}BinaryOpType;

typedef enum {
    UNARY_NEGATION, // -
    UNARY_COMPLEMENT, // ~
}UnaryOpType;

typedef struct Exp {
    ExpType type;
    union {
        int int_value; //for EXP_CONSTANT
        struct { //for EXP_UNARY
            UnaryOpType op;
            struct Exp *exp;
        }unary;
        struct {
            BinaryOpType op;
            struct Exp *left;
            struct Exp *right;
        }binary;
    };
} Exp;

// for save lexer make all tokens
Token * token_list=NULL;
int token_count=0;
int token_pos=0;

// c AST Node
typedef struct {Exp *exp;} Statement;
typedef struct {char *name; Statement *body; } Function;
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
        default: exit(1);
    }
}

//Asm AST Node
typedef enum {
    AS_MOV,
    AS_NEG,
    AS_NOT,
    AS_RET,
    AS_ALLOCATE_STACK,
    AS_BINARY, //for add,sub,mult
    AS_IDIV, //div and get remainder
    AS_CDQ //sign extend EAX to EDX
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
    REG_R11D //for imul fix
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
    AsmOperand src;
    AsmOperand dst;
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
    TACKY_INST_BINARY
} TackyInstType;

typedef struct TackyInstruction{
    TackyInstType type;
    UnaryOpType unary_op;
    BinaryOpType binary_op;
    TackyVal *src;
    TackyVal *src2;
    TackyVal *dst;
    struct TackyInstruction *next;
} TackyInstruction;

typedef struct {
    char *function_name;
    TackyInstruction *instructions;
} TackyProgram;

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
            case '~':
                add_token(TOK_COMPLEMENT,NULL);
                break;
            case '+':
                add_token(TOK_PLUS,NULL);
                break;
            case '*':
                add_token(TOK_MULT,NULL);
                break;
            case '/':
                add_token(TOK_DIV,NULL);
                break;
            case '%':
                add_token(TOK_REMAINDER,NULL);
                break;
            case '-':
                if (*(p+1)=='-'){ //deal with --
                    add_token(TOK_DECREMENT,NULL);
                    p++;
                }else{ //deal with -
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
                if (*(p+1)=='='){
                    add_token(TOK_LTE,NULL);
                    p++;
                }else{
                    add_token(TOK_LT,NULL);
                }
                break;
            case '>':
                if (*(p+1)=='='){
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
                }else{
                    fprintf(stderr, "Lexer Error: Single '&' is not supported (need &&)\n");
                    exit(1);
                }
                break;
            case '|':
                if (*(p+1)=='|'){
                    add_token(TOK_OR,NULL);
                    p++;
                }else{
                    fprintf(stderr, "Lexer Error: Single '|' is not supported (need ||)\n");
                    exit(1);
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

void expect(TokenType type){
    Token t=take();
    if (t.type!=type){
        fprintf(stderr, "Parse Error: Expected type %d but got %d\n", type, t.type);
        exit(1);//pass invalid_parse test
    }
}

//<exp> ::= <factor> | <exp> <binop> <exp>
Exp *parse_exp(int min_prec){
   Exp *left=parse_factor();
   Token next=peek();

   while (get_precedence(next.type)>=min_prec){
        Token op_tok=take();
        int prec=get_precedence(op_tok.type);

        //because implement is left-associative, pass prec+1
        Exp *right=parse_exp(prec+1);

        left=make_binary_exp(token_to_binary_op(op_tok.type),left,right);
        next=peek();

   }

   return left;
} 

//deal with number and parentheses and unary operator
//note:unary operator make sure higher precedence than binary operator
//<factor> ::= <int> | <unop> <factor> | "(" <exp> ")"
Exp *parse_factor(){
    Token t=peek();
    
    if (t.type==TOK_CONSTANT){
        take();
        return make_int_exp(atoi(t.value));
    }
    else if (t.type==TOK_NEGATION || t.type==TOK_COMPLEMENT){
        take();
        UnaryOpType op=(t.type==TOK_NEGATION) ? UNARY_NEGATION :UNARY_COMPLEMENT;
        Exp *inner=parse_factor(); //unary after factor
        return make_unary_exp(op,inner);
    }
    else if (t.type==TOK_LPAREN){
        take();
        Exp *inner=parse_exp(0); // PAREN inter predence reset to 0
        expect(TOK_RPAREN);
        return inner;
    }else{
        fprintf(stderr, "Parse Error: Expected factor but got %d\n", t.type);
        exit(1);
    }

    
}

// <statement> ::= "return" <exp> ";"
Statement *parse_statement() {
    expect(TOK_RETURN);
    Statement *s=malloc(sizeof(Statement));
    s->exp=parse_exp(0);
    expect(TOK_SEMICOLON);
    return s;
}

// <function> ::= "int" <identifier> "(" "void" ")" "{" <statement> "}"
Function *parse_function() {
    expect(TOK_INT);
    Token id=take();
    if (id.type!=TOK_IDENTIFIER) exit(1);
    expect(TOK_LPAREN);
    expect(TOK_VOID);
    expect(TOK_RPAREN);
    expect(TOK_LBRACE);
    Function *f=malloc(sizeof(Function));
    f->name=strdup(id.value);
    f->body=parse_statement();
    expect(TOK_RBRACE);
    return f;
}

// <program> ::= <function>
Program *parse_program() {
    Program *prog=malloc(sizeof(Program));
    prog->fn=parse_function();
    if (peek().type!=TOK_EOF) exit(1); //make sure not extra tokens
    return prog;
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

void append_asm(AsmInst **head,AsmInstType type,BinaryOpType op,AsmOperand src,AsmOperand dst){
    AsmInst *new_inst=malloc(sizeof(AsmInst));
    new_inst->type=type;
    new_inst->binary_op=op;
    new_inst->src=src;
    new_inst->dst=dst;
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
    asmp->fn=malloc(sizeof(AsmFunc));
    asmp->fn->name=strdup(tacky->function_name);
    asmp->fn->instructions=NULL;

    TackyInstruction *curr=tacky->instructions;
    while (curr!=NULL){
        if (curr->type==TACKY_INST_RETURN){
            append_asm(&asmp->fn->instructions,AS_MOV,0,convert_tacky_val(curr->src),as_reg(REG_EAX));
            append_asm(&asmp->fn->instructions,AS_RET,0,(AsmOperand){0},(AsmOperand){0});
        }
        else if (curr->type==TACKY_INST_UNARY){
            append_asm(&asmp->fn->instructions,AS_MOV,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst));
            AsmInstType type=(curr->unary_op==UNARY_NEGATION)?AS_NEG:AS_NOT;
            append_asm(&asmp->fn->instructions,type,0,(AsmOperand){0},convert_tacky_val(curr->dst));
        }
        else if (curr->type==TACKY_INST_BINARY){
            if (curr->binary_op==BINARY_DIV || curr->binary_op==BINARY_REM) {
                //1. move src1,eax
                //2. cdq
                //3. idiv src2
                //4. mov eax/edx ,dst
                append_asm(&asmp->fn->instructions,AS_MOV,0,convert_tacky_val(curr->src),as_reg(REG_EAX));
                append_asm(&asmp->fn->instructions,AS_CDQ,0,(AsmOperand){0},(AsmOperand){0});
                append_asm(&asmp->fn->instructions,AS_IDIV,0,convert_tacky_val(curr->src2),(AsmOperand){0});

                AsmReg res_reg=(curr->binary_op==BINARY_DIV) ? REG_EAX: REG_EDX;
                append_asm(&asmp->fn->instructions,AS_MOV,0,as_reg(res_reg),convert_tacky_val(curr->dst));
            }else{
                // op: add , sub , mult
                // mov src1 ,dst
                // op src2, dst
                append_asm(&asmp->fn->instructions,AS_MOV,0,convert_tacky_val(curr->src),convert_tacky_val(curr->dst));
                append_asm(&asmp->fn->instructions,AS_BINARY,curr->binary_op,convert_tacky_val(curr->src2),convert_tacky_val(curr->dst));
            }
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
        AsmInst *allocate=malloc(sizeof(AsmInst));
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
TackyVal *gen_tacky_exp(Exp *e,TackyInstruction **inst_list){
    if (e->type==EXP_CONSTANT){
        return tacky_val_constant(e->int_value);
    }
    else if (e->type==EXP_UNARY){
        //deal with inner expression first,get sorce operator
        TackyVal *src=gen_tacky_exp(e->unary.exp,inst_list);
        
        //build a temporal variable for dest
        char *t_name=make_temporary();
        TackyVal *dst=tacky_val_var(t_name);

        //emit unary instruction and add list
        TackyInstruction *inst=malloc(sizeof(TackyInstruction));
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
        //deal with left side expression，get result position v1
        TackyVal *v1=gen_tacky_exp(e->binary.left,inst_list);

        TackyVal *v2=gen_tacky_exp(e->binary.right,inst_list);

        //build temporal variable to save result
        char *t_name=make_temporary();
        TackyVal *dst=tacky_val_var(t_name);

        //build and emit binary instruction
        TackyInstruction *inst=malloc(sizeof(TackyInstruction));
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

TackyProgram *generate_tacky(Program *prog){
    TackyProgram *t_prog=malloc(sizeof(TackyProgram));
    t_prog->function_name=strdup(prog->fn->name);
    t_prog->instructions=NULL;

    //deal with return sentence expression
    TackyVal *final_val=gen_tacky_exp(prog->fn->body->exp,&t_prog->instructions);

    //emit RETURN instruction
    TackyInstruction *ret_inst=malloc(sizeof(TackyInstruction));
    ret_inst->type=TACKY_INST_RETURN;
    ret_inst->src=final_val;
    ret_inst->next=NULL;
    append_tacky_inst(&t_prog->instructions,ret_inst);

    return t_prog;
    
}

void emit_op(AsmOperand op,FILE *out){
    if (op.type==AS_IMM){
        fprintf(out,"$%d",op.imm);
    }
    else if (op.type==AS_REG){
        const char *reg_names[]={"%eax","%r10d","%edx","%r11d"};
        fprintf(out,"%s",reg_names[op.reg]);
    }
    else if (op.type==AS_STACK){
        fprintf(out, "%d(%%rbp)", op.stack_offset);
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
                    const char *ops[]={"addl","subl","imull"};
                    fprintf(out, "    %s    ", ops[curr->binary_op]);
                    emit_op(curr->src,out);
                    fprintf(out,", ");
                    emit_op(curr->dst,out);
                    fprintf(out,"\n");

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
    int stage=5;

    for(int i=1;i<argc;i++){
        if (strcmp(argv[i],"--lex")==0){
            stage=1;
            continue;
        }
        else if (strcmp(argv[i],"--parse")==0){
            stage=2;
            continue;
        }
        else if (strcmp(argv[i],"--codegen")==0){
            stage=3;
            continue;
        }
        else if (strcmp(argv[i],"--tacky")==0){
            stage=4;
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

    //TACKY Generation
    TackyProgram *tacky_prog=generate_tacky(prog);
    if (stage==4){
        return 0;
    }

    //codegen
    AsmProg *asmp=tacky_to_asm(tacky_prog);
    fix_and_allocate(asmp);
    if (stage==3){
        return 0;
    }

    //emit Asm to .s file
    char asm_path[256];
    strncpy(asm_path, input_path, sizeof(asm_path) - 1);
    asm_path[sizeof(asm_path) - 1] = '\0';
    char *dot = strrchr(asm_path, '.');
    if (dot) strcpy(dot, ".s"); else strcat(asm_path, ".s");

    FILE *out=fopen(asm_path,"w");
    emit_asm_new(asmp,out);
    fclose(out);

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

    return res;
}