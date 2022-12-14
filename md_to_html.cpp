#include "../lcf/lcf.h"
#include "../lcf/lcf.c"

#include "md_to_html.h"

/* Take list of Text nodes with undecided type, parse and return typed list */
Text* parse_text(Arena* arena, Text* text) {
    Text *root = text;
    b32 ignore_next = false;
    Text *curr = root;
    Text pre_filler = {curr, Text::NIL, 0};
    Text *pre = &pre_filler;
    b32 inside = 0;

    u32 sz1 = sizeof(Text);

#define PUSH_TEXT(TYPE, END, SKIP) {                                    \
        if (END == 0 &&                                                 \
            (curr->type == Text::TEXT || curr->type == Text::NIL)) {    \
            curr->type = TYPE;                                          \
            curr->text = str8_skip(curr->text, (SKIP));                 \
            curr = pre; /* overwrite current node */                    \
        } else {                                                        \
            Text* temp = curr->next;                                    \
            curr->next = Arena_take_struct_zero(arena, Text);           \
            curr->next->type = TYPE;                                    \
            curr->next->text = str8_skip(curr->text, (END) + (SKIP));   \
            curr->next->next = temp;                                    \
            curr->text = str8_first(curr->text, (END));                 \
        }                                                               \
        if (TEST_FLAG(inside, TO_FLAG(TYPE))) {                         \
            curr->next->end = true;                                     \
        }                                                               \
        TOGGLE_FLAG(inside, TO_FLAG(TYPE));                             \
        if (TYPE != Text::TEXT) {                                       \
            REM_FLAG(inside, TO_FLAG(Text::TEXT));                      \
        }                                                               \
    }

    for (; curr->text.len > 0; pre = curr, curr = curr->next) {
        str8 s = curr->text;
        str8_iter(s) {
            if (ignore_next) {
                ignore_next = false;
                continue;
            }
            else if (s.str[i] == '*' && s.len > i+1 && s.str[i+1] == '*') {
                /* bold */
                PUSH_TEXT(Text::BOLD, i, 2);
                break;
            }
            else if (s.str[i] == '*') {
                /* italic */
                PUSH_TEXT(Text::ITALIC, i, 1);
                break;
            }
            else if (s.str[i] == '~') {
                /* strike */
                PUSH_TEXT(Text::STRUCK, i, 1);
                break;
            }
            else if (s.str[i] == '`') {
                /* code */
                PUSH_TEXT(Text::CODE_INLINE, i, 1);
                break;
            }
            else if (s.str[i] == '@' && i+1 < s.len && s.str[i+1] == '(') {
                /* link */
                PUSH_TEXT(Text::LINK, i, 2);
                curr = curr->next;
                u64 next_space = str8_char_location(curr->text, ' ');
                ASSERT(next_space != LCF_STRING_NO_MATCH);
                PUSH_TEXT(Text::TEXT, next_space, 1);
                break;
            } else if (s.str[i] == '!' && i+1 < s.len && s.str[i+1] == '(') {
                /* TODO handle image url and such */
            }
            else if (s.str[i] == ')') {
                if (TEST_FLAG(inside, TO_FLAG(Text::LINK))) {
                    PUSH_TEXT(Text::LINK, i, 1);
                }
            }
            else if (s.str[i] == '\\') {
                ignore_next = true;
            }
        } /* end str8_iter */
        if (s.len == 0) {
            if (curr->next) {
                curr->type = Text::BREAK;
            } else {
                curr->type = Text::NIL;
            }
        } else if (curr->type == Text::NIL) {
            curr->type = Text::TEXT;
        }
    }

    if (root && root->type == Text::NIL) {
        if (root->text.len > 0) {
            root->type = Text::TEXT;
        } else {
            root = root->next;
        }
    }

    return root;
}

/* Take str8, return tree of blocks representing markdown structure */
Block* parse(Arena *arena, str8 str) {
    Block* root = Arena_take_struct_zero(arena, Block);
    Block* curr = root;
    Block next = {0};
    next.text = Arena_take_struct_zero(arena, Text);
    Text* end = next.text;
        
    u64 next_len = 0;
    b32 code_lock = false;

#define PUSH_STR(str) { \
    end->text = str; \
    end->next = Arena_take_struct_zero(arena, Text);    \
    end = end->next;                                    \
}

#define PUSH_BLOCK() if (next.type != Block::NIL) {     \
    *curr = next;                                       \
    curr->next = Arena_take_struct_zero(arena, Block);  \
    curr = curr->next;                                  \
    next = {0};                                         \
    next.text = Arena_take_struct_zero(arena, Text);    \
    end = next.text;                                    \
}

#define BREAK_BLOCK_IF_NOT(TYPE) {                      \
    if (next.type != TYPE) {                            \
        PUSH_BLOCK();                                   \
    }}
    
    str8_iter_pop_line(str) {
        if (line.len == 0) {
            if (next.type == Block::CODE) {
                PUSH_STR(line);
            } else {
                BREAK_BLOCK_IF_NOT(Block::CODE);
            }
            continue;
        }
        chr8 c = line.str[0];
        if (c == '`' && line.len > 2 && line.str[1] == '`' && line.str[2] == '`') {
            if (next.type != Block::CODE) {
                PUSH_BLOCK();
                next.type = Block::CODE;
                code_lock = true;
            } else {
                PUSH_BLOCK();
                code_lock = false;
            }
        } else if (code_lock) {
            PUSH_STR(line);
        } else if (c == '#') {
            BREAK_BLOCK_IF_NOT(Block::HEADING);
            
            /* heading */
            u64 n = 1;
            for (; n < line.len && line.str[n] == '#'; n++)
                ;
            next.type = Block::HEADING;
            next.num = n;
            str8 rem = str8_skip(line, n);
            next.id = rem;
            next.id.len = str8_char_location(rem, ' ');
            
            PUSH_STR(str8_skip(rem, next.id.len+1));
            PUSH_BLOCK();
        }
        else if (c == '>' && line.str[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::QUOTE);
            next.type = Block::QUOTE;
            PUSH_STR(str8_skip(line, 2));
        }
        else if (c >= '1' && c <= '9' && line.len > 2 && line.str[1] == '.' && line.str[2] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::ORD_LIST);
            /* TODO: lists */
        }
        else if (c == '-' && line.str[1] == ' ') {
            BREAK_BLOCK_IF_NOT(Block::UN_LIST);
            /* TODO: lists */
        }
        else if (c == '-' && line.len > 2 && line.str[1] == '-' && line.str[2] == '-') {
            PUSH_BLOCK();
            /* rule */
            next.type = Block::RULE;
            PUSH_BLOCK();
        } else {
            /* add text */
            if (next.type != Block::PARAGRAPH && next.type != Block::CODE) {
                PUSH_BLOCK();
                next.type = Block::PARAGRAPH;
            }
            PUSH_STR(line);
        }
    }
    PUSH_BLOCK();

    for (curr = root; curr->type != Block::NIL; curr = curr->next) {
        if (curr->type == Block::CODE) {
            for (Text* t = curr->text; t->text.len > 0; t = t->next) {
                t->type = Text::CODE_BLOCK;
            }
        } else {
            curr->text = parse_text(arena, curr->text);
        }
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
            str8 s[2] = {
                str8_lit("<b>"),
                str8_lit("</b>")
            };
            Str8List_add(arena, &out, s[t->end]);
            Str8List_add(arena, &out, t->text);
        } break;
        case Text::ITALIC: {
            str8 s[2] = {
                str8_lit("<em>"),
                str8_lit("</em>")
            };
            Str8List_add(arena, &out, s[t->end]);
            Str8List_add(arena, &out, t->text);
        } break;
        case Text::STRUCK: {
            str8 s[2] = {
                str8_lit("<s>"),
                str8_lit("</s>")
            };
            Str8List_add(arena, &out, s[t->end]);
            Str8List_add(arena, &out, t->text);
        } break;
        case Text::CODE_INLINE: {
            str8 s[2] = {
                str8_lit("<code>"),
                str8_lit("</code>")
            };
            Str8List_add(arena, &out, s[t->end]);
            Str8List_add(arena, &out, t->text);
        } break;
        case Text::CODE_BLOCK: {
            Str8List_add(arena, &out, t->text);
            Str8List_add(arena, &out, str8_NEWLINE);
        } break;
        case Text::LINK: {
            if (t->end) {
                Str8List_add(arena, &out, str8_lit("</a>"));
                Str8List_add(arena, &out, t->text);
            } else {
                Str8List_add(arena, &out, str8_lit("<a href='"));
                Str8List_add(arena, &out, t->text);
                Str8List_add(arena, &out, str8_lit("'>"));
            }
        } break;
        case Text::IMAGE: {
            NOTIMPLEMENTED();
        } break;
        case Text::BREAK: {
            Str8List_add(arena, &out, str8_lit("<br>\n"));
        } break;
        case Text::TEXT: {
            Str8List_add(arena, &out, t->text);
            if (prev->type == Text::TEXT) {
                Str8List_add(arena, &out, str8_NEWLINE);
            } else {
                Str8List_add(arena, &out, str8_lit(" "));
            }
        } break;
        }
    }
    return out;
}

/* take fully parsed markdown and render as html tags, returning a string */
str8 render(Arena* arena, Block* root) {
    Str8List out = {0};
    for (Block* b = root; b->type != Block::NIL; b = b->next) {
        /* TODO: add to out by going through blocks.
           might need a render_text proc as well.
           should be able to just put some html tags, and then also paste in whatever
           text has already been parsed. */

        switch (b->type) {
        case Block::HEADING: {
            str8 h = str8_create_size(arena, 2);
            h.str[0] = 'h';
            h.str[1] = '0' + b->num;
            Str8List_add(arena, &out, str8_lit("<"));
            Str8List_add(arena, &out, h);
            if (b->id.len > 0) {
                if (str8_eq(b->id, str8_lit("center"))) {
                    Str8List_add(arena, &out, str8_lit(" style='text-align:center'"));
                } else if (str8_eq(b->id, str8_lit("right"))) {
                    Str8List_add(arena, &out, str8_lit(" style='text-align:right'"));
                } else {
                    Str8List_add(arena, &out, str8_lit(" id='"));
                    Str8List_add(arena, &out, b->id);
                    Str8List_add(arena, &out, str8_lit("'"));
                }
            }
            Str8List_add(arena, &out, str8_lit(">"));
            
            Str8List_append(&out, render_text(arena, b->text));
            Str8List_add(arena, &out, str8_lit("</"));
            Str8List_add(arena, &out, h);
            Str8List_add(arena, &out, str8_lit(">"));
        } break;
        case Block::QUOTE: {
            Str8List_add(arena, &out, str8_lit("<blockquote>\n"));
            Str8List_append(&out, render_text(arena, b->text));
            Str8List_add(arena, &out, str8_lit("</blockquote>\n"));
        } break;
        case Block::ORD_LIST: { NOTIMPLEMENTED() } break;
        case Block::UN_LIST: { NOTIMPLEMENTED() } break;
        case Block::LIST_ITEM: { NOTIMPLEMENTED() } break;
        case Block::CODE:  {
            Str8List_add(arena, &out, str8_lit("<pre><code>"));
            Str8List_append(&out, render_text(arena, b->text));            
            Str8List_add(arena, &out, str8_lit("</code></pre>\n"));
        } break;
        case Block::RULE:  {
            Str8List_add(arena, &out, str8_lit("<hr>\n"));
        } break;
        case Block::PARAGRAPH: {
            Str8List_add(arena, &out, str8_lit("<p>\n"));
            Str8List_append(&out, render_text(arena, b->text));            
            Str8List_add(arena, &out, str8_lit("</p>\n"));
        } break;  
        default:
            NOTIMPLEMENTED();
            break;
        }
    }
    return Str8List_join(arena, out, str8_lit(""), str8_lit(""), str8_lit(""));
}

int main() {
    Arena *a = Arena_create_default();

    str8 md = str8_lit(
        "###hello-world **Hello World**\n"
        R"(# Nec regina dixi claustraque fragore naturaeque

##center Numerum custodia nunc

Lorem markdownum et ultra Harpalos fuit, est et blanda, enixa. Cara sive
liquidas, facta causa ut *nescia* pontus, in? Meos funeribus? Cycno est coniuge 
**mihi palmas festum** in nec.

## Lapitheia missi populante et caede tale illo

Propiore blandarum litore: occupat malo adunci captantia, domos, hostem neve
amat, est quid tempora sacra. ~Altera enim, lacrimis floresque~ nostra fugiunt ore
alios deque abiit suo: Cytoriaco submovit Arethusa caecisque. Quae freta colatur
quemque iniqui, pallentemque tantum fuga tinguitur placet gravis, cum inminet
meo troades? Una Achille? Paene egit non ramis victima etiamnum ponite aper
fuere postquam mihi malo candore amoris: posse dabit facientia coniuge palmae.

```
b32 str8_has_prefix(str8 s, str8 prefix);
b32 str8_has_postfix(str8 s, str8 postfix);
b32 chr8_is_whitespace(chr8 c);
b32 str8_contains_char(str8 s, chr8 c);
b32 str8_contains_substring(str8 s, str8 sub);
b32 str8_contains_delimiter(str8 s, str8 delims);
#define LCF_STRING_NO_MATCH 0x8000000000000000
u64 str8_char_location(str8 s, chr8 c);
u64 str8_substring_location(str8 s, str8 sub);
```

##right Est fata Cinyran

Vidit inachus, `cultros` et novem ad adsuetudine credas parentis mansit
Mithridateisque annisque is pluma. Fulva famem enim Helopsque cornua fecit,
redditus ire; adgrediar habet terribili dixit et humum.
@(#hello-world ***back to top***) enjoy :)\n

)"
        );

    Block* parsed = parse(a, md);
    str8 html = render(a, parsed);
}

