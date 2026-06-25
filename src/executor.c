#include "helix.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>


void execute(TokenList *tokens) {
  char **args = tokens->tokens;
  pid_t pid;
  int status;
  
  pid = fork();

  switch(pid){
    case -1:
      perror("fork");
      exit(EXIT_FAILURE);
    case 0:
    if (strcmp(args[0], "ls") == 0) {
        int argc = tokens->count;
    
        char **new_args = malloc((argc + 3) * sizeof(char *));
        if (!new_args) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    
        new_args[0] = "eza";
        new_args[1] = "--icons";
        new_args[2] = "--group-directories-first";
    
        for (int i = 1; i < argc; i++) {
            new_args[i + 2] = args[i];
        }
    
        new_args[argc + 2] = NULL;
    
        execvp("eza", new_args);
    
        perror("eza");
        free(new_args);
        exit(127);
    }
      execvp(args[0], args);
      perror(args[0]);
      free_tokens(*tokens);
      exit(127);
    default:
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        last_exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        last_exit_status = 128 + WTERMSIG(status);
    }
  }
}