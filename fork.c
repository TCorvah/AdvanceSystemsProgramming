#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	int pfds[2];
	char buf[30];
    pid_t pid;

	
    if (pipe(pfds) < 0)
               perror("pipe error");
    if ((pid = fork()) < 0) {
               perror("fork error");
    }
	if (pid == 0) {
		printf(" CHILD: reading from pipe: %ld\n", (long)pid);
        //printf("PARENT: writing to the pipe\n");
		//write(pfds[1], "test", 5);
        read(pfds[0], buf, 5);
		printf("CHILD: read \"%s\"\n", buf);
        //wait(NULL);
		exit(0);
	} else if(pid > 0){
		printf("PARENT: writting to the pipe: %d\n", getppid());
        write(pfds[1], "test", 5);
        //printf(" CHILD: reading from pipe\n");
		// This read blocks.  Fortunately, the child
		// writes to it in non-blocking mode and then closes it,
		// causing the OS trigger this process to wake up.
		//read(pfds[0], buf, 5);
        printf(" PARENT: wait for child\n");
		//printf("CHILD: read \"%s\"\n", buf);
		wait(NULL);
        //exit(0);
	}

	return 0;
}