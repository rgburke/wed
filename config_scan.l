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

#define YY_DECL int yylex(Session *sess, const char *file_path)
#define YY_USER_ACTION cp_update_parser_location(yytext, file_path);

%}

%option yylineno
%option noinput
%option nounput
%option noyywrap

%%

#.*$
[[:space:]]+               
(-|\+)?[[:digit:]]+           { yylval.string = strdupe(yytext); return TKN_INTEGER; }
true|false                    { yylval.string = strdupe(yytext); return TKN_BOOLEAN; }
\"(\\.|[^"])*\"               { yylval.string = strdupe(yytext); return TKN_STRING;  }
\/(\\\/|[^\/])*\/[[:lower:]]* { yylval.string = strdupe(yytext); return TKN_REGEX;   }
[[:alpha:]_]+                 { yylval.string = strdupe(yytext); return TKN_NAME;    }
"="                           { return TKN_ASSIGN;        }
";"                           { return TKN_SEMI_COLON;    }
"{"                           { return TKN_LEFT_BRACKET;  }
"}"                           { return TKN_RIGHT_BRACKET; }
.                             { se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_CHARACTERS, 
                                                                       &yylloc, "Invalid character \"%s\"", 
                                                                       yytext)); }

%%

void cp_start_scan_file(FILE *file)
{
    yypush_buffer_state(yy_create_buffer(file, YY_BUF_SIZE));   
}

void cp_start_scan_string(const char *str)
{
    yypush_buffer_state(yy_scan_string(str));
}

void cp_finish_scan(void)
{
    yypop_buffer_state();
}

