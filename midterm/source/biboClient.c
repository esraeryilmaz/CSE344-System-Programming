/************************************************************************
 *	CSE344 - System Programming - Midterm Project						*
 *	Author: Esra Eryılmaz												*
 *	Date: 13.05.2023													*
 *																		*
 *	A Concurrent File Access System		(Client side)					*
 ***********************************************************************/

#include "header.h"


// Prints the list of possible client requests
void print_requests(char* word)
{
	if(word != NULL){
		if(strcmp(word, REQUESTS[0]) == 0) {
			printf("help :	display the list of possible client requests\n");
		}
		else if(strcmp(word, REQUESTS[1]) == 0) {
			printf("list :	sends a request to display the list of files in Servers directory (also displays the list received from the Server)\n");
		}
		else if(strcmp(word, REQUESTS[2]) == 0) {
			printf("readF <file> <line #> :	requests to display the # line of the <file>, if no line number is given the whole contents of the file is requested (and displayed on the client side)\n");
		}
		else if(strcmp(word, REQUESTS[3]) == 0) {
			printf("writeT <file> <line #> <string> :	request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time\n");
		}
		else if(strcmp(word, REQUESTS[4]) == 0) {
			printf("upload <file> :	uploads the file from the current working directory of client to the Servers directory (beware of the cases no file in clients current working directory and file with the same name on Servers side)\n");
		}
		else if(strcmp(word, REQUESTS[5]) == 0) {
			printf("download <file> :	request to receive <file> from Servers directory to client side\n");
		}
		else if(strcmp(word, REQUESTS[6]) == 0) {
			printf("quit :	Send write request to Server side log file and quits\n");
		}
		else if(strcmp(word, REQUESTS[7]) == 0) {
			printf("killServer :	Sends a kill request to the Server\n");
		}
		else {
			printf("Invalid request: %s\n", word);
		}
	}
	else{
		printf("\tAvailable comments are :\n");
		for(int i=0 ; i<NUM_REQUESTS; i++) {
			printf("%s, ", REQUESTS[i]);
		}
	}


	printf("\n");
}


int main(int argc, char const *argv[])
{
	struct request req;
	struct response resp;

	//signal(SIGINT,handle_termination_signal);

	if (argc == 3) {
		const char* connection = argv[1];
		int serverPID = atoi(argv[2]);

		int pid = -1;
		char buf[MAX_LOG_LEN];
		int serverPIDcorrect ;
		FILE* logFile = fopen(SERVER_LOG, "r");
		if (logFile == NULL) {
			perror("Failed to open log file");
			exit(EXIT_FAILURE);
		}
		if (fgets(buf, MAX_LOG_LEN, logFile) != NULL) {
			sscanf(buf, "Server PID: %d", &pid);
		}
		fclose(logFile);

		if(pid == serverPID)
			serverPIDcorrect = 1;	//true
		else
			serverPIDcorrect = 0;

		if(serverPIDcorrect) {
			char buf[MAX_BUF];
			int server_fd, client_fd;
			char client_fifo[CLIENT_FIFO_NAME_LEN];

			req.is_first_req = 1;		//true
			// Prompt user to enter a message
			printf("Waiting for Que.. Connection established:\n");
			while (1)
			{
				printf("Enter comment : ");
				fgets(buf, MAX_BUF, stdin);

				// Split input string into words
				char* words[4];
				words[0] = strtok((char*)buf, " \n");
				for(int i = 1; i < 4; i++) {
					words[i] = strtok(NULL, " \n");
				}

				// Check if the first word is a valid request
				int valid_request = 0;
				int request_index = -1;
				for(int i=0 ; i<NUM_REQUESTS ; i++) {
					if(strcmp(words[0], REQUESTS[i]) == 0) {
						valid_request = 1;
						request_index = i;
						break;
					}
				}

				// If not a valid request, display error message and return
				if(!valid_request) {
					printf("Error: Invalid request. Type 'help' for a list of available requests.\n");
					//return -1;
				}

				// Handle request based on its index
				snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)getpid());	//****

				req.pid = (long)getpid();
				printf("[Client PID : %d]\n",req.pid);
				mkfifo(client_fifo, 0666);	//****

				server_fd = open(SERVER_FIFO, O_WRONLY);	//****

				switch(request_index) {
					case 0: // help
						print_requests(words[1]);
						break;

					case 1: // list
						//send_request("list", NULL, NULL, NULL);
						memset(req.buf, 0, sizeof(req.buf));	// içeriğini temizledik üst üste komutlar binmesin
						sprintf(req.buf, "%s", "list");	//****
						write(server_fd, &req, sizeof(struct request));	//****
		
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK sildimm

						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****

						printf("response from server : \n%s\n", resp.buffer); // Display response

						// Close client and server FIFOs
	//					close(client_fd);		//aşağıda kapandı
	//					close(server_fd);
	//					unlink(client_fifo);
						break;

					case 2: // readF
						if(words[1] == NULL || words[2] == NULL) {
							printf("Error: Missing argument. Usage: readF <file> <line#>\n");
							//return -1;
						}
						//send_request("readF", words[1], words[2], NULL);
						memset(req.buf, 0, sizeof(req.buf));	// içeriğini temizledik üst üste komutlar binmesin
						sprintf(req.buf, "%s %s %s", "readF",words[1],words[2]);	//****

						write(server_fd, &req, sizeof(struct request));	//****
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK sildimm
						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****

						printf("response from server : \n%s\n", resp.buffer); // Display response
						//buf boşalt
						break;

					case 3: // writeT
						if(words[1] == NULL || words[2] == NULL || words[3] == NULL) {
							printf("Error: Missing argument. Usage: writeT <file> <line#> <string>\n");
						}
						//send_request("writeT", words[1], words[2], words[3]);
						memset(req.buf, 0, sizeof(req.buf));	// içeriğini temizledik üst üste komutlar binmesin
						sprintf(req.buf, "%s %s %s %s", "writeT",words[1],words[2], words[3]);	//****
						

						write(server_fd, &req, sizeof(struct request));	//****
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK sildimm
						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****

						printf("response from server : \n%s\n", resp.buffer); // Display response
						
						break;

					case 4: // upload
						if(words[1] == NULL) {
							printf("Error: Missing argument. Usage: upload <file>\n");
						}
						//send_request("upload", words[1], NULL, NULL);
						memset(req.buf, 0, sizeof(req.buf));	// içeriğini temizledik üst üste komutlar binmesin
						sprintf(req.buf, "%s %s", "upload",words[1]);	//****

						write(server_fd, &req, sizeof(struct request));	//****
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK sildimm
						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****

						printf("response from server : \n%s\n", resp.buffer); // Display response
						break;

					case 5: // download
						if(words[1] == NULL) {
							printf("Error: Missing argument. Usage: download <file>\n");
						}
						//send_request("download", words[1], NULL, NULL);
						memset(req.buf, 0, sizeof(req.buf));	// içeriğini temizledik üst üste komutlar binmesin
						sprintf(req.buf, "%s %s", "download",words[1]);	//****
						
						write(server_fd, &req, sizeof(struct request));	//****
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK sildimm
						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****

						printf("response from server : \n%s\n", resp.buffer); // Display response
						break;

					case 6: // quit
						printf("Sending write request to server log file\nwaiting for log file ...\n");

						sprintf(req.buf, "%s", "quit");	//****
						write(server_fd, &req, sizeof(struct request));	//****
		
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK silindi

						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****
						printf("response from server : \n%s\n", resp.buffer); // Display response

						exit(0);
						break;

					case 7: // killServer
						//send_request("killServer", NULL, NULL, NULL);
						printf("Sending kill request to server...\nbye...\n");

						sprintf(req.buf, "%s", "killServer");	//****
						write(server_fd, &req, sizeof(struct request));	//****
		
						client_fd = open(client_fifo, O_RDONLY);		//****	// | O_NONBLOCK silindi

						// Read response from client FIFO
						read(client_fd, &resp, sizeof(struct response));	//****
						exit(0);
						break;
				}
				printf("\n");
				req.is_first_req = 0;	//false
			}
			close(client_fd);
			close(server_fd);
			unlink(client_fifo);
		}
		else {
			fprintf(stderr, "Usage: ./biboClient <Connect/tryConnect> ServerPID\n");
			return -1;
		}
	}
	else {
		fprintf(stderr, "Usage: ./biboClient <Connect/tryConnect> ServerPID\n");
		return -1;
	}
	return 0;
}
