/*
 * Demonstrate recovery of a shared-memory mutex when a process dies while
 * holding the mutex.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/wait.h>

typedef struct {
	pthread_mutex_t	mtx;
	// you'd usually have other members here for whatever you're sharing
} shmem_t;

int main(int argc, char *argv[])
{
    int ret;

    // create an anonymous shared memory object
    int fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    ret = ftruncate(fd, 4096);
    if (ret == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    shmem_t *ptr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // don't need fd anymore, so close it
    close(fd);

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    ret = pthread_mutex_init(&ptr->mtx, &mattr);
    // robust mutexes require a kernel sync object, so init could fail, so check
    if (ret != EOK) {
    	fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(ret));
    	exit(EXIT_FAILURE);
    }


    pid_t pid = fork();
    if (pid == -1) {
    	perror("fork");
    	exit(EXIT_FAILURE);
    }

    if (pid == 0) {
    	// we're the child, lock mutex and exit
        ret = pthread_mutex_lock(&ptr->mtx);
        if (ret != EOK) {
        	fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(ret));
        	exit(EXIT_FAILURE);
        }
        printf("child exiting with the mutex locked\n");
        exit(EXIT_SUCCESS);
    }

    // we're the parent, wait for the child process to die
    int tstatus;
    pid = waitpid(pid, &tstatus, 0);
    if (pid == -1) {
    	perror("waitpid");
    	exit(EXIT_FAILURE);
    }

    if (WIFEXITED(tstatus)) {
    	if (WEXITSTATUS(tstatus) == EXIT_SUCCESS) {
    		// child process died after locking the mutex
    		// try to lock it

    		ret = pthread_mutex_lock(&ptr->mtx);
    	    if (ret == EOWNERDEAD) {
    	    	// the child process died with the mutex locked and now
    	    	// we have the lock

    	    	// we don't have any data state to recover but we would do that
    	    	// here if we did

    	    	// recover the mutex
    	    	ret = pthread_mutex_consistent(&ptr->mtx);
    	    	if (ret != EOK) {
    	    		fprintf(stderr, "pthread_mutex_consistent failed: %s\n", strerror(ret));
    	    		exit(EXIT_FAILURE);
    	    	}

				printf("parent recovered the mutex\n");

				// unlock it
                ret = pthread_mutex_unlock(&ptr->mtx);
                if (ret != EOK) {
                	fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(ret));
                	exit(EXIT_FAILURE);
                }
    	    }
    	}
    }

    return 0;
}
