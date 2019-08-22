/*---------------------------------------------------*/
/*               CSC 360 - p1 Assingment             */
/*                                                   */
/* Date: September 29, 2014                          */
/* Name: Daniel Gilroy                               */
/* Student ID: V00813027                             */       
/*                                                   */
/*---------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STRING_LENGTH 32
#define PATH_ADDITIONAL 10
#define ARRAY_SIZE 16
#define GROW_RATE 2
#define DIRECTORY_NOT_FOUND 2
#define DIRECTORY_HOME_ERROR 3

typedef struct job{
	pid_t pid;
	char *name;
	char status;
	struct job *next;
}job;

static job *job_list = NULL;
static int update_jobs = 0;

int run_command(char **, char *);
int cd_command(int, char **);
int cd_to_home(char *);
void get_prompt_path(char **);
int parse_input(char *, int *, char ***);
int insert_job(pid_t, char *, char);
job *find_job(pid_t);
int print_remove_jobs();
int print_jobs();
int number_of_jobs();

static void sigchld_hdl (int sig){

	int status;	
	pid_t s_pid;

	while(1){
		s_pid = waitpid(-1, &status, WNOHANG);
		if(s_pid < 0){
			//perror ("waitpid");
			break;
		}
		if(s_pid > 0){
			find_job(s_pid)->status = 'T';
			update_jobs = 1; //Flag to let main know about job changes
		}
		if(s_pid == 0){
			break;
		}
	}
}

int main(){

    struct sigaction act;

    memset (&act, '\0', sizeof(act));
    act.sa_handler = sigchld_hdl;
    act.sa_flags = SA_NODEFER;

    if(sigaction(SIGCHLD, &act, 0)){
		perror ("sigaction");
		return 1;
    }

	int exit_switch = 0;
	int return_result = 0;
	char *user_input;
	char *raw_user_input;
	char *prompt_path;
	char **args = (char **) malloc(ARRAY_SIZE * sizeof(*args));
	if(args == NULL){
        printf("memory error\n");
        exit(1);		
	}
	int argc = 0;
	
	get_prompt_path(&prompt_path);


	while(!exit_switch){

		/* Continuously prompt user for input until text is entered */
		/* If update_jobs flag is set, print termination messages and remove jobs from job_list */
		do{
			user_input = readline(prompt_path);
		
			if(update_jobs == 1){
						print_remove_jobs();
				update_jobs = 0;
			}

		}while(!strcmp(user_input, ""));

		int length = strlen(user_input) + 1;
		raw_user_input = (char *) malloc(length * sizeof(char));
		if(raw_user_input == NULL){
        	printf("memory error\n");
        	exit(1);		
		}

		/* Parse arguments from user input and make a copy of */
		/* user input for later use in creatings jobs for job list */
		strncpy(raw_user_input, user_input, length);
		parse_input(user_input, &argc, &args);

		if(!strcmp(user_input, "exit") || !strcmp(user_input, "Exit")){
			exit_switch = 1; /* Exit Shell */
		}else if(!strcmp(args[0], "cd")){

			return_result = cd_command(argc, args);
			
			if(return_result == 0){
				
				/* Free previous prompt_path memory set at beginning of main */
				/* and get new prompt path */
				free(prompt_path);
				get_prompt_path(&prompt_path);
				
			}else if(return_result == DIRECTORY_NOT_FOUND){
				printf("cd: %s: No such file or directory\n", args[1]);
			}else if(return_result == DIRECTORY_HOME_ERROR){
				char *current_user = getlogin();
				printf("cd: Error changing directory to /home/%s\n", current_user);
			}else{
				printf("cd: Error changing directory\n");
			}

		}else if(!strcmp(args[0], "bglist")){
			return_result = print_jobs();
		}else{
			return_result = run_command(args, raw_user_input);
			if(return_result < 0){
				/* fork() failed */
				exit(1);
			} 
		}
	
		free(raw_user_input);
		free(user_input);
	}

	printf("Shell: Exited \n");
	return 0;
}

/* Call fork() and execvp() to run unix commands */
int run_command(char **args, char *user_input){

	char **temp_args = args;
	int status;

	int background = 0;
	if(!strcmp(args[0], "bg") && args[1] != NULL){
		background = 1;
		temp_args = &temp_args[1];
	}

	pid_t childpid = fork();

	if(childpid >= 0){ /* Successful fork */

		if(childpid == 0){

			/* Prints error and exits child process if command does not exist */
			if(execvp(temp_args[0], temp_args) < 0){
				perror(temp_args[0]);
				_exit(EXIT_FAILURE);
			}
		}
	
		if(childpid > 0){

			if(background == 1){

				/* Add 3 to user_input to skip over "bg" */
				/* Use user_input for job name */
				/* Prints childpid and user_input to let user know background process has started */
				user_input = (user_input + 3);
				printf("%d:\t%s\n", childpid, user_input);
				insert_job(childpid, user_input, 'R');

			}else{
	
				if(waitpid(childpid, &status, WUNTRACED | WCONTINUED) == -1){
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
			}
		}

	}else{
		perror("Fork");
		return -1;
	}

	return 0;
}

int cd_command(int argc, char **args){

	int return_result = 1;
	char *path = NULL;

	if(argc > 1){
		path = args[1];
	}

	if(path == NULL || *path == '~'){
		return_result = cd_to_home(NULL);
	}else{
		if(chdir(path) != 0){
			return_result = DIRECTORY_NOT_FOUND;
		}else{
			return_result = 0;
		}
	}

	return(return_result);
}

int cd_to_home(char *path_from_home){

	int size = 0;
	int error; /* DIRECTORY_NOT_FOUND or _DIRECTORY_HOME_ERROR */
	char *current_user = getlogin();
	if(current_user == NULL){
		return 1;
	}

	/* size has added value of PATH_ADDITIONAL to accommodate for additional "/home/" */
	/* and added value of strlen(path_from_home) if a path is given after the '~' */
	if(path_from_home == NULL){
		size = (strlen(current_user) + PATH_ADDITIONAL);
		error = DIRECTORY_HOME_ERROR;
	}else{
		size = (strlen(current_user) + strlen(path_from_home) + PATH_ADDITIONAL);
		error = DIRECTORY_NOT_FOUND;
	}

	char *path_change = (char *) malloc(size * sizeof(char)); 
	if(path_change == NULL){
        printf("memory error\n");
        exit(1);
    }

	snprintf(path_change, size, "/home/%s/%s", current_user, path_from_home);

	if(chdir(path_change) != 0){
		free(path_change);
		return error;
	}

	free(path_change);
	return 0;
}

void get_prompt_path(char **prompt_path_return){
	int size = STRING_LENGTH;
	char *current_directory = (char *) malloc(size * sizeof(char));
	if(current_directory == NULL){
        printf("memory error\n");
        exit(1);
    }

	while(getcwd(current_directory, size) == NULL){
		size = size * GROW_RATE;
		current_directory = (char *) realloc(current_directory, size * sizeof(char));
		if(current_directory == NULL){
            printf("memory error\n");
            exit(1);
        }
	}

	/* size has added value of PATH_ADDITIONAL to accommodate for additional "Shell: > " */
	size = (size + PATH_ADDITIONAL);

	*prompt_path_return  = (char *) malloc(size * sizeof(char));
	if(*prompt_path_return == NULL){
        printf("memory error\n");
        exit(1);
    }

	snprintf(*prompt_path_return, size, "Shell: %s> ", current_directory);
	free(current_directory);
}

int parse_input(char *user_input, int *argc, char ***args){

	int i = 0;
	int arg_count = 1;
	static int array_size = ARRAY_SIZE;

	while(user_input[i] != '\0'){

		if(user_input[i]  == ' ')
			arg_count++;

		i++;
	}

	if(arg_count > array_size){
		array_size = array_size * GROW_RATE;
		char **tmp = (char **) realloc(*args, array_size * sizeof(*args));
		if(tmp == NULL){
			perror("Error reallocating args array");
		}else{
			*args = tmp;
		}
	}

	int index = 0;
	(*args)[index] = strtok(user_input, " ");

	while((*args)[index] != NULL){
		index++;
		if(index >= ARRAY_SIZE){
			return 1;
		}

		(*args)[index] = strtok(NULL, " ");
	}

	*argc = arg_count;

	return 0;
}

int insert_job(pid_t pid, char *user_input, char status){

	int size;
	job *current = job_list;
	job *new_job;
	new_job = (job *) malloc(sizeof (job));
	if(new_job == NULL){
        printf("memory error\n");
        exit(1);	
	}

	/* Extra failsafe! insert_job should never run if user_input == NULL */
	if(user_input != NULL){
		size = strlen(user_input) + 1;
	}else{
		return 1;
	}

	char *job_name = (char *) malloc(size * sizeof(char));
	if(job_name == NULL){
		printf("memory error\n");
        exit(1);
	}

	strncpy(job_name, user_input, size);

	new_job->pid = pid;
	new_job->name = job_name;
	new_job->status = status;
	new_job->next = NULL;

	/* If job_list is empty, insert only job else insert job at end of list */
	if(job_list == NULL){
		job_list = new_job;
	}else{

		/* Get last job in list */
		while(current->next != NULL){
			current = current->next;
		}

		current->next = new_job;
	}

	return 0;
}

/* Returns a pointer to the job which matches the pid passed in as an argument */
job *find_job(pid_t job_pid){
	job *current = job_list;

	while(current != NULL){

		if(current->pid == job_pid){
			return current;
		}else{
			current = current->next;
		}
	}

	return NULL;
}

/* Prints jobs that have been terminated and marked with status 'T' */
/* After printing the jobs, the jobs will be removed from the job_list */
int print_remove_jobs(){

	job *current = job_list;
	job *previous = job_list;

	/* Job list is empty */
	if(job_list == NULL){
		return 1;
	}

	while(current != NULL){

		/* Only job in list */
		if(job_list->status == 'T'){
			
			printf("%d: %s\tTerminated\n", job_list->pid, job_list->name);

			if(job_list->next == NULL){
				free(job_list->name);
				free(job_list);
				job_list = NULL;
			}else{
				free(job_list->name);
				free(job_list);
				job_list = job_list->next;
			}

		}else if(current->status == 'T'){
			printf("%d:\t%s\tTerminated\n", current->pid, current->name);
			free(current->name);
			free(current);
			previous->next = current->next;
		}else{
			previous = current;
		}

		current = current->next;
	}

	return 0;
}

/* Prints jobs and total number of jobs currently */
/* running in the background */
int print_jobs(){

	job *current = job_list;
	int count = 0;

	if(job_list == NULL){
		printf("Total Background Jobs:\t%d\n", count);
		return 1;
	}else{

		while(current != NULL){
			printf("%d:\t%s\t\n", current->pid, current->name);
			current = current->next;
			count++;
		}

		printf("Total Background Jobs:\t%d\n", count);
	}

	return 0;
}

/* Prints the number of background jobs currently running */
int number_of_jobs(){

	job *current = job_list;
	int count = 0;

	while(current != NULL){
		current = current->next;
		count++;
	}

	return count;
}