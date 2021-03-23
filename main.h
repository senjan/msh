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

#include <sys/types.h>

/* I/O redirections */
typedef enum {
    REDIR_INPUT,	/* < */
    REDIR_OUTPUT,	/* > */
    REDIR_APPEND	/* >> */
} redir_t; 

void add_pipe(void);
void add_redir(char *fname, redir_t rd);
void add_command(char *name);
void add_argument(char *arg_value);

/* Some prototypes for yacc/bison */
void *yy_scan_string(const char*);
int yyparse();
