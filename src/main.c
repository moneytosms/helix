#include "helix.h"
#include "stdio.h"
#include <stdlib.h>
#include <sys/utsname.h>

int last_exit_status=0;

int main() {
  char input[1024];
  struct utsname info;
  while (1) {

    if (uname(&info)) {
      perror("uname");
      return 1;
    }

    char *hostname = info.nodename;
    char *username = getenv("USER");

    const char *chevron =
        (last_exit_status == 0)
            ? "\033[32m❯"
            : "\033[31m❯";

    printf("\033[36m%s \033[0mon \033[33m%s %s \033[0m",
           username,
           hostname,
           chevron);

    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin)) {
      break;
    }

    TokenList tokens = tokenize(input);
    if (tokens.tokens[0] == NULL) {
      free_tokens(tokens);
      continue;
    }
    // for (int i = 0; i < tokens.count; i++) {
    //   printf("%d : %s \n", i, tokens.tokens[i]);
    // }

    if (is_builtin(tokens.tokens[0])) {
      if (run_builtin(tokens)) {
        free_tokens(tokens);
        continue;
      }
    free_tokens(tokens);
    }
    execute(&tokens);

    printf("\n");
    free_tokens(tokens);
    fflush(stdout);
  }

  return 0;
}
