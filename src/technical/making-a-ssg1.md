@{article}
@{title, Making A Static Site Generator, December 18 2022}
@{desc, First post! A brief walkthrough of how I made the SSG for my personal website.}
@{sections}
---
##introd Introduction
I wanted to make a new portfolio site as I get ready to apply for jobs after I graduate in the Spring (Feel free to @(/contact.html reach out)!). The main options seem to be engines like WordPress, raw HTML/CSS/JS, or generators like Jekyll. The engine approach feels too high-level for me, and my attempts at raw HTML always feel tedious, so I lean towards generators. In the past I made a small site following the Jekyll tutorial, but it felt frustrating to me. There was a lot of setup, many different levels of abstraction that seemed unnecessary, a dizzying array of plugins which weren't quite right, and the result was fairly slow, often taking a noticeable (1-3s) amount of time for my small site.


I've been working on building my understanding of text-handling in low-level languages like C and C++, so I thought building a small static site generator would be a good test of what I've learned. My goals for the project are:
1. Easily extendable. Do exactly what I want, quickly.
1. Markdown-like language to write pages/articles in.
1. Small. Should be <1000 LOC.
---
##parse Parsing Markdown
I started by making a compiler for a simple markdown language. There is a specification called CommonMark that I *think* is the canonical version of Markdown, with a reference implementation @(https://github.com/commonmark/cmark cmark), clocking in at ~20,000 LOC. I read through their spec, and while it gave me some ideas, some of it seems like a bit much unless you're expecting to face highly adversarial inputs (like the @(https://spec.commonmark.org/0.30/#emphasis-and-strong-emphasis 17 rules) for parsing bold/italic combos). I decided to keep some of the basic syntax of Markdown but not worry about following the spec too closely, making extensions and changes as desired.


Taking a hint from the Markdown spec, I implemented my language as a composition of `Block` and `Text` structures:
```
/* md_to_html.h */
struct Text {
    Text *next;
    enum Types { 
        NIL = 0,
        TEXT,
        BOLD, ITALIC, STRUCK, CODE_INLINE, 
        LINK, IMAGE, EXPLAIN,
        LIST_ITEM, CODE_BLOCK,
        BREAK,
    } type;
    b32 end;
    str8 text;
};
```
```
struct Block {
    Block *next;
    enum { 
        NIL = 0,
        PARAGRAPH,
        HEADING, RULE, CODE, 
        QUOTE, ORD_LIST, UN_LIST,
    } type;
    u32 num; /* For headings */
    str8 id;
    Str8List content;
    Text* text;
};
```
`Blocks` represent distinct formatting of seperate sections of the document. `Text` handles formatting that composes. From the names it's hopefully easy to tell the equivalent HTML; putting a given tag in one category or the other has been done somewhat arbitrarily. These structures imply parsing the Blocks and then parsing the Text of each block. I decided to parse for Blocks line-by-line, detecting which type of block it is based on the first few characters:
```
/* md_to_html.cpp */
str8_iter_pop_line(str) { /* macro, sets str8 line */
    /* Remove windows newline encoding (\r\n) */
    line = str8_trim_suffix(line, str8_lit("\r"));
    if (line.len == 0) {
       /* Breaks block unless PARAGRAPH or CODE */
    }
    chr8 c[3];
    c[0] = line.str[0];
    c[1] = (line.len > 1)? line.str[1] : 0;
    c[2] = (line.len > 2)? line.str[2] : 0;
    if (c[0] == '`' && c[1] == '`' && c[2] == '`') {    
       /* Start/End Code */
       
    } else if (code_lock) {
        /* Just directly add line if in a code block */
        
    } else if (c[0] == '#') {
        /* Heading */
    
    } else if (c[0] == '>' && c[1] == ' ') {
        /* Quote */
        
    } else if (c[0] >= '1' && c[0] <= '9' && c[1] == '.' && c[2] == ' ') {
        /* Ordered List */
        
    } else if ((c[0] == '*' || c[0] == '-') && c[1] == ' ') {
        /* Un-Ordered List */
    
    } else if (c[0] == '-' && c[1] == '-' && c[2] == '-') {
        /* Horizontal Rule/Line */
        
    } else {
        /* Paragraph */
    }
}

```
Each of these cases has some additional semantics, such as ending the previous block, parsing out any extra needed information (for example, `HEADING` counts the number of # characters to determine the size of the heading, and `LINK` needs to grab the url), but for the most part they are fairly straight-forward and can be tweaked. The main idea is that each case will either add more Text to the current Block, or end the previous block and start a new one.


Each Text node at first has a `NIL` type, to represent that they are unparsed. After all the blocks are parsed, their Text is parsed as well:
```
for (curr = root; curr->type != Block::NIL; curr = curr->next) {
    curr->text = parse_text(arena, curr->text);
}
```
The Text parsing is similar to the Block parsing, except each character is checked, and most nodes come in start/end pairs. Because I want to support composing formatting like `***bold-and-italic* just-bold**` generating ***bold-and-italic* just-bold**, it's not enough to just have `BOLD` node encapsulate the bolded text in a pair of tags. For this reason each text node has an `end` flag marking it as the start or end node of a pair:
```
for (; curr->next != 0; pre = curr, curr = curr->next) {
    str8 s = curr->text;
    if ((curr->type == Text::LIST_ITEM) /* Already formatted, do not parse */
        || (curr->type == Text::CODE_BLOCK)
        || (curr->type == Text::BREAK)) {
        continue;
    }
    if (curr->type == Text::NIL) {
        curr->type = Text::TEXT;
    }
    if (s.len == 0) {
        if (curr->type == Text::TEXT) {
            curr->type = Text::BREAK;
        } else {
            PUSH_TEXT(Text::BREAK, 0, 1);
        }
        continue;
    }
    chr8 c[3]; 
    c[1] = s.str[0];
    c[2] = (s.len > 1)? s.str[1] : 0;
    str8_iter_custom(s, i, _unused) {
        c[0] = c[1];
        c[1] = c[2];
        c[2] = (s.len > i+2)? s.str[i+2] : 0;
        if (ignore_next) {
            PUSH_TEXT(Text::TEXT, i-1, 1);
            ignore_next = false;
        } else if (c[0] == '`') {
            /* Inline Code */
            
        } else if (curr->type == Text::CODE_INLINE && !curr->end) {
            /* Do nothing, do not parse stuff inside code */
        } else if (c[0] == '*' && c[1] == '*') {
            /* Bold *.
            
        } else if (c[0] == '*') {
            /* Italic */
            
        } else if (c[0] == '~' && c[1] == '~') {
            /* Strikethrough */
            
        } else if (c[0] == '@' && c[1] == '(') {
            /* Links */
            
        } else if (c[0] == '!' && c[1] == '(') {
            /* Images */
            
        } else if (c[0] == '?' && c[1] == '(') {
            /* Explain - Hover over to see expanded text */
            
        } else if (c[0] == ')') {
            /* Closing parenthesis can end one of the above ^ */
            if (paren_stacki > 0) {
                Text::Types t = paren_stack[--paren_stacki];
                PUSH_TEXT(t, i, 1);
            }
            break;
        } else if (c[0] == '\\') {
            /* Backslash ignores next formatting char */
        }
    } /* end str8_iter */
    ASSERTM(pre == &pre_filler || pre->type != Text::NIL, "Must not leave NIL nodes in Text linked-list!");
}
```
I have been leaving out the details inside the if statements in the parsing, but you can see the full details @(https://github.com/dev-dwarf/dev-dwarf.github.io/blob/main/static-site-gen/md_to_html.cpp here). The insides are mostly just small amounts of parsing text and then some macros for pushing new `Block` or `Text` nodes onto linked lists. You might notice in the above parsing some departures from Markdown, such as `@(link text)` to notate a link, instead of a `[text](link)` pair.
---
##compile Render as HTML
As a basic example of what we have so far, parsing the following:
```
## Hello
It's nice to be **loud**!
```
Will give this structure:
```
Block(type=Header, num=2, text=[
    Text(type=Text, str="Hello")
]),
Block(type=Paragraph, text=[
    Text(type=Text, str="It's nice to be "),
    Text(type=Bold, str="loud", end=false),
    Text(type=Bold, str="!", end=true)
])
```
The desired HTML is something like:
```html
<h2>Hello</h2>
<p>It's nice to be <b>loud</b>!</p>
```
With the above as the goal, it's not hard to imagine rendering the parsed nodes to HTML using a couple loops:
```
Str8List render(Arena* arena, Block* root) {
    Str8List out = {0};
    for (Block* b = root; b->type != Block::NIL; b = b->next) {
        switch (b->type) {
            /* Do pre-content tags for Block type. EX: */
            case Block::ORD_LIST: {
                Str8List_add(arena, &out, str8_lit("&ltol&gt\n"));
            } break;
        }
    
        b->content = render_text(arena, b->text);
        Str8List_append(&out, Str8List_copy(arena, b->content));    
    
        switch (b->type) {
            /* Do post-content tags for Block type. EX: */
            case Block::ORD_LIST: {
                Str8List_add(arena, &out, str8_lit("\n&lt/ol&gt\n"));
            } break;                    
        }
    }
    return out;
}
```
```
Str8List render_text(Arena* arena, Text* root) {
    Str8List out = {0};
    Text prev_filler = {root, Text::NIL, 0};
    for (Text* t = root, *prev = &prev_filler; t->type != Text::NIL; prev = t, t = t->next) {
        switch (t->type) {
            /* Add start or end tags based on t->end. EX: */
            case Text::BOLD: {
                str8 s[2] = {str8_lit("&ltb&gt"), str8_lit("&lt/b&gt")};
                Str8List_add(arena, &out, s[t->end]);
                Str8List_add(arena, &out, t->text);
            } break;
        }
    }
    return out;
}
```
---
##generate Generating the site.
The Markdown compiler is a good step, but it needs to be told what to compile, and the results are still missing necessary HTML boilerplate. For my site I decided to have a `src` folder for the markdown contents of my articles, and then compile everything to a `deploy` folder containing the generated HTML and other assets:
```
- dev-dwarf.github.io
    - src
        foo.md
        etc...
    - deploy
        foo.html
        etc...
```
I started a new file to drive this process. First I defined some structs to store data for each page, and help me manage lists of pages:
```
/* site.h */
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
        /* don't worry about the other types for now */
    } type;
};
struct PageList {
    Page *first;
    Page *last;
    u64 count;
};
```
Then there is a main loop that goes something like:
```
global Str8List dir;
int main() {
    Arena *longa = Arena_create_default();
    Arena *tempa = Arena_create_default();

    /* ... set dir to src directory */
    
    PageList allPages = get_pages_in_dir(longa, Page::DEFAULT);

    for (Page *n = allPages.first; n != 0; n = n->next) {
        compile_page(longa, tempa, n);
        Arena_reset_all(tempa);
    }
}
```
The `global Str8List dir` holds the current directory/file. It's convenient to have it as a list so that I can pop off or switch out nodes to change the targeted file or directory. `get_pages_in_dir` just uses filesystem calls to make a list of markdown files in the `src` folder. `compile_page` loads the raw markdown, compiles it, and adds some enclosing HTML to the front and back. Finally it's written out to the equivalent HTML file in the `deploy` folder:
```
void compile_page(Arena *longa, Arena *tempa, Page *page) {
    Str8List_append(&dir, page->base_dir);
        
    filename.str = page->filename;
    Str8List_add_node(&dir, &filename);
    switch_to_dir(&src);
    page->content = win32_load_entire_file(tempa, build_dir(tempa));
    Str8List_pop_node(&dir);

    filename.str = str8_concat(tempa, str8_cut(page->filename, 3), str8_lit("html\0"));
    Str8List_add_node(&dir, &filename);

    Str8List html = {0};
    Str8List_add(tempa, &html, HEADER);
    Str8List_add(tempa, &html, str8_lit("\t<title>LCF/DD:"));
    Str8List_add(tempa, &html, page->title);
    Str8List_add(tempa, &html, str8_lit("</title>\n"));

    Block* blocks = parse(tempa, page->content);
    Str8List md = render(tempa, blocks);
    Str8List_append(&html, md);
    
    Str8List_add(tempa, &html, FOOTER);

    switch_to_dir(&deploy);
    win32_write_file(build_dir(tempa).str, html);

    page->content = str8_EMPTY; /* clear this because it was on the temp arena */
    Str8List_pop_node(&dir);
    Str8List_pop(&dir, page->base_dir.count);
}
```
And that's pretty much it for a heavily-idealized version of my static site generator! The actual thing can be found @(https://github.com/dev-dwarf/dev-dwarf.github.io on GitHub). You may have noticed an unused type field for pages; the real version of the generator has `ARTICLE` pages and an `INDEX` page. `ARTICLE`s have slightly different HTML generated, and the `INDEX` gets a list of links to articles appended to it. I don't think it's worth writing about these yet as they are very hacked in and I want to change that system soon! However I am pleased with how easy it is to quickly hack in features like those given what I have described here as a base.
---
##conc Conclusion
Overall I'm pretty happy with the results of this project so far. The up-front time investment was a bit more than using Jekyll (about 4-days of hacking and writing), but for it I have a small, fast, and extendable static site generator tailored to my needs. The current version is ~700 lines of C-like C++, well under the 1000 LOC goal. I already hacked in some basic features to write this article, but I'd like to rework these soon. In addition, there's quite a few things I'd like to add:
1. Generate an index/section list for articles.
1. Generate an RSS feed from recent articles.
1. After the first compile of each page, run in the background checking for changes and compile files automatically. Right now I manually run `site.exe` to see my changes each time, but it would help my flow if that was taken care of for me.
1. Introduce some sort of templating/custom generation for individual pages. I **abhor** how most generators handle this sort of feature so I'm excited to look for a unique approach. I'd prefer something where I can easily hack in new templates in C++ instead of using some bogus templating language.
1. Additional miscellaneous features like captions for images, subsections, and asides/expandable text. 
I should also mention that although the source code for the markdown compiler and my site are on github, they can't be run as is without lcf, my @(http://nothings.org/stb.h stb)-esque standard library. I've only fairly recently started my library and I tend to make tweaks and additions almost everytime I work on a project right now, so I don't feel comfortable having it out there as a public repo. If you really want a copy so you can compile my site yourself @(/contact.html send me a message).
