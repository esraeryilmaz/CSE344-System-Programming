#include "terminalEmulator.h"

#define MAX_COMMANDS 20
#define MAX_COMMAND_SIZE 1024


volatile sig_atomic_t childLiving = 0;	// for SIGCHLD

/*
 * Main function to execute user commands
 */
void terminalEmulator()
{
	char user_input[MAX_COMMAND_SIZE];		// it holds input commands entered by the user
	char *commands[MAX_COMMANDS];			// array to parse the entered command
	int num_commands = 0;					// number of commands
	int pipes[MAX_COMMANDS-1][2];			// create pipes for connecting commands, it is 2D matrix because may we have more than one pipe
	int input_fd = STDIN_FILENO;			// file descriptor for input file
	int output_fd = STDOUT_FILENO;			// file descriptor for output file
	int temp_input_fd, temp_output_fd;		// temporary file descriptors
	char log_filename[50];					// log file name

	printf("Welcome to the terminal emulator!\n");

	signal(SIGINT, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGTSTP, signalHandler);
	signal(SIGCHLD, signalHandler);

	while(1)	// It loops continuously until an error occurs or the user types ":q"
	{
		// open log file with current time
		time_t current_time;
		struct tm *time_info;
		time(&current_time);
		time_info = localtime(&current_time);
		strftime(log_filename, sizeof(log_filename), "log_%Y-%m-%d_%H-%M-%S.log", time_info);

		printf(">> ");
		fgets(user_input, sizeof(user_input), stdin);		// reads user input

		// remove "\n" from command
		user_input[strcspn(user_input, "\n")] = '\0';

		// exit if :q entered
		if (strcmp(user_input, ":q") == 0) {
			printf("Exiting...\n");
			exit(0);
		}

		// Parse user input into commands (it splits the command according to the pipes.)
		num_commands = parseUserInput(user_input, commands);

		// Create pipes for connecting commands
		createPipes(pipes, num_commands);

		// start executing user commands..
		for(int i=0 ; i<num_commands ; i++)
		{
			// Set input and output file descriptors
			temp_input_fd = input_fd;
			temp_output_fd = output_fd;

			// if there is more than one command to execute:
			if (i > 0) {
				temp_input_fd = pipes[i - 1][0];
				close(pipes[i - 1][1]); // Close write end of previous pipe
			}
			if (i < num_commands - 1) {
				temp_output_fd = pipes[i][1];
			}
			
			// Execute command
			executeCommand(commands[i], temp_input_fd, temp_output_fd, log_filename);
			
			// Cleanup fds
			if (temp_input_fd != input_fd) {
				close(temp_input_fd);
			}
			if (temp_output_fd != output_fd) {
				close(temp_output_fd);
			}
		}

		// Close last pipe
		if (num_commands > 1) {
			close(pipes[num_commands - 2][0]);
		}

		// reset num_commands for next user_input
		num_commands = 0;
	}
	//free(commands);
}


/*
 * Helper function to parse user input into commands
 */
int parseUserInput(char *user_input, char **commands) {
	int num_commands = 0;
	char* command = strtok(user_input, "|");
	while (command != NULL && num_commands < MAX_COMMANDS) {
		commands[num_commands] = command;
		num_commands++;
		command = strtok(NULL, "|");	// NULL parameter tells the function to continue where it left off from the previous strtok() call
	}
	return num_commands;
}

/*
 * Helper function to create pipes for connecting commands
 */
void createPipes(int pipes[][2], int num_commands) {
	int i;
	for(i=0 ; i<num_commands-1 ; i++) {
		if(pipe(pipes[i]) == -1) { // pipes for connecting commands
			perror("pipe");
			exit(EXIT_FAILURE);
		}
	}
}


/*
 * Important function that forks a child process and calls either
 */
void executeCommand(char *command, int input_fd, int output_fd, char filename[50])
{
	// Each shell command should be executed via a newly created child process
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	else if (pid == 0) {
		childProcess(command, input_fd, output_fd);		// Child process
	}
	else {
		parentProcess(pid, command, filename);		// Parent process
	}
}

/*
 * This function is called by the child process and redirects input and output as needed using the redirectInputOutput().
 * It then parses the command string using the parseCommand() and executes the command using the execute().
 * IT is also free args so that there is no memory leak.
 */
void childProcess(char *command, int input_fd, int output_fd) {
	redirectInputOutput(input_fd, output_fd);
	char *args[MAX_COMMAND_SIZE/2];
	int num_args = parseCommand(command, args);
	execute(args, num_args);

	for(int i=0 ; i<num_args ; i++) {
		free(args[i]);
	}
}

/*
 * Helper function used by childProcess(): to redirect input_fd and output_fd as needed.
 * With the dup2() function : STDOUT_FILENO and STDIN_FILENO can be redirected.
 */
void redirectInputOutput(int input_fd, int output_fd) {
	if (input_fd != STDIN_FILENO) {
		dup2(input_fd, STDIN_FILENO);
	}
	if (output_fd != STDOUT_FILENO) {
		dup2(output_fd, STDOUT_FILENO);
	}
}

/*
 * Helper function used by childProcess(): to parse the command line arguments and separate them into an array of arguments.
 * this function handles redirections
 */
int parseCommand(char *command, char **args) {
	char *token = strtok(command, " ");
	int num_args = 0;
	while (token != NULL) {
		if (strcmp(token, "<") == 0) {
			token = handleInputRedirection(token);
		}
		else if (strcmp(token, ">") == 0) {
			token = handleOutputRedirection(token);
		}
		else {
			args[num_args++] = token;
		}
		token = strtok(NULL, " ");
	}
	args[num_args] = NULL;
	return num_args;
}

/*
 * Helper function used by parseCommand(): to handle input redirection "<"
 */
char *handleInputRedirection(char *token) {
	token = strtok(NULL, " ");
	int input_file_fd = open(token, O_RDONLY);	// for reading
	if (input_file_fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	dup2(input_file_fd, STDIN_FILENO);	// it redirects the standard input stream to the input_file_fd
	close(input_file_fd);
	return strtok(NULL, " ");
}

/*
 * Helper function used by parseCommand(): to handle output redirection ">"
 */
char *handleOutputRedirection(char *token) {
	token = strtok(NULL, " ");
	int output_file_fd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);	// for writing
	if (output_file_fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	dup2(output_file_fd, STDOUT_FILENO);	// it redirects the standard output stream to the output_file_fd
	close(output_file_fd);
	return strtok(NULL, " ");
}

/*
 * This function executes the command using the execvp()
 */
void execute(char **args, int num_args) {
	// Execute command
	if (execvp(args[0], args) == -1) {
		perror("execvp");
		exit(EXIT_FAILURE);
	}
}

/*
 * This function is called by the parent process and waits for the child process to complete using 
 * the wait(). Then it logs the child process's ID and the command using the logChildPidAndCommand() function
 */
void parentProcess(pid_t pid, char *command, char filename[50]) {
	if (wait(NULL) == -1) {
		perror("wait");
		exit(EXIT_FAILURE);
	}
	logChildPidAndCommand(pid, command, filename);
}

/*
 * This function logs the child process's ID and the command
 */
void logChildPidAndCommand(int pid, char *command, char *filename) {
	FILE *fp = fopen(filename, "a");
	if (fp == NULL) {
		printf("Failed to open log file %s\n", filename);
		return;
	}
	fprintf(fp, "PID: %d, Command: %s\n", pid, command);
	fclose(fp);
}

/*
 * This function is a signal handler that is used to handle these signals: SIGINT, SIGQUIT, SIGTSTP, and SIGCHLD.
 * When the program receives a signal, the corresponding signal handler is called to handle that signal. 
 */
void signalHandler(int sig) {
	if (sig == SIGINT) {
		printf("\nReceived SIGINT signal\n");
	}
	else if (sig == SIGQUIT) {
		printf("\nReceived SIGQUIT signal\n");
	}
	else if (sig == SIGTSTP) {
		printf("\nReceived SIGTSTP signal\n");
	}
	else if (sig == SIGCHLD) {
		childLiving = 0;
	}
}

