# Linux-shell

Intro to Computer Systems assignment :



### [tsh.c](tsh.c) is an interactive command-line interpreter that runs programs on behalf of the user

* It runs each executable in its own child process and supports the following built-in commands:  

1.   The `quit` command terminates the shell.
2.   The `jobs` command lists all background jobs.
3.   It supports both input and output redirection in the same command line. For example:
```
tsh> /bin/cat < foo > bar
```

4.   It redirects the output from the built-in jobs command. For example:
```
tsh> jobs > foo
```
5.   The `bg job` command resumes job  and then runs it in the background. The `job` argument can be either a PID or a JID.
6.   The `fg job` command resumes job and then runs it in the foreground. The `job` argument can be either a PID or a JID.
7.   If the command line ends with an ampersand (&), then tsh should run the job in the background.
8.  It handles `SIGINT` and `SIGTSTP` appropriately when typing `Crtl + C` or `Crtl + Z` respectively

* Each job can be identified by either a process ID (PID) or a job ID (JID). The latter is a positive integer assigned by tsh. JIDs are denoted on the command line with the prefix “%”. For example, “%5” denotes a JID of 5, and “5” denotes a PID of 5.






