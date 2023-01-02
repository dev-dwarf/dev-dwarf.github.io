#include "../../lcf/lcf.h"

struct Text {
    Text *next;
    
    enum Types { 
        NIL = 0,
        TEXT,
        BOLD, ITALIC, STRUCK, CODE_INLINE, 
        LINK, IMAGE, EXPLAIN,
        LIST_ITEM, CODE_BLOCK,
        BREAK,
    } type;
    b32 end;
    
    str8 text;
};

struct Block {
    /* Navigation */
    Block *next;

    enum { 
        NIL = 0,
        PARAGRAPH,
        HEADING, RULE, CODE, 
        QUOTE, ORD_LIST, UN_LIST,
        EXPAND,
        SPECIAL, /* Let caller deal with these */
    } type;

    /* Node Contents */
    u32 num;
    str8 id;
    str8 title;
    Str8List content;
    Text* text;
};

