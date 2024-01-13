@{article}
@{title, Implementing A Simple Undo System, 8 Jan 2024}
@{desc, Details about a nice undo system I implemented recently. }
@{sections}
---
##intro Introduction
Recently I've been implementing a level editor for my game engine, and I'd been dreading adding
undo/redo support to it. In the past I've usually skipped it altogether, even though it's great
to have. When I have implemented it, I usually used a command-pattern style approach, where for
each type of action the user can do there is a method to (re)do and undo the action.
This resulted in quite a lot of repetitive code for various types of actions, and slowed down 
my ability to add more.


However, a great article by rxi offers a @(https://rxi.github.io/a_simple_undo_system.html simple alternative). 
The system described offers a lower-level perspective that handles things generically not at a type level, 
instead targeting the less structured binary representation of the data being changed. It reminds me of similar 
sentiment from @(https://www.rfleury.com/p/emergence-and-composition Ryan Fleury's posts). Many techniques I've 
learned from following Ryan are implemented at this low level of abstraction, chiefly 
@(https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator Arenas). Programming this way has 
opened my eyes to the composability and leverage against problems you can get from a **data-generic**, 
rather than **type-generic** approach.


rxi's article is great at explaining the system and resultant immediate-mode undo api, but is open-ended on implementation. 
In the rest of this article I'll walk through how I implemented the system using arena allocators, and how I used 
and added to the system for my games. Here's some footage of the final result in my editor:


!(/assets/editorpreview.mp4)

##impl Implementation 
My desired API is roughly the same as rxi's:
```
void undo_push(void *source, s64 size); /* Mark regions that will potentially change */
void undo_commit(); /* Check marked regions and finalize action or discard regions. */
void undo();
void redo();

/* Usage */
for (;;) { /* Event/Game loop */
  handle_events();

  if (mouse.is_pressed) {
    undo_push(bitmap, sizeof(bitmap))
    set_pixel(bitmap, mouse.x, mouse.y);
  }

  if (!mouse.is_pressed) { undo_commit(); }

  draw();
}
```
At the core of the undo system are the `Delta` structs which are the basic primitives for constructing commits, 
which together comprise an undo/redo action that can be surfaced to the user. 
```
typedef struct {
    s64 size;
    u8* copy;
    u8* source;
} Delta;
```
The `Undo` struct holds all the Deltas and other information needed for the overall Undo system. The copy arena 
will hold the copies of all the data pointed to by the deltas. 
For my current use case I just have a global instance of this struct, so the methods just operate on that instance.
You can easily switch over to passing the Undo struct explicitly if needed. 
```
#define UNDO_MEMORY MB(10)
#define MAX_UNDOS 0x10000 /* ~3 MB of undos, 7 MB for copied state */
typedef struct {
    Arena* copy;
    Delta delta[MAX_UNDOS];
    s32 undo;
    s32 redo; /* >= undo */
    s32 temp; /* >= redo */
    u8 *copy_redo_start;
    u8 *copy_temp_start;
    s32 tag;
} Undo;
Undo *UNDO;
```
Following rxi's recommendations, the undo, redo, and temp state are stored in 3 stacks. However in my implementation 
all of these stacks live in the same `Undo->delta` array, where the undo stack is elements [0, undo), redo is [undo, redo),
and temp is [redo, temp). The copies allocated for deltas corresponding to each stack will have the same order as the stacks 
themselves, but will vary in size according to the data. 


The `undo_push` and `undo_commit` functions are the core of the api. `undo_push` marks regions that may change by pushing 
them onto the temp stack. `undo_commit` then checks each currently marked region for any changes. The copies for changed regions 
are moved so that they are next to previous undo information, overwriting any redo information.
```
void undo_push(void* source, s64 size) {
    u8* copy = Arena_take(UNDO->copy, size);
    memcpy(copy, source, size);
    UNDO->delta[UNDO->temp] = (Delta){
        .size = size,
        .copy = copy,
        .source = source,
    };
    UNDO->temp++;
}

void undo_commit() {
    UNDO->tag = 0; /* Ignore for now : ) */

    s32 changes = 0;
    u8* new_pos = UNDO->copy_temp_start;
    
    for (s32 i = UNDO->redo; i < UNDO->temp; i++) {
        Delta c = UNDO->delta[i];
        if (memcmp(c.copy, c.source, c.size)) {
            if (changes == 0 && UNDO->redo != UNDO->undo) {
                /* Set pos to overwrite redos */
                new_pos = UNDO->copy_redo_start;
            }
        
            c.copy = memcpy(new_pos, c.copy, c.size);
            UNDO->delta[UNDO->undo++] = c;
            new_pos += c.size;
            changes++;
        }
    }
    
    if (changes) { /* Add header for commit */
        UNDO->delta[UNDO->undo++] = (Delta){ 
            .size = changes,
            .copy = ((u8*)UNDO->copy) + UNDO->copy->pos,
            .source = 0, /* null source identifies headers */
        };
        UNDO->redo = UNDO->undo; 
    } 

    UNDO->temp = UNDO->redo;
    Arena_resetp(UNDO->copy, new_pos);
    UNDO->copy_temp_start = new_pos;
}
```
Given a stack of undo information built this way, the implementation of undo is straightforward. The amount of deltas for 
the commit is identified using the header delta, and then those are looped over and swap their copy with what is currently
at the source. 
```
void swap_delta(Delta d) {
    for (s64 j = 0; j < d.size; j++) {
        u8 temp = *d.copy;
        *d.copy++ = *d.source;
        *d.source++ = temp;
    }
}
void undo() {
    if (UNDO->undo > 0) {
        s32 N = UNDO->undo-1;
        Delta header = UNDO->delta[N];
        s32 changes = header.size;
        s32 n = N - changes;
        for (s32 i = N-1; i >= n; i--) {
            swap_delta(UNDO->delta[i]);
        }

        Delta first = UNDO->delta[n];
        UNDO->copy_redo_start = first.copy;
        UNDO->undo = n;
    }
}
```
The swap in the undo is important, as these same deltas now can be used to redo by swapping again. The redo function needs to 
scan to find its header, but otherwise is quite dual to undo as you would expect:
```
void redo() {
    if (UNDO->redo - UNDO->undo > 0) {
        /* Do not redo while temp stack has delta 
           this could be fine if they touch disjoint memory, but if 
           there is overlap things will break!
        */
        ASSERT(UNDO->redo == UNDO->temp);
        /* Find header for first change */
        s32 N = UNDO->undo;
        for (; N < UNDO->redo; N++) {
            Delta c = UNDO->delta[N];
            if (!c.source) {
                break;
            }
        }
        
        Delta header = UNDO->delta[N];
        s32 changes = header.size;
        s32 n = N - changes;
        for (s32 i = n; i < N; i++) {
            swap_delta(UNDO->delta[i]);
        }

        UNDO->copy_redo_start = header.copy;
        UNDO->undo = N+1;
    }
}
```
And that's it for the basic API implementation! In the following sections I'll show some other additions to this core that I made as 
I added the functionality to my editor. But first a larger example code showing the API in use: 
```
#define NSQUARES 10
static Color squares[NSQUARES];
static b32 changed[NSQUARES];
static Color text_color = {.r=0xFF, .g=0xFF, .b=0xFF, .a=0xFF};
static Color end_color = {0};
static char* message = "";

Rectangle r = { .width = 16, .height = 16 };
for (s32 i = 0; i < NSQUARES; i++) {
    b32 selected = 0;
    if (CheckCollisionPointRec(G->mouse, r)) {
        selected = 1;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !changed[i]) {
            changed[i] = 1;
            undo_push(squares+i, sizeof(Color));
            squares[i].r = randu32(&G->rng);
            squares[i].g = randu32(&G->rng);
            squares[i].b = randu32(&G->rng);
            text_color = HYELLOW;
        }
    }

    Color c = squares[i]; c.a = 0xFF;
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 2, (selected)? HBLUE : (changed[i])? HGREEN : HDBLUE);
    r.x += r.width+2;
}

if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    undo_commit(HBLUE); 
    memset(changed, 0, sizeof(changed));
    end_color = text_color = HYELLOW; message = "Commit!";
}

if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
    if (!IsKeyDown(KEY_LEFT_SHIFT)) {
        undo(); 
        end_color = text_color = HRED; message = "Undo!";
    } else {
        redo(); 
        end_color = text_color = HBLUE; message = "Redo!";
    }
}

char buf[128];
stbsp_sprintf(buf, "%d <= %d <= %d", UNDO->undo, UNDO->redo, UNDO->temp);
DrawText(buf, 32, 32, 13, text_color);
DrawText(message, 32, 64, 13, end_color);

text_color = LERP_COLOR(text_color, HFF, 0.2);
end_color.a = LERP(end_color.a, 0, 0.05);
```
This gif of the demo shows how the undo stack is built up with commits, can be undone/redone, and how redo history is 
overwritten with new changes:


!(/assets/coloredcubes.gif)
##problems Problems 
In the simple example given above, there is only one type of edit action happening at any time. In my more complicated 
level editor I needed to make sure that two different actions aren't in progress simultaneously, as this would corrupt the
temp stack. This was simple to add, by wrapping relevant code with an additional check, `undo_begin`:
```
#define undo_begin() undo_begin_ex(__LINE__)
b32 undo_begin_ex(s32 tag) {
    b32 can_initiate = UNDO->redo == UNDO->temp || UNDO->tag == tag;
    if (can_initiate) UNDO->tag = tag;
    return can_initiate;
}
```
The tag is reset by a commit:
```
void undo_commit() {
    UNDO->tag = 0; /* Remember this? */

    /* .. snip .. */
}
```
And the usage code looks like:
```
for (;;) { /* event loop */

    if (undo_begin()) { /* Action 1 */
        if (IsMouseDown(MOUSE_BUTTON_LEFT)) {
            undo_push(..., ...);

            // Do action
        } else if (IsMouseReleased(MOUSE_BUTTON_LEFT)) {
            undo_commit();
        }
    }

    if (undo_begin()) { /* Action 1 */
        if (IsMouseDown(MOUSE_BUTTON_RIGHT)) {
            undo_push(..., ...);

            // Do action
        } else if (IsMouseReleased(MOUSE_BUTTON_RIGHT)) {
            undo_commit();
        }
    }

}
```
Both of these actions can progress across multiple frames, but because of the "locking" provided by `undo_begin` they 
will never be in progress at the same time. 


A more troublesome issue is that sometimes entity state used by the editor would be changed by other code for the game (In my engine 
the user can swap back and forth between the editor and engine). This would cause the undo state targeting the same memory 
to become invalid, meaning the user's expected undo or redo would not work. There are a few ways I thought of to fix this,
with varying levels of complexity. The easiest to implement in my case is to keep another copy of the relevant state when 
switching to the game, and then swap it back when the editor is opened. 


In a more complicated situation, it may be better to add a layer on top of the simple undo system that allows for more 
serialized undo/redo commands. Regardless, I think for these more complex situations that the simple api will provide 
a great foundation for the more complex implementation. It's easier to add structure on top of something formless than 
to try and handle formless situations with structure. 

##potential Upgrades 
Because of how simple the undo system is, it's easy to store extra information alongside the deltas. In my editor I added information 
about the current size and color of the cursor rectangle at each push, which lends a great visual flair to the undos and redos:
```
void undo_push(void* source, s64 size) {
    /* ... */
    UNDO->temp_rect = RectangleGrow(UNDO->temp_rect, cursor_rect);
}

void undo_commit(Color c) {
    /* ... */
    if (changes) { /* Add header for commit */
        UNDO->delta[UNDO->undo++] = (Delta){ 
            .size = changes,
            .copy = ((u8*)UNDO->copy) + UNDO->copy->pos,
            .source = 0, /* null source identifies headers */
            .cursor_color = c,
            .cursor_rect  = UNDO->temp_rect,
        };
        UNDO->redo = UNDO->undo; 
    } 
    /* ... */
}
void undo() {
    if (UNDO->undo > 0) {
        /* ... */

        cursor_color = header.cursor_color;
        cursor_rect = header.cursor_rect;
        cursor_lerp = 0; cursor_lerp_reset = 3;
        /* Invert add/delete colors for undos */
        Color c = HRED;
        if (!memcmp(&cursor_color, &c, sizeof(Color))) {
            cursor_color = HBLUE;
        } else {
            Color c = HBLUE; 
            if (!memcmp(&cursor_color, &c, sizeof(Color))) cursor_color = HRED;
        }
    }
}

void redo() {
    if (UNDO->redo - UNDO->undo > 0) {
        /* ... */
        
        cursor_color = header.cursor_color;
        cursor_rect = header.cursor_rect;
        cursor_lerp = 0; cursor_lerp_reset = 3;
    }
}
```

!(/assets/undoingyourmom.mp4)

There are also performance enhancements that could be implemented in the base layer, such as scanning the changes in smaller 
chunks and only committing what has actually changed. Even better might be applying general purpose compression to the copies.
But for me the simple implementation has great performance for my application and has been great to use! I hope this write up 
gives you a good starting point for doing your own implementation of a simple undo system.
