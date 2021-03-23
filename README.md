# msh

`msh` stands for Micro (or My) Shell, it's a very simple UNIX Shell inspired by [Shell Command Language](https://pubs.opengroup.org/onlinepubs/009604499/utilities/xcu_chap02.html).

It's just a toy I wrote some time ago to refresh my `lex(1)` and `yacc(1)` knowledge and exercise some basic UNIX System programing skills. Thus it does not do anything useful or interesting. Apart from running executables it supports basic standard input (`<`) and output (`>`, `>>`) redirections and pipes (`|`). Only two internal commands are currently supported: `exit(1)` and `cd(1)`. It can execute "scripts" (i.e. read commands from a file) or get commands from the commands line, using -c option. 

### Build
I originally wrote msh on Solaris 10, now it builds cleanly on Solaris 11.4 and (given it's simpicity) I suppose it will build on any UNIX-like(-ish) system.

### TODO
  * add support for running processes in the background (`&`, `fg`, and `bg`) and simple job control
  * add variables



