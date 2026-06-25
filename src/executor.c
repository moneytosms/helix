#include "helix.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

void execute(TokenList *tokens) {
  char **args = tokens->tokens;
  pid_t pid;

  pid = fork();

  switch(pid){
    case -1:
      perror("fork");
      exit(EXIT_FAILURE);
    case 0:
      execvp(args[0], args);
      perror("execvp");
      printf("execvp failed: %s\n command not found", args[0]);
      free_tokens(*tokens);
      exit(EXIT_FAILURE);
    default:
      waitpid(pid, NULL, 0);
      break;
  }
}