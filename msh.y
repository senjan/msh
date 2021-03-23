%{
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
%}

%{
#include <stdio.h>
#include <stdlib.h>

#include "main.h"

#define YYSTYPE char*

int yylex();
void yyerror(char *e);

%}

%token WORD
%token SEP
%token EOL
%token RDR_OUT
%token RDR_OUTA
%token RDR_IN
%token PIPE

%start cmdline

%%
cmdline: /* empty */ 
 | commands
 | commands SEP
;

commands: cmd
 | commands SEP cmd
;

cmd: one_cmd
 | cmd PIPE one_cmd { add_pipe(); }
;

one_cmd: bin args redirs
;

args: /* empty */
 | args arg
;

redirs: /* emptry */
 | redirs RDR_OUT out_file { }
 | redirs RDR_OUTA out_file_append { }
 | redirs RDR_IN in_file { }
; 

bin: WORD {add_command($1); }
arg: WORD {add_argument($1); }
in_file: WORD {add_redir($1, REDIR_INPUT); }
out_file: WORD {add_redir($1, REDIR_OUTPUT); }
out_file_append: WORD {add_redir($1, REDIR_APPEND); }

%%

void
yyerror(char *e)
{
	fprintf(stderr, "error: %s at char '%d'\n", e, yychar);
}

