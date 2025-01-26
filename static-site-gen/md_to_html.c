#include "md_to_html.h"

str parse_inline(Arena *a, Text *text);

static Text *new_text(Arena *a, str text, enum TextTypes type) {
    Text *out = Arena_take_struct(a, Text);
    *out = (Text) { .type = type, .text = text };
    return out;
}

static s32 parse_start(Arena *a, Text *text, str *s, enum TextTypes type, str open) {
    if (str_has_prefix(*s, open)) {
        text->last_child->text = str_from_pointer_range(text->last_child->text.str, (*s).str);
        *s = str_skip(*s, open.len);

        if (text->last_child->text.len) {
            text->last_child->next = new_text(a, *s, type);
            text->last_child = text->last_child->next;
        } else {
            *text->last_child = (Text) { .type = type, .text = *s};
        }
        
        str parsed = parse_inline(a, text->last_child);
        *s = parsed;
        return 1;
    } 
    return 0;
}

static s32 parse_end(Arena *a, Text *text, str *s, enum TextTypes type, str close) {
    if (text->type == type && (str_has_prefix(*s, close) || s->len == 0)) {
        if (text->last_child->text.str + text->last_child->text.len >= (*s).str) {
            text->last_child->text = str_from_pointer_range(text->last_child->text.str, (*s).str);
        }
        text->text = str_from_pointer_range(text->text.str, (*s).str);
        *s = str_skip(*s, close.len);
        return 1;
    }
    return 0;
}

// returns remaining string to parse
str parse_inline(Arena *a, Text *text) {
    // Grab data for certain elements
    if (text->type == LINK) {
        s64 loc = str_char_location(text->text, ' ');
        ASSERT(loc);
        text->data = str_first(text->text, loc);
        text->text = str_skip(text->text, loc+1);
    } else if (text->type == IMAGE) {
        s64 loc = str_char_location(text->text, ')'); ASSERT(loc >= 0);
        text->data = str_first(text->text, loc);
    } else if (text->type == EXPLAIN) {
        s64 loc = str_char_location(text->text, ',');
        ASSERT(loc);
        text->data = str_first(text->text, loc);
        text->text = str_skip(text->text, loc+1);
    }
    
    str s = text->text;
    text->last_child = text->child = new_text(a, s, TEXT);

    s32 ignore_next = false;
    for (;;) {
        while ((s.len > 0) && (char_is_whitespace(*s.str) || char_is_alpha(*s.str) || char_is_num(*s.str))) {
            s.str++; s.len--;
        }

        if (s.str[0] == '\\') {
            text->last_child->text = str_from_pointer_range(text->last_child->text.str, s.str);
            s = str_skip(s, 1); /* skip \ */
            text->last_child->next = new_text(a, s, TEXT);
            text->last_child = text->last_child->next;
            s = str_skip(s, 1); // dont parse next char
        } 
        else if (parse_end(a, text, &s, CODE_INLINE, strl("`"))) { break; }
        else if (parse_end(a, text, &s, ITALIC, strl("*"))) { break; }
        else if (parse_end(a, text, &s, BOLD, strl("**"))) { break; } 
        else if (parse_end(a, text, &s, STRUCK, strl("~~"))) { break; } 
        else if (parse_end(a, text, &s, LINK, strl(")"))) { break; }
        else if (parse_end(a, text, &s, IMAGE, strl(")"))) { break; }
        else if (parse_end(a, text, &s, EXPLAIN, strl(")"))) { break; }
        else if (parse_end(a, text, &s, TABLE_CELL, strl("|"))) { break; }

        else if (parse_start(a, text, &s, CODE_INLINE, strl("`"))) {}
        else if (parse_start(a, text, &s, BOLD, strl("**"))) {} 
        else if (parse_start(a, text, &s, ITALIC, strl("*"))) {}
        else if (parse_start(a, text, &s, STRUCK, strl("~~"))) {} 
        else if (parse_start(a, text, &s, LINK, strl("@("))) {}
        else if (parse_start(a, text, &s, IMAGE, strl("!("))) {}
        else if (parse_start(a, text, &s, EXPLAIN, strl("?("))) {} 
        else if (parse_start(a, text, &s, TABLE_CELL, strl("|"))) {}
        else if (s.len > 0) {
            s.str++; s.len--;
        }

        if (s.len > 0) {
            if (text->last_child->type != TEXT) {
                text->last_child->next = new_text(a, s, TEXT);
                text->last_child = text->last_child->next;
            }
        } else {
            break;
        }
    }

    return s;
}    

static void push_text(Arena *a, Block *b, str line) {
    Text *t = Arena_take_struct_zero(a, Text);
    t->text = line;

    Text *last_text = b->text;
    if (!last_text) {
        b->text = t;
    } else {
        while (last_text->next) {
            last_text = last_text->next;
        }
        last_text->next = t;
    }
    
    if (b->type != CODE) {
        parse_inline(a, t);
    } 
}

static s32 new_block_if_not(Arena *a, Block **curr, enum BlockTypes type) {
    if ((*curr)->type != type) {
        if ((*curr)->type != 0) {
            (*curr) = (*curr)->next = Arena_take_struct_zero(a, Block);
        }
        (*curr)->type = type;
        return 1;
    }
    return 0;
}

static s32 code_block_number;
Block* parse_md(Arena *a, str s) {
    Block *first = Arena_take_struct_zero(a, Block);
    Block *curr = first;

    code_block_number = 0;

    str_iter_line(s, line) {
        str line_raw = line;
        line = str_trim_suffix(line, strl("\n"));
        line = str_trim_suffix(line, strl("\r"));
        line = str_trim_whitespace_front(line);

        char c[3] = {0}; memcpy(c, line.str, MIN(3, line.len));
        if (str_has_prefix(line, strl("```"))) {
            if (curr->type == CODE) {
                curr = curr->next = Arena_take_struct_zero(a, Block);
            } else {
                curr = curr->next = Arena_take_struct(a, Block);
                *curr = (Block){ .type = CODE, .id = str_trim_whitespace(str_skip(line, 3)) };
                if (curr->id.len == 0) {
                    curr->id = strf(a, "code%03d", code_block_number);
                }
                code_block_number++;
            }
        } else if (curr->type == CODE) {
            push_text(a, curr, line_raw);
            continue;
        } else if (line.len == 0) {
            curr = curr->next = Arena_take_struct_zero(a, Block); 
        } else if (c[0] >= '0' && c[0] <= '9' && c[1] == '.') {
            new_block_if_not(a, &curr, ORD_LIST);
            line = str_skip(line, 2);
            push_text(a, curr, line);
        } else if ((c[0] == '*' || c[0] == '-') && c[1] == ' ') {
            new_block_if_not(a, &curr, UN_LIST);
            line = str_skip(line, 2);
            push_text(a, curr, line);
        } else if (c[0] == '-' && c[1] == '-' && c[2] == '-') {
            curr = curr->next = Arena_take_struct_zero(a, Block);
            curr->type = RULE;
            curr = curr->next = Arena_take_struct_zero(a, Block);
        } else if (c[0] == '>' && c[1] == ' ') {
            new_block_if_not(a, &curr, QUOTE);
            line = str_skip(line, 2);
            push_text(a, curr, line);
        } else if (c[0] == '[' && c[1] == ' ') {
            line = str_skip(line, 2);
            if (new_block_if_not(a, &curr, EXPAND)) {
                curr->title = line;
            } else {
                push_text(a, curr, line);
            }
        } else if (c[0] == '!' && c[1] == '|') {
            line = str_skip(line, 2);
            s64 loc = str_char_location(line, '|'); ASSERT(loc >= 0);
            if (new_block_if_not(a, &curr, TABLE)) {
                curr->id = str_first(line, loc);
            }
            line = str_skip(line, loc);
            push_text(a, curr, line);
        } else if (c[0] == '@' && c[1] == '{') {
            line = str_skip(line, 2);
            curr = curr->next = Arena_take_struct_zero(a, Block);
            curr->type = SPECIAL;
            curr->id = str_pop_at_first_delimiter(&line, strl(",}"));
            str_iter_delimiter(line, strl(",}"), sub) {
                StrList_push(a, &curr->content, str_trim_whitespace(sub));
            }
        } else if (c[0] == '#') {
            s32 n = 1;
            while (n < line.len && line.str[n] == '#') {
                n++;
            }
            line = str_skip(line, n);

            curr = curr->next = Arena_take_struct_zero(a, Block);
            curr->type = HEADING;
            curr->num = n;
            curr->id = line;
            curr->id.len = str_char_location(line, ' ');
            
            push_text(a, curr, str_skip(line, curr->id.len));
        } else {
            new_block_if_not(a, &curr, PARAGRAPH);
            push_text(a, curr, line);
        }
    }

    return first;
}

StrList render_text_inline(Arena *a, Text* first);

StrList render_text_inline_loop(Arena *a, Text *first) {
    StrList o = {0};
    for (Text *t = first; t; t = t->next) {
        StrList_append(&o, render_text_inline(a, t));
    }
    return o;
}

static void render_wrap_text(Arena *a, StrList *out, Text *t, enum TextTypes type, str open, str close)  {
    if (t->type == type) {
        StrList_push(a, out, open);
        StrList_append(out, render_text_inline_loop(a, t->child));
        StrList_push(a, out, close);
    }
}

// Render text on the same line
StrList render_text_inline(Arena *a, Text* first) {
    StrList o = {0};
    StrList *out = &o;

    Text *t = first;
    if (t) {
        render_wrap_text(a, out, t, BOLD, strl("<b>"), strl("</b>"));
        render_wrap_text(a, out, t, ITALIC, strl("<em>"), strl("</em>"));
        render_wrap_text(a, out, t, STRUCK, strl("<s>"), strl("</s>"));
        render_wrap_text(a, out, t, CODE_INLINE, strl("<code>"), strl("</code>"));
        render_wrap_text(a, out, t, TABLE_CELL, strl("<td>"), strl("</td>"));
        
        if (t->type == IMAGE) {
            if (str_has_suffix(t->data, strl(".mp4"))) {
                StrList_pushv(a, out, strl("<video controls><source src='"), t->data, strl("' type='video/mp4'></video>"));
            } else {
                StrList_pushv(a, out, strl("<img src='"), t->data, strl("'>"));
            }
        }

        if (t->type == EXPLAIN) {
            StrList_pushv(a, out, strl("<abbr title='"), t->data, strl("'>"));
            StrList_append(out, render_text_inline_loop(a, t->child));
            StrList_push(a, out, strl("</abbr>"));
        }

        if (t->type == LINK) {
            StrList_pushv(a, out, strl("<a href='"), t->data, strl("'>"));
            StrList_append(out, render_text_inline_loop(a, t->child));
            StrList_push(a, out, strl("</a>"));
        }

        if (t->type == TEXT) {
            StrList_push(a, out, t->text);
        }

        if (t->type == 0) {
            if (t->child) {
                StrList_append(out, render_text_inline_loop(a, t->child));
            }
        }
    }

    return o;
}

typedef struct WrapBlock {
    enum BlockTypes type;
    str open;
    str close;
    str open_line;
    str close_line;
} WrapBlock;

void render_wrap_block(Arena *a, Block *b, StrList *out, WrapBlock params) {
    if (params.close_line.len == 0) {
        params.close_line = strl("\n");
    }
    
    if (b->type == params.type) {
        StrList rendered = (StrList){0};
        for (Text *t = b->text; t; t = t->next) {
            StrList r = render_text_inline(a, t);
            if (r.total_len > 0) {
                if (params.open_line.len) {
                    StrList_push(a, &rendered, params.open_line);
                }
                StrList_append(&rendered, r);    
                StrList_push(a, &rendered, params.close_line);
            }
        }

        if (b->type == PARAGRAPH && rendered.total_len == 0) {
            StrList_push(a, out, strl("<br>\n"));
        } else {
            StrList_push(a, out, params.open);
            StrList_append(out, rendered);
            StrList_push(a, out, params.close);
        }
    }
}

StrList render_block(Arena *a, Block *b) {
    StrList o = {0};
    StrList *out = &o;

    render_wrap_block(a, b, out, (WrapBlock) { PARAGRAPH, strl("<p>\n"), strl("</p>\n") });
    render_wrap_block(a, b, out, (WrapBlock) { QUOTE, strl("<blockquote><p>\n"), strl("</p></blockquote>\n")});

    str open = (b->id.len > 0)? strf(a, "<h%d id='%.*s'>\n", (s32) b->num, str_PRINTF_ARGS(b->id)) : strf(a, "<h%d>\n", (s32) b->num);
    render_wrap_block(a, b, out, (WrapBlock) { HEADING, open, strf(a, "</h%d>\n", (s32) b->num)});
    
    render_wrap_block(a, b, out, (WrapBlock) { ORD_LIST, strl("<ol>\n"), strl("</ol>\n"), strl("<li>"), strl("</li>\n")});
    render_wrap_block(a, b, out, (WrapBlock) { UN_LIST, strl("<ul>\n"), strl("</ul>\n"), strl("<li>"), strl("</li>\n")});
    render_wrap_block(a, b, out, (WrapBlock) { TABLE, strf(a, "<table class=\"%.*s\">\n", str_PRINTF_ARGS(b->id)), strl("</table>\n"), strl("<tr>"), strl("</tr>\n")});
    render_wrap_block(a, b, out, (WrapBlock) { EXPAND, strf(a, "<details>\n<summary>%.*s</summary>\n<p>", (s32) b->title.len, b->title.str), strl("</p>\n</details>\n")});
    
    render_wrap_block(a, b, out, (WrapBlock) { RULE, strl("<hr>\n")});
    render_wrap_block(a, b, out, (WrapBlock) { 0, strl("\n")});
    
    if (b->type == CODE) {
        StrList_pushv(a, out, strl("<code id='"), b->id, strl("'><pre>\n"));
        s32 line = 1;

        s32 in_comment = 0;
        str comment_span = strc("<span style='color: var(--comment);'>");
        for (Text *t = b->text; t; t = t->next, line++) {
            str line_id = strf(a, "%.*s-%d", b->id.len, b->id.str, line);
            StrList_pushv(a, out, strl("<span id='"), line_id, strl("'>"));
            StrList_pushv(a, out, strl("<a href='#"), line_id, strl("' aria-hidden='true'></a>"));

            if (in_comment == 2) {
                StrList_push(a, out, comment_span);
            }
            
            char in_string = 0;
            str s = t->text;
            str_iter(s, i, c) { /* Escape HTML characters in code blocks */
                if (str_has_prefix(str_skip(s, i), strl("//"))) {
                    in_comment = 1;
                    StrList_pushv(a, out, str_first(s, i), comment_span);
                    s = str_skip(s, i); i = 2;
                }
                
                if (str_has_prefix(str_skip(s, i), strl("/*"))) {
                    in_comment = 2;
                    StrList_pushv(a, out, str_first(s, i), comment_span);
                    s = str_skip(s, i); i = 2;
                } else if (in_comment == 2 && str_has_prefix(str_skip(s, i), strl("*/"))) {
                    in_comment = 0;
                    StrList_pushv(a, out, str_first(s, i), strl("*/</span>"));
                    s = str_skip(s, i+2); i = -1;
                }

                switch (c) {
                case '<': {
                    StrList_pushv(a, out, str_first(s, i), strl("&lt;"));
                    s = str_skip(s, i+1); i = -1;
                } break;
                case '>': {
                    StrList_pushv(a, out, str_first(s, i), strl("&gt;"));
                    s = str_skip(s, i+1); i = -1;
                } break;
                case '&': {
                    StrList_pushv(a, out, str_first(s, i), strl("&amp;"));
                    s = str_skip(s, i+1); i = -1;
                } break;
                case '\'': 
                case '"': {
                    if (in_comment == 0) {
                        s64 loc = str_char_location(str_skip(s, i+1), c);
                        if (in_string == 0 && loc >= 0) {
                            in_string = c;
                            StrList_pushv(a, out, str_first(s, i), strl("<span style='color: var(--red);'>"));
                            s = str_skip(s, i); i = 0;
                        } else if (in_string == c) {
                            in_string = 0;
                            StrList_pushv(a, out, str_first(s, i+1), strl("</span>"));
                            s = str_skip(s, i+1); i = 0;
                        }
                    }
                } break;
                }
            }

            StrList_push(a, out, s);
            if (in_comment > 0) {
                if (in_comment == 1) {
                    in_comment = 0;
                }
                StrList_push(a, out, strl("</span>"));
            }
            StrList_push(a, out, strl("</span>"));
        }
        StrList_pushv(a, out, strl("</pre></code>\n"));
    }

    return o;
}
