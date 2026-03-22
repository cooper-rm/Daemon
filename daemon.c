#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define LOG_PATH "/var/tmp/daemon.log"
#define PID_PATH "/var/tmp/daemon.pid"

// --- Global flags set by signal handlers ---
// 'volatile' tells the compiler not to optimize away reads of these variables,
// because they can change at any time (when a signal arrives).
// 'sig_atomic_t' is a type guaranteed to be read/written atomically,
// so we won't get a half-written value if a signal interrupts us mid-read.
static volatile sig_atomic_t got_sigterm = 0;
static volatile sig_atomic_t got_sighup = 0;

// --- Signal handlers ---
// These do as little as possible — just set a flag.
// The main loop checks the flags and acts on them.
// Doing real work in a signal handler (malloc, printf, etc.) is unsafe
// because the handler can fire in the middle of those same functions.
void handle_sigterm(int sig)
{
    (void)sig;  // unused parameter
    got_sigterm = 1;
}

void handle_sighup(int sig)
{
    (void)sig;
    got_sighup = 1;
}

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

// Write the daemon's PID to a file so other processes can find and signal it.
void write_pid_file(void)
{
    FILE *f = fopen(PID_PATH, "w");
    if (f == NULL) {
        log_message("Failed to write PID file");
        return;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
}

// Remove the PID file on shutdown.
void remove_pid_file(void)
{
    unlink(PID_PATH);
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
        exit(EXIT_FAILURE);
    }
    dup2(devnull, STDIN_FILENO);
    close(devnull);

    // Redirect stdout and stderr to the log file
    int logfd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logfd < 0) {
        exit(EXIT_FAILURE);
    }
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    // --- PID file ---
    // Write our PID to a known file so we can be found and controlled.
    // This also serves as a lock — if the file exists, check if the
    // process is still alive before starting another instance.
    write_pid_file();

    // --- Signal handling ---
    // Register handler functions for signals we care about.
    // sigaction() is preferred over signal() because its behavior is
    // consistent across platforms.
    //
    // SA_RESTART: if a signal interrupts a system call (like sleep),
    // automatically restart the call instead of failing with EINTR.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    // SIGTERM — "please terminate." Sent by 'kill <pid>' by default.
    sa.sa_handler = handle_sigterm;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);

    // SIGHUP — by convention, "reload configuration."
    // Sent by 'kill -HUP <pid>'.
    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, NULL);

    // --- Daemon main loop ---
    char buf[128];
    snprintf(buf, sizeof(buf), "Daemon started: PID = %d", getpid());
    log_message(buf);
    log_message("PID file written to " PID_PATH);

    while (!got_sigterm) {
        // Check if we received SIGHUP (reload signal)
        if (got_sighup) {
            log_message("Received SIGHUP — reloading configuration");
            // In a real daemon, you would re-read config files here.
            got_sighup = 0;
        }

        // This is where the daemon does its actual work.
        // For now, we just sleep. Stage 5 will add real work.
        sleep(5);
        log_message("Daemon is alive");
    }

    // --- Clean shutdown ---
    log_message("Received SIGTERM — shutting down");
    remove_pid_file();
    log_message("PID file removed. Goodbye.");

    return 0;
}
