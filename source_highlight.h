/*
 * Copyright (C) 2016 Richard Burke
 *
 * Based on lib/examples/infoformatter.h in source-highlight
 * by Lorenzo Bettini <http://www.lorenzobettini.it>, (C) 2009
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

#ifndef WED_SOURCE_HIGHLIGHT_H
#define WED_SOURCE_HIGHLIGHT_H

#include <srchilite/sourcehighlighter.h>
#include <srchilite/formattermanager.h>
#include <srchilite/formatterparams.h>

extern "C" {
#include "source_highlight_syntax.h"
}

namespace wed {

class Tokenizer;

/* An instance of this class is created for each source-highlight token.
 * These instances are passed to the source-highlight library, which invokes
 * the format function when a token of the specified type is matched. This
 * allows wed to determine the tokens and their positions within an
 * input string from source-highlight */
class TokenizerFormatter : public srchilite::Formatter {
    private:
        SyntaxToken token; /* The corresponding wed SyntaxToken */
        Tokenizer *tokenizer; /* Reference to main interface object */
        bool is_continuation_of_previous_token(int start);
    public:
        TokenizerFormatter(SyntaxToken token, Tokenizer *tokenizer);
        void format(const std::string &s,
                    const srchilite::FormatterParams *params);
};

typedef boost::shared_ptr<TokenizerFormatter> TokenizerFormatterPtr;

/* This class wraps around all objects used to interact with
 * source-highlight. An instance of this class is created for each filetype */
class Tokenizer {
    private:
        /* Processes each line of input and tokenizes it based on the
           lanuage definition it was created with */
        srchilite::SourceHighlighter *highlighter;
        /* Maps a source-highlight token to the relevant formater
           function */
        srchilite::FormatterManager *fmt_manager;
        /* Stores arguments that can be passed to formatter format function */
        srchilite::FormatterParams *fmt_params;
        /* Stores the tokens matched */
        SyntaxMatches *syn_matches;
        /* Tracks the offset into the input string that has been processed
           so far */
        size_t offset; 
    public:
        Tokenizer(srchilite::SourceHighlighter *highlighter,
                  srchilite::FormatterManager *fmt_manager,
                  srchilite::FormatterParams *fmt_params);
        ~Tokenizer();
        void tokenize(const std::string &str);
        SyntaxMatches *get_syn_matches() const;
        size_t get_offset() const;
};

}

#endif
