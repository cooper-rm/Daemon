# Daemon

A Unix daemon built from scratch in C, designed as a learning exercise to understand the OS-level mechanics of background processes.

## Overview

This project implements a daemon by hand — no libraries, no shortcuts. Each stage introduces a core concept of the Unix process model, building toward a fully functional background service.

## Build Stages

### Stage 1: Process Forking

Introduce `fork()` and observe parent/child process relationships using `ps`. Re-establish familiarity with the Unix process model.

#### Key Concepts

**Processes, Memory, and Threads**

Every process has its own isolated virtual address space. Threads exist within a process and share its memory. The hierarchy is:

```
CPU Cores  ←  managed by the OS scheduler, shared by all processes
  └── Processes  ←  isolated memory, unique PID
        └── Threads  ←  share the parent process's memory
```

**`fork()` System Call**

`fork()` creates a new process by cloning the calling process. The child receives a copy of the parent's memory via copy-on-write — both processes reference the same physical pages until one writes, at which point the OS copies only the modified page. The return value distinguishes the two: the parent receives the child's PID, the child receives `0`.

**Scheduling and Context Switching**

A new process does not immediately claim a CPU core. The OS adds it to a ready queue, and the scheduler assigns it time on available cores alongside every other process on the system. Context switching — saving one process's state and loading another's — happens thousands of times per second, transparently. There is no distinction at the CPU level between foreground and background processes.

**What Makes a Process "Background"**

A background process is not a special scheduling concept. It is simply a process that is:

- Not attached to a controlling terminal
- Not blocking on user input
- Not terminated when the user's shell session ends

The daemon pattern is about **detaching from the terminal**, not about changing how the process is scheduled or executed.

### Stage 2: Session Creation and the Double Fork

Implement the classic daemonization pattern: fork once, call `setsid()` to create a new session and detach from the controlling terminal, then fork again to prevent reacquisition of a terminal. Each step exists for a specific reason — we cover why.

#### Key Concepts

**Sessions and Process Groups**

A session is the OS's grouping of everything that belongs to one terminal window. When a terminal opens, the shell becomes the **session leader**. Every command launched from that shell joins the session. When the terminal closes, the OS sends `SIGHUP` to the session, killing all its processes.

A process group is a subset of a session — related processes (e.g., a pipeline like `ls | grep foo`) grouped together so they can receive signals as a unit.

**The Controlling Terminal (TTY)**

The controlling terminal is the terminal device attached to a session. It provides `stdin`/`stdout` and is the channel through which signals like `SIGHUP` reach the session's processes. A TTY value of `??` in `ps` output indicates no controlling terminal is attached.

**`setsid()` System Call**

`setsid()` creates a new session with no controlling terminal. The calling process becomes the session leader and sole member. This is what detaches a process from the original terminal — closing that terminal no longer affects it.

**The Double Fork Pattern**

The full daemonization sequence:

1. **First `fork()` + parent exits** — the child is not a process group leader, which is a prerequisite for calling `setsid()`. The shell gets its prompt back.
2. **`setsid()`** — the child creates a new session. It is now the session leader, detached from the original terminal. However, as session leader it *could* reacquire a controlling terminal by opening one.
3. **Second `fork()` + session leader exits** — the grandchild inherits the session but is not the session leader. Only session leaders can acquire a controlling terminal, so the grandchild is permanently detached.

```
Terminal Session (still alive, owned by the shell)
  └── shell (session leader)

New Session (leaderless, no controlling terminal)
  └── grandchild = the daemon
```

**TTY vs. Session Leader**

These are independent concepts. TTY (`??`) means no terminal is attached. An absent session leader means no process can attach one. The daemon has neither — no terminal, and no way to get one.

### Stage 3: File Descriptor Management

Close `stdin`, `stdout`, and `stderr` and redirect them to `/dev/null`. Set up file-based logging. This is what makes a daemon truly independent of any terminal.

#### Key Concepts

**File Descriptors**

Every open file, socket, or device is represented by an integer called a file descriptor. The first three are reserved by convention:

| FD | Name     | Default Target |
|----|----------|----------------|
| 0  | `stdin`  | keyboard (terminal) |
| 1  | `stdout` | screen (terminal) |
| 2  | `stderr` | screen (terminal) |

After detaching from the terminal, these descriptors are dangling — they still reference a terminal that may no longer exist. Writing to them is undefined behavior.

**`dup2()` System Call**

`dup2(source_fd, target_fd)` replaces `target_fd` with a copy of `source_fd`. This is how we rewire the standard file descriptors:

```
Before (dangling):            After (redirected):
  0 (stdin)  → terminal        0 (stdin)  → /dev/null
  1 (stdout) → terminal        1 (stdout) → /var/tmp/daemon.log
  2 (stderr) → terminal        2 (stderr) → /var/tmp/daemon.log
```

**`/dev/null`**

A special device that discards all writes and returns EOF on reads. Redirecting `stdin` to `/dev/null` ensures the daemon never blocks waiting for input that will never come.

**Daemon Logging**

With no terminal, a daemon communicates through log files. Key practices:

- Use `O_APPEND` when opening the log file so multiple writes don't overwrite each other
- Call `fflush()` after each write to ensure messages are persisted immediately — if the daemon crashes, unbuffered messages are lost
- Timestamp every message, since there is no interactive context to infer when events occurred

### Stage 4: PID File and Signal Handling

Write the daemon's PID to a file for process management. Implement signal handlers for `SIGTERM` (clean shutdown) and `SIGHUP` (configuration reload). Build a CLI interface to start and stop the daemon.

### Stage 5: Work Loop

Give the daemon a task — a loop that performs observable work (e.g., directory watching or file-based job processing) to confirm correct behavior.

### Stage 6: Lifecycle Review

Trace the full daemon lifecycle from start to stop. Compare the implementation to how production daemons and service managers (e.g., `systemd`) operate.

## Requirements

- A Unix-like OS (macOS or Linux)
- A C compiler (`gcc` or `clang`)
