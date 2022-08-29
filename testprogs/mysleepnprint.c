/*
 * mysleepnprint.c - a program to test that fg correctly waits for the job exit.
 *
 * A correctly written shell will echo the SIGTSTP back to the child.
 * When continuing the programm waits two second before printing the message
 * Bad shells would then print prompt (and possibly quit) before the message.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include "config.h"

#include "testprogs/helper.h"

void sigalrm_handler(int signum) {
    exit(0);
}

int main(void) {
    Signal(SIGALRM, sigalrm_handler);
    alarm(JOB_TIMEOUT);

    if (kill(getppid(), SIGTSTP) < 0) {
        perror("kill");
        exit(1);
    }

    usleep(250000);
    printf("Hello, World!\n");
    exit(0);
}
