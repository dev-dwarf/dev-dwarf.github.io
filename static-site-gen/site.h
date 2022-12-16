struct Page {
    Page *next; 
    str8 filename;
    str8 base_href;
    Str8List base_dir;
    u64 created_time;
    u64 modified_time;
    str8 title;
    str8 content;
    enum Types {
        DEFAULT,
        ARTICLE,
        INDEX
    } type;
};

struct PageList {
    Page *first;
    Page *last;
    u64 count;
};
