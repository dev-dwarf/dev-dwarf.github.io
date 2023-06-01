#include "md_to_html.h"

/* Take list of Text nodes with undecided type, parse and return typed list */
Text* parse_text(Arena *arena, Block *block) {
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
            curr->text = str_skip(curr->text, (SKIP));                 \
            curr = pre; /* overwrite current node */                    \
        } else {                                                        \
            Text* temp = curr->next;                                    \
            curr->next = Arena_take_struct_zero(arena, Text);          \
            curr->next->type = TYPE;                                    \
            curr->next->text = str_skip(curr->text, (END) + (SKIP));   \
            curr->next->next = temp;                                    \
            curr->text = str_first(curr->text, (END));                 \
        }                                                               \
        if (TEST_FLAG(inside, FLAG(TYPE))) {                            \
            curr->next->end = true;                                     \
        }                                                               \
        TOGGLE_FLAG(inside, FLAG(TYPE));                                \
        REM_FLAG(inside, FLAG(Text::TEXT));                             \
        REM_FLAG(inside, FLAG(Text::BREAK));                            \
    }

    for (; curr->next != 0; pre = curr, curr = curr->next) {
        str s = curr->text;
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
        ch8 c[3]; 
        c[1] = s.str[0];
        c[2] = (s.len > 1)? s.str[1] : 0;
        str_iter_custom(s, i, _unused) {
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
                PUSH_TEXT(Text::STRUCK, i, 2);
                break;
            } else if (c[0] == '@' && c[1] == '(') {
                PUSH_TEXT(Text::LINK, i, 2);
                curr = curr->next;
                s64 sentinel = str_char_location(curr->text, ' ');
                ASSERT(sentinel != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, sentinel, 1);
                paren_stack[paren_stacki++] = Text::LINK;
                break;
            } else if (c[0] == '!' && c[1] == '(') {
                PUSH_TEXT(Text::IMAGE, i, 2);
                curr = curr->next;
                s64 sentinel = str_char_location(curr->text, ')');
                ASSERT(sentinel != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, sentinel, 1);
                break;
            } else if (c[0] == '?' && c[1] == '(') {
                PUSH_TEXT(Text::EXPLAIN, i, 2);
                curr = curr->next;
                s64 sentinel = str_char_location(curr->text, ',');
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
        } /* end str_iter */
        ASSERTM(pre == &pre_filler || pre->type != Text::NIL, "Must not leave NIL nodes in Text linked-list!");
    }

    return text;
}

/* Take str, return tree of blocks representing markdown structure */
Block* parse(Arena *arena, str s) {
    Block* root = Arena_take_struct_zero(arena, Block);
    Block* curr = root;
    Block next = {0};
    next.text = Arena_take_struct_zero(arena, Text);
    Text* end = next.text;
        
    u64 next_len = 0;

#define PUSH_STR(str) {                                     \
        end->text = str;                                    \
        end->next = Arena_take_struct_zero(arena, Text);        \
        end = end->next;                                    \
}

#define PUSH_BLOCK() if (next.type != Block::NIL) {     \
    *curr = next;                                       \
    curr->next =  Arena_take_struct_zero(arena, Block);     \
    curr = curr->next;                                  \
    next = {0};                                         \
    next.text = Arena_take_struct_zero(arena, Text);   \
    end = next.text;                                    \
}

#define BREAK_BLOCK_IF_NOT(TYPE) {                      \
    if (next.type != TYPE) {                            \
        PUSH_BLOCK();                                   \
    }                                                   \
}
    
    str_iter_pop_line(s) {
        /* Remove windows newline encoding (\r\n) */
        line = str_trim_suffix(line, strlit("\r"));
        if (line.len == 0) {
            if (next.type == Block::TABLE_ROW) {
                PUSH_BLOCK();
                next.type = Block::TABLE_END;
            }
            PUSH_STR(line);
            if (!(next.type == Block::CODE || next.type == Block::PARAGRAPH)) {
                PUSH_BLOCK();
            }
            continue;
        }
        ch8 c[3];
        c[0] = line.str[0];
        c[1] = (line.len > 1)? line.str[1] : 0;
        c[2] = (line.len > 2)? line.str[2] : 0;
        if (c[0] == '`' && c[1] == '`' && c[2] == '`') {
            if (next.type != Block::CODE) {
                PUSH_BLOCK();
                next.type = Block::CODE;
                next.id = str_skip(line, 3);
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
            str rem = str_skip(line, n);
            next.id = rem;
            next.id.len = str_char_location(rem, ' ');
            
            PUSH_STR(str_skip(rem, next.id.len+1));
            PUSH_BLOCK();
            
        } else if (c[0] == '!' && c[1] == '|') {
            PUSH_BLOCK();
            line = str_skip(line, 2);
            next.id = line;
            next.id.len = str_char_location(line, '|');
            line = str_skip(line, next.id.len);
            next.type = Block::TABLE_ROW;
            PUSH_STR(line);
        } else if (c[0] == '>' && c[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::QUOTE);
            next.type = Block::QUOTE;
            PUSH_STR(str_skip(line, 2));
        } else if (c[0] == '[' && c[1] == ' ')  {
            if (next.type != Block::EXPAND) {
                PUSH_BLOCK();
                next.type = Block::EXPAND;
                str rem = str_skip(line, 2);
                u32 n = 0;
                for (; n < rem.len && rem.str[n] == '#'; n++)
                    ;
                rem = str_skip(rem, n);
                if (n > 0) {
                    next.id = rem;
                    next.id.len = str_char_location(rem, ' ');
                    rem = str_skip(rem, next.id.len+1);
                }
                next.title = rem;
                next.num = n;
            } else {
                PUSH_STR(str_skip(line, 2));
            }
        } else if (c[0] >= '1' && c[0] <= '9' && c[1] == '.' && c[2] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::ORD_LIST);
            next.type = Block::ORD_LIST;
            end->type = Text::LIST_ITEM;
            end->next = Arena_take_struct_zero(arena, Text);
            end = end->next;
            PUSH_STR(str_skip(line, 2));
            end->type = Text::LIST_ITEM;
            end->next = Arena_take_struct_zero(arena, Text);
            end->end = true;
            end = end->next;
        } else if ((c[0] == '*' || c[0] == '-') && c[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::UN_LIST);
            next.type = Block::UN_LIST;
            end->type = Text::LIST_ITEM;
            end->next = Arena_take_struct_zero(arena, Text);
            end = end->next;
            PUSH_STR(str_skip(line, 2));
            end->type = Text::LIST_ITEM;
            end->next = Arena_take_struct_zero(arena, Text);
            end->end = true;
            end = end->next;
        } else if (c[0] == '@' && c[1] == '{') {
            PUSH_BLOCK();
            line = str_skip(line, 2);
            next.type = Block::SPECIAL;
            next.id = str_pop_at_first_delimiter(&line, strlit(",}"));
            str_iter_pop_delimiter(line, strlit(",}")) {
                /* NOTE(lcf): optionally allow space in args list */
                if (str_char_location(sub, ' ') == 0) {
                    sub = str_skip(sub, 1);
                }
                StrList_add(arena, &next.content, sub);
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
StrList render_text(Arena* arena, Text* root) {
    StrList out = {0};
    Text prev_filler = {root, Text::NIL, 0};
    for (Text* t = root, *prev = &prev_filler; t->type != Text::NIL; prev = t, t = t->next) {
        switch (t->type) {
        case Text::BOLD: {
            str s[2] = {strlit("<b>"), strlit("</b>")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::ITALIC: {
            str s[2] = {strlit("<em>"), strlit("</em>")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::STRUCK: {
            str s[2] = {strlit("<s>"), strlit("</s>")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::CODE_INLINE: {
            str s[2] = {strlit("<code>"), strlit("</code>")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::CODE_BLOCK: {
            StrList_add(arena, &out, t->text);
            StrList_add(arena, &out, str_NEWLINE);
        } break;
        case Text::LINK: {
            if (!t->end) {
                StrList_add(arena, &out, strlit("<a href='"));
                StrList_add(arena, &out, t->text);
                StrList_add(arena, &out, strlit("'>"));
            } else {
                StrList_add(arena, &out, strlit("</a>"));
                StrList_add(arena, &out, t->text);
            }
        } break;
        case Text::EXPLAIN: {
            if (!t->end) {
                StrList_add(arena, &out, strlit("<abbr title='"));
                StrList_add(arena, &out, t->text);
                StrList_add(arena, &out, strlit("'>"));
            } else {
                StrList_add(arena, &out, strlit("</abbr>"));
                StrList_add(arena, &out, t->text);
            }
        } break;
        case Text::IMAGE: {
            /* TODO alt text, styles, etc */
            StrList_add(arena, &out, strlit("<img src='"));
            StrList_add(arena, &out, t->text);
            StrList_add(arena, &out, strlit("'>"));
        } break;
        case Text::TABLE_CELL: {
            str s[2] = {strlit("<td>"), strlit("</td>\n")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::BREAK: {
            StrList_add(arena, &out, strlit("<br>"));
            StrList_add(arena, &out, t->text);
        } break;
        case Text::LIST_ITEM: {
            const str s[2] = {strlit("<li>"), strlit("</li>\n")};
            StrList_add(arena, &out, s[t->end]);
            StrList_add(arena, &out, t->text);
        } break;
        case Text::TEXT: {
            StrList_add(arena, &out, t->text);
            if (prev->type == Text::TEXT) {
                StrList_add(arena, &out, str_NEWLINE);
            }
        } break;
        default: {
            ASSERTM(0,"Forgot to add a case in render_text!");
        } break;
        }
    }

    return out;
}

/* take fully parsed markdown blocks and render as html tags */
StrList render_block(Arena *arena, Block *block) {
    StrList out = {0};
    switch (block->type) {
    case Block::HEADING: {
        str h = str_create_size(arena, 2);
        h.str[0] = 'h';
        h.str[1] = '0' + (u8) block->num;
        StrList_add(arena, &out, strlit("<"));
        StrList_add(arena, &out, h);
        if (block->id.len > 0) {
            static const str center = strlit("center");
            static const str right = strlit("right");
            if (str_has_prefix(block->id, center)) {
                block->id = str_skip(block->id, center.len+1);
                StrList_add(arena, &out, strlit(" style='text-align:center'"));
            } else if (str_eq(block->id, right)) {
                block->id = str_skip(block->id, right.len+1);
                StrList_add(arena, &out, strlit(" style='text-align:right'"));
            }
            StrList_add(arena, &out, strlit(" id='"));
            StrList_add(arena, &out, block->id);
            StrList_add(arena, &out, strlit("'"));
        }
        StrList_add(arena, &out, strlit(">"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</"));
        StrList_add(arena, &out, h);
        StrList_add(arena, &out, strlit(">"));

    } break;
    case Block::TABLE_ROW: {
        StrList_add(arena, &out, strlit("<tr>"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</tr>\n"));
        if (block->next->type != Block::TABLE_ROW) {
            StrList_add(arena, &out, strlit("</table>\n"));
        }
    } break;
    case Block::TABLE_END: {
    } break;
    case Block::QUOTE: {
        StrList_add(arena, &out, strlit("<blockquote>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</blockquote>\n"));

    } break;
    case Block::EXPAND: {
        StrList_add(arena, &out, strlit("<details>\n<summary>\n"));
        str h = {};
        if (block->num) {
            h = str_create_size(arena, 2);
            h.str[0] = 'h';
            h.str[1] = '0' + (u8) block->num;
            StrList_add(arena, &out, strlit("<"));
            StrList_add(arena, &out, h);
            StrList_add(arena, &out, strlit(" id='"));
            StrList_add(arena, &out, block->id);
            StrList_add(arena, &out, strlit("'>"));
        }
        StrList_add(arena, &out, block->title);
        if (block->num) {
            StrList_add(arena, &out, strlit("</"));
            StrList_add(arena, &out, h);
            StrList_add(arena, &out, strlit(">"));
        }
        StrList_add(arena, &out, strlit("</summary>\n<p>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</p>\n</details>\n"));

    } break;
    case Block::ORD_LIST: {
        StrList_add(arena, &out, strlit("<ol>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("\n</ol>\n"));
    } break;
    case Block::UN_LIST: {
        StrList_add(arena, &out, strlit("<ul>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("\n</ul>\n"));
    } break;
    case Block::CODE:  {
        StrList_add(arena, &out, strlit("<pre><code id='"));
        StrList_add(arena, &out, block->id);
        StrList_add(arena, &out, strlit("'>"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</code></pre>\n"));
    } break;
    case Block::RULE:  {
        StrList_add(arena, &out, strlit("<hr>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
    } break;
    case Block::PARAGRAPH: {
        StrList_add(arena, &out, strlit("<p>\n"));
        StrList_append(&out, block->content = render_text(arena, block->text));
        StrList_add(arena, &out, strlit("</p>\n"));
    } break;
    default: {
        NOTIMPLEMENTED();
        break;
    }
    }

    if (block->type != Block::TABLE_ROW && block->next->type == Block::TABLE_ROW) {
        StrList_add(arena, &out, strlit("<table class=\""));
        StrList_add(arena, &out, block->next->id);
        StrList_add(arena, &out, strlit("\">"));
    }
    
    return out;
}
