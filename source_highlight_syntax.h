/*
 * Copyright (C) 2016 Richard Burke
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

#ifndef WED_SOURCE_HIGHLIGHT_SYNTAX_H
#define WED_SOURCE_HIGHLIGHT_SYNTAX_H

#include "syntax.h"
#include "session.h"

/* GNU Source Highlight syntax definition */
typedef struct {
    SyntaxDefinition syn_def; /* Interface */
    Session *sess; /* Session reference used to determine the source highlight
                      data dir if set */
    void *tokenizer; /* Instance of a wed::Tokenizer class */
} SHSyntaxDefinition;

SyntaxDefinition *sh_new(Session *);

#endif

