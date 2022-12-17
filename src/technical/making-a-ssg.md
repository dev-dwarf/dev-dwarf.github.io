## Making A Static Site Generator (December 15, 2022)

---
##intro Introduction
I wanted to make a new portfolio site as I get ready to apply for jobs after I graduate in the Spring (Feel free to @(/contact.html reach out)!). The main options seem to be some engines like WordPress, raw html/css/js, or generators like Jekyll. I respect the engine approach, but generally don't like that sort of thing, and my attempts at raw html always feel tedious, so I lean towards generators. In the past I made a small site following the Jekyll tutorial, but it felt frustrating to me. There was a lot of setup, many different levels of abstraction that seemed unnecessary, a dizzying array of plugins which weren't quite right, and the result was fairly slow, often taking a noticeable (1-3s) amount of time for my small site.


I've been working on building my understanding of text-handling in low-level languages like C and C++, so I thought building a small static site generator would be a good test of what I've learned. My goals for the project are:
1. Easily extendable. Do exactly what I want, quickly.
1. Markdown-like language to write pages/articles in.
1. Small Codebase. Should be <1000 LOC.
---
##compile Compile Markdown to HTML
I started by making a compiler for a simple markdown language. There is a specification called CommonMark that I *think* is the canonical version of Markdown, with a reference implementation @(https://github.com/commonmark/cmark cmark), clocking in at -20,000 LOC. I read through their spec, and while it gave me some ideas, some of it seems like a bit much unless you're expecting to face highly adversarial inputs (like the @(https://spec.commonmark.org/0.30/#emphasis-and-strong-emphasis 17 rules) for parsing bold/italic combos). I decided to keep some of the basic syntax of Markdown but not worry about following too closely, making extensions and changes as desired.

Taking a hint from the Markdown spec, I implemented my language as a composition of structures:
```
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
`Blocks` represent distinct formatting of seperate sections of the document. `Text` handles formatting that composes. From the names it's hopefully easy to tell the equivalent html; putting a given tag in one category or the other has been done somewhat arbitrarily. These structures imply parsing the Blocks and then parsing the Text of each block. I decided to parse for Blocks line-by-line, detecting which type of block it is based on the first few characters:
```
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
Each of these cases has some additional semantics, such as ending the previous block, parsing out any extra needed information (for example, `HEADING` counts the number of # characters to determine the size of the heading, and `LINK` grabs a url), but for the most part they are fairly straight-forward and can be tweaked. The main idea is that each case will either add more Text to the current Block, or end the previous block and start a new one.

Each Text node at first has a `NIL` type, to represent that they are unparsed. After all the blocks are parsed, their Text is parsed as well:
```
for (curr = root; curr->type != Block::NIL; curr = curr->next) {
    curr->text = parse_text(arena, curr->text);
}
```
The Text parsing is similar to the Block parsing, except each character is checked, and most nodes come in start/end pairs. Because I want to support patterns like `***bold-and-italic* just-bold**` generating ***bold-and-italic* just-bold**, its not enough to just have `BOLD` node encapsulate the bolded text. For this reason each text node has an `end` flag marking it as the start or end node of a pair:
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
I have been leaving out the details inside the if statements in the parsing, but you can see the full details @(https://github.com/dev-dwarf/dev-dwarf.github.io/blob/main/static-site-gen/md_to_html.cpp here). The insides are mostly just small amounts of parsing text and then macros for pushing new nodes onto the linked list. You might notice in the above parsing some departures from Markdown, such as `@(` to open a link, instead of a `(<`.

As a basic example, parsing the following:
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
The desired html is something like:
```
<h2>Hello</h2>
<p>It's nice to be <b>loud</b>!</p>
```
