# helix

![C](https://img.shields.io/badge/language-C-555555)
![Dependencies](https://img.shields.io/badge/dependencies-libc%20only-brightgreen)
![Platform](https://img.shields.io/badge/platform-Linux-informational)

A Unix shell written from scratch in C. No libraries beyond libc: the pipeline
executor, job control, line editor, history, syntax highlighter and prompt are
all hand written.

```
╔══════════════════════════════════════════════╗
║ helix v1.0.0   a shell of your own           ║
╠══════════════════════════════════════════════╣
║ CPU   █░░░░░░░░░    8%                       ║
║ MEM   ██░░░░░░░░   18%   7.6 GB              ║
║ DISK  ░░░░░░░░░░    4%                       ║
╠══════════════════════════════════════════════╣
║ Uptime  0d 2h 8m                             ║
║ User    sms                                  ║
╚══════════════════════════════════════════════╝

 sms on Aether at …/Projects/helix via  main * ⇡6 ❯ ls | grep .c
```

## Highlights

- Pipelines of any length, per-command redirection (`>` `>>` `<`)
- Real job control: process groups, `Ctrl+Z`, `bg`, `fg`, `jobs`
- Hand-written raw-mode line editor with persistent history
- Live syntax highlighting: green for found commands, red for missing
- Git-aware prompt with dirty and ahead/behind markers, exit status and timing
- Startup dashboard (CPU, memory, disk, uptime) read from `/proc`
- `~/.helixrc` for aliases and settings
- Zero dependencies beyond libc

## Requirements

Linux (reads `/proc`), `gcc` and `make`. `git` is needed for the prompt's branch info. `eza`, `bat`, `fd` and `rg` are used when installed.

## Build

```sh
make          # gcc -g -fsanitize=address -Wall -Wextra
./helix
```

```sh
./helix --version
./helix --help
make clean
```

## What it does

**Commands and pipelines.** Any number of stages: `cat file | sort | uniq | wc -l`.
Each stage gets its own process; the shell wires the pipes and waits for the lot.

**Redirection.** `>` truncates, `>>` appends, `<` reads. Redirection is per
command, so `ls | grep .c > out.txt` sends only grep's output to the file.

**Job control.**

| Action | Effect |
| --- | --- |
| `sleep 30 &` | runs in the background, prints `[1] 12345` |
| `Ctrl+Z` | stops the foreground job, prompt comes back |
| `jobs` | lists running and stopped jobs |
| `bg [n]` | resumes a stopped job in the background |
| `fg [n]` | brings a job back to the foreground |
| `Ctrl+C` | kills the foreground job, never the shell |

Every job runs in its own process group and is handed the terminal while it is
in the foreground, so Ctrl+C and Ctrl+Z are delivered by the kernel to the job
rather than to helix. Finished background jobs are reported at the next prompt.

**Line editing.** Raw mode, one byte at a time.

| Key | Effect |
| --- | --- |
| `←` `→` | move the cursor, insert anywhere in the line |
| `↑` `↓` | walk through history |
| `Home` / `Ctrl+A` | start of line |
| `End` / `Ctrl+E` | end of line |
| `Ctrl+K` | delete from the cursor to the end |
| `Ctrl+L` | clear the screen |
| `Ctrl+C` | throw the line away |
| `Ctrl+D` | exit on an empty line |

**History.** Kept in `~/.helix_history` and reloaded at startup. Consecutive
duplicates are dropped. The `history` builtin prints the list.

**Syntax highlighting.** While you type, a command name turns green if it can be
found (in `$PATH`, as a builtin or as an alias) and red if it cannot. Operators
are cyan, quoted text is yellow. `$PATH` lookups are cached for the session.

**Prompt.** User, host, working directory, git branch with a `*` when the tree
is dirty and `⇡`/`⇣` when it differs from its upstream, and how long the last
command took once it passes two seconds. Deep paths are cut to their last two
components: `/mnt/d/Projects/helix` becomes `…/Projects/helix`. The chevron is
green when the last command succeeded and red when it failed, with the exit
code shown next to it.

**Dashboard.** CPU, memory, disk and uptime read from `/proc` and `statvfs`,
drawn once at startup. Turn it off with `set DASHBOARD=off`.

**Builtins.** `cd` `exit` `pwd` `jobs` `fg` `bg` `history` `alias`. They run
inside the shell process when they are the whole command line, and in a forked
child when they appear in a pipeline.

## Configuration: `~/.helixrc`

Read once at startup. One directive per line, `#` starts a comment.

```sh
# aliases: the name is replaced by these words, arguments are kept
alias ll="ls -la"
alias gs="git status"

# settings
set HISTORY_SIZE=500     # entries kept, default 1000, max 1000
set DASHBOARD=off        # skip the startup dashboard
```

A line that is neither `alias` nor `set` produces a warning and is skipped.

Aliases are expanded once, so `alias ls="ls -F"` cannot loop.

## Command mapping

If a modern replacement is installed, helix uses it: `ls` runs `eza --icons
--group-directories-first`, `cat` runs `bat --paging=never`, `find` runs `fd`,
`grep` runs `rg`. The table lives at the top of `src/executor.c`.

## Layout

```
src/
  main.c        the read-eval-print loop, startup and --version/--help
  lexer.c       splits a line into tokens, in place
  parser.c      tokens -> Pipeline of Commands, redirects, trailing &
  executor.c    fork/exec, pipes, redirection, process groups, command maps
  builtins.c    cd exit pwd jobs fg bg history alias
  jobs.c        the jobs table, reaping, fg/bg handover of the terminal
  readline.c    raw mode line editing and history
  prompt.c      the prompt: directory, git, exit status
  highlight.c   $PATH lookup and colouring of the line being typed
  dashboard.c   /proc readers and the startup box
  config.c      ~/.helixrc parser
include/
  helix.h       every shared type and prototype
```

## Roadmap

Quote-aware lexing, globbing, variable expansion, `&&` / `||` / `;`, tab completion.

## Notes and limits

- Finished background jobs are reaped once per prompt instead of in a `SIGCHLD`
  handler: almost nothing is safe to call from a signal handler, and the notice
  is only ever shown when the prompt is redrawn anyway.
- Quotes are highlighted but not yet honoured by the lexer, which splits on
  whitespace only. `echo "a b"` is two arguments.
- No globbing, no variable expansion, no `&&`/`||`/`;`.
- The line editor counts bytes, so a multi-byte character typed into the middle
  of a line moves the cursor by more than one column.
