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

### Stage 3: File Descriptor Management

Close `stdin`, `stdout`, and `stderr` and redirect them to `/dev/null`. Set up file-based logging. This is what makes a daemon truly independent of any terminal.

### Stage 4: PID File and Signal Handling

Write the daemon's PID to a file for process management. Implement signal handlers for `SIGTERM` (clean shutdown) and `SIGHUP` (configuration reload). Build a CLI interface to start and stop the daemon.

### Stage 5: Work Loop

Give the daemon a task — a loop that performs observable work (e.g., directory watching or file-based job processing) to confirm correct behavior.

### Stage 6: Lifecycle Review

Trace the full daemon lifecycle from start to stop. Compare the implementation to how production daemons and service managers (e.g., `systemd`) operate.

## Requirements

- A Unix-like OS (macOS or Linux)
- A C compiler (`gcc` or `clang`)
