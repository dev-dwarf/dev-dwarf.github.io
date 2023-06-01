struct Page {
    Page *next; 
    str filename;
    str base_href;
    StrList base_dir;
    u64 created_time;
    u64 modified_time;
    str title;
    str date;
    str desc;
    str content;
};

struct PageList {
    Page *first;
    Page *last;
    s64 count;
};
