/************************************************************************
 *	CSE344 - System Programming - Homework1 Part3						*
 *	Author: Esra Eryılmaz												*
 *	Date: 28.03.2023													*
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define OPEN_FLAGS (O_CREAT | O_RDWR)
#define OPEN_MODES (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)	// -rw-r--r--

// function declarations
int offsetCheck();


int main(int argc, char *argv[])
{
	offsetCheck();
	return 0;
}

/*
 * This function verify that duplicated file descriptors share a file offset value and open file
 */
int offsetCheck() {
	int fd1, fd2;
	off_t offset1, offset2;
	struct stat stat1, stat2;

	fd1 = open("testOffset.txt", OPEN_FLAGS, OPEN_MODES);	// open file for test
	if (fd1 == -1) {
		perror("open");
		return 1;
	}

	const char* str1 = "Hello from fd1!\n";
	if (write(fd1, str1, strlen(str1)) == -1) {			// write str to the file
		perror("write");
		return 1;
	}

	offset1 = lseek(fd1, 0, SEEK_CUR);		// get the current offset of the file

	fd2 = dup(fd1);		// duplicate the fd1
	if (fd2 == -1) {
		perror("dup");
		return 1;
	}

	offset2 = lseek(fd2, 0, SEEK_CUR);		// get the current offset of the duplicated fd2

	printf("offset1 : %ld, offset2 : %ld\n", offset1, offset2);

	//  Verify that duplicated file descriptors share a file offset value or not.
	if (offset1 != offset2) {
		printf("DUPLICATED FILE DESCRIPTORS DO NOT SHARE A FILE OFFSET VALUE !!!!\n");
	}
	else {
		printf("DUPLICATED FILE DESCRIPTORS SHARE A FILE OFFSET VALUE !!!!\n");
	}

	if (fstat(fd1, &stat1) == -1) {
		perror("fstat");
		return 1;
	}
	if (fstat(fd2, &stat2) == -1) {
		perror("fstat");
		return 1;
	}

	// Verify that duplicated file descriptors open a same file
	// The "st_ino" and "st_dev", taken together, uniquely identify the file
	if (stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev) {
		printf("DUPLICATED FILE DESCRIPTORS OPEN A SAME FILE!!!!\n");
	}
	else {
		printf("DUPLICATED FILE DESCRIPTORS DO NOT OPEN A SAME FILE!!!!\n");
	}

	close(fd1);
	close(fd2);
}
