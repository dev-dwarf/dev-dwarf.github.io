@REM I push a subtree of the repo to github so that it deploys with the deploy folder as root.
@echo off
site.exe
git subtree push --prefix=deploy origin gh-pages
