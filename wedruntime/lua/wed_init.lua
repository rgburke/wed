-- Initialise wed global variable. Data and functions defined within this
-- object are used by the wed editor to interface with Scintillua
wed = { }

-- Load Scintillua lexer module
wed.lexer_loader = require('lexer')

-- Map of wed filetypes to Scintillua syntax types
wed.lexer_name_map = {
    c = 'ansi_c',
    sh = 'bash'
}

-- Map a wed filetype to a Scintillua syntax type using the lexer map above
wed.determine_lexer_name = function(wed, lexer_name)
    local mapped_name = wed.lexer_name_map[lexer_name] 

    if mapped_name ~= nil then
        return mapped_name
    end

    return lexer_name
end

-- The two functions below are invoked from wed when performing
-- syntax highlighting

-- Attempt to load a language lexer
wed.load_lexer = function(wed, lexer_name)
    if lexer_name == nil or lexer_name == '' then
        error('Invalid lexer name')
    end

    lexer_name = wed:determine_lexer_name(lexer_name)

    -- This functions will throw an error if a lexer with this name cannot
    -- be loaded
    wed.lexer_loader.load(lexer_name)
end

-- Tokenize "str" using the lexer "lexer_name". Return the array of tokens
-- making them available to wed on its lua stack
wed.tokenize = function(wed, lexer_name, str)
    if lexer_name == nil or lexer_name == '' then
        error('Invalid lexer name')
    elseif str == nil then
        error('Cannot lex NULL string')
    end

    lexer_name = wed:determine_lexer_name(lexer_name)

    -- The lexer module caches loaded lexers so we can simply call load again
    -- to retrieve a lexer without any overhead
    local lexer = wed.lexer_loader.load(lexer_name) 

    if lexer == nil then
        error('No lexer loaded for language ' .. lexer_name)
    end

    local tokens = lexer:lex(str)

    if lexer._tokenstyles ~= nil then
        -- A lexer has defined tokens with custom names. Therefore use the
        -- _tokenstyles table to map these custom names to styles from which
        -- wed can infer the standardised token names
        local style_token
        local i

        for i = 1, #tokens, 2 do
            style_token = lexer._tokenstyles[tokens[i]]

            if style_token ~= nil then
                tokens[i] = style_token
            end
        end
    end

    return tokens
end

