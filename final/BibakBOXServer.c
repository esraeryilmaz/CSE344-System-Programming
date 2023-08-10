#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <utime.h>
#include <arpa/inet.h>

#define bool int
#define true 1
#define false 0
#define MAX_FILES 4096
#define BUFFER_SIZE 1024
#define MAX_CLIENTS_NUM 512

typedef struct{
	char name[512];
	char content[4096];
	mode_t mode;	// Permissions
	long size;
	time_t modifiedTime;
} file;

volatile int ok=0;
char * serverPath;

char * connectedPath[MAX_CLIENTS_NUM] = {0};
int connectedClients[MAX_CLIENTS_NUM] = {0};
int totalClientsConnected=0;
int threadPoolSize;
pthread_t * threads;		//thread pool
int server_fd;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
char *logfileName;


void traverse_directory(const char* directory_path, const char* base_directory, char* buffer, size_t buffer_size) {
	DIR* dir;
	struct dirent* entry;
	// Open the directory
	dir = opendir(directory_path);
	if (dir == NULL) {
		perror("Error opening directory");
		return;
	}
	// Iterate over directory entries
	while ((entry = readdir(dir)) != NULL) {
		// Skip "." and ".." entries
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		// Build the path for the entry
		char entry_path[BUFFER_SIZE];
		snprintf(entry_path, BUFFER_SIZE, "%s/%s", directory_path, entry->d_name);

		// Check if the entry is a regular file or directory
		if (entry->d_type == DT_REG) {
			// Skip the base directory if it matches the path
			if (strcmp(entry_path, base_directory) == 0) {
				continue;
			}
			// Add the file path to the buffer
			strncat(buffer, entry_path + strlen(base_directory), buffer_size - strlen(buffer) - 1);
			strncat(buffer, "\n", buffer_size - strlen(buffer) - 1);
		} else if (entry->d_type == DT_DIR) {
			// Recursively traverse subdirectories
			traverse_directory(entry_path, base_directory, buffer, buffer_size);
		}
	}
	// Close the directory
	closedir(dir);
}


int readFolderContents(const char* folderPath, file* fileList2, int maxFiles) {
	DIR* directory;
	struct dirent* entry;
	struct stat fileStat;
	int count = 0;

	// Open the directory
	directory = opendir(folderPath);
	if (directory == NULL) {
		printf("Failed to open directory: %s\n", folderPath);
		return -1;
	}

	// Read directory entries
	while ((entry = readdir(directory)) != NULL) {
		// Get the file path
		char filePath[1024];
		snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);

		// Get file information
		if (stat(filePath, &fileStat) == -1) {
			printf("Failed to get file information: %s\n", filePath);
			continue;
		}

		// Skip directories and system files
		if (S_ISDIR(fileStat.st_mode) || entry->d_name[0] == '.') {
			continue;
		}

		// Check if the file count exceeds the maximum limit
		if (count >= maxFiles) {
			printf("Maximum file count reached. Skipping remaining files.\n");
			break;
		}

		// Copy file information to the file list
		strncpy(fileList2[count].name, entry->d_name, sizeof(fileList2[count].name) - 1);
		fileList2[count].name[sizeof(fileList2[count].name) - 1] = '\0';
		fileList2[count].mode = fileStat.st_mode;
		fileList2[count].size = fileStat.st_size;
		fileList2[count].modifiedTime = fileStat.st_mtime;

		// Open the file for reading
		FILE* file = fopen(filePath, "r");
		if (file == NULL) {
			printf("Failed to open file: %s\n", filePath);
			continue;
		}

		// Read the file content
		size_t bytesRead = fread(fileList2[count].content, sizeof(char), sizeof(fileList2[count].content) - 1, file);
		fileList2[count].content[bytesRead] = '\0';

		// Close the file
		fclose(file);

		// Increment the file count
		count++;
	}
	// Close the directory
	closedir(directory);

	return count;
}


bool compareFiles(file* fileList2, int count, const char* path) {
	DIR* directory;
	struct dirent* entry;

	// Open the directory
	directory = opendir(path);
	if (directory == NULL) {
		perror("Failed to open directory");
		return false;
	}

	int fileFound[count];
	memset(fileFound, 0, sizeof(fileFound));

	// Iterate over directory entries
	while ((entry = readdir(directory)) != NULL) {
		if (entry->d_type == DT_REG) {
			char filePath[512];
			snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

			// Check if the file exists in fileList2
			int i;
			for (i = 0; i < count; i++) {
				if (strcmp(fileList2[i].name, entry->d_name) == 0) {
					fileFound[i] = 1;
					struct stat fileStat;
					if (stat(filePath, &fileStat) == 0) {
						// Compare file attributes
						if (fileList2[i].mode != fileStat.st_mode ||
							fileList2[i].size != fileStat.st_size ||
							fileList2[i].modifiedTime != fileStat.st_mtime) {
							closedir(directory);
							return false; // Differences found
						}
					}
					break;
				}
			}
			if (i == count) {
				closedir(directory);
				return false; // File in the directory is not found in fileList2
			}
		}
	}

	closedir(directory);
	// Check if all files in fileList2 have been found in the directory
	int i;
	for (i = 0; i < count; i++) {
		if (!fileFound[i]) {
			return false; // File in fileList2 is not found in the directory
		}
	}
	return true; // No differences found
}


int countFiles(file* fileList2) {
	int count = 0;
	file* currentFile = fileList2;
	while (currentFile->name[0] != '\0' && count < MAX_FILES) {
		count++;
		currentFile++;
	}
	return count;
}


void empty_directory(const char* folderPath) {
	DIR* directory;
	struct dirent* entry;
	char filePath[1024];

	// Open the directory
	directory = opendir(folderPath);
	if (directory == NULL) {
		printf("Failed to open directory: %s\n", folderPath);
		return;
	}

	// Read directory entries
	while ((entry = readdir(directory)) != NULL) {
		// Skip directories and system files
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		// Get the file path
		snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);

		// Check if it is a directory
		struct stat fileStat;
		if (stat(filePath, &fileStat) == 0 && S_ISDIR(fileStat.st_mode)) {
			// Recursively empty the subdirectory
			empty_directory(filePath);
			// Remove the subdirectory
			rmdir(filePath);
		} else {
			// Remove the file
			remove(filePath);
		}
	}

	// Close the directory
	closedir(directory);
}


void reCreateFolder(const char* basePath, const file* fileList2, int count) {
	empty_directory(basePath);
	for (int i = 0; i < count; i++) {
		char filePath[1024];
		snprintf(filePath, sizeof(filePath), "%s/%s", basePath, fileList2[i].name);

		FILE* fp = fopen(filePath, "w");
		if (fp == NULL) {
			perror("folder creation error");
			continue;
		}

		fprintf(fp, "%s", fileList2[i].content);
		fclose(fp);

		// Dosya izinlerini ayarla
		if (chmod(filePath, fileList2[i].mode) == -1) {
			perror("mode error");
		}

		// Dosya değiştirilme zamanını ayarla
		struct utimbuf utimeBuf;
		utimeBuf.actime = fileList2[i].modifiedTime;
		utimeBuf.modtime = fileList2[i].modifiedTime;
		if (utime(filePath, &utimeBuf) == -1) {
			perror("modified time error");
		}
	}
}

void printFiles(const file* fileList2, int count) {
	for (int i = 0; i < count; i++) {
		const file* f = &fileList2[i];
		printf("File %d:\n", i + 1);
		printf("Name: %s\n", f->name);
		printf("Mode: %o\n", f->mode);
		printf("Content: %s", f->content);
		printf("Size: %ld\n", f->size);
		printf("Modified Time: %s", ctime(&f->modifiedTime));
		printf("-------------------\n");
	}
}


void signalHandler(int sig)	{
	if(sig == SIGPIPE){
		fprintf(stderr,"one client was closed...\n");
		ok=1;
	}
	else {
		close(server_fd);
		int i;
		for(i=0;i<threadPoolSize;i++) {
			if(connectedPath[i]!=NULL) {
				free(connectedPath[i]);
			}
		}
		for(i=0;i<threadPoolSize;i++) {
			if(connectedClients[i]!=0) {
				close(connectedClients[i]);
			}
		}
		free(serverPath);
		//free(logfileName);

		printf("\nExiting BibakBOXServer...\n\n");
		//ok=1;
		exit(0);
	}
}

void createLogfile(const char* message) {
	FILE* logFile = fopen(logfileName, "a");
	if (logFile != NULL) {
		fprintf(logFile, "%s\n", message);
		fclose(logFile);
	}
	else {
		fprintf(stderr, "Failed to open log file.\n");
	}
}

void * threadMain(void * var)		// gelen var : server_fd!
{
	int socketFd;
	struct sockaddr_in address;
	socklen_t addrlen;
	addrlen = sizeof(struct sockaddr_in);
	char readBuffer[128];
	file* fileList;
	int fileCount;
	int serverFd = *((int*)var);		//!!

	fileList = malloc(MAX_FILES * sizeof(file));
	if (fileList == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	// Receive file events from the client
	//while(!ok)
	while(true)
	{
		//##############################	Critical Section Started	##################################
		pthread_mutex_lock(&lock); //lock
		//printf("accept :  \n");
		if ((socketFd = accept(serverFd, (struct sockaddr *)&address,&addrlen))<0){
			printf("accept");
			fflush(stdout);
			exit(EXIT_FAILURE);
		}

		printf("\nServer accepted client number %d...\n", totalClientsConnected);
		// Clientın IP adresini ekrana yazma:
		char clientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(address.sin_addr), clientIP, INET_ADDRSTRLEN);
		printf("Client connected with address : %s\n\n", clientIP);

		connectedClients[totalClientsConnected] = socketFd;
		totalClientsConnected++;
		//printf("socketfd : %d\n\n",socketFd);

		memset(fileList, 0, sizeof(file)*MAX_FILES);
		readFolderContents(serverPath, fileList, MAX_FILES);
		fileCount = countFiles(fileList);
		//printf("fileCount() : %d\n",fileCount);
////
		int total_bytes_to_send = sizeof(file)*fileCount;
		write(socketFd, &total_bytes_to_send, sizeof(int));		//size bilgisini gönder

		//Send and get status response
		int remaining_bytes = total_bytes_to_send;
		int total_bytes_send = 0;
		while(remaining_bytes >0){
			int bytes_sent = write(socketFd, fileList + total_bytes_send, remaining_bytes);
			if (bytes_sent == -1) {			// Hata oluştu, gönderme başarısız oldu
				perror("write");
				break;
			}
			total_bytes_send += bytes_sent;
			remaining_bytes-= bytes_sent;
		}
		//if (remaining_bytes == 0) {
		//	printf("Veri başarıyla gönderildi.\n");
		//}
////
		read(socketFd, readBuffer,128);		// "ok"
		memset(readBuffer, 0, sizeof(readBuffer));

		ok=0;		// yeni gelen clientları karşılamak adına önemliiiiii
		pthread_mutex_unlock(&lock); //unlock
		//##############################	Critical Section End	##################################

		int send_bytes=0;
		char * receiveBuffer = malloc(MAX_FILES * sizeof(file));
		file *files = malloc(MAX_FILES * sizeof(file));

		const int no = -1;
		int received_byte_num=0;
		int total_bytes_received = 0;

		//while(true)
		while(!ok)
		{
			int changesFoundNo = compareFiles(fileList, fileCount, serverPath);
			//printf("changesFoundNo : %d\n",changesFoundNo);		// 1 se değişiklik yok

			if (changesFoundNo) {		// server da değişiklik yoksa -1 gönder
				//printf("no difference... (this server side)\n");
				send_bytes = write(socketFd, &no, sizeof(int));
				//printf("send byte numb: %d\n", send_bytes);// Veri başarıyla gönderildi
				read(socketFd, readBuffer, 128);		//ok
				memset(readBuffer, 0, sizeof(readBuffer));
			}
			else {		// server da değişiklik varsa önce size gönder sonra files gönder
				printf("Differences found...(this server side)\n");
				memset(fileList, 0, sizeof(file)*MAX_FILES);
				createLogfile("Differences found...(server side)");

				fileCount = readFolderContents(serverPath, fileList, MAX_FILES);
				total_bytes_to_send = sizeof(file)*fileCount;
				write(socketFd, &total_bytes_to_send, sizeof(int));
				//read();
				remaining_bytes = total_bytes_to_send;
				total_bytes_send = 0;

				while(remaining_bytes >0) {
					int bytes_sent = write(socketFd, fileList + total_bytes_send, remaining_bytes);
					if (bytes_sent == -1) {		// Hata oluştu, gönderme başarısız oldu
						perror("write");
						break;
					}
					total_bytes_send += bytes_sent;
					remaining_bytes-= bytes_sent;
				}

				//if (remaining_bytes == 0) {
					//printf("Veri başarıyla gönderildi.\n");
				//}
				read(socketFd, readBuffer, 128);		//ok
				memset(readBuffer, 0, sizeof(readBuffer));
			}

			//printf("-------------------\n");
			sleep(1);

			//****************************************************************************************

			memset(receiveBuffer, 0, sizeof(file)*MAX_FILES);
			memset(files, 0, sizeof(file)*MAX_FILES);

			read(socketFd, &received_byte_num, sizeof(int));
			if (received_byte_num == -1) {			// client ta değişiklik yok -1 gelmiş
				//printf("Alınan bayt sayısı: %d\n", received_byte_num);
				//printf("client no difference\n");
				write(socketFd, "ok", 128);
			}
			else {		// client ta değişiklik var, file size gelmiş
				printf("Differences found on the CLIENT SIDE !!\n");
				createLogfile("Differences found...(client side)");
				remaining_bytes = received_byte_num;
				total_bytes_received = 0;

				while(remaining_bytes >0) {
					int bytes_read = read(socketFd, receiveBuffer + total_bytes_received, remaining_bytes);
					if (bytes_read == -1) {				// Hata oluştu, gönderme başarısız oldu
						perror("read error");
						//return 0;
						break;
					}
					else if (bytes_read == 0) {
						//printf("Veri akışı sonlandı.\n");
						break;
					}
					//printf("bytes_read : %d\n",bytes_read);
					total_bytes_received += bytes_read;
					remaining_bytes -= bytes_read;
				}
				files = (file*)receiveBuffer;
				int count = countFiles(files);
				//printf("COUNT    : %d",count);
				reCreateFolder(serverPath, files, count);
				memset(fileList, 0, sizeof(file)*MAX_FILES);
				fileCount = readFolderContents(serverPath, fileList, MAX_FILES);	//*********
				write(socketFd, "ok", 128);
			}
			send_bytes=0;

			//memset(receiveBuffer, 0, sizeof(file)*MAX_FILES);
			//memset(files, 0, sizeof(file)*MAX_FILES);
			//memset(fileList, 0, sizeof(file)*MAX_FILES);
			//fileCount = readFolderContents(serverPath, fileList, MAX_FILES);
		}
		pthread_mutex_lock(&lock); //lock
		totalClientsConnected--;
		pthread_mutex_unlock(&lock); //unlock
	}

	free(fileList);
	//close(socketFd);
	pthread_exit(NULL);
	return NULL;
}



int main(int argc, char const *argv[])
{
	int port;
	if(argc<4) {
		printf("Usage : BibakBOXServer [directory] [threadPoolSize] [portnumber] \n");
		return 0;
	}
	else {
		serverPath = malloc(512);
		strcpy(serverPath,argv[1]);
		threadPoolSize = atoi(argv[2]);
		port = atoi(argv[3]);
	}

	for(int i=0 ; i<threadPoolSize ; i++){
		connectedPath[i] = calloc(255, sizeof(char));
	}

	if (signal(SIGINT, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGINT\n");
	if (signal(SIGTERM, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGTERM\n");
	if (signal(SIGQUIT, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGQUIT\n");
	if (signal(SIGPIPE, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGPIPE\n");

	printf("~~~~~~~~~~~~\tSERVER SIDE\t~~~~~~~~~~~~\n");
	int opt = 1;
	struct stat st = {0};
	if (stat(serverPath, &st) == -1) {
		printf("The folder %s does not exist, the program created it!!\n",serverPath);
		mkdir(serverPath, 0700);
	}

	logfileName = malloc(512);
	logfileName = "logfile.log";
	FILE* file = fopen(logfileName, "w");
	createLogfile("Log file created! ");

	fclose(file);

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
		printf("socket failed");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
				   &opt, sizeof(opt))){
		printf("setsockopt");
		exit(EXIT_FAILURE);
	}
	printf("Server socket created !\n");

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( port );
	if (bind(server_fd, (struct sockaddr *)&address,sizeof(address))<0){
		printf("bind failed");fflush(stdout);
		exit(EXIT_FAILURE);
	}
	printf("Server binded !\n");

	if (listen(server_fd, 0) < 0){
		printf("listen");
		fflush(stdout);
		exit(EXIT_FAILURE);
	}
	printf("Server listening on %d...\n",port);
	fflush(stdout);

	threads = malloc(threadPoolSize* sizeof(pthread_t));

	for(int i=0 ; i<threadPoolSize; i++){
		pthread_create(&threads[i], NULL, threadMain, (void*)&server_fd);
		//printf("i : %d\n",i);
	}

	for(int i=0 ; i<threadPoolSize; i++){
		pthread_join(threads[i],NULL);
	}

	free(logfileName);

	return 0;
}
