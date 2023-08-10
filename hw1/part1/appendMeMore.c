/************************************************************************
 *	CSE344 - System Programming - Homework1 Part1						*
 *	Author: Esra Eryılmaz												*
 *	Date: 28.03.2023													*
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define WRITE_OPEN_FLAGS (O_RDWR | O_CREAT | O_APPEND)
#define LSEEK_OPEN_FLAGS (O_RDWR | O_CREAT)
#define OPEN_MODES (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH)	// -rwxr--r--

// function declarations
int writeSyscall(char *filename, int byteNumber);
int lseekSyscall(char *filename, int byteNumber);


int main(int argc, char *argv[])
{
	int byteNum;
	// Command line arguments check
	if (argc == 3) {			// $./appendMeMore f1 1000000
		byteNum = atoi(argv[2]);
		writeSyscall(argv[1], byteNum);
	}
	else if (argc == 4 && strcmp(argv[3],"x") == 0 ) {		// $./appendMeMore f1 1000000 x
		byteNum = atoi(argv[2]);
		lseekSyscall(argv[1], byteNum);
	}
	else {
		fprintf(stderr, "Usage: 2 or 3 command line arguments are required!\nEXAMPLES :\n./appendMeMore f1 1000000 & ./appendMeMore f1 1000000\n./appendMeMore f2 1000000 x & ./appendMeMore f2 1000000 x\n");
		return -1;
	}

	return 0;
}

/*
 * This function should open the specified filename (creating it if necessary) and append 
 * num-bytes bytes to the file by using write() to write a byte at a time.
 */
int writeSyscall(char *filename, int byteNumber) {
	// Open the file for writing. If it exists, append to it; otherwise, create a new file.
	int fd = open(filename, WRITE_OPEN_FLAGS, OPEN_MODES);
	if( fd == -1){
		perror("open");
		return 1;
	}

	char c = 'e';
	for (int i=0 ; i<byteNumber ; i++) {
		if (write(fd, &c, 1) < 0 ) {
			perror("write");
			return 1;
		}
	}
	close(fd);
	return 0;	// return 0 for normal execution
}

/*
 * If a third command-line argument (x) is supplied, then the program should perform an 
 * lseek(fd, 0, SEEK_END) call before each write().
 */
int lseekSyscall(char *filename, int byteNumber) {
	int fd = open(filename, LSEEK_OPEN_FLAGS, OPEN_MODES);
	if( fd == -1){
		perror("open");
		return 1;
	}

	char c = 'e';
	for (int i=0 ; i<byteNumber ; i++) {
		if (lseek(fd, 0, SEEK_END) < 0) {
			perror("lseek");
			return 1;
		}
		if (write(fd, &c, 1) < 0 ) {
			perror("write");
			return 1;
		}
	}
	close(fd);
	return 0 ;
}
