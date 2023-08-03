@{article}
@{title, Upgrading A Static Site Generator, Thu, 09 Jun 2023}
@{desc, It's been a while! See how I've upgraded the site with the 'special' node.}
@{sections}
---
##introd Introduction
It's been a while since my first post, but I'm hoping to actually use this blog now that I've graduated and have a bit more time. I think a good way to continue would be walking through more of my site generator. When I left off @(/writing/making-a-ssg1.html last time), the main thing missing from my site was a way to handle special elements, like an index for articles on the @(/writing.html writing page).


In this post I'll walk through how adding one simple feature to my markdown parser lets me handle these cases in my site specific code easily. In most static site generators I see special cases like this implemented using a templating language, like @(https://shopify.github.io/liquid/ Liquid) which is recommended by the Jekyll documentation. In my experience these templates enedd up scattered around my website code, adding yet another underpowered language to the already overpopulated web-dev stack. My approach allow templates to be made in C just like the rest of my site, with full access to all the existing data structures for the website pages.

##special The Special Block
So instead of writing an interpreter for some crappy template language, here's how the markdown compiler has been updated to handle special templates (`md_to_html.cpp`):
```
/* in parse()... */
} else if (c[0] == '@' && c[1] == '{') {
PUSH_BLOCK(); /* Immediately end the previous block. */
line = str_skip(line, 2);
/* Mark this block as special, save first arg as .id */
next.type = Block::SPECIAL;
next.id = str_pop_at_first_delimiter(&line, strl(",}"));
/* Push remaining args to .content */
str_iter_pop_delimiter(line, strl(",}")) {
    /* NOTE(lcf): optionally allow space in args list */
    if (str_char_location(sub, ' ') == 0) {
        sub = str_skip(sub, 1);
    }
    StrList_push(arena, &next.content, sub);
}
/* End the special block */
PUSH_BLOCK();
}
```
That's it. There's no additional rendering code or anything else in the markdown compiler, it just packages up arguments into the Block data structure and then is done. All the remaining work of handling the special templates is in the site specific code (`site.cpp`).

So now in `site.cpp:compile_page()`, instead of:
```
/* ... snip ... */
Block* blocks = parse(tempa, page->content);
StrList md = {0};
for (Block* b = blocks; b->type != Block::NIL; b = b->next) {
    md = render_block(tempa, b);
    StrList_append(&html, md);
}
/* ... snip ... */
```

I now have:
```
/* ... snip ... */
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
/* ... snip ... */
```
Where `render_special_block()` contains the code to handle each special node. That's all fine, but really I haven't done anything useful yet, so lets look at some examples of templates that I've implemented for the site so far.

##examples Example Special Blocks
###article @{article}
The markdown for @(https://github.com/dev-dwarf/dev-dwarf.github.io/blob/main/src/technical/making-a-ssg2.md this page) starts off with the `@{article}` special block, which just puts a short message at the end of the page. This code is even slightly more complicated than it needs to be at the moment, because in the future when I have more articles I will make the link take you back to the part of the index page where the link for the current article is.
```
if (str_eq(block->id, strl("article"))) {
    str link_ref = StrList_join(tempa, page->base_dir, {strl("#"), strl("/  "), {}});
    StrList_pushv(tempa, back,
                  strl("<hr><p class='centert'> Feel free to message me with any comments about this article! <br> Email: <code>contact@loganforman  .com</code> </p>"),
                  strl("<a class='btn' href='/writing.html"),
                  link_ref,
                  strl("'>←  back to index</a>"));
}
```
###sections @{sections}
A `@{sections}` block will make a list of clickable links to headers on the page. This node is a bit bigger than the others as it handles a few special cases like headers in expandable sections and different levels of indentation for different sizes of header. The code iterates through the block list for the page, searching for headings with a non-empty id (the id is used to create the link url, and so indicates that it can be linked to at all). Additional list levels are created or ended based on the difference of size between the previous and next heading.
```
if (str_eq(block->id, strl("sections"))) {
    StrList_push(tempa, front, strl("<ul class='sections'>\n"));
    u32 n = 0; u32 nfirst = 0;
    for (Block* b = block; b->type != Block::NIL; b = b->next) {
        if ((b->type == Block::HEADING || b->type == Block::EXPAND)
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
        if (b->type == Block::HEADING && str_not_empty(b->id)) {
            StrList_pushv(tempa, front, strl("<li><a href='#"), b->id, strl("'>"));
            StrList_append(front, render_text(tempa, b->text));
            StrList_push(tempa, front, strl("</a></li>\n"));
        }
        if (b->type == Block::EXPAND && str_not_empty(b->id)) {
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
```
###index @{index}
The original inspiration for designing the special block feature this way was to create an index of pages. Implementing this feature requires some metadata about other pages that exist in the site, as well as the ability to iterate over these pages. An index special block will be written as `@{index, /dir/}`, where `dir` is the directory containing the pages to index. The code for this special block simply accesses the list of all the pages in the site, and checks for those which are in the directory specified by the parameter. For each of these pages, it adds an entry to a table with the title of the page (which itself is set by a title special block).

```
if (str_eq(block->id, strl("index"))) {
    str base_href = block->content.first->str;
    StrList_push(tempa, front, strl("<table><tr><td>Date</td><td>Title</td><td></td></tr>"));
    for (Page *p = allPages.first; p != 0; p = p->next) {
        if (str_eq(p->base_href, base_href) && str_not_empty(p->title)) {
            StrList_pushv(tempa, front, strl("<tr><td><code>"),
                         p->date,
                         strl("</code></td><td>"),
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
    StrList_push(tempa, front, strl("</table>"));
}
```
##other Other Updates
I've added some additional features to the markdown compiler, such as text with an explanation on hover, expandable sections, and tables. I made a @(/splat.html splat page) to play around with the various features. You can see tables and other things in action on various pages of the site now. There is also an RSS feed now, which is generated in a similar way to the index node above. 

I covered all of the list of features I wanted from last time (except for automatically compiling pages on save which I think is not very valuable actually), but as always there is more I want to do now that I have finished that. However my first priority with the site is going to be writing some more posts covering topics that aren't this project, so I'll only add features as needed for that. There might be a part 3 eventually though!
