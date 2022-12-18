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
    <link rel="canonical" href="http://lcfd.dev/" />
    <meta property="og:url" content="http://lcfd.dev/"/>
    <meta property="og:site_name" content="Logan Forman / Dev-Dwarf" />
    <meta property="og:type" content="website" />
    <meta name="twitter:card" content="summary" />
    <meta property="twitter:title" content="Stuffed Wombat" />
    <script type="application/ld+json">
      {"@context":"https://schema.org","@type":"WebSite","headline":"Logan Forman / Dev-Dwarf","name":"Logan Forman / Dev-Dwarf","url":"http://lcfd.dev/"}</script>
    <link rel="stylesheet" href="/dwarf.css">
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

void win32_write_file(chr8* filepath, Str8List html) {
    HANDLE file = CreateFileA(filepath, FILE_APPEND_DATA | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    ASSERT(file != INVALID_HANDLE_VALUE);
    u32 toWrite = 0;
    u32 written = 0;
    u32 bytesWrittenTotal = 0;
    Str8Node* n = html.first;
    for (u64 i = 0; i < html.count; i++, n = n->next) {
        toWrite = (u32) n->str.len;
        written = 0;

        while (written != toWrite) {
            WriteFile(file, n->str.str, toWrite, (LPDWORD) &written, 0);
        }
            
        bytesWrittenTotal += written;
    }
    ASSERT(bytesWrittenTotal == html.total_len);
    CloseHandle(file);
}

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
    return Str8List_join(arena, dir, str8_lit(""), str8_lit("\\"), str8_lit("\0"));
}

PageList get_pages_in_dir(Arena *arena, Page::Types type) {
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
            base_href = Str8List_join(arena, href, str8_lit("/"), str8_lit("/"), str8_lit("/"));
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
        next->type = type;

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

void compile_page(Arena *longa, Arena *tempa, Page *page) {
    Str8List_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    Str8List_add_node(&dir, &filename);
    switch_to_dir(&src);
    page->content = win32_load_entire_file(tempa, build_dir(tempa));
        
    if (page->type == Page::ARTICLE) {
        str8 dummy = page->content;
        str8 first_line = str8_pop_at_first_delimiter(&dummy, str8_NEWLINE);
        page->title = str8_copy(longa, str8_cut(str8_skip(first_line, 2), 1));
    }  else {
        page->title = str8_cut(page->filename, 4);
    }
        
    printf("%.*s \"%.*s\" ", str8_PRINTF_ARGS(filename.str), str8_PRINTF_ARGS(page->title));

    Str8List_pop_node(&dir);

    filename.str = str8_concat(tempa, str8_cut(page->filename, 3), str8_lit("html\0"));
    Str8List_add_node(&dir, &filename);

    Str8List html = {0};
    Str8List_add(tempa, &html, HEADER);
    Str8List_add(tempa, &html, str8_lit("\t<title>LCF/DD:"));
    Str8List_add(tempa, &html, page->title);
    Str8List_add(tempa, &html, str8_lit("</title>\n"));

    if (page->type == Page::ARTICLE) {
        Str8List_add(tempa, &html, str8_lit("<br><a href='/writing.html#"));
        str8 link_ref = Str8List_join(tempa, page->base_dir, str8_lit(""), str8_lit("/"), str8_lit(""));
        Str8List_add(tempa, &html, link_ref);
        Str8List_add(tempa, &html, str8_lit("'>back</a>"));
    }

    Block* blocks = parse(tempa, page->content);
    Str8List md = render(tempa, blocks);

    /* TODO: make this replace a designated block in the text */
    /* Should have a SPECIAL block that we replace with arbitrary content */
    // if (page->type == Page::ARTICLE) {
    //     Str8List_add(tempa, &html, str8_lit("<ul>\n"));
    //     for (Block* b = blocks; b != 0; b = b->next) {
    //         if (b->type == Block::HEADING) {
    //             if (str8_not_empty(b->id)) {
    //                 Str8List_add(tempa, &html, str8_lit("<li><a href='#"));
    //                 Str8List_add(tempa, &html, b->id);
    //                 Str8List_add(tempa, &html, str8_lit("'>"));
    //                 Str8List_append(&html, b->content);
    //                 Str8List_add(tempa, &html, str8_lit("</a></li>\n"));
    //             }
    //         }
    //     }
    //     Str8List_add(tempa, &html, str8_lit("</ul>\n"));
    // }
    
    Str8List_append(&html, md);
        
    if (page->type == Page::ARTICLE) {
        str8 link_ref = Str8List_join(tempa, page->base_dir, str8_lit(""), str8_lit("/  "), str8_lit(""));
        Str8List_add(tempa, &html, str8_lit("<br><a href='/writing.html#"));
        Str8List_add(tempa, &html, link_ref);
        Str8List_add(tempa, &html, str8_lit("'>back</a>"));
    }

    if (page->type == Page::INDEX) {
        Str8List_add(tempa, &html, str8_lit("<ul>"));
        for (Page *link = allPages.first; link != 0; link = link->next) {
            if (link->type == Page::ARTICLE) {
                Str8List_add(tempa, &html, str8_lit("<li><a href='"));
                Str8List_add(tempa, &html, link->base_href);
                Str8List_add(tempa, &html, str8_cut(link->filename,3));
                Str8List_add(tempa, &html, str8_lit("html' id='"));
                Str8List_add(tempa, &html, link->filename);
                Str8List_add(tempa, &html, str8_lit("'>"));
                Str8List_add(tempa, &html, link->title);
                Str8List_add(tempa, &html, str8_lit("</a></li>"));
            }
        }
        Str8List_add(tempa, &html, str8_lit("</ul>"));
    }

    Str8List_add(tempa, &html, FOOTER);

    switch_to_dir(&deploy);
    win32_write_file(build_dir(tempa).str, html);

    printf("> %.*s%.*s\n", str8_PRINTF_ARGS(page->base_href), str8_PRINTF_ARGS(page->filename));

    page->content = str8_EMPTY;
    Str8List_pop_node(&dir);
    Str8List_pop(&dir, page->base_dir.count);
}

int main() {
    Arena *longa = Arena_create_default();
    Arena *tempa = Arena_create_default();

    Str8Node technical = {0, str8_lit("technical")};
    Str8List articleDirs = {0};
    Str8List_add_node(&articleDirs, &technical);

    dir = {0};
    chr8 root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    Str8List_add_node(&dir, &root);

    filename = {0};
    Str8List_add_node(&dir, &src);
    
    /* TODO: loop this as a daemon, whenever any of the files change update them automatically
        REF: https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
    */
    /* TODO: str8_trim_suffix might not work, check */
    /* TODO: generate RSS feed from 10 recent posts */
    /* TODO: some form of templating maybe? */
    /* TODO: captions for images */
    /* TODO: aside/expandable text like marc-ten-bosch */
    PageList topPages = get_pages_in_dir(longa, Page::DEFAULT);
    Str8List_add_node(&dir, &technical);
    PageList technicalPages = get_pages_in_dir(longa, Page::ARTICLE);
    Str8List_pop_node(&dir);
    allPages = technicalPages;
    allPages.count += topPages.count;
    allPages.last->next = topPages.first;
    allPages.last = topPages.last;

    /* NOTE: Special case since writing is the only index page for now */
    for (Page *n = allPages.first; n != 0; n = n->next) {
        if (str8_has_prefix(n->filename, str8_lit("writing.md"))) {
            n->type = Page::INDEX;
            break;
        }
    }
    
    for (Page *n = allPages.first; n != 0; n = n->next) {
        compile_page(longa, tempa, n);
        Arena_reset_all(tempa);
    }
}

