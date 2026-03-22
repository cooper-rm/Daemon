#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(void)
{
    printf("Original process: PID = %d, SID = %d\n", getpid(), getsid(0));

    // --- First fork ---
    // Create a child process so we can call setsid().
    // setsid() fails if the caller is already a process group leader,
    // which the original process might be. The child is guaranteed not to be.
    pid_t pid = fork();
    if (pid < 0) {
        perror("first fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // Parent exits — the shell gets its prompt back,
        // and the child is no longer tied to the shell's process group.
        printf("First parent (PID %d): exiting\n", getpid());
        exit(EXIT_SUCCESS);
    }

    // --- setsid() ---
    // Create a new session. This does three things:
    //   1. Our process becomes the session leader of a NEW session
    //   2. Our process becomes the process group leader of a new process group
    //   3. The new session has NO controlling terminal
    // We are now fully detached from the original terminal.
    if (setsid() < 0) {
        perror("setsid failed");
        exit(EXIT_FAILURE);
    }

    printf("After setsid: PID = %d, new SID = %d\n", getpid(), getsid(0));

    // --- Second fork ---
    // The process is currently a session leader, which means it COULD
    // reacquire a controlling terminal by opening one (e.g., /dev/tty).
    // Forking again and exiting the parent ensures the final process
    // is NOT a session leader and can never acquire a terminal.
    pid = fork();
    if (pid < 0) {
        perror("second fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        printf("Second parent / session leader (PID %d): exiting\n", getpid());
        exit(EXIT_SUCCESS);
    }

    // --- This is the daemon process ---
    // Not a session leader. No controlling terminal. Fully detached.
    printf("Daemon: PID = %d, SID = %d, PPID = %d\n",
           getpid(), getsid(0), getppid());
    printf("Daemon: sleeping for 60 seconds — inspect with:\n");
    printf("  ps -o pid,ppid,pgid,sid,tty,comm -p %d\n", getpid());

    sleep(60);
    printf("Daemon: exiting\n");

    return 0;
}
