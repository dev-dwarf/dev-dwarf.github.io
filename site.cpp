#include "../lcf/lcf.h"
#include "../lcf/lcf.c"
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
    <link rel="stylesheet" href="dwarf.css">
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
    <div class="nav-right">
    <nav>
    <a class="nav-link" href="index.html">home</a>
    <a class="nav-link current" href="projects.html">projects</a>
    <a class="nav-link" href="writing.html">writing</a>
    <a class="nav-link" href="contact.html">contact</a>
    </nav>
    <div/>
    </div>
    </html>)");

int main() {
    Arena *a = Arena_create_default();

    WIN32_FIND_DATA ffd;
    HANDLE find = INVALID_HANDLE_VALUE;

    find = FindFirstFile(".\\*.md", &ffd);
    ASSERT(find != INVALID_HANDLE_VALUE);

    do {
        str8 filename = str8_from_cstring(ffd.cFileName);
        str8 contents = win32_load_entire_file(a, filename);

        Str8List html = {0};
        Str8List_add(a, &html, HEADER);
        Str8List_add(a, &html, str8_lit("<title>"));
        Str8List_add(a, &html, str8_lit("LCF/DD: "));
        Str8List_add(a, &html, filename);
        Str8List_add(a, &html, str8_lit("</title>"));

        Str8List md = md_to_html(a, contents);
        Str8List_append(&html, md);
        
        Str8List_add(a, &html, FOOTER);
        PathRenameExtension(ffd.cFileName, ".html");
        HANDLE file = CreateFileA(ffd.cFileName, FILE_APPEND_DATA | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

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

        Arena_reset_all(a);
    } while (FindNextFile(find, &ffd) != 0);

    FindClose(find);
}

