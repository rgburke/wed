/*
 * Copyright (C) 2015 Richard Burke
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

%{
#include <stdio.h>
#include "config_parse_util.h"
#include "config_parse.h"
#include "util.h"

static int yycolumn = 1;

#define YY_DECL int yylex(Session *sess, const char *file_path)
#define YY_USER_ACTION update_parser_location(&yycolumn, yylineno, yyleng);

%}

%option yylineno
%option noinput
%option nounput
%option noyywrap

%%

#.*$
[\n]            { yycolumn = 1; }
[[:space:]]+
-?[[:digit:]]+  { yylval.string = strdupe(yytext); return TKN_INTEGER; }
true|false      { yylval.string = strdupe(yytext); return TKN_BOOLEAN; }
\"(\\.|[^"])*\" { yylval.string = strdupe(yytext); return TKN_STRING;  }
[[:alpha:]_]+   { yylval.string = strdupe(yytext); return TKN_NAME;    }
"="             { return TKN_ASSIGN;     }
";"             { return TKN_SEMI_COLON; }
.               { add_error(sess, get_config_error(ERR_INVALID_CONFIG_CHARACTERS, file_path, "Invalid character \"%s\"", yytext)); }

%%

