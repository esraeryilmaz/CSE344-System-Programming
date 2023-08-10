#ifndef TERMINALEMULATOR_H
#define TERMINALEMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>


void terminalEmulator();
int parseUserInput(char *user_input, char **commands);
void createPipes(int pipes[][2], int num_commands);
void signalHandler(int sig);
void logChildPidAndCommand(int pid, char *command, char *filename);

void executeCommand(char *command, int input_fd, int output_fd,char filename[50]);
void childProcess(char *command, int input_fd, int output_fd);
void parentProcess(pid_t pid, char *command, char filename[50]);
int parseCommand(char *command, char **args);
void redirectInputOutput(int input_fd, int output_fd);
char *handleInputRedirection(char *token);
char *handleOutputRedirection(char *token);
void execute(char **args, int num_args);


#endif