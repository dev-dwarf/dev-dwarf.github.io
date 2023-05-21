##center Programming
---
##engine Custom Game Engine (very WIP)
Custom game engine codebase built in C++ with minimal libraries.


Not much to say about this yet except that I have been deep-diving into Platform APIs for Windowing, Input, Rendering, Sound, and seemingly endless other topics. Once I duct-tape some games together with this I'll have more to say!

## Static Site Generator (WIP)
A Static Site Generator used to make this website, made in (C-like) C++. 
Features:
* Compile a Markdown-like language to html.
* Add html boilerplate for each page automatically.
* Easily customizeable, Small Codebase (<1000 LOC).
Source on @(https://github.com/dev-dwarf/dev-dwarf.github.io Github). 
Read my posts about making this @(/writing.html#making-a-ssg1.html here).

## Handmade Math 2.0
I was hired to bring HandmadeMath, A single-file C/C++ library implementing math useful for 3d graphics and games, to its 2.0 milestone as described in this @(https://github.com/HandmadeMath/HandmadeMath/issues/147 github issue). You can see my changes in one @(https://github.com/HandmadeMath/HandmadeMath/pull/149 big pull request). A summary:
- Implemented 2x2 and 3x3 matrices, with basic operations.
- Implemented determinants and inverses for all matrices, with several fast paths for common types of 4x4 matrices. 
- Created a portable tool in C to update user's code to the new version, to help with the many breaking changes that were made to the API. 
The code for the update tool can be found in a seperate @(https://github.com/dev-dwarf/HMM2.0UpdateTool github repository).

## LCF (WIP)
My personal @(http://nothings.org/stb.h stb.h) style standard libary for my C/C++ projects. Features:
* Shorter type names and macros for commonly used functionality.
* Arena-oriented memory management library.
* Length-based string library (composes with Arena library).
Source is not available yet because I update this very frequently and don't want others to use it yet. It may seem unnecessary to do this to some but I've found that making my own standard library has given me deeper insight into how the available primitives in a language affect my programming style, as well as greatly increasing my comfort level in my C/C++ codebases (Feels worth it just for string handling alone!)

##center Finished Games
---
@{project, FEWAR-DVD, December 2021, https://store.steampowered.com/app/1769510/FEWARDVD/, /assets/FewarDVD.png}
Speedrunning arcade game with maze-like procedurally generated levels and eclectic visual style. Positive reviews on steam.
> A rapid arcade game. Avoid the swords. Find the key. Enter the portal. It's a migraine.
> 
> 
> “Every run is short enough that you think 'Ok, just one more go' and before you know it you've been acupunctured to oblivion. ... This is a surreal arcade game that you should experience.” 
> -- *PC Gamer UK*
> 
> "Here’s a great game: a scratchy, noisy, ridiculously tough arcade game that I can’t stop one-more-trying."
> 
> -- *@(https://buried-treasure.org/2022/01/quicky-fewar-dvd/ Buried Treasure)*
@{project-end}

@{project, SELF, February 2021, https://dev-dwarf.itch.io/self, /assets/SELF.gif}
2D Platforming game with precise controls. 100,000+ players, licensed by
Cool Math Games and Armor Games. Featured on @(https://superraregames.com/products/srg-mixtape-volume-4 Super Rare Mixtape Vol 4).
> who are you
> 
> SELF is a short game about a lost soul
> 
> recover the fragments of your SELF
> 
> be touched by the higher being
@{project-end}

@{project, Lianthus, September 2020, https://dev-dwarf.itch.io/lianthus, /assets/Lianthus.png}
3D adventure game with unique graphical style. 5000+ downloads on itch.io. Featured on @(https://superraregames.com/products/srg-mixtape-volume-1 Super Rare Mixtape Vol 1).
> A small adventure game with sunflowers, magic, and skeletons. After the darkness is unleashed, journey through the light and dark worlds, rediscover your powers, and seal the dark away.
@{project-end}

##center Jam Games
---
Every so often I get together with some friends and do a game jam! You can find them all on @(https://dev-dwarf.itch.io itch.io). 

Here are some screenshots:
<div class="project">
<div class="project-text">
@(https://mmatt-ugh.itch.io/the-saloon !(/assets/the-saloon.gif))
@(https://dev-dwarf.itch.io/blood-and-voltz !(/assets/blood-and-voltz1.gif))
</div>
<div class="project-image">
@(https://dev-dwarf.itch.io/ld47 !(/assets/fear-of-gears.gif))
@(https://dev-dwarf.itch.io/c-co !(/assets/control-co1.gif))
</div>
</div>
