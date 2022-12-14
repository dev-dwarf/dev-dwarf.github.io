#include "../lcf/lcf.h"

struct Text {
    Text *next;
    
    enum { /* Most nodes can contain any (inner) structure. Some need more: */
        NIL = 0,
        
        BOLD,
        ITALIC,
        STRUCK,
        CODE_INLINE,
        CODE_BLOCK,
        LINK,
        IMAGE,
        BREAK,
        TEXT,
    } type;
    b32 end;
    
    str8 text;
};

struct Block {
    /* Navigation */
    Block *next;
    Block *child;

    enum { /* Most nodes can contain any (inner) structure of text nodes. Some need more: */
        NIL = 0,

        HEADING, /* (num, inner, id) */
        QUOTE,
        ORD_LIST, /* (child[(num, inner), ...]) */
        UN_LIST, /* (child[(inner), ...]) */
        LIST_ITEM, /* (num, inner) */
        CODE, /* (text) */
        RULE, /* () */
        PARAGRAPH,
    } type;

    /* Node Contents */
    u32 num;
    str8 id;
    Text* text;
};

