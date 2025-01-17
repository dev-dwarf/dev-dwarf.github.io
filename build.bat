@REM Compile (Unity/Jumbo build, one file)
@echo off
cl -nologo -GR- -Oi -WX -W4 -wd4127 -wd4201 -wd4100 -wd4189 -Zi -FC .\static-site-gen\site.c -Fmsite.map


