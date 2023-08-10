/************************************************************************
 *	CSE344 - System Programming - Homework1 Part2						*
 *	Author: Esra Eryılmaz												*
 *	Date: 28.03.2023													*
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define OPEN_FLAGS (O_CREAT | O_WRONLY)
#define OPEN_MODES (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)	// -rw-r--r--

// function declarations
int myDup(int oldfd);
int myDup2(int oldfd, int newfd);


/*
 * Driver code to test myDup() and myDup2() functions.
 */
int main(int argc, char *argv[])
{
/***************	TEST myDup()	***************/
	int fd1 = open("file1.txt", OPEN_FLAGS, OPEN_MODES);
	if (fd1 == -1) {
		perror("open");
		return 1;
	}
	int fd2 = myDup(fd1);		// calling myDup() !!!!
	if (fd2 == -1) {
		perror("myDup");
		return 1;
	}
	printf("Test myDup()  =>  int fd2 = myDup(fd1);\nfd1 = %d,\nfd2 = %d\n\n", fd1, fd2);

	const char* str1 = "Hello from fd1 for myDup()!\n";
	if (write(fd1, str1, strlen(str1)) == -1) {	// write to the original fd1
		perror("write");
		return 1;
	}
	const char* str2 = "Hello from fd2 for myDup()!!\n";
	if (write(fd2, str2, strlen(str2)) == -1) {	// write to the duplicated fd2
		perror("write");
		return 1;
	}

/***************	TEST myDup2()	***************/
	int fd3 = open("file2.txt", OPEN_FLAGS, OPEN_MODES);
	if (fd3 == -1) {
		perror("open");
		return 1;
	}
	int fd4 = myDup2(fd1, fd3);		// calling myDup2() !!!!
	if (fd4 == -1) {
		perror("dup2");
		return 1;
	}
	printf("Test myDup2()  =>  int fd4 = myDup2(fd1, fd3);\nfd1 = %d,\nfd3 = %d,\nfd4 = %d\n\n", fd1, fd3, fd4);

	const char* str3 = "Hello from fd1 for myDup2()!\n";
	if (write(fd1, str3, strlen(str3)) == -1) {
		perror("write");
		return 1;
	}
	const char* str4 = "Hello from fd3 for myDup2()!!\n";
	if (write(fd3, str4, strlen(str4)) == -1) {
		perror("write");
		return 1;
	}

	close(fd1);
	close(fd2);
	close(fd3);
	close(fd4);

	printf("And also you can look at the created files in the part2 directory for the writing test results.\n");

	return 0;
}

/*
 * This function implements dup() using fcntl()
 */
int myDup(int oldfd) {
	// creating new file descriptor
	int newfd = fcntl(oldfd, F_DUPFD); // F_DUPFD: Return a new file descriptor which shall be the lowest numbered available
	if (newfd == -1) {
		perror("fcntl");
		return -1;
	}
	// if succedd return new file descriptor
	return newfd;
}

/*
 * This function implements dup2() using fcntl()
 */
int myDup2(int oldfd, int newfd) {
	// special case : where oldfd equals newfd
	if (oldfd == newfd) {
		if (fcntl(oldfd, F_GETFL) == -1) { // check whether oldfd is valid
			errno = EBADF;	// if oldfd is not valid
			return -1;
		}
		// if oldfd == newfd, no need to duplicate, just return oldfd.
		return oldfd;
	}

	// close newfd, because one of dup2()'s tasks is to turn it off.
	close(newfd);

	// actual dup2() part is here ; create newfd which is a duplicate of oldfd
	int newlyCreatedfd = fcntl(oldfd, F_DUPFD, newfd);
	if (newlyCreatedfd == -1) {
		perror("fcntl");
		return -1;
	}

	return newlyCreatedfd;
}
