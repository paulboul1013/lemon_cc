#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//define Token type
typedef  enum{
    TOK_INT, TOK_VOID, TOK_RETURN,
    TOK_IDENTIFIER, TOK_CONSTANT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMICOLON, TOK_EOF, TOK_INVALID
}TokenType;

typedef struct {
    TokenType type;
    char *value;
}Token;

// for save lexer make all tokens
Token * token_list=NULL;
int token_count=0;
int token_pos=0;

//Ast Node define
typedef struct {
    int value;
}Exp;

typedef struct{
    Exp *exp;
} Statement;

typedef struct {
    char *name;
    Statement *body;
} Function;

typedef struct {
    Function *fn;
} Program;


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

//<exp> :: = <int>
Exp *parse_exp(){
    Token t=take();
    if (t.type!=TOK_CONSTANT) exit(1);
    Exp *e=malloc(sizeof(Exp));
    e->value=atoi(t.value);
    return e;
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
    char *filename=NULL;
    int parse_flag=0;

    for(int i=1;i<argc;i++){
        if (strcmp(argv[i],"--lex")==0){
            continue;
        }
        else if (strcmp(argv[i],"--parse")==0){
            parse_flag=1;
            continue;
        }
        else if (argv[i][0]!='-'){
            filename=argv[i];
        }
    }

    if (!filename){
        fprintf(stderr, "Usage: %s <input_file> [--lex]\n", argv[0]);
        return 1;
    }

    
    char *input=read_file(filename);
    if (!input){
        fprintf(stderr, "Error: Could not read file %s\n", filename);
        return 1;
    }

    lex(input);

    free(input);


    Program *prog = parse_program();

    


    return 0;
}