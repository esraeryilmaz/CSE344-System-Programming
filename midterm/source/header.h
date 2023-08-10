#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>

// Possible client requests
const char* REQUESTS[] = {"help", "list", "readF", "writeT", "upload", "download", "quit", "killServer"};
const int NUM_REQUESTS = 8;

#define SERVER_LOG "server.log" // log file for the server
#define MAX_BUF 1024

#define SERVER_FIFO "/tmp/myfifo"
#define MAX_LOG_LEN 100 // maximum length of log filename

#define CLIENT_FIFO_TEMPLATE "/tmp/client_%ld_fifo"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)

#define SHM_SIZE 1024
#define SEM_NAME "/my_semaphore"
#define SHM_NAME "/my_shared_memory"


// request (client --> server)
struct request {
	pid_t pid;		    //pid of client
	char buf[MAX_BUF];
	int is_first_req;
};

// response (server --> client)
struct response {
	char buffer[MAX_BUF];		// response of server
};


#endif