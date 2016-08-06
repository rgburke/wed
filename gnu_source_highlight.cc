/*
 * Copyright (C) 2016 Richard Burke
 *
 * Based on lib/examples/infoformatter-main.cpp in source-highlight
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

#include <string>
#include <cstring>
#include <fstream>
#include <streambuf>
#include <srchilite/langdefmanager.h>
#include <srchilite/regexrulefactory.h>
#include <srchilite/parserexception.h>
#include <srchilite/settings.h>
#include "gnu_source_highlight.h"

namespace wed {

static Tokenizer *construct_tokenizer(const std::string &lang_dir,
                                      const std::string &lang_name);

Tokenizer::Tokenizer(srchilite::SourceHighlighter *highlighter,
                     srchilite::FormatterManager *fmt_manager,
                     srchilite::FormatterParams *fmt_params) :
                     highlighter(highlighter), fmt_manager(fmt_manager),
                     fmt_params(fmt_params), syn_matches(NULL)
{
}

Tokenizer::~Tokenizer()
{
    delete this->fmt_params;
    delete this->highlighter;
    delete this->fmt_manager;
}

/* Tokenizes an input string */
void Tokenizer::tokenize(const std::string &str)
{
    this->syn_matches = sy_new_matches(0);

    if (this->syn_matches == NULL) {
        return;
    }

    std::memset(this->syn_matches, 0, sizeof(SyntaxMatches));
    this->offset = 0;
    this->highlighter->clearStateStack();
    this->highlighter->setCurrentState(this->highlighter->getMainState());

    std::istringstream input_stream(str);
    std::string line;

    while (std::getline(input_stream, line)) {
        /* highlightParagraph actually only highlights a single line */
        this->highlighter->highlightParagraph(line);
        this->offset += line.length() + 1;
    }
}

SyntaxMatches *Tokenizer::get_syn_matches() const
{
    return this->syn_matches;
}

size_t Tokenizer::get_offset() const
{
    return this->offset;
}

TokenizerFormatter::TokenizerFormatter(SyntaxToken token,
                                       Tokenizer *tokenizer) :
                                       token(token), tokenizer(tokenizer)
{
}

void TokenizerFormatter::format(const std::string &s,
                                const srchilite::FormatterParams *params = NULL
                                ) {
    if (this->token != ST_NORMAL && s.size() > 0) {
        SyntaxMatches *syn_matches = this->tokenizer->get_syn_matches();

        if (is_continuation_of_previous_token(params->start)) {
            SyntaxMatch *prev_match = &syn_matches->matches[
                                          syn_matches->match_num - 1
                                      ];

            prev_match->length += s.size();
        } else if (syn_matches->match_num < MAX_SYNTAX_MATCH_NUM) {
            SyntaxMatch *syn_match = &syn_matches->matches[
                                         syn_matches->match_num++
                                     ];
            syn_match->offset = this->tokenizer->get_offset() + params->start;
            syn_match->length = s.size();
            syn_match->token = this->token;
        }
    }
}

bool TokenizerFormatter::is_continuation_of_previous_token(int start)
{
    SyntaxMatches *syn_matches = this->tokenizer->get_syn_matches();

    if (syn_matches->match_num == 0) {
        return false;
    }

    size_t offset = this->tokenizer->get_offset() + start;
    const SyntaxMatch *prev_match = &syn_matches->matches[
                                        syn_matches->match_num - 1
                                    ];

    return prev_match->token == this->token &&
           prev_match->offset + 1 == offset;
}

static Tokenizer *construct_tokenizer(const std::string &lang_dir,
                                      const std::string &lang_name)
{
    srchilite::RegexRuleFactory rule_factory;
    srchilite::LangDefManager lang_def_manager(&rule_factory);

    srchilite::SourceHighlighter *highlighter = 
        new srchilite::SourceHighlighter(
            lang_def_manager.getHighlightState(lang_dir, lang_name)
        );

    srchilite::FormatterParams *fmt_params = new srchilite::FormatterParams();
    fmt_params->start = 0;
    highlighter->setFormatterParams(fmt_params);

    srchilite::FormatterManager *fmt_manager =
        new srchilite::FormatterManager(
            TokenizerFormatterPtr(new TokenizerFormatter(ST_NORMAL, NULL))
        );
    highlighter->setFormatterManager(fmt_manager);

    Tokenizer *tokenizer = new Tokenizer(highlighter, fmt_manager, fmt_params);

    /* Map source-highlight tokens to wed tokens */
    std::map<std::string, SyntaxToken> element_token;
    element_token["classname"]   = ST_TYPE;
    element_token["comment"]     = ST_COMMENT;
    element_token["function"]    = ST_IDENTIFIER;
    element_token["keyword"]     = ST_STATEMENT;
    element_token["label"]       = ST_STATEMENT;
    element_token["number"]      = ST_CONSTANT;
    element_token["preproc"]     = ST_SPECIAL;
    element_token["specialchar"] = ST_SPECIAL;
    element_token["string"]      = ST_CONSTANT;
    element_token["todo"]        = ST_TODO;
    element_token["type"]        = ST_TYPE;
    element_token["url"]         = ST_SPECIAL;
    element_token["usertype"]    = ST_TYPE;
    element_token["regexp"]      = ST_CONSTANT;
    element_token["variable"]    = ST_IDENTIFIER;
    element_token["property"]    = ST_TYPE;
    element_token["value"]       = ST_CONSTANT;
    element_token["selector"]    = ST_IDENTIFIER;
    /* TODO Need new syntax types for these? */
    element_token["difflines"]   = ST_COMMENT;
    element_token["newfile"]     = ST_TYPE;
    element_token["oldfile"]     = ST_CONSTANT;

    std::map<std::string, SyntaxToken>::iterator iter;
    TokenizerFormatter *tokenizer_formatter;

    for (iter = element_token.begin(); iter != element_token.end(); iter++) {
        tokenizer_formatter = new TokenizerFormatter(iter->second, tokenizer);
        fmt_manager->addFormatter(iter->first,
                                  TokenizerFormatterPtr(tokenizer_formatter));
    }

    return tokenizer;
}

}

/* The functions below are the C interface exposed to the rest of wed */

extern "C" {

Status sh_init(SHSyntaxDefinition *sh_def, const char *lang_dir,
               const char *lang_name)
{
    std::string dir(lang_dir);
    std::string lang(lang_name);
    Status status = { ERR_NONE, NULL, 0 };

    if (dir.empty()) {
        dir = srchilite::Settings::retrieveDataDir();
    }

    try {
        sh_def->tokenizer = wed::construct_tokenizer(dir, lang + ".lang");
    } catch (srchilite::ParserException e) {
        std::stringstream ss;
        ss << e;

        std::string err_msg = "Unable to load source-highlight definition \"" +
                              lang + "\" - " + ss.str();

        status = st_get_error(ERR_INVALID_SYNTAXTYPE, err_msg.c_str());
    }

    return status;
}

SyntaxMatches *sh_tokenize(const SHSyntaxDefinition *sh_def,
                           const char *str, size_t str_len)
{
    wed::Tokenizer *tokenizer = (wed::Tokenizer *)sh_def->tokenizer;
    std::string input(str, str_len);
    tokenizer->tokenize(input);
    return tokenizer->get_syn_matches();
}

void sh_free_tokenizer(SHSyntaxDefinition *sh_def)
{
    wed::Tokenizer *tokenizer = (wed::Tokenizer *)sh_def->tokenizer;
    delete tokenizer;
}

}
