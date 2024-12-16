#include <stddef.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

typedef int Myfunc(const char *,const char *patt, const struct stat *, int);
static Myfunc myfunc;


#define REGULAR_FILE   1  /* file other than directory */
#define DIRECTORY   2   /* directory */
#define NO_PERM 3    /* no perm */
#define CANT_STAT 4 /* file that can't be stat */


#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"
#define EXEC  "/"
#define MAXLINE 4096


/*Initialized in main() based on command-line arguments*/ 
size_t pattern_len;
size_t file_print_offset;
static char *fullpath;
struct stat buf;
static long nreg, ndir;

static inline void exit_error(char *msg) {
    perror(msg);
    exit(2);
}


static int myfunc(const char *pathname, const char *pattern, const struct stat *statptr, int type)
{    
    switch (type) {
    case REGULAR_FILE:
        // Check file type
        if ((statptr->st_mode & S_IFMT) == S_IFREG) {
            nreg++; // Increment regular file counter
        } else if ((statptr->st_mode & S_IFMT) == S_IFDIR) {
            fprintf(stderr, "Error: Directory found when expecting a file: %s\n", pathname);
        } else if ((statptr->st_mode & S_IFMT) == S_IFLNK) {
            fprintf(stderr, "Skipping symbolic link: %s\n", pathname);
        } else {
            fprintf(stderr, "Unknown file type for %s\n", pathname);
        }
        break;     
    case DIRECTORY:
        ndir++; // Increment directory counter
        break;
    case NO_PERM:
        fprintf(stderr, "Permission denied for %s (type: %d)\n", pathname, type);
        break;
    case CANT_STAT:
        fprintf(stderr, "Cannot stat %s (type: %d): %s\n", pathname, type, strerror(errno));
        break;
    default:
        fprintf(stderr, "Unknown type %d for pathname %s\n", type, pathname);
    }   
    return 0; 
}


int search_file(char *file_path, char *pattern, off_t file_size) {  
    // Implement this function  
    FILE *fptr;
    char *match;
    printf("Searching in file: %s for pattern: %s\n", file_path, pattern);

    char *path = malloc(file_size);
    if(!path){
        exit_error("malloc failed");
    }
    //stat file
    if(lstat(file_path, &buf) < 0){
        free(path);
        exit_error("can't stat");

    }
    //open file for readonly
    fptr = fopen(file_path, "r" );
        if(!fptr){
            free(path);
            exit_error("can't open file");
        }
    int num = 1;
    while(fgets(path, buf.st_size, fptr ) != NULL){
        while(!strchr(path, '\n') && !feof(fptr)){
            file_size *= 2;
            path = realloc(path, file_size);
            if(!path){
                fclose(fptr);
                exit_error("realloc failed");
            }
            fgets(path + strlen(path), file_size - strlen(path), fptr);
        }
        if((match = strstr(path, pattern))){
            fprintf(stdout, "\n%s\n %d:%s", file_path, num, match);

        }
        num++;
    }
    fclose(fptr);
    free(path);

    
return(0);
}

static int  helper_func(char *path, char *pattern) {
    struct dirent *dirp;
    DIR *dp;
    int ret, n, reg;

    if(lstat(path, &buf) < 0)
        return(myfunc(path, pattern, &buf, CANT_STAT)); /*stat error*/
    
    // Handle regular files
    if (S_ISREG(buf.st_mode)) {
        printf("Regular file: %s\n", path);
        return search_file(path, pattern, buf.st_size);
    }
    if (S_ISDIR(buf.st_mode)){
        ret = myfunc(path, pattern, &buf, DIRECTORY);
        if(ret != 0){
            return ret;
        }
    }
  
    if ((dp = opendir(path)) == NULL)   /* can’t read directory */
        return(EXIT_FAILURE);

    while ((dirp = readdir(dp)) != NULL){
       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;

        // Build the full path for the subdirectory or file
        n = strlen(path) + strlen(dirp->d_name) + 2; // +2 for '/' and null terminator

        // Ensure that fullpath has enough space for the new directory
        if (n > pattern_len) {
            pattern_len = n * 2; // Double the allocated space to ensure it fits
            fullpath = realloc(fullpath, pattern_len);
            if (!fullpath) {
                perror("realloc failed");
                closedir(dp);
                return -1;
                }
            }
        snprintf(fullpath, pattern_len, "%s/%s", path, dirp->d_name);
        // Append directory name to the path
        strcpy(&fullpath[strlen(path)], dirp->d_name);  // Copy the directory name to the fullpath

       // Get stat info for the entry
        if (lstat(fullpath, &buf) < 0) {
            perror("lstat failed");
            continue; // Skip this entry
        }

        // Recursively process directories or search files
        if (S_ISDIR(buf.st_mode)) {
            ret = helper_func(fullpath, pattern);
        } else if (S_ISREG(buf.st_mode)) {
            ret = search_file(fullpath, pattern, buf.st_size);
        }

        // Stop if an error occurred
        if (ret != 0) {
            closedir(dp);
            return ret;
        }
    }


    if (closedir(dp) < 0){
        fprintf(stderr,"can’t close directory %s", fullpath);
    }
   
    return 0;   

}

static int traverse_directory(char *pathname, char *p)
{   
    fullpath = malloc(file_print_offset + 1);
    if(!fullpath){
         free(fullpath);
          exit_error("malloc() failed");
    }
    if(file_print_offset <= strlen(pathname)){
        file_print_offset = strlen(pathname) * 2; 
        if ((fullpath = realloc(fullpath,file_print_offset)) == NULL) 
            exit_error("realloc failed");      
    }
  
    strcpy(fullpath, pathname);
    return(helper_func(fullpath, p));
  
   
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <pattern>\n", argv[0]);
        return 1;
    }

     char *directory_path = argv[1];  // Can be "."
    char *pattern = argv[2];

    printf("Searching for pattern '%s' in directory '%s':\n", pattern, directory_path);

    int result = traverse_directory(directory_path, pattern);

    if (result == 0) {
        printf("Search completed successfully.\n");
    } else {
        printf("An error occurred during the search. Return code: %d\n", result);
    }

    return 0;
}