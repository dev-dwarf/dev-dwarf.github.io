#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "site.h"
#include "md_to_html.cpp"
#include <stdio.h>

#define MAX_FILEPATH 512

global Str8List dir = {};
global Str8Node root = Str8Node({});
global Str8Node src = Str8Node(str8("src"));
global Str8Node deploy = Str8Node(str8("deploy"));
global Str8Node wildcard = Str8Node(str8("*.md"));
global Str8Node filename = Str8Node({});
global PageList allPages = {};

str8 HEADER = str8(
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

str8 FOOTER = str8(R"(
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
    return dir.join(arena, {str8(""), str8("\\"), str8("\0")});
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
        dir.add_node(&wildcard);
        str8 search = build_dir(arena);
        find = FindFirstFile(search.str, &ffd);
        ASSERT(find != INVALID_HANDLE_VALUE);
        dir.pop_node();
    }

    str8 base_href;
    Str8List base_dir;
    {
        Str8List href = dir;
        href.skip(2);
        if (href.total_len == 0) {
            base_href = str8("/");
            base_dir = {0};
        } else {
            base_href = href.join(arena, {str8("/"), str8("/"), str8("/")});
            base_dir = href.copy(arena);
        }
    }
    
    do {
        Page *next = arena->take_struct_zero<Page>();
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
    
    if (str8_eq(block->id, str8("sections"))) {
        front->add(tempa, str8("<ol class='sections'>\n"));
        for (Block* b = block; b->type != Block::NIL; b = b->next) {
            if (b->type == Block::HEADING && str8_not_empty(b->id)) {
                front->add(tempa, str8("<li><a href='#"));
                front->add(tempa, b->id);
                front->add(tempa, str8("'>"));
                front->append(render_text(tempa, b->text));
                front->add(tempa, str8("</a></li>\n"));
            }
            if (b->type == Block::EXPAND && str8_not_empty(b->id)) {
                front->add(tempa, str8("<li><a href='#"));
                front->add(tempa, b->id);
                front->add(tempa, str8("'>"));
                front->add(tempa, b->title);
                front->add(tempa, str8("</a></li>\n"));
            }
        }
        front->add(tempa, str8("</ol>\n"));
    }
    if (str8_eq(block->id, str8("title"))) {
        page->title = Str8List_join(longa, block->content, {{}, str8(" ("), str8(")")});
        front->add(tempa, str8("<div style='clear: both'><h1>"));
        front->add(tempa, block->content.first->str);
        front->add(tempa, str8("</h1><h3>"));
        front->add(tempa, block->content.last->str);
        front->add(tempa, str8("</h3></div>\n"));
    }
    if (str8_eq(block->id, str8("desc"))) {
        page->desc = Str8List_join(longa, block->content, {{}, str8(" "), {}});
    }
    if (str8_eq(block->id, str8("article"))) {
        str8 link_ref = page->base_dir.join(tempa, {str8("#"), str8("/  "), {}});
        back->add(tempa, str8("<br><a href='"));
        /* NOTE(lcf): could change based on type of article */
        back->add(tempa, str8("/writing.html"));
        back->add(tempa, link_ref);
        back->add(tempa, str8("'>back</a>"));
    }
    if (str8_eq(block->id, str8("index"))) {
        str8 base_href = block->content.first->str;
        front->add(tempa, str8("<ul>"));
        for (Page *p = allPages.first; p != 0; p = p->next) {
            if (str8_eq(p->base_href, base_href)) {
                front->add(tempa, str8("<li><a href='"));
                front->add(tempa, p->base_href);
                front->add(tempa, str8_cut(p->filename,2));
                front->add(tempa, str8("html'>"));
                front->add(tempa, p->title);
                front->add(tempa, str8("</a></li>"));
            }
        }
        front->add(tempa, str8("</ul>"));
    }
    if (str8_eq(block->id, str8("project"))) {
        Str8Node *param = block->content.first;
        str8 title = param->str; param = param->next;
        str8 date = param->str; param = param->next;
        str8 link = param->str; param = param->next;
        str8 img = param->str;
        front->add(tempa, str8("<h2><a href="));
        front->add(tempa, link);
        front->add(tempa, str8(">"));
        front->add(tempa, title);
        front->add(tempa, str8("</a>"));
        front->add(tempa, str8(" ("));
        front->add(tempa, date);
        front->add(tempa, str8(")</h2>"));
        front->add(tempa, str8("<div class='project'><div class='project-image'><a href='"));
        front->add(tempa, link); 
        front->add(tempa, str8("'><img src='"));
        front->add(tempa, img);
        front->add(tempa, str8("'></a></div><div class='project-text'>"));
    }
    if (str8_eq(block->id, str8("project-end"))) {
        front->add(tempa, str8("</div></div>"));
    }
}

void compile_page(Arena *longa, Arena *tempa, Page *page) {
    Str8List_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    dir.add_node(&filename);
    switch_to_dir(&src);
    page->content = os_LoadEntireFile(tempa, build_dir(tempa));
    page->title = str8_cut(page->filename, 3);
        
    dir.pop_node();

    filename.str = str8_concat(tempa, str8_cut(page->filename, 2), str8("html\0"));
    dir.add_node(&filename);

    Str8List html = {0};
    Str8List back = {0};

    Block* blocks = parse(tempa, page->content);
    Str8List md = {0};
    for (Block* b = blocks; b->type != Block::NIL; b = b->next) {
        if (b->type != Block::SPECIAL) {
            md = render_block(tempa, b);
            html.append(md);
        } else {
            render_special_block(longa, tempa, page, &html, &back, blocks, b);
        }
    }

    /* Header and Footer */
    Str8List head = {0};
    head.add(tempa, HEADER);
    head.add(tempa, str8("\t<title>LCF/DD:"));
    head.add(tempa, page->title);
    head.add(tempa, str8("</title>\n"));
    back.add(tempa, FOOTER);
    html.prepend(head);
    html.append(back);

    printf("%.*s \"%.*s\" ", str8_PRINTF_ARGS(filename.str), str8_PRINTF_ARGS(page->title));

    switch_to_dir(&deploy);
    os_WriteFile(build_dir(tempa), html);

    printf("> %.*s%.*s\n", str8_PRINTF_ARGS(page->base_href), str8_PRINTF_ARGS(page->filename));

    page->content = str8_EMPTY;
    dir.pop_node();
    dir.pop(page->base_dir.count);
}

global str8 RSS_HEADER = str8(R"(<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>Logan Forman</title>
    <link>http://loganforman.com/</link>
    <atom:link href="http://loganforman.com/rss.xml" rel="self" type="application/rss+xml" />
    <description>Journey to the competence.</description>
)");
global str8 RSS_FOOTER = str8(R"(
  </channel>
</rss>
)");
void compile_feeds(Arena *arena, PageList pages) {
    printf("RSS Feed:\n");
    Str8List rss = {0};
    rss.add(arena, RSS_HEADER);
    Page *n = pages.first;
    for (s64 i = 0; i < pages.count; i++, n = n->next) {
        printf("\t %.*s\n", str8_PRINTF_ARGS(n->title));
        rss.add(arena, str8("<item>\n<title>"));
        rss.add(arena, n->title);
        rss.add(arena, str8("</title>\n<description>"));
        rss.add(arena, n->desc);
        rss.add(arena, str8("</description>\n<link>https://loganforman.com/"));
        rss.add(arena, n->base_href);
        rss.add(arena, str8_cut(n->filename, 2));
        rss.add(arena, str8("html</link><guid isPermaLink='true'>https://loganforman.com/"));
        rss.add(arena, n->base_href);
        rss.add(arena, str8_cut(n->filename, 2));
        rss.add(arena, str8("</guid>\n"));
        /* .DO: date for RSS feed items, from created_time */
        rss.add(arena, str8("</item>"));
    }
    rss.add(arena, RSS_FOOTER);
    switch_to_dir(&deploy);
    dir.add(arena, str8("rss.xml"));
    os_WriteFile(build_dir(arena), rss);
    dir.pop_node();
    arena->reset();
}

int main() {
    Arena *longa = Arena::create();
    Arena *tempa = Arena::create();

    Str8Node technical = Str8Node(str8("technical"));
    dir = {};
    chr8 root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    dir.add_node(&root);

    dir.add_node(&src);
    
    /* TODO: loop this as a daemon, whenever any of the files change update them automatically
        REF: https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
        Would need UTF-16 to use appropriate APIs.. dont want to do that right now.
    */
    PageList topPages = get_pages_in_dir(longa);
    dir.add_node(&technical);
    PageList technicalPages = get_pages_in_dir(longa);
    dir.pop_node();
    allPages = technicalPages;
    allPages.count += topPages.count;
    allPages.last->next = topPages.first;
    allPages.last = topPages.last;
    
    for (Page *n = allPages.first; n != 0; n = n->next) {
        compile_page(longa, tempa, n);
        tempa->reset();
    }

    compile_feeds(tempa, technicalPages);
}

