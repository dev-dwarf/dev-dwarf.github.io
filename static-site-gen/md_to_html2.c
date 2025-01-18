
// #include "md_to_html.h"

void print_tree(Text *text) {
    if (!text) return;
    switch (text->type) {
        case TEXT: { printf("%.*s\n", (s32) text->text.len, text->text.str); } break;
        case BOLD: { printf("<BOLD\n"); } break;
        case ITALIC: { printf("<ITALIC\n"); } break;
        case STRUCK: { printf("<STRUCK\n"); } break;
        case LINK: { printf("<LINK\n"); } break;
    }
    print_tree(text->child);
    switch (text->type) {
        case BOLD: { printf("BOLD>\n"); } break;
        case ITALIC: { printf("ITALIC>\n"); } break;
        case STRUCK: { printf("STRUCK>\n"); } break;
        case LINK: { printf("LINK>\n"); } break;
    }
    print_tree(text->next);
}

str parse_inline(Arena *a, Text *text);

Text *new_text(Arena *a, str text, enum TextTypes type) {
    Text *out = Arena_take_struct(a, Text);
    *out = (Text) { .type = type, .text = text };
    return out;
}

static s32 parse_start(Arena *a, Text *text, str *s, enum TextTypes type, str open, str close) {
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

static s32 parse_end(Arena *a, Text *text, str *s, enum TextTypes type, str open, str close) {
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
    str s = text->text;
    text->last_child = text->child = new_text(a, s, TEXT);
    for (;;) {
        while ((s.len > 0) && (char_is_whitespace(*s.str) || char_is_alpha(*s.str) || char_is_num(*s.str))) {
            s.str++; s.len--;
        }

        if (0) {}
        else if (parse_end(a, text, &s, ITALIC, strl("*"), strl("*"))) { break; }
        else if (parse_end(a, text, &s, BOLD, strl("**"), strl("**"))) { break; } 
        else if (parse_end(a, text, &s, STRUCK, strl("~~"), strl("~~"))) { break; } 
        else if (parse_end(a, text, &s, LINK, strl("@("), strl(")"))) { break; }
        else if (parse_start(a, text, &s, BOLD, strl("**"), strl("**"))) {} 
        else if (parse_start(a, text, &s, ITALIC, strl("*"), strl("*"))) {}
        else if (parse_start(a, text, &s, STRUCK, strl("~~"), strl("~~"))) {} 
        else if (parse_start(a, text, &s, LINK, strl("@("), strl(")"))) {}
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
    
s32 find_section(str *next, str *text, str first, str end) {
    if (str_has_prefix(*next, first)) {
        *next = str_skip(*next, first.len);
        s64 loc = str_substring_location(*next, end);
        ASSERT(loc >= 0);
        *text = str_skip(*next, loc + end.len);
        *next = str_first(*next, loc); 
        return 1;
    } else {
        return 0;
    }
}

/* TODO what the fuck am I doing bro */
void parse_text_push_next(Arena *a, Text* curr, Text t) {
    // curr->child = Arena_take_struct_zero(arena, Text);
    // *(curr->child) = t;
}

/* Input text should be unparsed, all lines */
Text* parse_text2(Arena *arena, Text *block_text) {
    Text *out = block_text;
    Text *curr = block_text;
    
    s32 ignore_next = 0;
    u32 inside_flags = 0;
    enum TextTypes paren_stack[32] = {0};
    u32 paren_stack_index = 0;

    str text = curr->text;
    for (;;) {
        str next = text;
        if (find_section(&next, &text, strl("**"), strl("**"))) { /* bold */
            parse_text_push_next(arena, curr, (Text) { 
                .text = next, 
                .type = BOLD
            });
        } else if (find_section(&next, &text, strl("*"), strl("*"))) { /* italic */
            parse_text_push_next(arena, curr, (Text) { 
                .text = next, 
                .type = ITALIC
            });
        } else if (find_section(&next, &text, strl("~~"), strl("~~"))) { /* struck */
            parse_text_push_next(arena, curr, (Text) { 
                .text = next, 
                .type = STRUCK
            });
        } else if (find_section(&next, &text, strl("@("), strl(")"))) { /* Link */
            s64 loc = str_char_location(next, ' ');
            ASSERT(loc >= 0);
            parse_text_push_next(arena, curr, (Text) { 
                .data = str_first(next, loc),
                .text = str_skip(next, loc+1), 
                .type = LINK
            });
        } else if (find_section(&next, &text, strl("!("), strl(")"))) { /* Image */
            parse_text_push_next(arena, curr, (Text) { 
                .data = next, 
                .type = STRUCK
            });
        } else if (find_section(&next, &text, strl("|"), strl("|"))) { /* Table */
            parse_text_push_next(arena, curr, (Text) { 
                .text = next, 
                .type = TABLE_CELL
            });
        } else if (find_section(&next, &text, strl("?("), strl(")"))) { /* Explain */
            s64 loc = str_char_location(next, ',');
            ASSERT(loc >= 0);
            parse_text_push_next(arena, curr, (Text) { 
                .data = str_first(next, loc),
                .text = str_skip(next, loc+1), 
                .type = LINK
            });
        } else {
            curr = curr->next;
            if (!curr) {
                break;
            }
            text = curr->text;
        }
    }

    return out;
}
