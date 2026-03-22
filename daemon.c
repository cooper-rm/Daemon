#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define LOG_PATH "/var/tmp/daemon.log"

// Write a timestamped message to the log file.
void log_message(const char *msg)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(stderr, "[%s] %s\n", timestamp, msg);
    fflush(stderr);
}

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
        exit(EXIT_SUCCESS);
    }

    // --- File descriptor redirection ---
    // The daemon has no terminal, so stdin/stdout/stderr are dangling.
    // We redirect them to well-defined destinations:
    //   stdin  → /dev/null   (daemon reads no interactive input)
    //   stdout → log file    (daemon output goes to a file)
    //   stderr → log file    (daemon errors go to the same file)
    //
    // dup2(fd, target) replaces file descriptor 'target' with a copy of 'fd'.
    // File descriptors are just integers: 0 = stdin, 1 = stdout, 2 = stderr.

    // Redirect stdin to /dev/null
    int devnull = open("/dev/null", O_RDONLY);
    if (devnull < 0) {
        perror("open /dev/null failed");
        exit(EXIT_FAILURE);
    }
    dup2(devnull, STDIN_FILENO);  // STDIN_FILENO = 0
    close(devnull);

    // Redirect stdout and stderr to the log file
    // O_WRONLY: write only
    // O_CREAT:  create the file if it doesn't exist
    // O_APPEND: append to the file instead of overwriting
    // 0644:     file permissions (owner read/write, others read)
    int logfd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logfd < 0) {
        // Can't even open a log file — nothing we can do
        exit(EXIT_FAILURE);
    }
    dup2(logfd, STDOUT_FILENO);   // STDOUT_FILENO = 1
    dup2(logfd, STDERR_FILENO);   // STDERR_FILENO = 2
    close(logfd);

    // --- This is the daemon process ---
    // From this point on, all printf/fprintf output goes to the log file.
    // There is no terminal. stdin reads nothing. We are fully independent.

    char buf[128];
    snprintf(buf, sizeof(buf), "Daemon started: PID = %d, SID = %d",
             getpid(), getsid(0));
    log_message(buf);

    snprintf(buf, sizeof(buf), "Logging to %s", LOG_PATH);
    log_message(buf);

    log_message("Sleeping for 60 seconds...");
    sleep(60);
    log_message("Daemon exiting");

    return 0;
}
