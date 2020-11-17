#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>


// GLOBALS
int FGonly = 0;           // used to check for foreground only mode

// checks for foreground sig
// if foreground only on, turns it off and vice versa
void catchSIGSTP(int signo) {
	if (FGonly == 0) {
		char * message = "\nEntering foreground-only mode (& is now ignored)\n"; 
		write(STDOUT_FILENO, message, 51); 
		FGonly = 1;
	} 
	else {
		char * message = "\nExiting foreground-only mode\n"; 
		write(STDOUT_FILENO, message, 31);  
		FGonly = 0; 
	}
}

// checkes for term sig
void catchSIGINT(int signo) {
	char * message = "terminated by signal 2\n";
	write(STDOUT_FILENO, message, 24);
	exit(2); 
}

// takes stdin and copies it into input
void readInput(char *input){
  char *currLine = NULL;
  size_t len = 0;

  getline(&currLine, &len, stdin);
  currLine[strlen(currLine) - 1] = '\0';
  strcpy(input, currLine); 
}

// takes each word in input, seperated by a space, and puts each word into an args element 
// returns the number of elements in args
int parseInput(char *input, char *args[]){
  char *token = strtok(input, " ");
  int count = 0;
  
  while(token != NULL){
    args[count] = malloc(strlen(token) * sizeof(char));
    strcpy(args[count], token);
    token = strtok(NULL, " ");
    count ++;
  }
  return count;
}

// used for testing purposes to print array
void printArray(char* someArray[]){
  printf("PRINTING ARRAY\n");
  for(int i = 0; someArray[i]!= 0; i++){
    printf("Array Element %d: %s \n", i, someArray[i]);
  }
}

// checks args for "<" or ">"
// if found places element into proper filepath and changes int to reflect the direction
// returns 1 if directionals were found and 0 if no directionals were found
int getRedirection(char *args[], char *outputFile, char *inputFile, int *redirectOut, int *redirectIn){
  int count = 0;
 
  for(int i = 0; args[i] != NULL; i++){
    
    if(!strcmp(args[i], "<")){
      strcpy(inputFile, args[i+1]);
      *redirectIn = 1;
      count++;
      }
    if(!strcmp(args[i], ">")){
      strcpy(outputFile, args[i+1]);
      *redirectOut = 1;
      count++;
    }
  }

  if(count == 0){
    return 0;
  }
  else{
    return 1;
  }

}

// called after verifying that args[0] is "cd"
// if args[1] is NULL, then the destination is the HOME dir
// otherwise uses chdir to move directories 
void changeDirectory(char *args[]){
  
  // printArray(args);
  if(args[1] == NULL){
    chdir(getenv("HOME"));
  }
  else{
    if(chdir(args[1]) == -1){
      printf("Directory not found\n");
    }
  }
}

// called on status command
// prints exit value of int
void getStatus(int status){
  if(WIFEXITED(status)){
    printf("exit value %d\n", WIFEXITED(status));
  }
  else{
    printf("terminated by signal %d\n", WTERMSIG(status));
  }
}

// take two strings
// looks for "$$" in args
// replaces each instance of $$ with pid 
void replaceCashSigns(char *args, char *pid){
    char *find = "$$";
    char *replace =  calloc(strlen(pid), sizeof(char));
    strcpy(replace, pid);
    char *dest = malloc (strlen(args)-strlen(find)+strlen(replace)+1);
    char *ptr;

    strcpy (dest, args);

    // Creates pointer to first part char of dest that is "$$"
    ptr = strstr (dest, find);

    while(ptr != NULL)
    {
        memmove (ptr+strlen(replace), ptr+strlen(find), strlen(ptr+strlen(find))+1);
        strncpy (ptr, replace, strlen(replace));
        // check for another find var
        ptr = strstr(dest,find);
    }
    strcpy(args, dest);  
}

// checks for $$ in args
// if found calls replaceCashSigns to expand
void replace$$(char *args[], char *pid){
  char *ptr;
  sprintf(pid, "%d", getpid());
  for(int i = 0; args[i] != 0; i++){
    ptr = strstr(args[i], "$$");
    if(ptr != NULL){
      replaceCashSigns(args[i], pid);
    }
  }
}

// checks if the last element in args is "&"
// if yes, changes the int background to 1 and deletes last element
void checkBackground(char* args[], int *background, int lastElement)
{
  if(args[lastElement] != NULL){
    if(!strcmp(args[lastElement], "&")){
      *background = 1;
      // printf("Last elemnet: %s\n",args[lastElement]);
      args[lastElement] = NULL;
    }
  } 
}


// Where the magic happens
void cmdprompt(){
  
  // These are the Signal handling Structs
  // Parent SIG_INT struct - ignores SIG_INT
  struct sigaction SIGINT_parent_struct;
  SIGINT_parent_struct.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_parent_struct.sa_mask);
  SIGINT_parent_struct.sa_flags = SA_RESTART;

  // Parent SIG_TSTP struct - calls the bg_handler
  struct sigaction SIGSTP_parent_struct;
  SIGSTP_parent_struct.sa_handler = catchSIGSTP;
  sigfillset(&SIGSTP_parent_struct.sa_mask);
  SIGSTP_parent_struct.sa_flags = SA_RESTART;

  // Child SIG_TSTP struct - ignores SIG_TSTP
  struct sigaction SIGSTP_child_struct;
  SIGSTP_child_struct.sa_handler = SIG_IGN;
  sigfillset(&SIGSTP_child_struct.sa_mask);
  SIGSTP_child_struct.sa_flags = 0;

  // Child SIG_INT struct - default action
  struct sigaction SIGINT_child_struct;
  SIGINT_child_struct.sa_handler = SIG_DFL;
  sigfillset(&SIGINT_child_struct.sa_mask);
  SIGINT_child_struct.sa_flags = 0;

  // Sigaction calls for the parent sigint and sigtstp signals
  sigaction(SIGINT, &SIGINT_parent_struct, NULL);
  sigaction(SIGTSTP, &SIGSTP_parent_struct, NULL);


  int run = 1;
  
  while(run){
    char *input = {NULL};
    char *args[512] = {NULL};
    char *execArray[512] = {NULL};
    char inputFile[30];
    char outputFile[30];
    char pid[10];
    int count, status, foreGND, lastElement, redirectIn = 0, redirectOut = 0, background = 0;
    pid_t spawnPid;

    printf(": ");
    fflush(stdout);

    // get input, supported size is 2048 chars
    input = calloc(2048, sizeof(char));
    readInput(input);

    // put input in args array 
    count = parseInput(input, args);
    lastElement = count -1;

    // check for file directionals
    int check = getRedirection(args, outputFile, inputFile, &redirectOut, &redirectIn);

    // fill execArray based on directionals
    // used for exec call 
    if(check){
      execArray[0] = (char *)malloc((strlen(args[0])+1)*sizeof(char));
      strcpy(execArray[0],args[0]);
    }
    else{
      for(int i = 0; args[i] != NULL; i++){
        execArray[i] = (char *)malloc((strlen(args[i])+1)*sizeof(char));
        strcpy(execArray[i],args[i]);
      }
    }

    // expand $$ into pid 
    // checks for $$ in each args and replace with pid num
    replace$$(execArray, pid);


    // check for background cmd
    checkBackground(execArray, &background, lastElement);

    // checks for comments or blank line
    // if blank or comment, continue onword
    if((args[0] == NULL) || !strncmp(args[0], "#", 1)){
    continue;
    }

    // built in commands
    else if(!strcmp(execArray[0], "exit")){
      run = 0;
    }
    else if(!strcmp(execArray[0], "status")){
      getStatus(status);
    }
    else if(!strcmp(execArray[0], "cd")){
      changeDirectory(execArray);
    }

    
    // command isn't a built in command, empty line, or comment.
    else{

      // create child
      spawnPid = fork();

        // make sure fork was successful
        if(spawnPid == -1){ 
          printf("Error Forking\n");
          fflush(stdout);
          exit(1);
          }

        // when successful 
        if(spawnPid == 0){
          
          // Child Sigs
          sigaction(SIGINT, &SIGINT_child_struct, NULL);
          sigaction(SIGTSTP, &SIGSTP_child_struct, NULL);

          // file to take input from is present
          if(redirectIn == 1) {

            int sourceFile = open(inputFile, O_RDONLY);
            if(sourceFile == -1){
              printf("can't open %s for input\n", inputFile);
              fflush(stdout);
              status = 1;
              exit(1);
            }

            int sourceResult = dup2(sourceFile, 0);
            if(sourceResult == -1){
              printf("can't open %s for input\n", inputFile);
              fflush(stdout);
              status = 1;
              exit(2);
              }
          }

          // file to write to is present
          if (redirectOut == 1){

            int dest = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(dest == -1){
              printf("can't open %s for output\n", outputFile);
              fflush(stdout);
              status = 1;
              exit(1);
            }

            int destResult = dup2(dest, 1);
            if(destResult == -1){
              printf("can't open %s for output\n", outputFile);
              fflush(stdout);
              status = 1;
              exit(1);
              }
          }

          // used to call all other cmds 
          status = execvp(execArray[0], execArray);

          if(status == -1){
            kill(getpid(),SIGTERM);
          }
        }

      // if background was signaled and not in foreground only mode
      if (background == 1 && FGonly == 0) {

              printf("background pid is %d\n",spawnPid);
              fflush(stdout);
              int bgChildStatus;
              waitpid(spawnPid, &bgChildStatus, WNOHANG);

            // If no &, parent will wait
            } else{

                int childStatus;
                waitpid(spawnPid, &childStatus, 0);

                if (WIFEXITED(childStatus)) {
                    status = WEXITSTATUS(childStatus);
                
                // If child killed by a signal, return signal to status
                } else if (WIFSIGNALED(childStatus)) {
                    status = WTERMSIG(childStatus);

                    // 15 means no file or directory
                    if (status == 15) {
                        printf("%s: no such file or directory\n",args[0]);
                        fflush(stdout);

                    } else {
                        printf("terminated by signal %d\n",status);
                        fflush(stdout);
                    }
                }
            }
          }
    // No zombies!       
    while ((spawnPid = waitpid(-1, &status, WNOHANG)) > 0) {
			    if (WIFEXITED(status)) {
				    printf("background pid %d is done: exit value %d\n", spawnPid, WEXITSTATUS(status)); 
			    }
			    else {
				    printf("background pid %d is done: terminated by signal %d\n", spawnPid, WTERMSIG(status)); 
			}
		}
  }     
}

int main(void) {
  
  cmdprompt();

  return 0;
}