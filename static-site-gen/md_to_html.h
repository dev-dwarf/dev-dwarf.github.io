#include "../../lcf/lcf.h"

struct Text {
    Text *next;
    
    enum Types { 
        NIL = 0,
        TEXT,
        BOLD, ITALIC, STRUCK, CODE_INLINE, 
        LINK, IMAGE, EXPLAIN,
        TABLE_CELL,
        LIST_ITEM, CODE_BLOCK,
        BREAK,
    } type;
    b32 end;
    
    str text;
};

struct Block {
    Block *next;

    enum { 
        NIL = 0,
        PARAGRAPH,
        HEADING, RULE, CODE,
        TABLE_ROW, TABLE_END,
        QUOTE, ORD_LIST, UN_LIST,
        EXPAND,
        SPECIAL, /* Let caller deal with these */
    } type;

    u32 num;
    str id;
    str title;
    StrList content;
    Text* text;
};

