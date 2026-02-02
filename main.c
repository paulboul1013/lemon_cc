#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *read_file(const char *filename){
    FILE *f=fopen(filename,"r");
    if (!f){
        perror(filename);
        return NULL;
    }

    fseek(f,0,SEEK_END);
    long length=ftell(f);
    fseek(f,0,SEEK_SET);
    char *buffer=malloc(length+1);
    fread(buffer,1,length,f);

    fclose(f);

    buffer[length]='\0';
    return buffer;
}

int main(int argc,char *argv[]){
    if (argc < 2){
        fprintf(stderr,"Usage: %s <input_file> [--lex|--parse|...]\n",argv[0]);
        return 1;
    }

    char *filename=argv[1];

    int lex_only=0;
    for(int i=1;i<argc;i++){
        if (strcmp(argv[i],"--lex")==0){
            lex_only=1;
            break;
        }
    }

    
    char *input=read_file(filename);
    if (!input){
        perror("Failed to read file");
        return 1;
    }

    


    return 0;
}