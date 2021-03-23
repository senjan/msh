/*
 * msh - Micro Shell, https://github.com/senjan/msh
 * Copyright (C) 2021 Jan Senolt.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <assert.h>

#include "main.h"

/* pipe's ends */
#define	WRITE	0
#define	READ	1

#define	COMMENT_CHAR	'#'
#define	OLDPWD		"OLDPWD"

#define GENERIC_ERR	1
#define	SYNTAX_ERR	2
#define SYS_ERR		3

#define	FLG_PIPE	1
#define	FLG_APPEND	2

typedef struct arg {
	char *value;
	TAILQ_ENTRY(arg) entries;
} arg_t;

TAILQ_HEAD(cmdshead, cmd) cmds;
struct cmdshead *cmdsheadp;

typedef struct cmd {
	char *name;		/* exec file name */
	char *out_file;		/* output file name or NULL */
	char *in_file;		/* input file name or NULL */
	int argc;
	int flags;
	TAILQ_HEAD(, arg) args;
	TAILQ_ENTRY(cmd) entries;
	int pipe_fd[2];
} cmd_t;

char *prompt = "mysh$ ";

void
add_argument(char *arg_value) {
	arg_t *a = malloc(sizeof (arg_t));
	cmd_t *c = TAILQ_LAST(&cmds, cmdshead);
	assert(a != NULL);
	a->value = strdup(arg_value);
	c->argc++;
	TAILQ_INSERT_TAIL(&c->args, a, entries);
}

void
add_command(char *name)
{
	cmd_t *c = malloc(sizeof (cmd_t));
	assert(c != NULL);
	c->name = strdup(name);
	c->out_file = c->in_file = NULL;
	c->argc = 0;
	c->flags = 0;
	c->pipe_fd[WRITE] = c->pipe_fd[READ] = -1;
	TAILQ_INIT(&c->args);
	TAILQ_INSERT_TAIL(&cmds, c, entries);
	add_argument(name);	/* 1st argument is the program name */
}

void
destroy_command(cmd_t *c)
{
	arg_t *args;

	free(c->name);
	if (c->out_file != NULL)
		free(c->out_file);
	if (c->in_file != NULL)
		free(c->in_file);
	while ((args = TAILQ_FIRST(&c->args)) != NULL) {
		TAILQ_REMOVE(&c->args, args, entries);
		free(args);
	}
	assert(TAILQ_EMPTY(&c->args));
	free(c);
}

void
add_redir(char *fname, redir_t rd)
{
	cmd_t *c = TAILQ_LAST(&cmds, cmdshead);
	switch (rd) {
	case REDIR_INPUT:
		c->in_file = strdup(fname);
		break;
	case REDIR_APPEND:
		c->flags |= FLG_APPEND;
	case REDIR_OUTPUT:
		c->out_file = strdup(fname);
		break;
	}
}

void
add_pipe(void)
{
	cmd_t *c = TAILQ_LAST(&cmds, cmdshead);
	assert((c->flags & FLG_PIPE) == 0);
	/* Input redirection has a higher priority than pipe */
	if (c->in_file == NULL)
		c->flags |= FLG_PIPE;
}

/*
 * Moves program arguments from cmd.args to an array required by exec(2).
 */ 
char **
args2array(cmd_t *c, int *argc)
{
	char **args;
	arg_t *a;
	int idx = 0;

	/* 
	 * The list of args always contains at least the program name.
	 */
	assert(!TAILQ_EMPTY(&c->args));

	/* Add one more slot for NULL. */
	args = malloc(sizeof (char *) * (c->argc + 1));

	while ((a = TAILQ_FIRST(&c->args)) != NULL) {
		TAILQ_REMOVE(&c->args, a, entries);
		args[idx++] = a->value;
		free(a);
	}

	assert(TAILQ_EMPTY(&c->args));
	*argc = idx;
	assert(c->argc == idx);
	args[idx] = NULL;	/* args are terminated by NULL */
	c->argc = 0;		/* all args moved */

	return (args);
}

/*
 * Implement cd(1) command. This must be an internal command executed by the
 * parent as it's not possible for child to change parent's current dir.
 */
int
do_cd(int argc, char *argv[])
{
	char *new_dir;
	char *old_dir;
	char *oldpwd;
	int err;

	if (argc > 2) {
		(void) fprintf(stderr, "cd: too many arguments\n");
		return (SYNTAX_ERR);
	}

	if (argc == 1) {
		/* cd to $HOME */
		if ((new_dir = getenv("HOME")) == NULL) {
			(void) fprintf(stderr, "cd: HOME not set\n");
			return (GENERIC_ERR);
		}
	} else {
		if (strcmp(argv[1], "-") == 0) {
			/* cd to $OLDPWD */
			if ((new_dir = getenv(OLDPWD)) == NULL) {
				(void) fprintf(stderr, "cd: OLDPWD not set\n");
				return (GENERIC_ERR);
			}
			printf("%s\n", new_dir);
		} else {
			/* cd to argv[1] */
			new_dir = argv[1];
		}
	}

	if (new_dir == NULL) {
		(void) fprintf(stderr, "cd: cannot determine directory!\n");
		return (0);	/* Shells seem to treat this as not-error. */
	}

	old_dir = getcwd(NULL, 0);
	err = chdir(new_dir);
	if (err != 0) {
		perror(argv[0]);
		free(old_dir);
		return (err);
	}
	err = setenv(OLDPWD, old_dir, 1);
	assert(err == 0);
	free(oldpwd);

	return (0);
}

void
process_commands(int *rc)
{
	cmd_t *c;

	TAILQ_FOREACH(c, &cmds, entries) {
		cmd_t *next_cmd = TAILQ_NEXT(c, entries);
		int to_pipe = (next_cmd != NULL && next_cmd->flags & FLG_PIPE);
		int argc;
		char **argv = args2array(c, &argc);
		pid_t p;
		int err;

		/*
		 * We don't want to create a child process for the following
		 * two internal commands.
		 */
		if (strcmp(argv[0], "exit") == 0) {
			exit(*rc);
		}
	
		*rc = 0;
		if (strcmp(argv[0], "cd") == 0) {
			*rc = do_cd(argc, argv);
			continue;	/* We are done with this command. */
		}

		if (to_pipe) {
			/* Prepare the pipe to the next command. */
			err = pipe(c->pipe_fd);
			if (err != 0) {
				perror("pipe");
				exit(SYS_ERR);
			}
		}

		/* Fork and execute the command in sub-process. */
		p = fork();
		if (p == (pid_t)-1) {	/* Error */
			perror("fork");
			exit(SYS_ERR);
		} else if (p > 0) {	/* Parent */
			int status;

			/* Close the write end of the pipe. */
			if (c->pipe_fd[1] != -1)
				close(c->pipe_fd[WRITE]);

			/* Wait for the child to finish */
			for (;;) {
				err = waitpid(p, &status, 0);
				if (err == p)
					break;
				assert(errno == EINTR);
			}
		
			if (WIFEXITED(status)) {
				*rc = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				*rc = WTERMSIG(status);
				(void) fprintf(stderr, "Killed by signal %d.\n",
				    *rc);
			} else {
				(void) fprintf(stderr,
				    "Unknown child status: %d\n", status);
				abort();
			}
			continue;
		}

		/* Child */

		/* Close the read end of the pipe for this command. */
		close(c->pipe_fd[READ]);
	
		if (c->flags & FLG_PIPE) {
			cmd_t *pc = TAILQ_PREV(c, cmdshead, entries);
			/*
			 * We are in the pipeline. Redirect the previous
			 * command's output pipe to our stdin.
			 */
			assert(pc != NULL);
			assert(pc->pipe_fd[READ] != -1);
			err = dup2(pc->pipe_fd[READ], STDIN_FILENO);
			close(pc->pipe_fd[READ]);
			assert(err >= 0);
		}
		if (to_pipe) {
			/*
			 * Pipe the stdout to the pipe for the next command.
			 */
			err = dup2(c->pipe_fd[WRITE], STDOUT_FILENO);
			close(c->pipe_fd[WRITE]);
			assert(err >= 0);
		}

		/* stdout redirection */
		if (c->out_file != NULL) {
			int flags = (c->flags & FLG_APPEND) ? O_APPEND :
			    O_TRUNC;
			int fd;

			if ((fd = open(c->out_file, flags | O_WRONLY | O_CREAT,
			    0600)) < 0) {
				(void) fprintf(stderr, "cannot create %s: %s",
				    c->out_file, strerror(errno));
				break;
			}
			(void) dup2(fd, STDOUT_FILENO);
			(void) close(fd);
		}
		/* stdin redirection */
		if (c->in_file != NULL) {
			int fd = open(c->in_file, O_RDONLY);
			if (fd < 0) {
				(void) fprintf(stderr, "cannot open %s: %s",
				    c->in_file, strerror(errno));
				break;
			}
			(void) dup2(fd, STDIN_FILENO);
			(void) close(fd);	
		}

		err = execvp(argv[0], argv);
		if (err == -1) {
			perror(argv[0]);
			err = errno;
			if (errno == ENOENT)
				err = 127;
			/* TODO: bash returns 126 when file is not executable */
			exit(err);
		}
	}

	while ((c = TAILQ_FIRST(&cmds)) != NULL) {
		TAILQ_REMOVE(&cmds, c, entries);
		destroy_command(c);
	}
}

static void
int_handler(int status)
{
	putchar('\n');
	rl_on_new_line();
	rl_replace_line("", 0);
	rl_redisplay();
}

void
usage(const char *myname)
{
	(void) fprintf(stderr, "Usage %s [-c cmd] | cmd_file\n", myname);
       	exit(SYNTAX_ERR);
}

int
process_line(const char *line, int *rc)
{
	char *cmnt;
	int err;

	if (line == NULL)
		return (0);

	/* Remove eventual comment(s) from the command line. */
	if ((cmnt = strchr(line, COMMENT_CHAR)) != NULL) {
		char *new_line = strndup(line, cmnt - line);
		free((void *)line);
		line = new_line;
	}

	/* Empty line is a noop. */
	if (strlen(line) == 0) {
		return (0);
	}

	yy_scan_string(line);

	err = yyparse();
	if (err != 0) {
		*rc = SYNTAX_ERR;
		return (err);
	}

	process_commands(rc);

	return (0);
}

int
main(int argc, const char *argv[])
{
	struct sigaction act;
	int c_flag = 0;
	FILE *cmdfile = NULL;
	char *line = NULL;
	int rc = 0;

	if (argc > 3)
		usage(argv[0]);
	
	if (argc == 2) {
		if ((cmdfile = fopen(argv[1], "r")) == NULL) {
			perror(argv[1]);
			exit(1);
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "-c") == 0) {
			c_flag++;
		} else {
			usage(argv[0]);
		}
	}

	TAILQ_INIT(&cmds);

	/* We got line using -c option. */
	if (c_flag) {
		(void) process_line(argv[2], &rc);
		return (rc);
	}

	/* We are going to read commands from a cmd file. */
	if (cmdfile != NULL) {
		size_t len = 0;
		ssize_t cnt;
	
		while ((cnt = getline(&line, &len, cmdfile)) != -1) {
			int err;
			line[cnt - 1] = '\0'; /* Remove \n */
			err = process_line(line, &rc);
			free(line);
			line = NULL;
			/*
			 * Do not continue execting the scrip, rc contains
			 * the actual error code.
			 */
			if (err > 0)
				break;
		}
		fclose(cmdfile);
		return (rc);
	}

	/*
	 * We are running in the interactive mode, install the Ctrl-C handler.
	 */
	act.sa_handler = int_handler;
	act.sa_flags = 0;

	if ((sigemptyset(&act.sa_mask) == -1) ||
	    (sigaction(SIGINT, &act, NULL) == -1)) {
		perror("Cannot set SIGINT handler");
		exit(1);
	}

	while ((line = readline(prompt)) != NULL) {
		int err;

		add_history(line);
		/* Ignore (syntax) errors in interactive mode. */
		(void) process_line(line, &rc);
		free(line);
	}

	return (0);
}
