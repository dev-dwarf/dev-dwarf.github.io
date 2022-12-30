@REM I push a subtree of the repo to github so that it deploys with the deploy folder as root.
@echo off
site.exe
git remote set-url origin git@github.com:dev-dwarf/dev-dwarf.github.io.git
git subtree push --prefix=deploy origin gh-pages
