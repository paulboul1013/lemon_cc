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

//keyword check
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
        
        if (isalpha(*p) || *p=='_'){
            const char *start=p;
            while (isalnum(*p) || *p=='_') p++;
            int len=p-start;
            char *buf=malloc(len+1);
            strncpy(buf,start,len);
            buf[len]='\0';
            
            printf("Token: %s\n",buf);
            free(buf);
            continue;
        }


        if (isdigit(*p)){
            while (isdigit(*p)) p++;

            //123abc can't pass，need to 123_abc will pass
            if (isalpha(*p) || *p=='_'){
                fprintf(stderr, "Lexer Error: Invalid identifier starting with digit at '%c'\n", *p);
                exit(1);
            }
            continue;
        }
        
        switch (*p){
            case '(' : case ')': case '{': case '}': case ';':
                p++;
                continue;
            default:
                fprintf(stderr,"Lexer Error: Invalid character '%c'\n",*p);
                exit(1);
        }
    }
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
    int lex_only=0;

    for(int i=1;i<argc;i++){
        if (strcmp(argv[i],"--lex")==0){
            lex_only=1;
        }
        else if (argv[i][0]=='-'){
            continue;
        }else{
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




    


    return 0;
}