#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <string.h>

#define BUF_SIZE 8096

int main(int argc, char **argv) {
    FILE *fp;
    char buf[BUF_SIZE];
    int n, i = 0;
    

    // 8KB file
    fp = fopen("testfile.txt", "rb");

    // Read up to 1KB at a time -> loop 8 times
    while ((n = fgets(buf, BUF_SIZE, fp ) > 0)) {
            if (buf[strlen(buf) - 1 ] == '\n' )
                buf[strlen(buf) - 1] = 0;
    
            
            printf("i=%d:n=%d, bytes\n",i++, n);
            n++;
    }
    
    fclose(fp);
    printf("sum=%d\n",i);
    return 0;
}