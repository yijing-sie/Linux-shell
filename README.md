# Linux-shell

Intro to Computer Systems assignment. This assignment was built and (supposed to) run in Linux :



### [tsh.c](tsh.c) is an interactive command-line interpreter that runs programs on behalf of the user

To run my shell, type `tsh` to the command line:
```
linux> ./tsh
tsh> [type commands for tsh to run]
```
* Each command may be either be the path to an executable file (e.g., ```tsh> /bin/ls```) or a builtin command (e.g., ```tsh> quit```)

* It runs each executable in its own child process and supports the following built-in commands:  

1.   The `quit` command terminates the shell.
2.   The `jobs` command lists all background jobs.
3.   It supports both input and output redirection in the same command line. For example:
```
tsh> /bin/cat < foo > bar
```

4.   It can redirect the output from the built-in `jobs` command. For example:
```
tsh> jobs > foo
```
5.   The `bg` *job* command resumes *job*  and then runs it in the background. The *job* argument can be either a PID or a JID.
6.   The `fg` *job* command resumes *job* and then runs it in the foreground. The *job* argument can be either a PID or a JID.
7.   If the command line ends with an ampersand (&), then tsh should run the job in the background; otherwise, in the foreground. 
8.  It handles `SIGINT` and `SIGTSTP` appropriately when typing `Crtl + C` or `Crtl + Z` respectively

> Each job can be identified by either a process ID (PID) or a job ID (JID). The latter is a positive integer assigned by tsh. JIDs are denoted on the command line with the prefix “%”. For example, “%5” denotes a JID of 5, and “5” denotes a PID of 5.
### Example of running an echo command via tsh


https://user-images.githubusercontent.com/84282744/187297643-541e762a-a3ad-4029-8ec5-b1f00ae5358a.mp4



### Evaluation
* To test the implemented shell performance, the professor provided a driver program which runs 33 trace files on the [tsh.c](tsh.c), and to help detect race conditions in the codes, the driver runs each trace file 3 times. 

> All the trace files can be found in [traces](traces)

* The goal is to pass each of the runs to for all trace files get full points
Here is the results for my shell (skip to the end to see the result score):




https://user-images.githubusercontent.com/84282744/187299591-c4e0cbcc-ea0a-436e-b47d-81915e7ed420.mp4





