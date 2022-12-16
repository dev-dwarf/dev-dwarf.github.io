#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "md_to_html.cpp"

#define MAX_FILEPATH 512

global Str8List dir = {0};
global Str8Node root = {0};
global Str8Node src = {0, str8_lit("src")};
global Str8Node deploy = {0, str8_lit("deploy")};
global Str8Node technical = {0, str8_lit("technical")};
global Str8Node wildcard = {0, str8_lit("*.md")};
global str8 filename_storage;
global Str8Node filename;

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
    var all_links = document.getElementById("nav-links").getElementsByTagName("a"),
        i=0, len=all_links.length,
        full_path = location.href.split('#')[0]; //Ignore hashes?
    for(; i<len; i++) {
        if(all_links[i].href.split("#")[0] == full_path) {
            all_links[i].className += " current";
        }
    }
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

void fileext_md_to_html(str8 *filepath) {
    s64 len_start = filepath->len;
    *filepath = str8_cut(*filepath, 2);
    str8 html = str8_lit("html");
    memcpy(filepath->str+filepath->len-1, html.str, html.len);
    filepath->len += html.len;
    dir.total_len += filepath->len - len_start;
    ASSERT(filepath->len < MAX_FILEPATH);
}

void switch_to(Str8Node *new_folder_node) {
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

void check_dir_length() {
    u64 actual_len = 0;
    for (Str8Node *n = dir.first; n != 0; n = n->next) {
        actual_len += n->str.len;
    }
    ASSERT(actual_len == dir.total_len);
}

void add_header(Arena *a, Str8List *html, str8 filepath) {
    Str8List_add(a, html, HEADER);
    Str8List_add(a, html, str8_lit("<title>"));
    Str8List_add(a, html, str8_lit("LCF/DD: "));
    Str8List_add(a, html, filepath);
    Str8List_add(a, html, str8_lit("</title>"));
}

void add_md(Arena *a, Str8List *html, str8 filedata) {
    Str8List md = md_to_html(a, filedata);
    Str8List_append(html, md);
}

str8 build_dir(Arena *a) {
    return Str8List_join(a, dir, str8_lit(""), str8_lit("\\"), str8_lit("\0"));
}

void add_title_list(Arena *a, Str8List *html) {
    Str8List_add(a, html, str8_lit("<ul>"));
        {
        HANDLE find = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATA ffd = {0};
        ARENA_SESSION(a) { /* Compose search string */
            Str8List_add_node(&dir, &wildcard);
            check_dir_length();
            str8 search = build_dir(a);
            find = FindFirstFile(search.str, &ffd);
            ASSERT(find);
            Str8List_pop_node(&dir);
            check_dir_length();
        }
        do {
            MemoryZero(filename.str.str, filename.str.len);
            filename.str = str8_from_cstring_custom(filename_storage, ffd.cFileName);
            Str8List_add_node(&dir, &filename);
            check_dir_length();
            printf("\n\t%.*s ", str8_PRINTF_ARGS(filename.str));
            switch_to(&src);
            check_dir_length();
            str8 input_path = build_dir(a);
            str8 filedata = win32_load_entire_file(a, input_path);
            fileext_md_to_html(&filename.str);

            str8 line = str8_pop_at_first_delimiter(&filedata, str8_NEWLINE);
            line = str8_skip(line, 3);

            switch_to(&deploy);
            check_dir_length();
            Str8List end = dir;
            end.count -= 2;
            end.total_len -= end.first->str.len;
            end.first = end.first->next;
            end.total_len -= end.first->str.len;
            end.first = end.first->next;
            str8 output_href = Str8List_join(a, end, str8_lit("/"), str8_lit("/"), str8_lit(""));
            Str8List_add(a, html, str8_lit("<li><a href='"));
            Str8List_add(a, html, str8_cut(output_href, 1));
            Str8List_add(a, html, str8_lit("' id='"));
            Str8List_add(a, html, str8_copy(a, filename.str));
            Str8List_add(a, html, str8_lit("'>"));
            Str8List_add(a, html, line);
            Str8List_add(a, html, str8_lit("</a></li>"));
            
            Str8List_pop_node(&dir); /* pop filename */
            check_dir_length();
        } while(FindNextFile(find, &ffd) != 0);
        FindClose(find);
    }
    Str8List_add(a, html, str8_lit("</ul>"));

}

#define BUILD_HTML_FUNCTION(name) Str8List name(Arena *a, str8 filedata)

BUILD_HTML_FUNCTION(build_html_standard) {
    Str8List html = {0};
    add_header(a, &html, str8_cut(filename.str, 5)); /* cut ".html" */
    add_md(a, &html, filedata);

    /* TODO: remove this somehow if possible, would like to specify this
       from writing.md not here */
    if (str8_has_prefix(filename.str, str8_lit("writing.html"))) {
        check_dir_length();
        str8 filename_copy = str8_copy(a, filename.str);
        ASSERT(dir.last == &filename);
        Str8List_pop_node(&dir);
        Str8List_add_node(&dir, &technical);
        add_title_list(a, &html);
        Str8List_pop_node(&dir);
        MemoryZero(filename.str.str, filename.str.len);
        str8_copy_custom(filename.str.str, filename_copy);
        Str8List_add_node(&dir, &filename);
        check_dir_length();
    }
            
    Str8List_add(a, &html, FOOTER);
    return html;
}

BUILD_HTML_FUNCTION(build_html_article) {
    Str8List html = {0};
    add_header(a, &html, str8_cut(filename.str, 5)); /* cut ".html" */

    Str8List_add(a, &html, str8_lit("<br><a href='/writing.html#technical'>back</a><hr>"));
    add_md(a, &html, filedata);
    Str8List_add(a, &html, str8_lit("<a href='/writing.html#technical'>back</a>"));
    Str8List_add(a, &html, FOOTER);

    return html;
}

int main() {
    Arena *a = Arena_create_default();

    dir = {0};
    chr8 root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    Str8List_add_node(&dir, &root);

    filename_storage = str8_create_size(a, MAX_FILEPATH);
    filename = {0, filename_storage};
    Str8List_add_node(&dir, &src);
    
    /* TODO: loop this, whenever any of the files change update them automatically */
    /* TODO: better api for Str8List_join. also str8_trim_suffix might not work, check */
    {
        HANDLE find = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATA ffd = {0};
        ARENA_SESSION(a) { /* Compose search string */
            switch_to(&src);
            check_dir_length();
            Str8List_add_node(&dir, &wildcard);
            str8 search = build_dir(a);
            find = FindFirstFile(search.str, &ffd);
            ASSERT(find);
            Str8List_pop_node(&dir);
            check_dir_length();
        }
        do {
            MemoryZero(filename.str.str, filename.str.len);
            filename.str = str8_from_cstring_custom(filename_storage, ffd.cFileName);
            Str8List_add_node(&dir, &filename);
            ARENA_SESSION(a) {
                printf("%.*s ", str8_PRINTF_ARGS(filename.str));
                switch_to(&src);
                check_dir_length();
                str8 input_path = build_dir(a);
                str8 filedata = win32_load_entire_file(a, input_path);
                fileext_md_to_html(&filename.str);

                Str8List html = build_html_standard(a, filedata);
                
                switch_to(&deploy);
                check_dir_length();
                str8 output_path = build_dir(a);
                win32_write_file(output_path.str, html);
                printf("> %.*s\n", str8_PRINTF_ARGS(filename.str));
            }
            Str8List_pop_node(&dir); /* pop filename */
            check_dir_length();
        } while(FindNextFile(find, &ffd) != 0);
        FindClose(find);
    }

    Str8List_add_node(&dir, &technical);
    {
        HANDLE find = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATA ffd = {0};
        ARENA_SESSION(a) { /* Compose search string */
            switch_to(&src);
            check_dir_length();
            Str8List_add_node(&dir, &wildcard);
            str8 search = Str8List_join(a, dir, str8_lit(""), str8_lit(""), str8_lit("\0"));
            find = FindFirstFile(search.str, &ffd);
            ASSERT(find);
            Str8List_pop_node(&dir);
            check_dir_length();
        }
        do {
            MemoryZero(filename.str.str, filename.str.len);
            filename.str = str8_from_cstring_custom(filename_storage, ffd.cFileName);
            Str8List_add_node(&dir, &filename);
            ARENA_SESSION(a) {
                printf("%.*s ", str8_PRINTF_ARGS(filename.str));
                switch_to(&src);
                check_dir_length();
                str8 input_path = Str8List_join(a, dir, str8_lit(""), str8_lit(""), str8_lit("\0"));
                str8 filedata = win32_load_entire_file(a, input_path);
                fileext_md_to_html(&filename.str);

                Str8List html = build_html_article(a, filedata);
                
                switch_to(&deploy);
                check_dir_length();
                str8 output_path = Str8List_join(a, dir, str8_lit(""), str8_lit(""), str8_lit("\0"));
                win32_write_file(output_path.str, html);
                
                printf("> %.*s\n", str8_PRINTF_ARGS(filename.str));
            }
            Str8List_pop_node(&dir); /* pop filename */
            check_dir_length();
        } while(FindNextFile(find, &ffd) != 0);
        FindClose(find);        
    }
    Str8List_pop_node(&dir);
}

