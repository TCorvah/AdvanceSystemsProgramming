#include "greptile.h"
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Descend through the hierarchy, starting at "pathname".
 * The caller’s func() is called for every file.
 */
#define FTW_F   1       /* file other than directory */
#define FTW_D   2       /* directory */
#define FTW_DNR 3
#define FTW_NS  4

static size_t pathlen;

/* function type that is called for each filename */
typedef int Myfunc(const char *, const struct stat *, int);
static Myfunc myfunc;
static int myftw(char *, Myfunc func);
void dopath(const char *);
static long nreg, ndir;


static int myfunc(const char *pathname, const struct stat *statptr, int type)
{    
    switch (type) {
    case FTW_F:
        switch (statptr->st_mode  & S_IFMT){
        case S_IFREG:
            nreg++;
            fprintf(stdout,"reg file %s\n", pathname);
            break;
        case S_IFDIR:
            printf("wrong type for directory %s", pathname);
        }    
        break;     
    case FTW_D:           
        ndir++; 
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

void dopath(const char *path)
{ 
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;
    char fullpat[PATH_MAX];
   
    if (lstat(fullpat, &statbuf) < 0)  /* stat error */
        fprintf(stderr,"can't stat %s\n",fullpat);
    if(fullpat[strlen(fullpat)-1] == '\n')
       fullpat[strlen(fullpat) - 1] = 0;
    strcpy(fullpat, path);
    fullpat[strlen(fullpat)- 1] = '/';
    if ((dp = opendir(fullpat)) == NULL)   /* can’t read directory */
        fprintf(stderr,"can't open %s\n", fullpath);
    while ((dirp = readdir(dp)) != NULL) {
          printf("files%s\n", dirp->d_name);   
        if (strcmp(dirp->d_name, ".") == 0  ||
            strcmp(dirp->d_name, "..") == 0)
                continue;       /* ignore dot and dot-dot */
       
        while ( myfunc(fullpat, &statbuf, FTW_D)) /* not a directory */
            strcpy(&fullpat[n++], dirp->d_name); /* append name after "/" */
            fullpat[strlen(fullpat)- 1] = '/'; 
            strcpy(fullpat, dirp->d_name);
            printf("path=%s\n", dirp->d_name );
     
    }
    if (closedir(dp) < 0)
        fprintf(stderr,"can’t close directory %s", fullpat);
 
  
}



int main(int argc, char *argv[])
{
    
    if (argc != 2)
        perror("usage:  ftw  <starting-pathname>");

    dopath(argv[1]);
    exit(0);
   
   
}






