/************************************************************************
 *	CSE344 - System Programming - Homework 4							*
 *	Author: Esra Eryılmaz												*
 *																		*
 *	A Concurrent File Access System		(Server side)					*
 ***********************************************************************/

#include "header.h"

volatile sig_atomic_t is_running = 1; // flag to indicate whether the server is running
volatile sig_atomic_t num_clients = 0; // number of clients currently connected
volatile sig_atomic_t tempClient = 0; //sırayla bağlanan clientlara isim vermek için..

// when signal is arrived to server, this variable change to 1 and change return to 0
static volatile sig_atomic_t client_connected = 0;


static void signalHandler(int signo)
{
	client_connected = 1;
}

/*
void handle_termination_signal(int signum)
{
	termination_requested = 1;
	is_running = 0;

	// Clean up resources and exit gracefully
}
*/

void* clientHandler(void* re)
{
	struct request* req = (struct request*)re;

	// handle the client connection
	char client_log_name[MAX_LOG_LEN];

	FILE* logFile = fopen(SERVER_LOG, "a");
	if (logFile == NULL) {
		perror("Failed to open log file");
		exit(EXIT_FAILURE);
	}

	//printf("is first req : %d \n ",req.is_first_req);
	if(req->is_first_req == 1){
		printf("Client PID %d connected as \"client%d\" \n", req->pid, num_clients);
	}
	else{
		num_clients--;
	}

	// create client FIFO and open for write-only
	char client_fifo[MAX_BUF];
	int quit = 1;

	snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req->pid);	//****

	int client_fd = open(client_fifo, O_WRONLY);	//****

	//printf("Received: %s\n", req.buf);
	// Split input string into words
	char* words[4];
	words[0] = strtok((char*)req->buf, " \n");
	for(int i = 1; i < 4; i++) {
		words[i] = strtok(NULL, " \n");
	}

	// Check if the first word is a valid request
	int request_index = -1;
	for(int i=0 ; i<NUM_REQUESTS ; i++) {
		if(strcmp(words[0], REQUESTS[i]) == 0) {
			request_index = i;
			break;
		}
	}

	char buffer2[MAX_BUF];
	size_t len = 0;
	char* line = NULL;
	FILE* fp;
	char *file2;
	int line2;
	char buffer3[MAX_BUF];
	char mergedLines[MAX_BUF] = "";  // Boş birleştirilmiş satırlar dizisi
	int curr_line;
	int line3;
	char *file3;
	char *string3;
	char buffer4[MAX_BUF];
	char *file5;

	struct response resp;

	// Handle request based on its index
	switch(request_index) {
		case 1: // list
			// execute the command to list the files and send the output to the client
			fp = popen("ls", "r");
			while (getline(&line, &len, fp) != -1) {
				strcat(resp.buffer, line);
			}
			free(line);
			pclose(fp);

			//printf("%s\n",resp.buffer);
			break;

		case 2: // readF
		//	buffer2 = handle_read(words[1], words[2]);
			file2 = words[1];
			line2 = atoi(words[2]);
			//printf("file : %s  line2 : %d \n",file2,line2);

			FILE *filee = fopen(file2, "r");
				if (filee == NULL) {
					perror("Failed to open file");
					exit(EXIT_FAILURE);
				}
				if (line2 < 1) {
					// Read and display the entire file contents
					int i,c;
					while((c = fgetc(filee)) != EOF){
						buffer3[i++] = (char)c;
					}
					buffer3[i] = '\0';	// Null-terminate the string

					//printf("Read %d characters: %s\n", i, buffer3);

				} else {
					// Read and display the specified line number
					int i;
					for (i = 0; i < line2; i++) {
						//int c = fgetc(filee);	//single char
						//fgets(filee);	//single line
						if (fgets(buffer3, sizeof(buffer3), filee) != NULL) {
							//printf("Read line: %s", buffer3);
							strcat(mergedLines, buffer3);
						} else {
							printf("Failed to read line from file.");
						}
					}
					//printf("Read %d characters: %s\n", i, mergedLines);
				}
				fclose(filee);

				memset(resp.buffer, 0, sizeof(resp.buffer));
				strcat(resp.buffer, mergedLines);
				//printf("--------- %s\n",resp.buffer);
			break;

		case 3: // writeT
		//	buffer2 = handle_write(words[1], words[2], words[3]);
			file3 = words[1];
			line3 = atoi(words[2]);
			string3 = words[3];
			//printf("file : %s  line2 : %d  str : %s\n",file3, line3, string3);

			FILE* fileee = fopen(file3, "r+");
			if (fileee == NULL) {
				// File doesn't exist, create a new file
				fileee = fopen(file3, "w+");
				if (fileee == NULL) {
					printf("Failed to create file.\n");
					exit(EXIT_FAILURE);
				}
			}
	
			// Move file pointer to the desired line
			int currentLine = 1;
			while (fgets(buffer3, sizeof(buffer3), fileee) != NULL && currentLine < line3) {
				currentLine++;
			}

			// If the desired line is not found, move to the end of file
			if (currentLine < line3) {
				fseek(fileee, 0, SEEK_END);
			}

			// Write the text to the file
			fputs(string3, fileee);
			fclose(fileee);

			break;

		case 4: // upload
		//	buffer2 = handle_upload(words[1]);

			break;

		case 5: // download
		//	buffer2 = handle_download(words[1]);

			break;

		case 6: // quit
			printf("client%d disconnected..\n", tempClient);
			if (fprintf(logFile, "client comment: quit , client pid: %ld\n", (long)req->pid) < 0) {		//server pid tutmak için dosyaya yazdım
				perror("Failed to write to log file");
				exit(EXIT_FAILURE);
			}
			quit = 1;
			//tempClient = 0;
			exit(EXIT_SUCCESS);	 // Terminate
			break;

		case 7: // killServer
		//	buffer2 = handle_kill();
			printf("kill signal from client%d.. terminating...\nbye\n", tempClient);
			char command[100];
			char server_pidd[50];
			pid_t server_pid = getpid();
			sprintf(server_pidd, "%d", server_pid);
			snprintf(command, sizeof(command), "kill %s", server_pidd);
			system(command);
			exit(EXIT_SUCCESS);	 // Terminate
			break;
	}
	tempClient--;
	// Write response back to client FIFO
	write(client_fd, &resp.buffer, sizeof(struct response));	//****

	// CLIENT LOG
	//snprintf(client_log_name, MAX_LOG_LEN, "%s/%d.log", dirname, getpid());
	snprintf(client_log_name, MAX_LOG_LEN, "%s/%d.log", req->dirname, req->pid);
	FILE* logFile2 = fopen(client_log_name, "w");
	if (logFile2 == NULL) {
		perror("Failed to create log file");
		exit(EXIT_FAILURE);
	}
	fprintf(logFile2, "Log message for client PID :%d\n",req->pid);
	fclose(logFile2);

	memset(resp.buffer, 0, sizeof(resp.buffer));	// içeriğini temizledik üst üste komutlar binmesin

	// Close client FIFO
	close(client_fd);
	unlink(client_fifo);
	fclose(logFile);		// close server log 

	return NULL;
}

void* thread_function(void* arg) {
	// Bu fonksiyonda gerçekleştirilmesi gereken işlemler falan..
	return NULL;
}


int main(int argc, char const *argv[])
{
	struct request req;
	struct response resp;
	//signal(SIGINT,handle_termination_signal);

	if (argc == 4)		// ./biboServer dir maxClient poolSize
	{
		const char* dirname = argv[1];
		
		int maxClients = atoi(argv[2]);
		int poolSize = atoi(argv[3]);		// thread pool size, get it from user
		// Verilen dizine gir, yoksa oluştur
		struct stat st = {0};
		if (stat(dirname, &st) == -1) {
			mkdir(dirname, 0777);	// 0755
		}
		// create the server log file
		int server_log_fd;
		if(access(SERVER_LOG, F_OK) != -1) {
			server_log_fd = open(SERVER_LOG, O_WRONLY | O_TRUNC, 0644);			// dosya var, aç
		} else {
			server_log_fd = open(SERVER_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);	// dosya yok, oluştur ve aç
		}
		if (server_log_fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		// display the PID of the server
		pid_t server_pid = getpid();
		printf("Server Started PID %d...\n", server_pid);

		FILE* logFile = fopen(SERVER_LOG, "w");
		if (logFile == NULL) {
			perror("Failed to open log file");
			exit(EXIT_FAILURE);
		}

		if (fprintf(logFile, "Server PID: %d\n", server_pid) < 0) {		//server pid tutmak için dosyaya yazdım
			perror("Failed to write to log file");
			exit(EXIT_FAILURE);
		}

		fclose(logFile);

		int server_fd, client_fd;
		char buf[MAX_BUF];
		pid_t pid;

		mkfifo(SERVER_FIFO, 0666);	//****

		server_fd = open(SERVER_FIFO, O_RDONLY);	//****
		if (server_fd == -1) {
			perror("Failed to open server FIFO");
			exit(EXIT_FAILURE);
		}

		// Wait for the first client to connect
		printf("waiting for clients...\n");

		char *buffer;
		int tempPID1 = 0;
		int tempPID2 = 0;
		pthread_t threads[poolSize];
		int thread_count = 0;

		// Thread havuzu oluşturulacak
		if(thread_count < poolSize) {
			pthread_t thread;
			if(pthread_create(&thread, NULL, thread_function, NULL)) {
				perror("pthread create");
				exit(EXIT_FAILURE);
			}
			threads[thread_count++] = thread;
		}
	//	else {
	//		sleep(1);
	//	}

		// Ana döngü
		// thread for each client connection
		while (is_running && num_clients < maxClients)
		{
			// Read from server FIFO
			read(server_fd, &req, sizeof(struct request));		//****	// read request
			tempPID1 = req.pid;
			strcpy(req.dirname, dirname);

			pthread_t threadd;
			// pthread_create(&thread, NULL, clientHandler, (void*)&req
			int result = pthread_create(&threadd, NULL, clientHandler, (void*)&req);
			if (result != 0) {
				printf("Server thread creation failed\n");
				return 1;
			}
			
			result = pthread_join(threadd, NULL);
			if (result != 0) {
				printf("Server thread join failed\n");
				return 1;
			}

			tempClient++;
			num_clients++;

		}
		// Wait for the threads to finish
		for (int i = 0; i < thread_count; i++) {
			pthread_join(threads[i], NULL);
		}
	}
	else {
		fprintf(stderr, "Usage: ./biboServer <dirname> <max. #ofClients> <poolSize>\n");
		return -1;
	}
	return 0;
}