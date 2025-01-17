typedef struct Page {
    struct Page *next; 
    str filename;
    str base_href;
    StrList base_dir;
    u64 created_time;
    u64 modified_time;
    str title;
    str rss_day;
    str date;
    str desc;
    str content;
} Page;

typedef struct PageList {
    Page *first;
    Page *last;
    s64 count;
} PageList;
