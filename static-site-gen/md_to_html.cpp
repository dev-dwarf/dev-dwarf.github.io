#include "md_to_html.h"

/* Take list of Text nodes with undecided type, parse and return typed list */
Text* parse_text(Arena* arena, Block* block) {
    Text *text = block->text;
    Text *curr = text;
    Text pre_filler = {curr, Text::NIL, 0};
    Text *pre = &pre_filler;

    b32 ignore_next = false;
    b32 inside = 0;
    Text::Types paren_stack[32];
    u32 paren_stacki = 0;

#define PUSH_TEXT(TYPE, END, SKIP) {                                    \
        if (END == 0 &&                                                 \
            curr->type == Text::TEXT) {                                 \
            curr->type = TYPE;                                          \
            curr->text = str8_skip(curr->text, (SKIP));                 \
            curr = pre; /* overwrite current node */                    \
        } else {                                                        \
            Text* temp = curr->next;                                    \
            curr->next = arena->take_struct_zero<Text>();               \
            curr->next->type = TYPE;                                    \
            curr->next->text = str8_skip(curr->text, (END) + (SKIP));   \
            curr->next->next = temp;                                    \
            curr->text = str8_first(curr->text, (END));                 \
        }                                                               \
        if (TEST_FLAG(inside, FLAG(TYPE))) {                            \
            curr->next->end = true;                                     \
        }                                                               \
        TOGGLE_FLAG(inside, FLAG(TYPE));                                \
        REM_FLAG(inside, FLAG(Text::TEXT));                             \
        REM_FLAG(inside, FLAG(Text::BREAK));                            \
    }

    for (; curr->next != 0; pre = curr, curr = curr->next) {
        str8 s = curr->text;
        if ((curr->type == Text::LIST_ITEM)
            || (curr->type == Text::CODE_BLOCK)
            || (curr->type == Text::BREAK)) {
            continue;
        }
        if (curr->type == Text::NIL) {
            curr->type = Text::TEXT;
        }
        if (s.len == 0) {
            if (curr->type == Text::TEXT) {
                curr->type = Text::BREAK;
            } else if (curr->type != Text::TABLE_CELL) {
                PUSH_TEXT(Text::BREAK, 0, 1);
            }
            continue;
        }
        chr8 c[3]; 
        c[1] = s.str[0];
        c[2] = (s.len > 1)? s.str[1] : 0;
        str8_iter_custom(s, i, _unused) {
            c[0] = c[1];
            c[1] = c[2];
            c[2] = ((s64) s.len > i+2)? s.str[i+2] : 0;
            
            if (ignore_next) {
                PUSH_TEXT(Text::TEXT, i-1, 1);
                ignore_next = false;
            } else if (c[0] == '`') {
                PUSH_TEXT(Text::CODE_INLINE, i, 1);
                break;
            } else if (curr->type == Text::CODE_INLINE && !curr->end) {
                /* Do nothing, do not parse stuff inside code */
            } else if (c[0] == '|') {
                PUSH_TEXT(Text::TABLE_CELL, i, 1);
                break;
            } else if (c[0] == '|' && c[1] == ' ') {
                PUSH_TEXT(Text::TABLE_CELL, i, 2);
                break;
            } else if (c[0] == '*' && c[1] == '*') {
                PUSH_TEXT(Text::BOLD, i, 2);
                break;
            } else if (c[0] == '*') {
                PUSH_TEXT(Text::ITALIC, i, 1);
                break;
            } else if (c[0] == '~' && c[1] == '~') {
                PUSH_TEXT(Text::STRUCK, i, 1);
                break;
            } else if (c[0] == '@' && c[1] == '(') {
                PUSH_TEXT(Text::LINK, i, 2);
                curr = curr->next;
                s64 sentinel = str8_char_location(curr->text, ' ');
                ASSERT(sentinel != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, sentinel, 1);
                paren_stack[paren_stacki++] = Text::LINK;
                break;
            } else if (c[0] == '!' && c[1] == '(') {
                PUSH_TEXT(Text::IMAGE, i, 2);
                curr = curr->next;
                s64 sentinel = str8_char_location(curr->text, ')');
                ASSERT(sentinel != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, sentinel, 1);
                break;
            } else if (c[0] == '?' && c[1] == '(') {
                PUSH_TEXT(Text::EXPLAIN, i, 2);
                curr = curr->next;
                s64 sentinel = str8_char_location(curr->text, ',');
                ASSERT(sentinel != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, sentinel, 2);
                paren_stack[paren_stacki++] = Text::EXPLAIN;
                break;
            } else if (c[0] == ')') {
                if (paren_stacki > 0) {
                    Text::Types t = paren_stack[--paren_stacki];
                    PUSH_TEXT(t, i, 1);
                }
                break;
            } else if (c[0] == '\\') {
                ignore_next = true;
            }
        } /* end str8_iter */
        ASSERTM(pre == &pre_filler || pre->type != Text::NIL, "Must not leave NIL nodes in Text linked-list!");
    }

    return text;
}

/* Take str8, return tree of blocks representing markdown structure */
Block* parse(Arena *arena, str8 str) {
    Block* root = arena->take_struct_zero<Block>();
    Block* curr = root;
    Block next = {0};
    next.text = arena->take_struct_zero<Text>();
    Text* end = next.text;
        
    u64 next_len = 0;

#define PUSH_STR(str) {                                     \
        end->text = str;                                    \
        end->next = arena->take_struct_zero<Text>();         \
        end = end->next;                                    \
}

#define PUSH_BLOCK() if (next.type != Block::NIL) {     \
    *curr = next;                                       \
    curr->next =  arena->take_struct_zero<Block>();      \
    curr = curr->next;                                  \
    next = {0};                                         \
    next.text = arena->take_struct_zero<Text>();         \
    end = next.text;                                    \
}

#define BREAK_BLOCK_IF_NOT(TYPE) {                      \
    if (next.type != TYPE) {                            \
        PUSH_BLOCK();                                   \
    } \
}
    
    str8_iter_pop_line(str) {
        /* Remove windows newline encoding (\r\n) */
        line = str8_trim_suffix(line, str8("\r"));
        if (line.len == 0) {
            PUSH_STR(line);
            if (!(next.type == Block::CODE || next.type == Block::PARAGRAPH)) {
                PUSH_BLOCK();
            }
            continue;
        }
        chr8 c[3];
        c[0] = line.str[0];
        c[1] = (line.len > 1)? line.str[1] : 0;
        c[2] = (line.len > 2)? line.str[2] : 0;
        if (c[0] == '`' && c[1] == '`' && c[2] == '`') {
            if (next.type != Block::CODE) {
                PUSH_BLOCK();
                next.type = Block::CODE;
                next.id = str8_skip(line, 3);
            } else {
                PUSH_BLOCK();                
            }
        } else if (next.type == Block::CODE) {
            end->type = Text::CODE_BLOCK;
            PUSH_STR(line);
        } else if (c[0] == '#') {
            PUSH_BLOCK();
            u32 n = 1;
            for (; n < line.len && line.str[n] == '#'; n++)
                ;
            next.type = Block::HEADING;
            next.num = n;
            str8 rem = str8_skip(line, n);
            next.id = rem;
            next.id.len = str8_char_location(rem, ' ');
            
            PUSH_STR(str8_skip(rem, next.id.len+1));
            PUSH_BLOCK();
            
        } else if (c[0] == '!' && c[1] == '|') {
            PUSH_BLOCK();
            next.type = Block::TABLE_ROW;
            PUSH_STR(str8_skip(line, 1));
        } else if (c[0] == '>' && c[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::QUOTE);
            next.type = Block::QUOTE;
            PUSH_STR(str8_skip(line, 2));
        } else if (c[0] == '[' && c[1] == ' ')  {
            if (next.type != Block::EXPAND) {
                PUSH_BLOCK();
                next.type = Block::EXPAND;
                str8 rem = str8_skip(line, 2);
                u32 n = 0;
                for (; n < rem.len && rem.str[n] == '#'; n++)
                    ;
                rem = str8_skip(rem, n);
                if (n > 0) {
                    next.id = rem;
                    next.id.len = str8_char_location(rem, ' ');
                    rem = str8_skip(rem, next.id.len+1);
                }
                next.title = rem;
                next.num = n;
            } else {
                PUSH_STR(str8_skip(line, 2));
            }
        } else if (c[0] >= '1' && c[0] <= '9' && c[1] == '.' && c[2] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::ORD_LIST);
            next.type = Block::ORD_LIST;
            end->type = Text::LIST_ITEM;
            end->next = arena->take_struct_zero<Text>();
            end = end->next;
            PUSH_STR(str8_skip(line, 2));
            end->type = Text::LIST_ITEM;
            end->next = arena->take_struct_zero<Text>();
            end->end = true;
            end = end->next;
        } else if ((c[0] == '*' || c[0] == '-') && c[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::UN_LIST);
            next.type = Block::UN_LIST;
            end->type = Text::LIST_ITEM;
            end->next = arena->take_struct_zero<Text>();
            end = end->next;
            PUSH_STR(str8_skip(line, 2));
            end->type = Text::LIST_ITEM;
            end->next = arena->take_struct_zero<Text>();
            end->end = true;
            end = end->next;
        } else if (c[0] == '@' && c[1] == '{') {
            PUSH_BLOCK();
            line = str8_skip(line, 2);
            next.type = Block::SPECIAL;
            next.id = str8_pop_at_first_delimiter(&line, str8(",}"));
            str8_iter_pop_delimiter(line, str8(",}")) {
                /* NOTE(lcf): optionally allow space in args list */
                if (str8_char_location(sub, ' ') == 0) {
                    sub = str8_skip(sub, 1);
                }
                Str8List_add(arena, &next.content, sub);
            }
            PUSH_BLOCK();
        } else if (c[0] == '-' && c[1] == '-' && c[2] == '-') {
            PUSH_BLOCK();
            next.type = Block::RULE;
            PUSH_BLOCK();
        } else {
            if (next.type != Block::PARAGRAPH && next.type != Block::CODE) {
                PUSH_BLOCK();
                next.type = Block::PARAGRAPH;
            }
            PUSH_STR(line);
        }
    }
    PUSH_BLOCK();

    for (curr = root; curr->type != Block::NIL; curr = curr->next) {
        curr->text = parse_text(arena, curr);
    }
    
    return root;
}

/* render linked list of Text* to html tags */
Str8List render_text(Arena* arena, Text* root) {
    Str8List out = {0};
    Text prev_filler = {root, Text::NIL, 0};
    for (Text* t = root, *prev = &prev_filler; t->type != Text::NIL; prev = t, t = t->next) {
        switch (t->type) {
        case Text::BOLD: {
            str8 s[2] = {str8("<b>"), str8("</b>")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::ITALIC: {
            str8 s[2] = {str8("<em>"), str8("</em>")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::STRUCK: {
            str8 s[2] = {str8("<s>"), str8("</s>")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::CODE_INLINE: {
            str8 s[2] = {str8("<code>"), str8("</code>")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::CODE_BLOCK: {
            out.add(arena, t->text);
            out.add(arena, str8_NEWLINE);
        } break;
        case Text::LINK: {
            if (!t->end) {
                out.add(arena, str8("<a href='"));
                out.add(arena, t->text);
                out.add(arena, str8("'>"));
            } else {
                out.add(arena, str8("</a>"));
                out.add(arena, t->text);
            }
        } break;
        case Text::EXPLAIN: {
            if (!t->end) {
                out.add(arena, str8("<abbr title='"));
                out.add(arena, t->text);
                out.add(arena, str8("'>"));
            } else {
                out.add(arena, str8("</abbr>"));
                out.add(arena, t->text);
            }
        } break;
        case Text::IMAGE: {
            /* TODO alt text, styles, etc */
            out.add(arena, str8("<img src='"));
            out.add(arena, t->text);
            out.add(arena, str8("'>"));
        } break;
        case Text::TABLE_CELL: {
            str8 s[2] = {str8("<td>"), str8("</td>\n")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::BREAK: {
            out.add(arena, str8("<br>"));
            out.add(arena, t->text);
        } break;
        case Text::LIST_ITEM: {
            str8 s[2] = {str8("<li>"), str8("</li>\n")};
            out.add(arena, s[t->end]);
            out.add(arena, t->text);
        } break;
        case Text::TEXT: {
            out.add(arena, t->text);
            if (prev->type == Text::TEXT) {
                out.add(arena, str8_NEWLINE);
            }
        } break;
        default:
            ASSERTM(0,"Forgot to add a case in render_text!");
        }
    }
    return out;
}

/* take fully parsed markdown blocks and render as html tags */
Str8List render_block(Arena* arena, Block* block) {
    Str8List out = {0};
    switch (block->type) {
    case Block::HEADING: {
        str8 h = str8_create_size(arena, 2);
        h.str[0] = 'h';
        h.str[1] = '0' + (u8) block->num;
        out.add(arena, str8("<"));
        out.add(arena, h);
        if (block->id.len > 0) {
            static const str8 center = str8("center");
            static const str8 right = str8("right");
            if (str8_has_prefix(block->id, center)) {
                block->id = str8_skip(block->id, center.len+1);
                out.add(arena, str8(" style='text-align:center'"));
            } else if (str8_eq(block->id, right)) {
                block->id = str8_skip(block->id, right.len+1);
                out.add(arena, str8(" style='text-align:right'"));
            }
            out.add(arena, str8(" id='"));
            out.add(arena, block->id);
            out.add(arena, str8("'"));
        }
        out.add(arena, str8(">"));
    } break;
    case Block::TABLE_ROW: {
        out.add(arena, str8("<tr>"));
    } break;
    case Block::QUOTE: {
        out.add(arena, str8("<blockquote>\n"));
    } break;
    case Block::EXPAND: {
        out.add(arena, str8("<details>\n<summary>\n"));
        str8 h = {};
        if (block->num) {
            h = str8_create_size(arena, 2);
            h.str[0] = 'h';
            h.str[1] = '0' + (u8) block->num;
            out.add(arena, str8("<"));
            out.add(arena, h);
            out.add(arena, str8(" id='"));
            out.add(arena, block->id);
            out.add(arena, str8("'>"));
        }
        out.add(arena, block->title);
        if (block->num) {
            out.add(arena, str8("</"));
            out.add(arena, h);
            out.add(arena, str8(">"));
        }
        out.add(arena, str8("</summary>\n<p>\n"));
    } break;
    case Block::ORD_LIST: {
        out.add(arena, str8("<ol>\n"));
    } break;
    case Block::UN_LIST: {
        out.add(arena, str8("<ul>\n"));
    } break;
    case Block::CODE:  {
        out.add(arena, str8("<pre><code id='"));
        out.add(arena, block->id);
        out.add(arena, str8("'>"));
    } break;
    case Block::RULE:  {
        out.add(arena, str8("<hr>\n"));
    } break;
    case Block::PARAGRAPH: {
        // out.add(arena, str8("<p>\n"));
    } break;
    default: {
        NOTIMPLEMENTED();
        break;
    }
    }

    block->content = render_text(arena, block->text);
    out.append(block->content);    

    switch (block->type) {
    case Block::HEADING: {
        str8 h = str8_create_size(arena, 2);
        h.str[0] = 'h';
        h.str[1] = '0' + (u8) block->num;
                    
        out.add(arena, str8("</"));
        out.add(arena, h);
        out.add(arena, str8(">"));
    } break;
    case Block::TABLE_ROW: {
        out.add(arena, str8("</tr>\n"));
        if (block->next->type != Block::TABLE_ROW) {
            out.add(arena, str8("</table>\n"));
        }
    } break;
    case Block::QUOTE: {
        out.add(arena, str8("</blockquote>\n"));
    } break;
    case Block::EXPAND: {
        out.add(arena, str8("</p>\n</details>\n"));
    } break;
    case Block::ORD_LIST: {
        out.add(arena, str8("\n</ol>\n"));
    } break;
    case Block::UN_LIST: {
        out.add(arena, str8("\n</ul>\n"));
    } break;
    case Block::CODE:  {
        out.add(arena, str8("</code></pre>\n"));
    } break;
    case Block::RULE:  {} break;
    case Block::PARAGRAPH: {
        // out.add(arena, str8("</p>\n"));
    } break;
    default: {
        NOTIMPLEMENTED();
        break;
    }
    }

    if (block->type != Block::TABLE_ROW && block->next->type == Block::TABLE_ROW) {
        out.add(arena, str8("<table>"));
    }
    
    return out;
}
