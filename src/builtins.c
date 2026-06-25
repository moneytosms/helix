#include "helix.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


int is_builtin(char *cmd) {
  return (!strcmp(cmd,"cd")|| !strcmp(cmd,"exit")|| !strcmp(cmd,"pwd"));
}

int run_builtin(TokenList tokens) {
  char *cmd = tokens.tokens[0];
  if(!strcmp(cmd,"cd")) {
    char *path = tokens.tokens[1];
    if(path == NULL){
    chdir("~");
    perror("chdir");
    }
    else {
      chdir(path);
      perror("chdir");
    }
    return 1;
  }
  else if(!strcmp(cmd,"exit")) {
    exit(0);
    return 1;
  }
  else if(!strcmp(cmd,"pwd")) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
    return 1;
  }
  else {
    printf("Unknown builtin command: %s\n", cmd);
    return 0;
  }
}
