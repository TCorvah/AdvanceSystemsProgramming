#include "apue.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#define FILE_SIZE 1024
#define LINE 2048


int main(int argc, char **argv){
    FILE *fp, *temp;

    //filename pointer
    char *filename, *patt;

    //stores temp filename
    char temp_filename[FILE_SIZE];

    //stores line of the original file
    char buff[LINE];

    char *cwd = getcwd(NULL, 1024);
    if(cwd == NULL){
        printf("can't get cwd.\n");
    }


    if(argc != 3){
        printf("argument missing\n");
        return 1;
    
    //set filename to point to 2nd arg
    }
    filename = argv[1];
    patt = argv[2];

    strcpy(temp_filename, "temp---");
   
    strcat(temp_filename, filename);

    fp = fopen(filename, "r");
    temp = fopen(temp_filename, "w");

    if(fp == NULL || temp == NULL){
        printf("error opening file\n");
        return 1;
    }
    int i = 1;
    char *p;
    strcat(cwd, "/");
    if(cwd[strlen(cwd) - 1] == '/')
       strcat(cwd, argv[1]);
    while(fgets(buff, LINE, fp) != NULL){
        if((p = strstr(buff, patt)) != NULL)
            fprintf(temp, " \n%s\n %d:%s" , cwd, i, p);
         i++;
    }
  
    fclose(temp);
    fclose(fp);
    free(cwd);

   
    remove(filename);
    rename(temp_filename, filename);

    return 0;


}