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
    TOK_DECREMENT // --
}TokenType;

typedef struct {
    TokenType type;
    char *value;
}Token;

typedef enum {
    EXP_CONSTANT,
    EXP_UNARY
}ExpType;

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


//Asm AST Node
typedef enum {ASM_MOV,ASM_RET} AsmInstType;
typedef struct {
    AsmInstType type;
    int imm;
} AsmInstruction;

typedef struct {
    char *name;
    AsmInstruction *instructions;
    int inst_count;
} AsmFunction;

typedef struct {
    AsmFunction *fn;
} AsmProgram;

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
    TACKY_INST_UNARY
} TackyInstType;

typedef struct TackyInstruction{
    TackyInstType type;
    UnaryOpType unary_op;
    TackyVal *src;
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
            case '-':
                if (*(p+1)=='-'){
                    add_token(TOK_DECREMENT,NULL);
                    p++;
                }else{
                    add_token(TOK_NEGATION,NULL);
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

void expect(TokenType type){
    Token t=take();
    if (t.type!=type){
        fprintf(stderr, "Parse Error: Expected type %d but got %d\n", type, t.type);
        exit(1);//pass invalid_parse test
    }
}

//<exp> :: = <int> | <unop> <exp> | "(" <exp> ")"
Exp *parse_exp(){
   Token t=peek();

   if (t.type==TOK_CONSTANT){
        take();
        return make_int_exp(atoi(t.value));
   }

   else if (t.type==TOK_NEGATION || t.type==TOK_COMPLEMENT){
        take(); //take - or ~ out
        UnaryOpType op=(t.type==TOK_NEGATION) ? UNARY_NEGATION:UNARY_COMPLEMENT;
        Exp *inner=parse_exp(); //support -~2
        return make_unary_exp(op,inner);
   }
   else if (t.type==TOK_LPAREN){
    take(); //take (  out
    Exp *inner=parse_exp(); //parse ( exp )
    expect(TOK_RPAREN);
    return inner;
   }
   else{
        fprintf(stderr, "Parse Error: Unexpected token %d at position %d\n", t.type, token_pos);
        exit(1);
   }
} 

// <statement> ::= "return" <exp> ";"
Statement *parse_statement() {
    expect(TOK_RETURN);
    Statement *s=malloc(sizeof(Statement));
    s->exp=parse_exp();
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

//assemble generation
AsmProgram *generate_asm(Program *prog){
    AsmProgram *asm_prog=malloc(sizeof(AsmProgram));
    asm_prog->fn=malloc(sizeof(AsmFunction));
    asm_prog->fn->name=strdup(prog->fn->name);

    //one return statement to mov and ret two instructions
    asm_prog->fn->inst_count=2;
    asm_prog->fn->instructions=malloc(sizeof(AsmInstruction)*2);

    asm_prog->fn->instructions[0].type=ASM_MOV;

    if (prog->fn->body->exp->type==EXP_CONSTANT){
        asm_prog->fn->instructions[0].imm=prog->fn->body->exp->int_value;

    }else{
        asm_prog->fn->instructions[0].imm=0;
    }


    asm_prog->fn->instructions[1].type=ASM_RET;

    return asm_prog;
}

// code emission
void emit_asm(AsmProgram *asmp,FILE *out){
    #ifdef __APPLE__
        fprintf(out, ".globl _%s\n", asmp->fn->name);
        fprintf(out, "_%s:\n", asmp->fn->name);
    #else
        fprintf(out, ".globl %s\n", asmp->fn->name);
        fprintf(out, "%s:\n", asmp->fn->name);
    #endif

    for(int i=0;i<asmp->fn->inst_count;i++){
        AsmInstruction inst=asmp->fn->instructions[i];
        if (inst.type==ASM_MOV){
            fprintf(out, "    movl    $%d, %%eax\n", inst.imm);
        }else if (inst.type==ASM_RET){
            fprintf(out, "    ret\n");
        }
    }

    // Linux 安全堆棧標記
    #ifndef __APPLE__
        fprintf(out, ".section .note.GNU-stack,\"\",@progbits\n");
    #endif
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
    int stage=4;

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
    AsmProgram *asmp=generate_asm(prog);
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
    emit_asm(asmp,out);
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