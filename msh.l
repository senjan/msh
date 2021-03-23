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
	enum yytokentype {
		WORD = 258,
		SEP = 259,
		EOL = 260,
		RDR_OUT = 261,
		RDR_OUTA = 262,
		RDR_IN = 263,
		PIPE = 264
	};

	extern char * yylval;
%}

%%
[0-9a-zA-z\-\/\.\$]+	{ yylval = strdup(yytext); return WORD; }
[ \t]*;[ \t]*		{ return SEP; }
\n			{ return EOL; }
[ \t]			{ /* Discard white chars */}	
\>			{ return RDR_OUT; }
\<			{ return RDR_IN; }
\>\>			{ return RDR_OUTA; }
\|			{ return PIPE; }

.	{ printf("Wrong char '%c'\n", *yytext); }
%%

