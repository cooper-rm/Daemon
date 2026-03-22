#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    // PID is the unique ID of the current process.
    printf("Before fork: PID = %d\n", getpid());

    // fork() creates a new child process with a copy of the parent's memory
    // (via copy-on-write) and adds it to the scheduler's ready queue.
    // Returns the child's PID to the parent, and 0 to the child.
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* Parent process */
        printf("Parent: PID = %d, created child PID = %d\n", getpid(), pid);
        printf("Parent: exiting\n");
        exit(EXIT_SUCCESS);
    }

    /* Child process (pid == 0) */
    printf("Child:  PID = %d, parent PID = %d\n", getpid(), getppid());
    printf("Child:  sleeping for 30 seconds so you can inspect with ps\n");
    sleep(30);
    printf("Child:  exiting\n");

    return 0;
}
