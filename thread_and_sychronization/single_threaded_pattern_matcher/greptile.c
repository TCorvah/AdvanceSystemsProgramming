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

// Global variables initialized in `main()` based on command-line arguments
size_t pattern_len;
size_t file_print_offset;
static char *fullpath;
struct stat buf;
static long nreg, ndir;

// Function for handling errors and printing a message before exiting
static inline void exit_error(char *msg) {
    perror(msg);
    exit(2);
}

/* The following function increment the regular file and the directory and restrict all
searches between regular files and directories. 
*/
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
/* an array of extensions for pattern search*/
const char *allowed_extensions[] = {".c", ".cpp", ".h", ".py", ".txt", ".md"};


const int num_allowed_extensions = sizeof(allowed_extensions) / sizeof(allowed_extensions[0]);

/* the following function takes in a filename and check to ensure only the allowable
* file extensions are consider in the search. The time to determine a file extension is linear
* in the length of the allowed extensions. 
*/
static int has_allowed_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');  // Find the last dot in the filename
    if (!dot || dot == filename) {
        return 0;  // No extension found, 
    }
    for (int i = 0; i < num_allowed_extensions; i++) {
        if (strcmp(dot, allowed_extensions[i]) == 0) {
            return 1;  // Extension matches one of the allowed
        }
    }
    return 0;  // Extension not in allowed list
}
/* 
 * `search_file` searches a regular file for a given pattern and prints matching lines
 * with line numbers and colors. The function is O(N) in runtime complexity.
 */
static int search_file(char *file, char *pattern, off_t file_size) {  
    FILE *fptr;
    char *match;
    char *path = malloc(file_size);
    if(!path){
        exit_error("malloc failed");
    }
    //stat file, to prevent symbolic link following
    if(lstat(file, &buf) < 0){
        free(path);
        exit_error("can't stat");

    }
    //open file for readonly
    fptr = fopen(file, "r" );
        if(!fptr){
            free(path);
            exit_error("can't open file");
        }
    int num = 1;
    //file is read line by line and each line it checks if the
    // pattern exist using strstr
    while(fgets(path, buf.st_size, fptr ) != NULL){
        while(!strchr(path, '\n') && !feof(fptr)){ //if the path is too long, we realloc
            file_size *= 2;
            path = realloc(path, file_size);
            if(!path){
                fclose(fptr);
                exit_error("realloc failed");
            }
            fgets(path + strlen(path), file_size - strlen(path), fptr);
        }
        if ((match = strstr(path, pattern))) { // the print out is buggy and I am working on it
            printf("%s\n:", file);  // Print the file path
           // Print the line number in red
            printf(COLOR_RED "%d\n: " COLOR_RESET, num);
            
            // Print the part of the line before the match
            char *line_part_before_match = path;
            size_t before_match_len = match - path;
            printf("%.*s", (int)before_match_len, line_part_before_match);  // Print the part before match

            // Print the matched part in green
            printf(COLOR_GREEN "%.*s" COLOR_RESET, (int)strlen(pattern), match);  // Print the match in green

            // Print the remaining part of the line after the match
            printf("%s", match + strlen(pattern));  // Print the part after match
        }
        num++;
    }
    fclose(fptr);
    free(path);

    
return(0);
}

/* 
 * `myfunc` handles different file types, increments counters for regular files 
 * and directories, and handles errors like permission denial or stat errors.
 */                 
static int  helper_func(char *path, char *pattern) {
    struct dirent *dirp;
    DIR *dp;
    int ret;

    if(lstat(path, &buf) < 0)
        return(myfunc(path, pattern, &buf, CANT_STAT)); /*stat error*/
    
    // Handle regular files and extensions, I use the buffer size as it
    // is the size in bytes of the file contents
    if (S_ISREG(buf.st_mode)) {
        if(has_allowed_extension(path)){
            return search_file(path, pattern, buf.st_size);
        }  
        return 0;       
    }
    if (!S_ISDIR(buf.st_mode)){
            return 0;
    }
  
    if ((dp = opendir(path)) == NULL) {  /* can’t read directory */
        return(EXIT_FAILURE);
    }
    // Read the directory and recursively process files in the directory
    while ((dirp = readdir(dp)) != NULL){
       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;
        
        char subpath[MAXLINE];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, dirp->d_name);
         if((ret = helper_func(subpath, pattern)) != 0){
            break;
         }
    }
 
    if (closedir(dp) < 0){
        fprintf(stderr,"can’t close directory %s", fullpath);
    }   
    return 0;   
}
/* 
 * `traverse_directory` is the entry point for recursively searching a directory 
 * for files that match the pattern.
 */
static int traverse_directory(char *pathname, char *p)
{   
    char initial_path[MAXLINE];
    snprintf(initial_path, sizeof(initial_path), "%s", pathname);
    return helper_func(initial_path, p);  
}

/* 
 * `main` handles the command-line arguments, initializes the search, and
 * outputs the result of the search.
 */
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

    return result;
}