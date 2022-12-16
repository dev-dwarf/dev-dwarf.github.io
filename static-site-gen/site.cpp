#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "md_to_html.cpp"

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi")

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
    <link rel="stylesheet" href="./dwarf.css">
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

void win32_write_file(chr8* filename, Str8List html) {
    HANDLE file = CreateFileA(filename, FILE_APPEND_DATA | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

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

#define FIND_ALL_FILES(search, code) {         \
    WIN32_FIND_DATA ffd;                       \
    HANDLE find = INVALID_HANDLE_VALUE;        \
    find = FindFirstFile(search, &ffd);        \
    ASSERT(find != INVALID_HANDLE_VALUE);      \
    do {                                       \
        code                                   \
    } while (FindNextFile(find, &ffd) != 0);    \
    FindClose(find); \
}

void add_header(Arena *a, Str8List *html, str8 filename) {
    Str8List_add(a, html, HEADER);
    Str8List_add(a, html, str8_lit("<title>"));
    Str8List_add(a, html, str8_lit("LCF/DD: "));
    Str8List_add(a, html, filename);
    Str8List_add(a, html, str8_lit("</title>"));
}

void add_back_button(Arena *a, Str8List *html) {
}

void add_md(Arena *a, Str8List *html, str8 filedata) {
    Str8List md = md_to_html(a, filedata);
    Str8List_append(html, md);
}

void add_title_list(Arena *a, Str8List *html, str8 search_dir) {
    Str8List_add(a, html, str8_lit("<ul>"));
    str8 search_command = str8_concat(a, search_dir, str8_lit("*.md"));
    search_command = str8_concat(a, search_command, str8_lit("\0"));
    
    FIND_ALL_FILES(search_command.str, {
        str8 filename = str8_from_cstring(ffd.cFileName);
        str8 filepath = str8_concat(a, search_dir, filename);
        str8 filedata = win32_load_entire_file(a, filepath);
        str8 line = str8_pop_at_first_delimiter(&filedata, str8_NEWLINE);
        line = str8_skip(line, 3);
        PathRenameExtension(ffd.cFileName, ".html");
        filename.len += 2;
        Str8List_add(a, html, str8_lit("<li><a href='"));
        Str8List_add(a, html, str8_copy(a, search_dir));
        Str8List_add(a, html, str8_copy(a, filename));
        Str8List_add(a, html, str8_lit("' id='"));
        Str8List_add(a, html, str8_copy(a, filename));
        Str8List_add(a, html, str8_lit("'>"));
        Str8List_add(a, html, line);
        Str8List_add(a, html, str8_lit("</a></li>"));

        printf("\t");
        printf(ffd.cFileName);
        printf("\n");                                   
        })
        Str8List_add(a, html, str8_lit("</ul>"));
}

int main() {
    Arena *a = Arena_create_default();

    SetCurrentDirectory(".\\docs\\");
    str8 output_dir;
    chr8 output_dir_str[256];
    output_dir.str = output_dir_str;
    output_dir.len = GetCurrentDirectory(256, output_dir_str);
    output_dir.str[output_dir.len++] = '\\';
    SetCurrentDirectory("..");
    /* TODO: leave this running, whenever any of the files change update them automatically */
    /* TODO: I hate everything about the filename/path handling in this code */
    /* TODO: organize project folder a bit better. */
    SetCurrentDirectory(".\\src\\");
    FIND_ALL_FILES(".\\*.md", {
            str8 filename = str8_from_cstring(ffd.cFileName);
            str8 filedata = win32_load_entire_file(a, filename);
            PathRenameExtension(ffd.cFileName, ".html");
            filename.len += 3;

            printf(ffd.cFileName);
            printf("\n");
            
            Str8List html = {0};
            
            
            add_header(a, &html, filename);
            add_md(a, &html, filedata);

            /* TODO: remove this somehow if possible, would like to specify this
               from writing.md not here */
            if (str8_eq(filename, str8_lit("writing.html\0"))) {
                add_title_list(a, &html, str8_lit(".\\technical\\"));
            }
            
            Str8List_add(a, &html, FOOTER);

            str8 output_filename = str8_concat(a, output_dir, filename);
            output_filename = str8_concat(a, output_filename, str8_lit("\0"));
            win32_write_file(output_filename.str, html);

            Arena_reset_all(a);
        });

    SetCurrentDirectory(".\\technical\\");
        FIND_ALL_FILES(".\\*.md", {
            str8 filename = str8_from_cstring(ffd.cFileName);
            str8 filedata = win32_load_entire_file(a, filename);
            PathRenameExtension(ffd.cFileName, ".html");
            filename.len += 3;
            
            Str8List html = {0};
            
            add_header(a, &html, filename);
            Str8List_add(a, &html, str8_lit("<br><a href='/writing.html#technical'>back</a><hr>"));
            add_md(a, &html, filedata);
            Str8List_add(a, &html, str8_lit("<a href='/writing.html#technical'>back</a>"));
            Str8List_add(a, &html, FOOTER);

            str8 output_filename = str8_concat(a, output_dir, str8_lit("technical\\"));
            output_filename = str8_concat(a, output_filename, filename);
            output_filename = str8_concat(a, output_filename, str8_lit("\0"));
            win32_write_file(output_filename.str, html);
        
            printf("%.*s", str8_PRINTF_ARGS(filename));
            printf("\n");

            Arena_reset_all(a);
        });
}

