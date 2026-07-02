/*
 * spawn_example.c
 *
 */
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// will never be called
void sigfunc( int unused ) {
	return;
}

int main(int argc, char **argv, char **envp)
{
	pid_t pid;
	int child_status;
	sigset_t set;
	int ret;

	/* Avoid races:
	 * We need to attach a signal handler to SIGCHLD otherwise
	 * if the child exits before we get to our sigwaitinfo() call
	 * the default ignore behaviour will cause the signal to be
	 * discarded.  But, if we're going to handle it, we also must
	 * mask it before we attach our handler, so that our handler
	 * doesn't get called, rather than signal being pending until
	 * we look for it with sigwaitinfo().
	 */
	if (sigemptyset(&set) == -1) {
		perror("sigemptyset");
		exit(EXIT_FAILURE);
	}
	if (sigaddset(&set, SIGCHLD ) == -1) {
		perror("sigaddset");
		exit(EXIT_FAILURE);
	}
	if ((ret = pthread_sigmask( SIG_BLOCK, &set, NULL )) != EOK) {
		fprintf(stderr, "pthread_sigmask failed: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	if (signal(SIGCHLD, sigfunc) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}

	printf("my (parent) pid is %d\n", getpid() );

	/* launch the child process
	 * Note: depending on how your target is laid out, you may have
	 * to change the path to the sleep program
	 */
	char *child_argv[3] = {"sleep", "30", NULL };
	ret = posix_spawn(&pid, "/system/xbin/sleep", NULL, NULL, child_argv, envp);
	if(ret != EOK) {
		fprintf(stderr, "posix_spawn failed (%d) '%s'\n", ret, strerror(ret));
		exit(EXIT_FAILURE);
	}

	printf("child pid is %d\n", pid);
	printf("View the process list in the IDE or at the command line.\n");
	printf("In the IDE Target Navigator menu try group->by PID family\n");
	printf("With pidin, try 'pidin family' to get parent/child information.\n");

	/* wait for the SIGCHLD signal, or return immediately if already pending */
	if (sigwaitinfo(&set, NULL ) == -1) {
		perror("sigwaitinfo");
		exit(EXIT_FAILURE);
	}

	printf("Child has died, pidin should now show it as a zombie\n");
	sleep(30);

	/* get the status of the dead child and clean up the zombie */
	pid = wait(&child_status);
	if (pid == -1) {
		perror("wait");
		exit(EXIT_FAILURE);
	}
	printf("child process: %d, died with status %x\n", pid, child_status);
	printf("Zombie is now gone as we've waited on the child process.\n");

	sleep(30);
	return 0;
}

