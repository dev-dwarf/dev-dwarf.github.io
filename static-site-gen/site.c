/* TODO
    * Implement math expressions using mathml
        * Browsers now have support
        * Fallback: https://github.com/fred-wang/mathml.css
        * Just make a little custom lang for this, can fallback to mathml if needed :vomit:
    *  optional ids for code blocks (like headers) + line numbers that can be linked to
 */

#include <stdio.h>
#include <time.h>

#include "../../lcf/lcf.h"
#include "../../lcf/lcf.c"
#include "site.h"
#include "md_to_html.c"

#define MAX_FILEPATH 512

global StrList dir;
global StrNode root;
global StrNode src;
global StrNode deploy;
global StrNode wildcard;
global StrNode filename;
global PageList allPages;

str HEADER = strc("" 
"<!DOCTYPE html>\n"
"<!-- GENERATED -->\n"
"<html lang='en-US'>\n"
  "<head>\n"
    "<meta charset='utf-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
    "<meta property='og:title' content='Logan Forman' />\n"
    "<meta property='og:locale' content='en_US' />\n"
    "<meta property='og:image' content='/assets/dd.png' />\n"
    "<link rel='canonical' href='http://loganforman.com/' />\n"
    "<meta property='og:url' content='http://loganforman.com/'/>\n"
    "<meta property='og:site_name' content='Logan Forman / Dev-Dwarf' />\n"
    "<meta property='og:type' content='website' />\n"
    "<meta name='twitter:card' content='summary' />\n"
    "<meta property='twitter:title' content='Logan Forman' />\n"
    "<script type='application/ld+json'>\n"
      "{'@context':'https://schema.org','@type':'WebSite','headline':'Logan Forman / Dev-Dwarf','name':'Logan Forman / Dev-Dwarf','url':'http://loganforman.com/'}</script>\n"
    "<link rel='stylesheet' href='/dwarf.css'>\n"
    "<link rel='icon' type='image/x-icon' href='/assets/favicon.ico'>\n"
    "</head>\n"
    "<body>\n"
    "<script>\n"
        "var theme = localStorage.getItem('theme') || 'light'\n"
       "document.querySelector('body').setAttribute('data-theme', theme)\n"
        "function toggleNight() {\n"
            "console.log('toggle')\n"
            "theme = (theme == 'light')? 'night' : 'light'\n"
            "localStorage.setItem('theme', theme)\n"
            "document.querySelector('body').setAttribute('data-theme', theme);  \n"
        "}\n"
    "</script>\n"
    "<div class='wrapper'>\n"
    "<main class='page-content' aria-label='Content'>\n"
);

str FOOTER = strc(""
    "</main>\n"
    "</div>\n"
    "</body>\n"
    "<div>\n"
    "<hr>\n"
    "<nav>\n"
    "<table class='w33 left'><tr>\n"
    "<td><a href='/index.html'>home</a></td>\n"
    "<td><a href='/projects.html'>projects</a></td>\n"
    "<td><a href='/writing.html'>writing</a></td>\n"
    "<td><a style='text-decoration-color: #EE802F !important' href='/rss.xml'>rss</a></td>\n"
    "<td class='light'><a class='light' onClick='toggleNight()'>light</a></td>\n"
    "<td class='night'><a class='night' onClick='toggleNight()'>night</a></td>\n"
    "</tr></table>\n"
    "<table class='w33 right'><tr>\n"
    "<td><a href='https://github.com/dev-dwarf'>github</a></td>\n"
    "<td><a href='https://twitter.com/dev_dwarf'>twitter</a></td>\n"
    "<td><a href='https://bsky.app/profile/dev-dwarf.itch.io'>bluesky</a></td>\n"
    "<td><a href='https://store.steampowered.com/developer/dd'>steam</a></td>\n"
    "<td><a href='https://dev-dwarf.itch.io'>itch.io</a></td>\n"
    
    "</tr></table>\n"
    "<p><br><br><br></p>\n"
    "</nav>\n"
    "</div>\n"
    "</html>\n");

/* Swap between 'src' and 'deploy' folders of project directory. */
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
    return StrList_join(arena, dir, (StrJoin) {strl(""), strl("\\"), strl("\0")});
}

PageList get_pages_in_dir(Arena *arena) {
    PageList dirPages = {0};

    // build 
    str base_href;
    StrList base_dir;
    {
        StrList href = StrList_copy(arena, dir);
        StrList_skip(&href, 2);
        if (href.total_len == 0) {
            base_href = strl("/");
            base_dir = (StrList){0};
        } else {
            base_href = StrList_join(arena, href, (StrJoin){strl("/"), strl("/"), strl("/")});
            base_dir = StrList_copy(arena, href);
        }
    }

    switch_to_dir(&src);
    StrList_push_node(&dir, &wildcard);
    str search = build_dir(arena);
    os_FileSearchIter(arena, search, file) {
        /* Fill out page struct from file info */
        Page *next = Arena_take_struct_zero(arena, Page);

        next->filename = file.name;
        next->out_filename = file.name;
        if (char_is_num(file.name.str[0])) {
            str_iter(file.name, i, c) {
                next->out_filename = str_skip(next->out_filename, 1);
                if (char_is_num(c)) {
                    next->order = 10*next->order + (s32)(c - '0');
                } else {
                    break;
                }
            }
        }
        
        next->created_time = file.created;
        next->base_href = base_href;
        next->base_dir = base_dir;

        /* Sort pages by created time */
        Page *curr = dirPages.first, *pre = curr;
        if (curr == 0) {
            dirPages.first = next;
            dirPages.last = next;
        } else {
            for (; curr != 0; pre = curr, curr = curr->next) {
                if (next->order > curr->order) {
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
    }
    StrList_pop_node(&dir);
    
    return dirPages;
}

void render_special_block(Arena *longa, Arena *tempa, Page *page, StrList* front, StrList* back, Block* blocks, Block* block) {
    (void) blocks; /* NOTE(lcf): Unused for now. */
    
    if (str_eq(block->id, strl("sections"))) {
        StrList_push(tempa, front, strl("<ul class='sections'>\n"));
        s32 n = 0; s32 nfirst = 0;
        for (Block* b = blocks; b; b = b->next) {
            if ((b->type == HEADING || b->type == EXPAND)
                && str_not_empty(b->id)) {
                if (n == 0) {
                    n = b->num;
                    nfirst = n;
                }
                for (; n < b->num; n++) {
                    StrList_push(tempa, front, strl("<ul class='sections'>\n"));
                }
                for (; n > b->num; n--) {
                    StrList_push(tempa, front, strl("</ul>\n"));
                }
                n = b->num;
            }
            if (b->type == HEADING && str_not_empty(b->id)) {
                StrList_pushv(tempa, front, strl("<li><a href='#"), b->id, strl("'>"));
                StrList_append(front, render_text_inline(tempa, b->text));
                StrList_push(tempa, front, strl("</a></li>\n"));
            }
            if (b->type == EXPAND && str_not_empty(b->id)) {
                StrList_pushv(tempa, front, strl("<li><a href='#"),
                            b->id,
                            strl("'>"),
                            b->title,
                            strl("</a></li>\n"));
            }
        }
        for (; n >= nfirst; n--) {
            StrList_push(tempa, front, strl("</ul>\n"));
        }
    }
    if (str_eq(block->id, strl("title"))) {
        page->title = str_copy(longa, block->content.first->str);
        page->rss_day = str_copy(longa, block->content.first->next->str);
        page->date = str_copy(longa, block->content.last->str);
        StrList_pushv(tempa, front, strl("<div style='clear: both'><h1>"),
                    page->title,
                    strl("</h1><h3>"),
                    page->date,
                    strl("</h3></div>\n"));
    }
    if (str_eq(block->id, strl("desc"))) {
        page->desc = StrList_join(longa, block->content, (StrJoin){(str){0}, strc(" "), (str){0}});
    }
    if (str_eq(block->id, strl("article"))) {
        StrList_push(tempa, back, strl("<hr><p class='centert'> Feel free to message me with any comments about this article! <br> Email: <code>contact@loganforman.com</code> </p>"));
    }
    if (str_eq(block->id, strl("index"))) {
        str base_href = block->content.first->str;
        StrList_push(tempa, front, strl("<table><tr><td>Date</td><td>Title</td></tr>"));
        for (Page *p = allPages.first; p != 0; p = p->next) {
            if (str_eq(p->base_href, base_href) && str_not_empty(p->title)) {
                StrList_pushv(tempa, front, strl("<tr><td><code>"),
                             p->date,
                             strl("</code></td><td>"),
                             strl("<a href='"),
                             p->base_href,
                             str_cut(p->out_filename,2),
                             strl("html'>"),
                             p->title,
                             strl("</a>"),
                              strl("</td></tr>"));
            }
        }
        StrList_push(tempa, front, strl("</table>"));
    }
}

void compile_page(Arena *longa, Arena *tempa, Page *page) {
    StrList_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    StrList_push_node(&dir, &filename);
    switch_to_dir(&src);
    page->content = os_ReadFile(tempa, build_dir(tempa));
    page->title = str_cut(page->out_filename, 3);

    StrList_pop_node(&dir);

    filename.str = str_concat(tempa, str_cut(page->out_filename, 2), strl("html\0"));
    StrList_push_node(&dir, &filename);

    StrList html = {0};
    StrList back = {0};

    Block* blocks = parse_md(tempa, page->content);
    StrList md = {0};
    for (Block* b = blocks; b; b = b->next) {
        if (b->type != SPECIAL) {
            md = render_block(tempa, b);
            StrList_append(&html, md);
        } else {
            render_special_block(longa, tempa, page, &html, &back, blocks, b);
        }
    }

    /* Header and Footer */
    StrList head = {0};
    StrList_pushv(tempa, &head, HEADER, strl("\t<title>"), page->title, strl("</title>\n"));
    StrList_push(tempa, &back, FOOTER);
    StrList_prepend(&html, head);
    StrList_append(&html, back);

    printf("%.*s \"%.*s\" ", str_PRINTF_ARGS(filename.str), str_PRINTF_ARGS(page->title));

    switch_to_dir(&deploy);
    str file = build_dir(tempa);
    ASSERT(os_WriteFile(file, html));

    printf("> %.*s%.*s\n", str_PRINTF_ARGS(page->base_href), str_PRINTF_ARGS(page->filename));

    page->content = str_EMPTY;
    StrList_pop_node(&dir);
    StrList_pop(&dir, page->base_dir.count);
}

global str RSS_HEADER = strc(
"<rss version='2.0' xmlns:atom='http://www.w3.org/2005/Atom'>\n"
  "<channel>\n"
    "<title>Logan Forman</title>\n"
    "<link>http://loganforman.com/</link>\n"
    "<atom:link href='http://loganforman.com/rss.xml' rel='self' type='application/rss+xml' />\n"
    "<description>Journey to the competence.</description>\n"
);
global str RSS_FOOTER = strc(
  "</channel>\n"
"</rss>\n"
);
void compile_feeds(Arena *arena, PageList pages) {
    printf("RSS Feed:\n");
    StrList rss = {0};
    StrList_push(arena, &rss, RSS_HEADER);
    Page *n = pages.first;
    for (s64 i = 0; i < pages.count; i++, n = n->next) {
        printf("\t %.*s\n", str_PRINTF_ARGS(n->title));
        StrList_pushv(arena, &rss, strl("<item>\n<title>"),
                      n->title,
                      strl("</title>\n<description>"),
                      n->desc,
                      strl("</description>\n<link>https://loganforman.com/"),
                      n->base_href,
                      str_cut(n->out_filename, 2),
                      strl("html</link><guid isPermaLink='true'>https://loganforman.com/"),
                      n->base_href,
                      str_cut(n->out_filename, 2),
                      strl("</guid>\n<pubDate>"),
                      n->rss_day,
                      strl(", "),
                      n->date,
                      strl(" 08:00:00 MST</pubDate>\n"),
                      strl("</item>"));
    }
    StrList_push(arena, &rss, RSS_FOOTER);
    switch_to_dir(&deploy);
    
    StrList_push(arena, &dir, strl("rss.xml"));
    ASSERT(os_WriteFile(build_dir(arena), rss));
    StrList_pop_node(&dir);
    
    StrList_push(arena, &dir, strl("feed.xml"));
    ASSERT(os_WriteFile(build_dir(arena), rss));
    StrList_pop_node(&dir);

    Arena_reset(arena, 0);
}

int main() {
    Arena params = (Arena){
        .size = LCF_MEMORY_ARENA_SIZE, 
        .commit_size = (u32) LCF_MEMORY_COMMIT_SIZE, 
        .alignment = (u32) LCF_MEMORY_ALIGNMENT
    };
    Arena *longa = Arena_create_custom(params);
    Arena *tempa = Arena_create_custom(params);

    // TODO test code
    // Text *t = &(Text){
    //     .text = strl("**bold *bold-italic*** *italic @(link ~~ struck ~~) more italic* * escape \\* asterisk *")
    // };
    // parse_inline(tempa, t);
    // StrList output_list = render_text_inline(tempa, t->child);
    // str output = StrList_join(tempa, output_list, (StrJoin){0});
    // printf("%.*s", (s32) output.len, output.str);
    // print_tree(t);

    /*
    Block *block = parse_md(tempa, strl(
        "# SPLAT\n"
        "---\n"
        "\n"
        "*italic*\n"
        "\n"
        "**bold**\n"
        "\n"
        "***it-bold***\n"
        "\n"
        "~~struck~~\n"
        "\n"
        "`inline code`\n"
        "\n"
        "@(/splat.html link)\n"
        "\n"
        "!(/assets/dd.png)\n"
        "---\n"
        "@{sections}\n"
        "#h1 H1\n"
        "h1\n"
        "##h2 H2\n"
        "h2\n"
        "###h3 H3\n"
        "h3\n"
        "####h4 H4\n"
        "h4\n"
        "#####h5 H5\n"
        "h5\n"
        "---\n"
        "\n"
        "[ ###expand Expandable Note H3\n"
        "[ text of expandable note\n"
        "\n"
        "!|centered w33| Centered || Small || Table |\n"
        "!|| 1 || 2 || 3 |\n"
        "!|| 4 || 5 || 6 |\n"
        "\n"
        "?(Exoteric Explanation, Esoteric Phrase)\n"
        "\n"
        "```\n"
        "generic code block\n"
        "```\n"
        "\n"
        "\n"
        "1. item 1\n"
        "1. item 2\n"
        "1. item 3\n"
    ));

    StrList output_list = (StrList){0};
    for (Block *b = block; b; b = b->next) {
        StrList_append(&output_list, render_block(tempa, b));
    }

    str output = StrList_join(tempa, output_list, (StrJoin){0});
    printf("%.*s", (s32) output.len, output.str);
    
    return;
    */

     
    src = (StrNode) {0, strc("src")};
    deploy = (StrNode) {0, strc("deploy")};
    wildcard = (StrNode) {0, strc("*.md")};

    StrNode writing = {0, strl("writing")};
    dir =(StrList){0};
    char root_path_buffer[MAX_FILEPATH];
    root.str.str = root_path_buffer;
    root.str.len = GetCurrentDirectory(MAX_FILEPATH, root.str.str);
    StrList_push_node(&dir, &root);
    StrList_push_node(&dir, &src);
    
    PageList topPages = get_pages_in_dir(longa);
    StrList_push_node(&dir, &writing);
    PageList writingPages = get_pages_in_dir(longa);
    StrList_pop_node(&dir);
    allPages = writingPages;
    allPages.count += topPages.count;
    allPages.last->next = topPages.first;
    allPages.last = topPages.last;
    
    for (Page *n = allPages.first; n != 0; n = n->next) {
        compile_page(longa, tempa, n);
        Arena_reset(tempa, 0);
    }

    compile_feeds(tempa, writingPages);
    
    return 0;
    /* */
}

