#include "../lcf/lcf.h"

struct Text {
    Text *next;
    
    enum { 
        NIL = 0,
        
        BOLD,
        ITALIC,
        STRUCK,
        CODE_INLINE,
        CODE_BLOCK,
        LINK,
        IMAGE,
        BREAK,    
        LIST_ITEM,
        TEXT,
    } type;
    b32 end;
    
    str8 text;
};

struct Block {
    /* Navigation */
    Block *next;

    enum { 
        NIL = 0,

        HEADING, 
        QUOTE,
        ORD_LIST,
        UN_LIST,
        CODE, 
        RULE,
        /* TODO: something that provides additional info on
           hover using a <span title='additional info'>content</span> tag

           could look like ?('additional info' content)
        */
        PARAGRAPH,
    } type;

    /* Node Contents */
    u32 num;
    str8 id;
    Text* text;
};

