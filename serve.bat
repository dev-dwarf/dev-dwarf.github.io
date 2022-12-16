@REM Serve the site on localhost
@echo off
pushd deploy
start "local server" py -m http.server
start chrome.exe "http://localhost:8000/index.html"
popd  
