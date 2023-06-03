#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "site.h"
#include "md_to_html.cpp"
#include <stdio.h>

#define MAX_FILEPATH 512

global StrList dir = {};
global StrNode root = {};
global StrNode src = {0, strl("src")};
global StrNode deploy = {0, strl("deploy")};
global StrNode wildcard = {0, strl("*.md")};
global StrNode filename = {};
global PageList allPages = {};

str HEADER = strl(
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

str FOOTER = strl(R"(
    </main> 
    </div>
    </body>
    <div>
    <hr>
    <nav>
    <table class="w33 left"><tr>
    <td><a href="/index.html">home</a></td>
    <td><a href="/projects.html">projects</a></td>
    <td><a href="/writing.html">writing</a></td>
    <td><a href='./rss.xml'>rss</a></td>
    </tr></table>

    <table class="w33 right"><tr>
    <td><a href="https://github.com/dev-dwarf">github</a></td>
    <td><a href="https://twitter.com/dev_dwarf">twitter</a></td>
    <td><a href="https://dev-dwarf.itch.io">games</a></td>
    <td class="light"><a class="light" onClick='toggleNight()'>light</a></td>
    <td class="night"><a class="night" onClick='toggleNight()'>night</a></td> 
    </tr></table>
    <p><br><br><br></p>
    </nav>
    <script>
    
    function toggleNight() {
        document.body.classList.toggle("night");
        const val = document.body.classList.contains("night");
        localStorage.setItem('theme', val);
        console.log(val);
    }

window.onload = function() {
    const saved = localStorage.getItem('theme');
    if (saved === 'true') {
        document.body.classList.add('night');
    }
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

void switch_to_dir(StrNode *new_folder_node) {
    StrNode *cur_folder_node = dir.first->next;
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

str build_dir(Arena *arena) {
    return StrList_join(arena, dir, {strl(""), strl("\\"), strl("\0")});
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
        StrList_add_node(&dir, &wildcard);
        str search = build_dir(arena);
        find = FindFirstFile(search.str, &ffd);
        ASSERT(find != INVALID_HANDLE_VALUE);
        StrList_pop_node(&dir);
    }

    str base_href;
    StrList base_dir;
    {
        StrList href = dir;
        StrList_skip(&href, 2);
        if (href.total_len == 0) {
            base_href = strl("/");
            base_dir = {0};
        } else {
            base_href = StrList_join(arena, href, {strl("/"), strl("/"), strl("/")});
            base_dir = StrList_copy(arena, href);
        }
    }
    
    do {
        Page *next = Arena_take_struct_zero(arena, Page);
        next->filename = str_copy_cstring(arena, ffd.cFileName);
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

void render_special_block(Arena *longa, Arena *tempa, Page *page, StrList* front, StrList* back, Block* blocks, Block* block) {
    (void) blocks; /* NOTE(lcf): Unused for now. */
    
    if (str_eq(block->id, strl("sections"))) {
        StrList_add(tempa, front, strl("<ol class='sections'>\n"));
        for (Block* b = block; b->type != Block::NIL; b = b->next) {
            if (b->type == Block::HEADING && str_not_empty(b->id)) {
                StrList_addv(tempa, front, strl("<li><a href='#"), b->id, strl("'>"));
                StrList_append(front, render_text(tempa, b->text));
                StrList_add(tempa, front, strl("</a></li>\n"));
            }
            if (b->type == Block::EXPAND && str_not_empty(b->id)) {
                StrList_addv(tempa, front, strl("<li><a href='#"),
                            b->id,
                            strl("'>"),
                            b->title,
                            strl("</a></li>\n"));
            }
        }
        StrList_add(tempa, front, strl("</ol>\n"));
    }
    if (str_eq(block->id, strl("title"))) {
        page->title = str_copy(longa, block->content.first->str);
        page->date = str_copy(longa, block->content.last->str);
        StrList_addv(tempa, front, strl("<div style='clear: both'><h1>"),
                    page->title,
                    strl("</h1><h3>"),
                    page->date,
                    strl("</h3></div>\n"));
    }
    if (str_eq(block->id, strl("desc"))) {
        page->desc = StrList_join(longa, block->content, {{}, strl(" "), {}});
    }
    if (str_eq(block->id, strl("article"))) {
        str link_ref = StrList_join(tempa, page->base_dir, {strl("#"), strl("/  "), {}});
        StrList_addv(tempa, back,
                     strl("<hr><p class='centert'> Feel free to message me with any comments about this article! Contact info on <a href='/index.html'>home page.</a></p>"),
                     strl("<a class='btn' href='"),
                     strl("/writing.html"),
                     link_ref,
                     strl("'>← back</a>"));
    }
    if (str_eq(block->id, strl("index"))) {
        str base_href = block->content.first->str;
        StrList_add(tempa, front, strl("<table>"));
        StrList_add(tempa, front, strl("<tr><td>Date</td><td>Title</td><td></td></tr>"));
        for (Page *p = allPages.first; p != 0; p = p->next) {
            if (str_eq(p->base_href, base_href)) {
                StrList_addv(tempa, front, strl("<tr><td>"),
                             p->date,
                             strl("</td><td>"),
                             strl("<a href='"),
                             p->base_href,
                             str_cut(p->filename,2),
                             strl("html'>"),
                             p->title,
                             strl("</a>"),
                             strl("</td><td><a class='centered btn' href='"),
                             p->base_href,
                             str_cut(p->filename,2),
                             strl("html'>"),
                             strl("Read →</a></td></tr>"));
            }
        }
        StrList_add(tempa, front, strl("</table>"));
    }
    if (str_eq(block->id, strl("project"))) {
        StrNode *param = block->content.first;
        str title = param->str; param = param->next;
        str date = param->str; param = param->next;
        str link = param->str; param = param->next;
        str img = param->str;
        StrList_addv(tempa, front, strl("<h2><a href="),
                     link,
                     strl(">"),
                     title,
                     strl("</a>"),
                     strl(" ("),
                     date,
                     strl(")</h2>"),
                     strl("<div class='project'><div class='project-image'><a href='"),
                     link,
                     strl("'><img src='"),
                     img,
                     strl("'></a></div><div class='project-text'>"));
    }
    if (str_eq(block->id, strl("project-end"))) {
        StrList_add(tempa, front, strl("</div></div>"));
    }
}

void compile_page(Arena *longa, Arena *tempa, Page *page) {
    StrList_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    StrList_add_node(&dir, &filename);
    switch_to_dir(&src);
    page->content = os_LoadEntireFile(tempa, build_dir(tempa));
    page->title = str_cut(page->filename, 3);

    StrList_pop_node(&dir);

    filename.str = str_concat(tempa, str_cut(page->filename, 2), strl("html\0"));
    StrList_add_node(&dir, &filename);

    StrList html = {0};
    StrList back = {0};

    Block* blocks = parse(tempa, page->content);
    StrList md = {0};
    for (Block* b = blocks; b->type != Block::NIL; b = b->next) {
        if (b->type != Block::SPECIAL) {
            md = render_block(tempa, b);
            StrList_append(&html, md);
        } else {
            render_special_block(longa, tempa, page, &html, &back, blocks, b);
        }
    }

    /* Header and Footer */
    StrList head = {0};
    StrList_addv(tempa, &head, HEADER, strl("\t<title>LCF/DD:"), page->title, strl("</title>\n"));
    StrList_add(tempa, &back, FOOTER);
    StrList_prepend(&html, head);
    StrList_append(&html, back);

    printf("%.*s \"%.*s\" ", str_PRINTF_ARGS(filename.str), str_PRINTF_ARGS(page->title));

    switch_to_dir(&deploy);
    ASSERT(os_WriteFile(build_dir(tempa), html));

    printf("> %.*s%.*s\n", str_PRINTF_ARGS(page->base_href), str_PRINTF_ARGS(page->filename));

    page->content = str_EMPTY;
    StrList_pop_node(&dir);
    StrList_pop(&dir, page->base_dir.count);
}

global str RSS_HEADER = strl(R"(<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>Logan Forman</title>
    <link>http://loganforman.com/</link>
    <atom:link href="http://loganforman.com/rss.xml" rel="self" type="application/rss+xml" />
    <description>Journey to the competence.</description>
)");
global str RSS_FOOTER = strl(R"(
  </channel>
</rss>
)");
void compile_feeds(Arena *arena, PageList pages) {
    printf("RSS Feed:\n");
    StrList rss = {0};
    StrList_add(arena, &rss, RSS_HEADER);
    Page *n = pages.first;
    for (s64 i = 0; i < pages.count; i++, n = n->next) {
        printf("\t %.*s\n", str_PRINTF_ARGS(n->title));
        StrList_addv(arena, &rss, strl("<item>\n<title>"),
                     n->title,
                     strl("</title>\n<description>"),
                     n->desc,
                     strl("</description>\n<link>https://loganforman.com/"),
                     n->base_href,
                     str_cut(n->filename, 2),
                     strl("html</link><guid isPermaLink='true'>https://loganforman.com/"),
                     n->base_href,
                     str_cut(n->filename, 2),
                     strl("</guid>\n"),
                     strl("</item>"));
        /* TODO: date for RSS feed items, from created_time */
    }
    StrList_add(arena, &rss, RSS_FOOTER);
    switch_to_dir(&deploy);
    
    StrList_add(arena, &dir, strl("rss.xml"));
    ASSERT(os_WriteFile(build_dir(arena), rss));
    StrList_pop_node(&dir);
    
    StrList_add(arena, &dir, strl("feed.xml"));
    ASSERT(os_WriteFile(build_dir(arena), rss));
    StrList_pop_node(&dir);

    Arena_reset_all(arena);
}

int main() {
    Arena *longa = Arena_create();
    Arena *tempa = Arena_create();

    StrNode technical = {0, strl("technical")};
    dir = {};
    ch8 root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    StrList_add_node(&dir, &root);
    StrList_add_node(&dir, &src);
    
    PageList topPages = get_pages_in_dir(longa);
    StrList_add_node(&dir, &technical);
    PageList technicalPages = get_pages_in_dir(longa);
    StrList_pop_node(&dir);
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

