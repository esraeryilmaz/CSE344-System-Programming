/************************************************************************
 *	CSE344 - System Programming - Homework2								*
 *	Author: Esra Eryılmaz												*
 *	Date: 12.04.2023													*
 *																		*
 *	This program is a basic implementation of a terminal emulator		*
 ***********************************************************************/

#include <stdio.h>
#include "terminalEmulator.h"

int main(int argc, char const *argv[])
{
	if (argc == 1) {			// $./hw2
		terminalEmulator();
	}
	else {
		fprintf(stderr, "Usage: 1 command line arguments are required!\nEXAMPLES:\n$ ./hw2\n$ valgrind --leak-check=full --show-leak-kinds=all ./hw2\n(You can enter a command after running.)\n");
		return -1;
	}

	return 0;
}
