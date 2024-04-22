# PROGRESS

Tracking the build, one chapter at a time.

## Module 1 - Lexer + Parser

- [x] ch4: preparing our project (skeleton: compiler, cprocess, helpers, main)
- [x] ch6: creating our token structures (struct pos, TOKEN_TYPE_* enum, struct token)
- [x] ch7: preparing our lexer (lex_process + v-table, FILE*-backed adapters, empty lex())
- [x] ch8: creating a number token (NUMERIC_CASE, compiler_error/warning, read_next_token loop)
- [x] ch9: creating a string token (token_make_string with delim, naive '\\' skip)
- [x] ch10: creating an operator token (OPERATOR_CASE, greedy read_op, op_valid whitelist, paren counter, S_EQ macro, token.c)
- [x] ch11: creating a symbol token (SYMBOL_CASE, ')' drops paren counter via lex_finish_expression)
- [x] ch12: creating an identifier token (default-case fallback to read_special_token)
- [x] ch13: creating a keyword token (is_keyword whitelist, promotes IDENTIFIER -> KEYWORD)
- [x] ch14: creating a new-line token ('\n' becomes TOKEN_TYPE_NEWLINE, distinct from whitespace)
- [x] ch16: handling quotes + comments in the lexer (//, /* */, 'X' char literals, escapes)
- [x] ch17: implementing hexadecimal numbers (0xNN via lexer_pop_token + read_hex_number_str)
- [x] ch18: implementing binary numbers (0bNN; guard so bare 'b'/'x' falls back to identifier)
- [x] ch19: dealing with the parentheses buffer (in-paren chars accumulated, stamped onto tokens via between_brackets)
- [x] ch20: creating tokens outside of the input file (tokens_build_for_string, string-backed v-table)
- [x] ch21: creating number types (NUMBER_TYPE_NORMAL/LONG/FLOAT/DOUBLE, L/f suffix in num.type)
- [x] ch22: finalizing the lexer (compile_process.token_vec gets the lex output, parser-ready)

## Module 1 - Parser

- [x] ch24: creating our parser structures (NODE_TYPE_* enum, struct node + binded)
- [x] ch25: writing our parser template (parser.c skeleton, parse() stub, node_vec/node_tree_vec)
- [x] ch26: creating our node file (node.c with set_vector/push/peek/pop, root-aware pop)
- [x] ch27: creating our first node (parse() NUMBER/IDENTIFIER/STRING -> AST, token_next/peek, node_create)
- [x] ch28: creating an expression node (NODE_TYPE_EXPRESSION, history threading, make_exp_node, parse_exp_normal)
- [x] ch29: precedence in expressions part 1 (expressionable.c: op_precedence table, 14 groups, associativity)
- [ ] ch30..ch102: rest of Module 1 parser
- [ ] ...

## Module 2 + 3 - Code generator + resolver

- [ ] ch104: the code generator
- [ ] ...

## Module 4 - Preprocessor + expressionable system

- [ ] ch187: ...

## Module 5 - Semantic validator

- [ ] ch242: ...
