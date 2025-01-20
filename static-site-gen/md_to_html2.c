
// #include "md_to_html.h"

void print_tree(Text *text) {
    if (!text) return;
    switch (text->type) {
        case TEXT: { printf("%.*s", (s32) text->text.len, text->text.str); } break;
        case BOLD: { printf("**"); } break;
        case ITALIC: { printf("*"); } break;
        case STRUCK: { printf("~~"); } break;
        case LINK: { printf("@("); } break;
    }
    print_tree(text->child);
    switch (text->type) {
        case BOLD: { printf("**"); } break;
        case ITALIC: { printf("*"); } break;
        case STRUCK: { printf("~~"); } break;
        case LINK: { printf(")"); } break;
    }
    print_tree(text->next);
}

str parse_inline(Arena *a, Text *text);

Text *new_text(Arena *a, str text, enum TextTypes type) {
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
    if (text->type == type && str_has_prefix(*s, close)) {
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
        text->data = text->text;
        text->text.len = 0;
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
        } else if (parse_end(a, text, &s, ITALIC, strl("*"))) { break; }
        else if (parse_end(a, text, &s, BOLD, strl("**"))) { break; } 
        else if (parse_end(a, text, &s, STRUCK, strl("~~"))) { break; } 
        else if (parse_end(a, text, &s, LINK, strl(")"))) { break; }
        else if (parse_end(a, text, &s, IMAGE, strl(")"))) { break; }
        else if (parse_end(a, text, &s, EXPLAIN, strl(")"))) { break; }
        else if (parse_end(a, text, &s, TABLE_CELL, strl("|"))) { break; }
        
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

Block* parse_md(Arena *a, str s) {
    Block *first = Arena_take_struct(a, Block);
    Block *curr = first;

    str_iter_line(s, line) {
        line = str_trim_suffix(line, strl("\n"));
        line = str_trim_suffix(line, strl("\r"));
        line = str_trim_whitespace_front(line);

        if (line.len == 0) {
            if (curr->type != CODE) {
                curr = curr->next = Arena_take_struct(a, Block);
            }
        }

        char c[3]; memcpy(c, line.str, MIN(3, line.len));
        if (str_has_prefix(line, strl("```"))) {
            if (curr->type == CODE) {
                curr = curr->next = Arena_take_struct(a, Block);
            } else {
                curr = curr->next = Arena_take_struct(a, Block);
                curr->type = CODE;
            }
        } else if (curr->type == CODE) {
            // TODO escape text, push onto block
            
            continue;
        } else if (c[0] >= '0' && c[0] <= '9' && c[1] == '.') {
            
        } else if ((c[0] == '*' || c[0] == '-') && c[1] == ' ') {

        } else if (c[0] == '-' && c[1] == '-' && c[2] == '-') {
        
        } else if (c[0] == '>' && c[1] == ' ') {
        
        } else if (c[0] == '[' && c[1] == ' ') {

        } else if (c[0] == '!' && c[1] == '|') {
        
        } else if (c[0] == '@' && c[1] == '{') {
        
        } else if (c[0] == '#') {
        
        } else {
            if (curr->type != PARAGRAPH) {
                curr = curr->next = Arena_take_struct(a, Block);
            }
        }
    }

    return first;
}
