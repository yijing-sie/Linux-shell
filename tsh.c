/**
 * @file tsh.c
 * @brief A tiny shell program supports a simple form of job control and I/O
 * redirection, repeatedly prints a prompt, waits for a command line on stdin,
 * and then carries out some action, as directed by the contents of the command
 * line.
 *
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.
 *  Follow the 15-213/18-213/15-513 style guide at
 *  http://www.cs.cmu.edu/~213/codeStyle.html.>
 *
 * @author Yi-Jing Sie <ysie@andrew.cmu.edu>
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void wait_fg(void);
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);

/**
 * @brief it first redirects stderr to stdout, then parses the command line,
 * creates environment variable, installs the signal handlers, and
 *
 * @param argc number of arguments
 * @param argv argument table
 *
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
    }

    return -1; // control never reaches here
}

/**
 * @brief wait for foreground job
 */
void wait_fg(void) {
    sigset_t mask_all, mask_empty;
    sigfillset(&mask_all);
    sigemptyset(&mask_empty);
    sigprocmask(SIG_BLOCK, &mask_all, NULL);
    while (fg_job()) {
        sigsuspend(&mask_empty);
    }
    sigprocmask(SIG_SETMASK, &mask_empty, NULL);
    return;
}

/**
 * @brief Main routine that parses, interprets, and executes the command
 * line
 *
 *
 * NOTE: The shell is supposed to be a long-running process, so this
 * function (and its helpers) should avoid exiting on error.  This is not to
 * say they shouldn't detect and print (or otherwise handle) errors!
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;
    jid_t jid;
    pid_t pid;
    int in_fd = -1;
    int out_fd = -1;
    sigset_t mask_empty, mask_all, prev;
    const char *fst_arg;

    sigfillset(&mask_all);
    sigemptyset(&mask_empty);

    // Parse command line
    parse_result = parseline(cmdline, &token);

    // Check parseline error or empty parseline
    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
        return;
    }
    // Check input file
    if (token.infile) {
        if ((in_fd = open(token.infile, O_RDONLY)) < 0) {
            sio_printf("%s: %s\n", token.infile, strerror(errno));
            return;
        }
    }
    // Check output file
    if (token.outfile) {
        if ((out_fd = open(token.outfile, O_CREAT | O_TRUNC | O_WRONLY,
                           S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
            sio_eprintf("%s: %s\n", token.outfile, strerror(errno));
            return;
        }
    }

    /* Built-in jobs command */
    if (token.builtin == BUILTIN_JOBS) {
        sigprocmask(SIG_BLOCK, &mask_all, &prev); /* block all signals */
        if (token.outfile) {
            list_jobs(out_fd);
        } else {
            list_jobs(STDIN_FILENO);
        }
        sigprocmask(SIG_SETMASK, &mask_empty, NULL);
    }

    /* Built-in quit command */
    if (token.builtin == BUILTIN_QUIT) {
        _exit(0);
    }

    /* Built-in fg/bg job command */
    if (token.builtin == BUILTIN_FG || token.builtin == BUILTIN_BG) {
        // MISSING PID/JID
        if ((fst_arg = token.argv[1]) == NULL) {
            sio_printf("%s command requires PID or %%jobid argument\n",
                       token.argv[0]);
            return;
        }
        /* GETTING JID/PID */
        // case 1:JID
        bool pid_ = false;
        if (fst_arg[0] == '%') {
            if ((jid = atoi(&fst_arg[1])) == 0) {
                sio_printf("%s: argument must be a PID or %%jobid\n",
                           token.argv[0]);
                return;
            }
        } else {
            if ((pid = atoi(fst_arg)) == 0) {
                sio_printf("%s: argument must be a PID or %%jobid\n",
                           token.argv[0]);
                return;
            }
            pid_ = true;
        }
        sigprocmask(SIG_BLOCK, &mask_all, &prev);
        if (pid_) {
            jid = job_from_pid(pid);
        }
        if (!job_exists(jid)) {
            sio_printf("%%%d: No such job\n", jid);
            sigprocmask(SIG_SETMASK, &prev, NULL); /* restore blockmask */
            return;
        }
        // fg job command
        if (token.builtin == BUILTIN_FG) {
            job_set_state(jid, FG);
            kill(-(job_get_pid(jid)), SIGCONT);    /* send SIG_CONT*/
            sigprocmask(SIG_SETMASK, &prev, NULL); /* restore blockmask */
            wait_fg();
            return;
        } else {
            // bg job command
            job_set_state(jid, BG);
            sio_printf("[%d] (%d) %s\n", jid, job_get_pid(jid),
                       job_get_cmdline(jid));
            kill(-(job_get_pid(jid)), SIGCONT);  /* send SIG_CONT*/
            sigprocmask(SIG_BLOCK, &prev, NULL); /* restore blockmask */
            return;
        }
    }

    if (token.builtin == BUILTIN_NONE) {
        sigprocmask(SIG_BLOCK, &mask_all, &prev); /* BLOCK ALL SIG */
        // fork a child process
        if ((pid = fork()) == 0) {
            sigprocmask(SIG_SETMASK, &mask_empty, NULL);
            setpgid(0, 0);
            if (token.infile) {
                dup2(in_fd, STDIN_FILENO);
            }
            if (token.outfile) {
                dup2(out_fd, STDOUT_FILENO);
            }
            if (execve(token.argv[0], token.argv, environ) < 0) {
                sio_printf("%s:%s\n", token.argv[0], strerror(errno));
                _exit(1);
            }
        }
        /* background job */
        sigprocmask(SIG_BLOCK, &mask_all, &prev);
        if (parse_result == PARSELINE_BG) {
            jid = add_job(pid, BG, cmdline);
            sio_printf("[%d] (%d) %s\n", jid, pid, cmdline);
        }
        /* foreground job */
        if (parse_result == PARSELINE_FG) {
            jid = add_job(pid, FG, cmdline);
            wait_fg();
        }
        sigprocmask(SIG_SETMASK, &mask_empty, NULL);
    }

    /* CLOSE FILE */
    if (token.infile) {
        close(in_fd);
    }
    if (token.outfile) {
        close(out_fd);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief Reap a child and handle SIGINT and SIGTSTP appropriately
 *
 */
void sigchld_handler(int sig) {
    // save errno before running haddling signal child
    int old_errno = errno;
    int status;
    sigset_t mask_all, mask_empty;
    pid_t pid;
    jid_t jid;

    sigfillset(&mask_all);
    sigemptyset(&mask_empty);
    /* immediately return 0 or stopped/terminated Child PID */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Block ALL signals */
        jid = job_from_pid(pid);                 /* Get jid */

        // Child is stopped
        if (WIFSTOPPED(status)) {
            job_set_state(jid, ST);
            sio_printf("Job [%d] (%d) stopped by signal %d\n", jid, pid,
                       WSTOPSIG(status));
        }
        // Child terminated because of a signal that was not caught
        if (WIFSIGNALED(status)) {
            delete_job(jid);
            sio_printf("Job [%d] (%d) terminated by signal %d\n", jid, pid,
                       WTERMSIG(status));
        }
        // Chulde terminated normally
        if (WIFEXITED(status)) {
            delete_job(jid);
        }
        sigprocmask(SIG_SETMASK, &mask_empty, NULL);
    }
    // restore errno before return
    errno = old_errno;
}

/**
 * @brief Handle SIGINT for foregorund jobs
 *
 */
void sigint_handler(int sig) {
    // save errno before handling
    int old_errno = errno;
    sigset_t mask_all, mask_empty;
    pid_t pid;
    jid_t jid;

    sigfillset(&mask_all);
    sigemptyset(&mask_empty);
    sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Block ALL signals */
    // If jid is a foreground job
    if ((jid = fg_job()) != 0) {

        pid = job_get_pid(jid);
        sigprocmask(SIG_SETMASK, &mask_empty, NULL);
        kill(-pid, SIGINT); /* send all processes in jid SIGINT*/
    }
    // restore errno before return
    errno = old_errno;
    return;
}

/**
 * @brief Handles SIGTSTP signals for foreground jobs
 *
 */
void sigtstp_handler(int sig) {
    // save errno before handling
    int old_errno = errno;
    sigset_t mask_all, mask_empty;
    pid_t pid;
    jid_t jid;

    sigfillset(&mask_all);
    sigemptyset(&mask_empty);
    sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Block ALL signals */
    // If jid is a foreground job
    if ((jid = fg_job()) != 0) {

        pid = job_get_pid(jid);
        sigprocmask(SIG_SETMASK, &mask_empty, NULL);
        kill(-pid, SIGTSTP); /* send all processes in jid SIGTSTP*/
    }
    // restore errno before return
    errno = old_errno;
    return;
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}
