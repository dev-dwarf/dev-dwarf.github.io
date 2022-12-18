@REM Serve the site on localhost using python's built in server.
@echo off
pushd deploy
start "local server" py -m http.server
start chrome.exe "http://localhost:8000/index.html"
popd  
