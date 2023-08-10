#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>
#include <utime.h>

#define bool int
#define true 1
#define false 0
#define MAX_FILES 4096

char * clientPath;
int socketFd;
volatile int ok=0;

typedef struct{
	char name[512];
	char content[4096];
	mode_t mode;	// Permissions
	long size;
	time_t modifiedTime;
} file;


void signalHandler(int sig) {
	if(sig==SIGPIPE) {		// write() read() lerde geri dönüş olmazsa..
		fprintf(stderr,"Connection was closed...\n");
	}
	close(socketFd);
	printf("\nExiting client...\n\n");
	ok = 1;
	//free(clientPath);
	exit(0);
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

int readFolderContents(const char* folderPath, file* fileList, int maxFiles) {
	DIR* directory;
	struct dirent* entry;
	struct stat fileStat;
	int fileCount = 0;

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
		if (fileCount >= maxFiles) {
			printf("Maximum file count reached. Skipping remaining files.\n");
			break;
		}

		// Copy file information to the file list
		strncpy(fileList[fileCount].name, entry->d_name, sizeof(fileList[fileCount].name) - 1);
		fileList[fileCount].name[sizeof(fileList[fileCount].name) - 1] = '\0';
		fileList[fileCount].mode = fileStat.st_mode;
		fileList[fileCount].size = fileStat.st_size;
		fileList[fileCount].modifiedTime = fileStat.st_mtime;

		// Open the file for reading
		FILE* file = fopen(filePath, "r");
		if (file == NULL) {
			printf("Failed to open file: %s\n", filePath);
			continue;
		}

		// Read the file content
		size_t bytesRead = fread(fileList[fileCount].content, sizeof(char), sizeof(fileList[fileCount].content) - 1, file);
		fileList[fileCount].content[bytesRead] = '\0';

		// Close the file
		fclose(file);

		// Increment the file count
		fileCount++;
	}
	// Close the directory
	closedir(directory);

	return fileCount;
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

bool compareFiles(file* fileList, int fileCount, const char* path) {
	DIR* directory;
	struct dirent* entry;

	// Open the directory
	directory = opendir(path);
	if (directory == NULL) {
		perror("Failed to open directory");
		return false;
	}

	int fileFound[fileCount];
	memset(fileFound, 0, sizeof(fileFound));

	// Iterate over directory entries
	while ((entry = readdir(directory)) != NULL) {
		if (entry->d_type == DT_REG) {
			char filePath[512];
			snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

			// Check if the file exists in fileList
			int i;
			for (i = 0; i < fileCount; i++) {
				if (strcmp(fileList[i].name, entry->d_name) == 0) {
					fileFound[i] = 1;
					struct stat fileStat;
					if (stat(filePath, &fileStat) == 0) {
						// Compare file attributes
						if (fileList[i].mode != fileStat.st_mode ||
							fileList[i].size != fileStat.st_size ||
							fileList[i].modifiedTime != fileStat.st_mtime) {
							closedir(directory);
							return false; // Differences found
						}
					}
					break;
				}
			}

			if (i == fileCount) {
				closedir(directory);
				return false; // File in the directory is not found in fileList
			}
		}
	}

	closedir(directory);

	// Check if all files in fileList have been found in the directory
	int i;
	for (i = 0; i < fileCount; i++) {
		if (!fileFound[i]) {
			return false; // File in fileList is not found in the directory
		}
	}

	return true; // No differences found
}

void reCreateFolder(const char* basePath, const file* fileList, int fileCount) {
	empty_directory(basePath);		// önce folder ı temizliyorum, sonra recreate
	for (int i = 0; i < fileCount; i++) {
		char filePath[1024];
		snprintf(filePath, sizeof(filePath), "%s/%s", basePath, fileList[i].name);

		FILE* fp = fopen(filePath, "w");
		if (fp == NULL) {
			perror("folder creation error");
			continue;
		}

		fprintf(fp, "%s", fileList[i].content);
		fclose(fp);

		// Dosya izinlerini ayarla
		if (chmod(filePath, fileList[i].mode) == -1) {
			perror("mode error");
		}

		// Dosya değiştirilme zamanını ayarla
		struct utimbuf utimeBuf;
		utimeBuf.actime = fileList[i].modifiedTime;
		utimeBuf.modtime = fileList[i].modifiedTime;
		if (utime(filePath, &utimeBuf) == -1) {
			perror("modified time error");
		}
	}
	//printf("CREATED FOLDER !!!!!!!!!!!!!!!\n");
}


int main(int argc, char  *argv[])
{
	if (signal(SIGINT, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGINT\n");
	if (signal(SIGTERM, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGTERM\n");
	if (signal(SIGQUIT, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGQUIT\n");
	if (signal(SIGPIPE, signalHandler) == SIG_ERR)
		printf("\nCan't catch SIGPIPE\n");

	char * ip;
	int port;
	clientPath = malloc(512);

	if(argc == 3) {		//./BibakBOXClient [dirName] [portnumber]
		clientPath = argv[1];
		port = atoi(argv[2]);
		ip = "127.0.0.1";

	}
	else if( argc==4 ) {	//./BibakBOXClient [dirName] [portnumber] OPTIONAL[ip address]
		clientPath = argv[1];
		port = atoi(argv[2]);
		ip = argv[3];
	}
	else {
		printf("Usage : BibakBOXClient [dirName] [portnumber] OPTIONAL[ip address]\n");
		return 0;
	}
	printf("~~~~~~~~~~~~\tCLIENT SIDE\t~~~~~~~~~~~~\n");
	struct stat st = {0};
	if (stat(clientPath, &st) == -1) {
		printf("The folder %s does not exist, the program created it!!\n",clientPath);
		mkdir(clientPath, 0700);
	}
	empty_directory(clientPath);	// EMPTY YAPICAZ CLİENT I ÖNCELİKLE

	struct sockaddr_in address;
	int sock = 0;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("\n Socket creation error \n");
		return -1;
	}
	printf("Client socket created !\n");

	//memset(&serv_addr, '0', sizeof(serv_addr));
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	//serv_addr.sin_addr.s_addr = inet_addr(ip);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0){
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		printf("\nConnection failed, server is down. \n");
		return -2;
	}
	socketFd = sock;
	printf("Client connected to the server with port: %d, server address: %s !\n\n" ,port, ip);
//
	int received_byte_num=0;
	read(socketFd, &received_byte_num, sizeof(int));
	int remaining_bytes =received_byte_num;
	int total_bytes_received = 0;
	char * receiveBuffer = malloc(MAX_FILES * sizeof(file));
	int fileCount=0;

	while(remaining_bytes>0) {
		int bytes_read = read(socketFd, receiveBuffer + total_bytes_received, remaining_bytes);
		if (bytes_read == -1) {				// Hata oluştu, gönderme başarısız oldu
			perror("read error");
			//return 0;
			break;
		}
		else if (bytes_read == 0) {
	//		printf("Veri akışı sonlandı.\n");
			break;
		}
		//printf("bytes_read : %d\n",bytes_read);
		total_bytes_received += bytes_read;
		remaining_bytes -= bytes_read;
	}
	//if (remaining_bytes == 0) {
	//	printf("Veri başarıyla alındı.\n");
	//}


	// //// !max
	if(strcmp(receiveBuffer,"max")==0)	{	// ilk bağlantı ve server max client dedi bağlantı OLAMADI
		fprintf(stderr,"Maximum number of clients already connected !\n");
		close(socketFd);
		return 1;
	}

	file* fileList = malloc(MAX_FILES * sizeof(file));
	file *files = malloc(MAX_FILES * sizeof(file));

	files = (file*)receiveBuffer;
	int count = countFiles(files);
	//printf("After connection : Server send %d file !\n\n",count);

	reCreateFolder(clientPath, files, count);
	fileCount = readFolderContents(clientPath, fileList, MAX_FILES);

	write(socketFd, "ok",128);
	// BURAYA KADAR GELMİŞSE SUCCESSFULLY CONNECTED..........................................

	const int no = -1;

	int send_bytes;
	received_byte_num=0;
	int total_bytes_to_send=0;
	int total_bytes_send=0;
	char readBuffer[128];

	memset(receiveBuffer, 0, sizeof(file)*MAX_FILES);
	memset(files, 0, sizeof(file)*MAX_FILES);

	while(!ok)		// client ctrl+c yapmadığı ve server kapanmadığı sürece devam...
	{

		read(socketFd, &received_byte_num, sizeof(int));
		if (received_byte_num == -1) { 	// server da değişiklik yok -1 gelmiş
			//printf("Alınan bayt sayısı: %d\n", received_byte_num);
			//printf("No differences found.\n");
			write(socketFd, "ok", 128);
		}
		else { 		// server da değişiklik var, file size gelmiş
			printf("Differences found on the SERVER SIDE !!\n");
			remaining_bytes = received_byte_num;
			total_bytes_received = 0;

			while(remaining_bytes>0) {
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
			count = countFiles(files);
			//printf("COUNT  : %d\n",count);
			reCreateFolder(clientPath, files, count);
			memset(fileList, 0, sizeof(file)*MAX_FILES);
			fileCount = readFolderContents(clientPath, fileList, MAX_FILES);		// !!
			//printf("fileCOUNT  : %d\n",fileCount);
			write(socketFd, "ok",128);
		}
		sleep(1);

		//****************************************************************************************

		int changesFoundNo = compareFiles(fileList, fileCount, clientPath);	// 1 dönerse değişiklik yok

		if (changesFoundNo) {		 	// client ta değişiklik yok -1 gidecek
			//printf("no difference... (this client side)\n");
			send_bytes = write(socketFd, &no, sizeof(int));
			//printf("client side send byte numb: %d\n", send_bytes);// Veri başarıyla gönderildi
			read(socketFd, readBuffer, 128);
			memset(readBuffer, 0, sizeof(readBuffer));
		}
		else {				// client ta değişiklik var file size gidecek
			printf("Differences found...(this client side)\n");
			memset(fileList, 0, sizeof(file)*MAX_FILES);
			fileCount = readFolderContents(clientPath, fileList, MAX_FILES);
			total_bytes_to_send = sizeof(file)*fileCount;
			write(socketFd, &total_bytes_to_send, sizeof(int));
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
			//	printf("Veri başarıyla gönderildi.\n");
			//}
			read(socketFd, readBuffer, 128);
			memset(readBuffer, 0, sizeof(readBuffer));
		}
		//printf("-------------------\n");

		memset(receiveBuffer, 0, sizeof(file)*MAX_FILES);
		memset(files, 0, sizeof(file)*MAX_FILES);
		//memset(fileList, 0, sizeof(file)*MAX_FILES);
	}

	free(fileList);
	free(receiveBuffer);

	close(socketFd);	// Bağlantıyı kapat

	return 0;
}

