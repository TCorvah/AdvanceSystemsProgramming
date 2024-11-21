#include <dirent.h>
#include <limits.h>
#include <errno.h>      /* for definition of errno */
#include <stdarg.h>     /* ISO C variable aruments */
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sysdir.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

typedef int Myfunc(const char *, const char *pat, const struct stat *, int);
/*
 * Descend through the hierarchy, starting at "pathname".
 * The caller’s func() is called for every file.
 */
#define FTW_F   1       /* file other than directory */
#define FTW_D   2       /* directory */
#define FTW_DNR 3
#define FTW_NS  4
#define MAXLINE 4096
#define FILESZ 8196


static size_t pathlen;
static char *fullpath;
static long nreg, ndir;
struct stat sbuff;



static int search_file(char *file_path, char *pattern, off_t file_size) {
//malloc for offset
    FILE *fd;
    char buf[MAXLINE];
    char *t;

   
           
   //make sure file can be stat
    if(lstat(file_path,&sbuff) < 0){
       perror("opening file\n");
       return 1;

   }
       //open the file for readonly
    fd = fopen(file_path, "r");
        if (file_path == NULL){
            perror("opening file\n");
        } 
  
    // malloc using the offset which is the buffer size

    //search only IS_REG for pattern
    //check for newline or EOF
    char *path = malloc(sbuff.st_size);
    int num = 1;
    while((fgets(path, sbuff.st_size, fd) != NULL)){
    if((t = strstr(path, pattern)) != NULL)
            printf("\n%s\n %d:%s" , file_path, num, t);
            num++; 
       
   

    }
  
   
       
   
   
    fclose(fd);
    free(path);
    
   
        
   

   return(0);

}





static int myfunc(const char *pathname, const char *pat, const struct stat *statptr, int type)
{    
    switch (type) {
    case FTW_F:
        switch (statptr->st_mode & S_IFMT){
        case S_IFREG:
            strstr(pathname,pat );
            nreg++;
            break;
        case S_IFDIR:
            printf("error %s\n", pathname);
        }    
        break;     
    case FTW_D:           
       ndir++; 
        break;
    case FTW_DNR:
        break;
    case FTW_NS:
        fprintf(stderr,"can't stat %d for pathname %s", type, pathname);
        break;
    default:
        fprintf(stderr,"unknown type %d for pathname %s", type, pathname);
    }

    
return(0); 
}





/*
 * Descend through the hierarchy, starting at "fullpath".
 * If "fullpath" is anything other than a directory, we lstat() it,
 * call func(), and return.  For a directory, we call ourself
 * recursively for each name in the directory.
 */
static int dopath(const char *path, char *pattern)
{  
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    size_t ret, dret;
    int n;
    //search all files within directories
    //colorize output
    //if no directory argument, then default is " ."
    //make the file path to not have the follwing ./
    if (lstat(path, &statbuf) < 0)  /* stat error */
       return(myfunc(path, pattern, &statbuf, FTW_NS));
    if (S_ISDIR(statbuf.st_mode) == 0)
        return(myfunc(path,pattern, &statbuf, FTW_F));
       

   if ((ret = myfunc(path,pattern, &statbuf, FTW_D)) != 0)
        return(ret);
    n = strlen(fullpath);
    fullpath[n++]  = '/';
    fullpath[n] = 0;
    if ((dp = opendir(fullpath)) == NULL)   /* can’t read directory */
        exit(EXIT_FAILURE);
    
    while ((dirp = readdir(dp)) != NULL) {
        if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;
      
        strcpy(&fullpath[n], dirp->d_name);
    
        dret = search_file(fullpath, pattern, sizeof(path));
        if((ret = dopath(fullpath, pattern)) != 0){
            break;

        }
    }

  
    if (closedir(dp) < 0)
        fprintf(stderr,"can’t close directory %s", fullpath);
   
    return(dret);

   
}



static int myftw(char *pathname, char *p)
{   
    fullpath = malloc(pathlen);
    if(pathlen <= strlen(pathname)){
        pathlen = strlen(pathname) * 2; 
        if ((fullpath = realloc(fullpath,pathlen)) == NULL) 
            perror("realloc failed");      
    }
  
    strcpy(fullpath, pathname);
    return(dopath(fullpath, p));
  
   
}




int  main(int argc, char *argv[])
{
    size_t ret;

    if (argc != 3)
        printf("usage:  ftw  <args> <starting-pathname>");
    ret = myftw(argv[2], argv[1]);   
        /* does it all */

   exit(ret);
}
