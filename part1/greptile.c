#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <stdarg.h>
#include <fcntl.h>

typedef int Myfunc(const char *,const char *patt, const struct stat *, int);
static Myfunc myfunc;


#define REGULAR_FILE   1       /* file other than directory */
#define DIRECTORY   2       /* directory */
#define NO_PERM 3
#define CANT_STAT 4


#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"
#define EXEC "/opt/asp/bin/"
#define MAXLINE 4096


/*Initialized in main() based on command-line arguments*/ 
size_t pattern_len;
size_t file_print_offset;
static char *fullpath;
struct stat buf;
static long nreg, ndir;


static inline void die(char *msg) {
    perror(msg);
    exit(2);
}






static int myfunc(const char *pathname, const char *pattern, const struct stat *statptr, int type)
{    
    switch (type) {
    case REGULAR_FILE:
        switch (statptr->st_mode  & __S_IFMT){
        case __S_IFREG:
            nreg++;
            break;
        case __S_IFDIR:
            printf("wrong type for directory %s", pathname);
        }    
        break;     
    case DIRECTORY:           
        ndir++; 
        break;
    case NO_PERM:
        fprintf(stderr,"no perm %d for pathname %s", type, pathname);
        break;
    case CANT_STAT:
        fprintf(stderr,"can't stat %d for pathname %s", type, pathname);
        break;
    default:
        fprintf(stderr,"unknown type %d for pathname %s", type, pathname);
    }   
return(0); 
}



int parse(char *pathType, char *filename, char *args){
    char *ptr;
    char buff[MAXLINE];
    if(!strstr(pathType, EXEC)){
        strcpy(args, " ");
        strcpy(filename,".");
        strcat(filename, pathType);
        if(pathType[strlen(pathType)-1] == '/')
           strcat(filename, "test.txt");
        return 1;
    }
    else{
        ptr = strrchr(pathType, 'c');
        if(ptr){
            strcpy(args, ptr+1);
            *ptr = '\0';
        }
    
        else
            strcpy(args, " ");
        strcpy(filename, " .");
        strcat(filename, pathType);
        return 0;
    }
    
    return(0);
}



int search_file(char *file_path, char *pattern, off_t file_size) {  
    // Implement this function  
    FILE *fptr;
    char *match;

    //stat file
    if(lstat(file_path, &buf) < 0){
        die("can't stat");

    }
    //open file for readonly
    fptr = fopen(file_path, "r" );
        if(file_path == NULL){
            die("can't open file");

        }
    char *path = malloc(buf.st_size);
    int num = 1;
    while(fgets(path, buf.st_size, fptr ) != NULL){
        if((match = strstr(path, pattern))){
            fprintf(stdout, "\n%s\n %d:%s", file_path, num, match);
            num++;

        }
          


    }
    fclose(fptr);
    free(path);

    
return(0);
}

int helper_func(char *path, char *pattern) {
    // Implement this function
    struct dirent *dirp;
    DIR *dp;
    int ret, n, reg;
    if(lstat(path, &buf) < 0)
        return(myfunc(path, pattern, &buf, CANT_STAT)); /*stat error*/
    
    if (S_ISDIR(buf.st_mode) == 0)
        return(myfunc(path,pattern, &buf, REGULAR_FILE));
    
    
    if ((ret = myfunc(path,pattern, &buf, DIRECTORY)) != 0)
        return(ret);
 
    n = strlen(fullpath);
    fullpath[n++] = '/';
    if ((dp = opendir(fullpath)) == NULL)   /* can’t read directory */
        return(EXIT_FAILURE);

    while ((dirp = readdir(dp)) != NULL){

       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;

        strcpy(&fullpath[n], dirp->d_name);
        reg = search_file(fullpath, pattern, buf.st_size);
        
        
        if((ret = helper_func(fullpath, pattern)) != 0){
            break;

        }
        

    }

    if (closedir(dp) < 0)
        fprintf(stderr,"can’t close directory %s", fullpath);
   
    return(reg);
    

}

static int traverse_directory(char *pathname, char *p)
{   
    fullpath = malloc(file_print_offset);
    if(file_print_offset <= strlen(pathname)){
        file_print_offset = strlen(pathname) * 2; 
        if ((fullpath = realloc(fullpath,file_print_offset)) == NULL) 
            perror("realloc failed");      
    }
  
    strcpy(fullpath, pathname);
    return(helper_func(fullpath, p));
  
   
}



int main(int argc, char **argv) {
    char *directory_path = ".";
    char *pattern;

    if (argc == 2) {
        pattern = argv[1];
        file_print_offset = 2; // Skip "./" prefix if no directory argument
    } else if (argc == 3) {
        pattern = argv[1];
        directory_path = argv[2];
        file_print_offset = 0; // Print full path if directory argument is given
    } else {
        fprintf(stderr, "usage: greptile <pattern> [directory]\n");
        exit(2);
    }

    pattern_len = strlen(pattern);

    // Return 0 if a match was found, 1 otherwise
    return traverse_directory(directory_path, pattern) == 0;
}