// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */

static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	int ret1 = chdir(get_word(dir));
	return ret1;
}
/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	exit(SHELL_EXIT);
	return SHELL_EXIT;/* TODO: Replace with actual exit code. */
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	/* TODO: If builtin command, execute the command. */
	if (strcmp(get_word(s->verb), "exit") == 0 || strcmp(get_word(s->verb), "quit") == 0) {
		return shell_exit();
	}

	if (strcmp(get_word(s->verb), "cd") == 0) {
		if (s->out) // Output redirection
        {
			int out = dup(STDOUT_FILENO);
			close(STDOUT_FILENO);
			int outfd;
			if(s->io_flags == IO_OUT_APPEND) {
				outfd = open(get_word(s->out), O_WRONLY | O_CREAT |  O_APPEND, 0666);
			} else {
				outfd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			}
			dup2(outfd, STDOUT_FILENO);
			close(outfd);
			dup2(out, STDOUT_FILENO);
        }
		return shell_cd(s->params);
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	
	if ((s->verb->next_part != NULL && (get_word(s->verb->next_part) != NULL) 
									&& (*(get_word(s->verb->next_part)) == '='))) {
		char *elem = strtok(get_word(s->verb), "=");
		char *var = strdup(elem);
		elem = strtok (NULL, "=");
		char *value = strdup(elem);
		setenv(var, value, 1);
		return 0;
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	// Fork new process
	pid_t pid = fork();
	DIE(pid < 0, "failed fork");

	// Perform redirections in child
    if (pid == 0)
    {
		// Input redirection
        if (s->in) 
        {
            int infd = open(get_word(s->in), O_RDONLY);
			dup2(infd, STDIN_FILENO);
			close(infd);
        }
		// Output redirection
        if (s->out) 
        {
			int outfd;
			if(s->io_flags == IO_OUT_APPEND) {
				outfd = open(get_word(s->out), O_WRONLY | O_CREAT |  O_APPEND, 0666);
			} else {
				outfd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			}
			dup2(outfd, STDOUT_FILENO);
			close(outfd);

			// Error redirection
			if(s->err) {
				if(strcmp(get_word(s->out), get_word(s->err)) == 0)
					dup2(STDOUT_FILENO, STDERR_FILENO);
				else {
					int errfd;
					if (s->io_flags == IO_REGULAR) {
						errfd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);
					} else {
						errfd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
					}
					
					dup2(errfd, STDERR_FILENO);
					close(errfd);
				}
			}
        }
		// Error redirection
		if (s->err && !s->out) 
		{ 
			int errfd;
			if (s->io_flags == IO_REGULAR) {
				errfd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			} else {
				errfd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
			}

			dup2(errfd, STDERR_FILENO);
			close(errfd);
		}

		int argc;
		char** argvs =  get_argv(s, &argc);
		// Load executable in child
		execvp(get_word(s->verb), argvs);
		printf("Execution failed for '%s'\n", get_word(s->verb));
		exit(1);
    }

	// Wait for child
    int status_par;
    wait(&status_par);
    return status_par; /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	int status1, status2;
	int ret;
	pid_t pid1 = fork();
	DIE(pid1 < 0, "failed fork");

	if(pid1 == 0) {
		ret = parse_command(cmd1, level, father);
		exit(ret);
	}

	pid_t pid2 = fork();
	DIE(pid2 < 0, "failed fork");
	if(pid2 == 0) {
		ret = parse_command(cmd2, level, father);
		exit(ret);
	} 
	
	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);	
	return status2 && status1; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int status1, status2, ret;
	int pipfd[2];
	pipe(pipfd);
	pid_t pid1 = fork();
	DIE(pid1 < 0, "failed fork");

	if(pid1  == 0) {
		// closes read head of pipe
		ret = close(pipfd[0]);
		// redirects output
		ret = dup2(pipfd[1], STDOUT_FILENO);
		ret = parse_command(cmd1, level, father);
		exit(ret);
	}
	ret = close(pipfd[1]);
	pid_t pid2 = fork();
	DIE(pid2 < 0, "failed fork");
	if(pid2  == 0) {
		// closes write head of pipe
		ret = close(pipfd[1]);
		// redirects input
		ret = dup2(pipfd[0], STDIN_FILENO);
		ret = parse_command(cmd2, level, father);
		exit(ret);
	}
	ret = close(pipfd[0]);
	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);
	return status2;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return parse_simple(c->scmd, level + 1, father); /* TODO: Replace with actual exit code of command. */
	}

	int ret;
	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		return parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level + 1, father);
		break;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		if (ret)
			return parse_command(c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		if (ret == 0)
			return parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, father);
		break;

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
