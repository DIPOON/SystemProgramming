#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CMD_ARG 64

#define SHNAME "simplesh"

extern char **environ;

// Call this to exit from a fatal error
static void fatal(const char* str)
{
	perror(str);
	exit(1);
}

/*
 * makelist() will split the input string to list array
 * and return the number of items in the list array.
 */
static int makelist(char* input, const char* delimiters, char** list, int max)
{
	int tokens = 0;

	// XXX
	// Split input to list with strtok()
	// tokens++;
	char* Pointer = strtok(input, delimiters);
	while (Pointer != NULL)
	{
		*(list + tokens) = Pointer;
		Pointer = strtok(NULL, delimiters);
		tokens++;
	} 

	return tokens;
}

/*
 * Check /proc/self/fd for file-descriptor leaks.
 * If this function prints something, it means some file-descriptors are left opened.
 * This function runs on exit().
 */
static void check_fd(void)
{
	struct dirent* p;
	pid_t pid;
	char pidpath[32], path[PATH_MAX], buf[PATH_MAX];
	DIR* d;

	pid = getpid();
	sprintf(pidpath, "/proc/%d/fd", pid);
	d = opendir(pidpath);

	if (!d)
		return;

	while ((p = readdir(d))) {
		// Skip the names "." and ".." as we don't want to recurse on them
		if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
			continue;

		// Get real path
		sprintf(path, "%s/%s", pidpath, p->d_name);

		// Skip stdin, stdout, stderr
		if (strcmp(p->d_name, "0") == 0)
			continue;
		if (strcmp(p->d_name, "1") == 0)
			continue;
		if (strcmp(p->d_name, "2") == 0)
			continue;

		// All /proc/*/fd files are supposed to be symlinks
		if (p->d_type != DT_LNK)
			continue;

		// Initialize buf as readlink doesn't set EOF
		memset(buf, 0, PATH_MAX);
		readlink(path, buf, PATH_MAX);
		// Skip printing when this fd is check_fd() itself
		if (strcmp(buf, pidpath))
			printf("fd %s: %s\n", p->d_name, buf);
	}

	closedir(d);
}

int main(int argc, char** argv)
{
	char input[BUFSIZ]; // Input from the user
	char* input_arr[MAX_CMD_ARG]; // input split to string array
	char cwd[PATH_MAX]; // Current working directory

	int i, tokens;
	int flags; // open() flags
	bool eof;

	// Ignore Ctrl+Z (stop process)
	signal(SIGTSTP, SIG_IGN);
	// Allow forked processes to write to the terminal
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	// Re-ape child processes automatically (zombie processes)
	signal(SIGCHLD, SIG_IGN);

	// Check for file-descriptor leaks on exit()
	atexit(check_fd);

	while (1) {
		// XXX
		// Set cwd here
		if (getcwd(cwd, sizeof(cwd)) != NULL)
		{
			printf(SHNAME ":%s$ ", cwd);
		}
		else
		{
			fatal("getcwd() error");
		}

		// Initialize input by inserting NULL(EOF) to input[0]
		input[0] = '\0';
		while (!(eof = feof(stdin)) && !fgets(input, BUFSIZ, stdin));
		if (eof) {
			// Ctrl+D EOF
			printf("\n");
			exit(0);
		}
		input[strlen(input) - 1] = '\0'; // Replace '\n' at the end with NULL(EOF)

		// Blank Input Exception
		int InputCheckPoint = 0;
		while (input[InputCheckPoint] == ' ')
		{			
			InputCheckPoint++;
		}
		if (InputCheckPoint == strlen(input))
		{
			continue;
		}

		// Parse input
		tokens = makelist(input, " \t", input_arr, MAX_CMD_ARG);

		// XXX
		// Implement built-in cd
		if (!strcmp(input_arr[0], "cd"))
		{
			if (input_arr[1] == NULL)
			{
				printf(SHNAME ": cd: no arguments\n");
			}
			else if (input_arr[2] != NULL)
			{
				printf(SHNAME ": cd: too many arguments\n");
			}
			else if (!strcmp(input_arr[1], "/root"))
			{
				printf(SHNAME ": cd: /root: Permission denied\n");
			}
			else if (!strcmp(input_arr[1], "."))
			{
			}
			else if (!strcmp(input_arr[1], ".."))
			{
				char Buffer[PATH_MAX];
				strcpy(Buffer, cwd);
				char* Directory[PATH_MAX];
				int PathDepth = makelist(Buffer, "/", Directory, PATH_MAX);
				memset(cwd, 0, PATH_MAX);
				for (int i = 0; i < PathDepth - 1; i++)
				{
					strcat(cwd, "/");
					strcat(cwd, Directory[i]);
				}
				// when in /, Keep the directory
				if (cwd[0] != '/')
				{
					strcat(cwd, "/");
				}
				chdir(cwd);
			
			} 
			else if (input_arr[1])
			{
				// Change Absolute Directory
				if (input_arr[1][0] == '/')
				{
					if (chdir(input_arr[1]) == -1)
					{
						printf(SHNAME ": cd: %s: No such file or directory\n", input_arr[1]);
					}
					else if (chdir(input_arr[1]) == 0)
					{
					}
					else
					{
						fatal("chdir(input_arr[1] error");
					}
				}
				// Change Relative Directory
				else if (input_arr[1][0] != '/')
				{
					strcat(cwd, "/");
					strcat(cwd,input_arr[1]);
					if (chdir(cwd) == -1)
					{
						printf(SHNAME ": cd: %s: No such file or directory\n", input_arr[1]);
					}
					else if (chdir(cwd) == 0)
					{
					}
					else
					{
						fatal("chdir(cwd) error");
					}
				}
			}
			else
			{
				fatal("cd input_arr corrupted");
			}
			
		}

		// XXX
		// Implement built-in exit
		else if (!strcmp(input_arr[0], "exit"))
		{
			if (input_arr[2] != NULL)
			{
				printf(SHNAME ": exit: too many arguments\n");
			}
			else if (input_arr[1] != NULL)
			{
				exit(atoi(input_arr[1]));
			}
			else
			{
				exit(0);
			} 
		}

		// XXX
		// Implement built-in pwd
		else if (!strcmp(input_arr[0], "pwd"))
		{
			if (input_arr[1] != NULL)
			{
				printf(SHNAME ": pwd: too many arguments\n");
			}
			else
			{
				printf("%s\n", cwd);
			}
		}

		// XXX
		/*
		 * Implement command execution.
		 * Implement redirection.
		 */
		else
		{
			// Command Execution
			char Buffer[BUFSIZ]; // command but naming should be refined...
			// i don't understand why this is required... if not just error. why not local variable gone?
			memset(Buffer, 0, sizeof(Buffer));
			int child_status = 3;
			int FileDescriptor = 0;
			int FileDescriptorBackup = 0;
			int shouldPass = 0;
			// Absolute
			if (input_arr[0][0] == '/')
			{
				strcat(Buffer, input_arr[0]);
			}
			// In this directory
			else if (access(input_arr[0], R_OK) == 0)
			{
				strcat(Buffer, cwd);
				strcat(Buffer, "/");
				strcat(Buffer, input_arr[0]);
			}
			// System Basic Directory
			else
			{
				strcat(Buffer, "/usr/bin/");
				strcat(Buffer, input_arr[0]);
			}
			for (int i = 1; i < tokens; i++)
			{
				// Make Argument without >, >>
				// Redirect
				if (strcmp(input_arr[i], ">") == 0 || strcmp(input_arr[i], ">>") == 0)
				{
					// root deny
			                if (input_arr[i + 1][0] == '/' &&
			                    input_arr[i + 1][1] == 'r' &&
			                    input_arr[i + 1][2] == 'o' &&
                            		    input_arr[i + 1][3] == 'o' &&
                            		    input_arr[i + 1][4] == 't')
		                        {
						printf("Failed to open file for stdout redirection: Permission denied\n");
		                                shouldPass = 1;
						break;
		                        }
					// Check a separate file exists
					else if (access(input_arr[i + 1], W_OK) == -1)
                                        {
						// if a separate file is without None directory like out
						if (input_arr[i + 1][0] != '/')
						{
							FILE* pFile = fopen(input_arr[i + 1], "w");
							fclose(pFile);
						}
						else
						{
							shouldPass = 1;
                                                	printf("Failed to open file for stdout redirection: No such file or directory\n");
						}
                                        }
					if (access(input_arr[i + 1], W_OK) == 0)
					{
						if (strcmp(input_arr[i], ">") == 0)
						{
							FileDescriptor = open(input_arr[i + 1], O_RDWR | O_CREAT | O_TRUNC);
						}
						else if (strcmp(input_arr[i], ">>") == 0)
						{
							FileDescriptor = open(input_arr[i + 1], O_RDWR | O_CREAT | O_APPEND);
						}
						else
						{
							fatal("How did you get in? Check > or >> fails");
						}
						if (FileDescriptor == -1)
						{
							fatal("FileDescriptor opening in > fails");
						}
						FileDescriptorBackup = dup(1);
						dup2(FileDescriptor, 1);
						input_arr[i] = NULL;
						break;
					}
				}
				else
				{
				}
			}
			if (shouldPass == 1)
                        {
                        	shouldPass = 0;
                                memset(input_arr, 0, MAX_CMD_ARG);
                                continue;
                        }
			// Check vaild instruction
			if (access(Buffer, R_OK) == -1)
			{
				printf("Failed to exec: No such file or directory\n");
			}
			// fork
			else if (fork() == 0)
			{
				// Child Process execv
				if (execve(Buffer, input_arr, environ) < 0)
				{
					printf("Failed to %s: No such file or directory\n", input_arr[0]);
					exit(0);
					wait(&child_status);
				}
				else
				{
					fatal("execve fails");
				}
			}
			else
			{
				// Parent Process wait
				dup2(FileDescriptorBackup, 1);
				wait(&child_status);
			}
			if (FileDescriptor > 0)
			{
				int Return = close(FileDescriptor);
				if (Return < 0)
				{
					fatal("FileDescriptor fails");
				}
			}
			if (FileDescriptorBackup > 0)
			{
				int Return = close(FileDescriptorBackup);
				if (Return < 0)
				{
					fatal("FileDescriptorBackup fails");
				}
			}
		}
		
		
		// XXX
		// Initialize
		memset(input_arr, 0, sizeof(input_arr));
	}

	return 0;
}
