#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "site.h"
#include "md_to_html.cpp"

#define MAX_FILEPATH 512

global Str8List dir = {0};
global Str8Node root = {0};
global Str8Node src = {0, str8_lit("src")};
global Str8Node deploy = {0, str8_lit("deploy")};
global Str8Node wildcard = {0, str8_lit("*.md")};
global Str8Node filename;
global PageList allPages = {0};

str8 HEADER = str8_lit(
R"(
<!DOCTYPE html>
<!-- GENERATED -->
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta property="og:title" content="Logan Forman / Dev-Dwarf" />
    <meta property="og:locale" content="en_US" />
    <meta property="og:image" content="/assets/dd.png" />
    <link rel="canonical" href="http://loganforman.com/" />
    <meta property="og:url" content="http://loganforman.com/"/>
    <meta property="og:site_name" content="Logan Forman / Dev-Dwarf" />
    <meta property="og:type" content="website" />
    <meta name="twitter:card" content="summary" />
    <meta property="twitter:title" content="Logan Forman" />
    <script type="application/ld+json">
      {"@context":"https://schema.org","@type":"WebSite","headline":"Logan Forman / Dev-Dwarf","name":"Logan Forman / Dev-Dwarf","url":"http://loganforman.com/"}</script>
    <link rel="stylesheet" href="/dwarf.css">
    <link rel="icon" type="image/x-icon" href="/assets/favicon.ico">
    </head>
    <body>
    <div class="wrapper">
    <main class="page-content" aria-label="Content">

)");

str8 FOOTER = str8_lit(R"(
    </main> 
    </div>
    </body>
    <div class="nav">
    <hr>
    <div class="nav-left">
    <nav id="nav-links">
    <a class="nav-link-l" href="/index.html">home</a>
    <a class="nav-link-l" href="/projects.html">projects</a>
    <a class="nav-link-l" href="/writing.html">writing</a>
    <a class="nav-link-l" href="/contact.html">contact</a>
    </nav>
    </div>
    <div class="nav-right">
    <a class="nav-link-r" href="https://github.com/dev-dwarf">github</a>
    <a class="nav-link-r" href="https://twitter.com/dev_dwarf">twitter</a>
    <a class="nav-link-r" href="https://dev-dwarf.itch.io">games</a>
    </div>
    <script>
window.onload = function() { 
    full_path = location.href.split('#')[0];
    Array.from(document.getElementById("nav-links").getElementsByTagName("a"))
        .filter(l => l.href.split("#")[0] == full_path)
        .forEach(l => l.className += " current");
    Array.from(document.getElementsByTagName("code"))
        .filter(el => el.id == "html")  
        .forEach(el => el.innerText = el.innerHTML);
}    
    </script>
    </div>
    </html>)");

void switch_to_dir(Str8Node *new_folder_node) {
    Str8Node *cur_folder_node = dir.first->next;
    if (cur_folder_node != new_folder_node) {
        dir.total_len -= cur_folder_node->str.len;
        dir.total_len += new_folder_node->str.len;
        new_folder_node->next = cur_folder_node->next;
        cur_folder_node->next = 0;
        dir.first->next = new_folder_node;
        if (new_folder_node->next == 0) {
            dir.last = new_folder_node;
        }
    }
}

str8 build_dir(Arena *arena) {
    return Str8List_join(arena, dir, {str8_lit(""), str8_lit("\\"), str8_lit("\0")});
}

PageList get_pages_in_dir(Arena *arena) {
    PageList dirPages = {0};
    
    union {
        FILETIME ft;
        u64 u;
    } giveMeTheBits;

    HANDLE find = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd = {0};
    ARENA_SESSION(arena) { /* Compose search string */
        switch_to_dir(&src);
        Str8List_add_node(&dir, &wildcard);
        str8 search = build_dir(arena);
        find = FindFirstFile(search.str, &ffd);
        ASSERT(find != INVALID_HANDLE_VALUE);
        Str8List_pop_node(&dir);
    }

    str8 base_href;
    Str8List base_dir;
    {
        Str8List href = dir;
        Str8List_skip(&href, 2);
        if (href.total_len == 0) {
            base_href = str8_lit("/");
            base_dir = {0};
        } else {
            base_href = Str8List_join(arena, href, {str8_lit("/"), str8_lit("/"), str8_lit("/")});
            base_dir = Str8List_copy(arena, href);
        }
    }
    
    do {
        Page *next = Arena_take_struct_zero(arena, Page);
        next->filename = str8_copy_cstring(arena, ffd.cFileName);
        next->base_href = base_href;
        next->base_dir = base_dir;
        giveMeTheBits.ft = ffd.ftCreationTime;
        next->created_time = giveMeTheBits.u;
        giveMeTheBits.ft = ffd.ftLastWriteTime;
        next->modified_time = giveMeTheBits.u;

        Page *curr = dirPages.first, *pre = curr;
        if (curr == 0) {
            dirPages.first = next;
            dirPages.last = next;
        } else {
            for (; curr != 0; pre = curr, curr = curr->next) {
                if (next->created_time > curr->created_time) {
                    if (pre == curr) {
                        dirPages.first = next;
                    } else {
                        pre->next = next;
                    }
                    next->next = curr;
                    break;
                }
            }
            if (curr == 0) {
                ASSERT(pre == dirPages.last);
                pre->next = next;
                dirPages.last = next;
            }
        }
        dirPages.count++;
    } while(FindNextFile(find, &ffd) != 0);
    FindClose(find);
    
    return dirPages;
}

void render_special_block(Arena *longa, Arena *tempa, Page *page, Str8List* front, Str8List* back, Block* blocks, Block* block) {
    (void) blocks; /* NOTE(lcf): Unused for now. */
    
    if (str8_eq(block->id, str8_lit("sections"))) {
        Str8List_add(tempa, front, str8_lit("<ol class='sections'>\n"));
        for (Block* b = block; b->type != Block::NIL; b = b->next) {
            if (b->type == Block::HEADING && str8_not_empty(b->id)) {
                Str8List_add(tempa, front, str8_lit("<li><a href='#"));
                Str8List_add(tempa, front, b->id);
                Str8List_add(tempa, front, str8_lit("'>"));
                Str8List_append(front, render_text(tempa, b->text));
                Str8List_add(tempa, front, str8_lit("</a></li>\n"));
            }
            if (b->type == Block::EXPAND && str8_not_empty(b->id)) {
                Str8List_add(tempa, front, str8_lit("<li><a href='#"));
                Str8List_add(tempa, front, b->id);
                Str8List_add(tempa, front, str8_lit("'>"));
                Str8List_add(tempa, front, b->title);
                Str8List_add(tempa, front, str8_lit("</a></li>\n"));
            }
        }
        Str8List_add(tempa, front, str8_lit("</ol>\n"));
    }
    if (str8_eq(block->id, str8_lit("title"))) {
        page->title = Str8List_join(longa, block->content, {{0}, str8_lit(" ("), str8_lit(")")});
        Str8List_add(tempa, front, str8_lit("<div style='clear: both'><h1>"));
        Str8List_add(tempa, front, block->content.first->str);
        Str8List_add(tempa, front, str8_lit("</h1><h3>"));
        Str8List_add(tempa, front, block->content.last->str);
        Str8List_add(tempa, front, str8_lit("</h3></div>\n"));
    }
    if (str8_eq(block->id, str8_lit("desc"))) {
        page->desc = Str8List_join(longa, block->content, {{0}, str8_lit(" "), {0}});
    }
    if (str8_eq(block->id, str8_lit("article"))) {
        str8 link_ref = Str8List_join(tempa, page->base_dir, {str8_lit("#"), str8_lit("/  "), {0}});
        Str8List_add(tempa, back, str8_lit("<br><a href='"));
        /* NOTE(lcf): could change based on type of article */
        Str8List_add(tempa, back, str8_lit("/writing.html"));
        Str8List_add(tempa, back, link_ref);
        Str8List_add(tempa, back, str8_lit("'>back</a>"));
    }
    if (str8_eq(block->id, str8_lit("index"))) {
        str8 base_href = block->content.first->str;
        Str8List_add(tempa, front, str8_lit("<ul>"));
        for (Page *p = allPages.first; p != 0; p = p->next) {
            if (str8_eq(p->base_href, base_href)) {
                Str8List_add(tempa, front, str8_lit("<li><a href='"));
                Str8List_add(tempa, front, p->base_href);
                Str8List_add(tempa, front, str8_cut(p->filename,2));
                Str8List_add(tempa, front, str8_lit("html'>"));
                Str8List_add(tempa, front, p->title);
                Str8List_add(tempa, front, str8_lit("</a></li>"));
            }
        }
        Str8List_add(tempa, front, str8_lit("</ul>"));
    }
    if (str8_eq(block->id, str8_lit("project"))) {
        Str8Node *param = block->content.first;
        str8 title = param->str; param = param->next;
        str8 date = param->str; param = param->next;
        str8 link = param->str; param = param->next;
        str8 img = param->str;
        Str8List_add(tempa, front, str8_lit("<h2><a href="));
        Str8List_add(tempa, front, link);
        Str8List_add(tempa, front, str8_lit(">"));
        Str8List_add(tempa, front, title);
        Str8List_add(tempa, front, str8_lit("</a>"));
        Str8List_add(tempa, front, str8_lit(" ("));
        Str8List_add(tempa, front, date);
        Str8List_add(tempa, front, str8_lit(")</h2>"));
Str8List_add(tempa, front, str8_lit("<div class='project'><div class='project-image'><a href='"));
        Str8List_add(tempa, front, link); 
        Str8List_add(tempa, front, str8_lit("'><img src='"));
        Str8List_add(tempa, front, img);
        Str8List_add(tempa, front, str8_lit("'></a></div><div class='project-text'>"));
    }
    if (str8_eq(block->id, str8_lit("project-end"))) {
        Str8List_add(tempa, front, str8_lit("</div></div>"));
    }
}

void compile_page(Arena *longa, Arena *tempa, Page *page) {
    Str8List_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    Str8List_add_node(&dir, &filename);
    switch_to_dir(&src);
    page->content = win32_load_entire_file(tempa, build_dir(tempa));
    page->title = str8_cut(page->filename, 3);
        
    Str8List_pop_node(&dir);

    filename.str = str8_concat(tempa, str8_cut(page->filename, 2), str8_lit("html\0"));
    Str8List_add_node(&dir, &filename);

    Str8List html = {0};
    Str8List back = {0};

    Block* blocks = parse(tempa, page->content);
    Str8List md = {0};
    for (Block* b = blocks; b->type != Block::NIL; b = b->next) {
        if (b->type != Block::SPECIAL) {
            md = render_block(tempa, b);
            Str8List_append(&html, md);
        } else {
            render_special_block(longa, tempa, page, &html, &back, blocks, b);
        }
    }

    /* Header and Footer */
    Str8List head = {0};
    Str8List_add(tempa, &head, HEADER);
    Str8List_add(tempa, &head, str8_lit("\t<title>LCF/DD:"));
    Str8List_add(tempa, &head, page->title);
    Str8List_add(tempa, &head, str8_lit("</title>\n"));
    Str8List_add(tempa, &back, FOOTER);
    Str8List_prepend(&html, head);
    Str8List_append(&html, back);

    printf("%.*s \"%.*s\" ", str8_PRINTF_ARGS(filename.str), str8_PRINTF_ARGS(page->title));

    switch_to_dir(&deploy);
    win32_write_file(build_dir(tempa), html);

    printf("> %.*s%.*s\n", str8_PRINTF_ARGS(page->base_href), str8_PRINTF_ARGS(page->filename));

    page->content = str8_EMPTY;
    Str8List_pop_node(&dir);
    Str8List_pop(&dir, page->base_dir.count);    
}

global str8 RSS_HEADER = str8_lit(R"(<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>Logan Forman</title>
    <link>http://loganforman.com/</link>
    <atom:link href="http://loganforman.com/rss.xml" rel="self" type="application/rss+xml" />
    <description>Writings and such.</description>
)");
global str8 RSS_FOOTER = str8_lit(R"(
  </channel>
</rss>
)");
void compile_feeds(Arena *arena, PageList pages) {
    printf("RSS Feed:\n");
    Str8List rss = {0};
    Str8List_add(arena, &rss, RSS_HEADER);
    Page *n = pages.first;
    for (s64 i = 0; i < pages.count; i++, n = n->next) {
        printf("\t %.*s\n", str8_PRINTF_ARGS(n->title));
        Str8List_add(arena, &rss, str8_lit("<item>\n<title>"));
        Str8List_add(arena, &rss, n->title);
        Str8List_add(arena, &rss, str8_lit("</title>\n<description>"));
        Str8List_add(arena, &rss, n->desc);
        Str8List_add(arena, &rss, str8_lit("</description>\n<link>https://loganforman.com/"));
        Str8List_add(arena, &rss, n->base_href);
        Str8List_add(arena, &rss, str8_cut(n->filename, 2));
        Str8List_add(arena, &rss, str8_lit("html</link><guid isPermaLink='true'>https://loganforman.com/"));
        Str8List_add(arena, &rss, n->base_href);
        Str8List_add(arena, &rss, str8_cut(n->filename, 2));
        Str8List_add(arena, &rss, str8_lit("</guid>\n"));
        /* TODO: date for RSS feed items, from created_time */
        Str8List_add(arena, &rss, str8_lit("</item>"));
    }
    Str8List_add(arena, &rss, RSS_FOOTER);
    switch_to_dir(&deploy);
    Str8List_add(arena, &dir, str8_lit("rss.xml"));
    win32_write_file(build_dir(arena), rss);
    Str8List_pop_node(&dir);
    Arena_reset_all(arena);
}

int main() {
    Arena *longa = Arena_create_default();
    Arena *tempa = Arena_create_default();

    Str8Node technical = {0, str8_lit("technical")};
    dir = {0};
    chr8 root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    Str8List_add_node(&dir, &root);

    filename = {0};
    Str8List_add_node(&dir, &src);
    
    /* TODO: loop this as a daemon, whenever any of the files change update them automatically
        REF: https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
        Would need UTF-16 to use appropriate APIs.. dont want to do that right now.
    */
    PageList topPages = get_pages_in_dir(longa);
    Str8List_add_node(&dir, &technical);
    PageList technicalPages = get_pages_in_dir(longa);
    Str8List_pop_node(&dir);
    allPages = technicalPages;
    allPages.count += topPages.count;
    allPages.last->next = topPages.first;
    allPages.last = topPages.last;
    
    for (Page *n = allPages.first; n != 0; n = n->next) {
        compile_page(longa, tempa, n);
        Arena_reset_all(tempa);
    }

    compile_feeds(tempa, technicalPages);
}

