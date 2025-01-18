#include "../../lcf/lcf.h"

enum TextTypes { 
    NIL = 0,
    TEXT,
    BOLD, ITALIC, STRUCK, CODE_INLINE, 
    LINK, IMAGE, EXPLAIN,
    TABLE_CELL,
    LIST_ITEM, CODE_BLOCK,
    BREAK,
};

typedef struct Text {
    str data;
    str text;
    enum TextTypes type;
    struct Text *child;
    struct Text *last_child;
    struct Text *next;
    s32 end;
} Text;

enum BlockTypes { 
    PARAGRAPH,
    HEADING, RULE, CODE,
    TABLE_ROW, TABLE_END,
    QUOTE, ORD_LIST, UN_LIST,
    EXPAND,
    SPECIAL, /* Let caller deal with these */
};

typedef struct Block {
    struct Block *next;
    enum BlockTypes type;
    u32 num;
    str id;
    str title;
    StrList content;
    Text* text;
} Block;

