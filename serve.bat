@REM Serve the site on localhost
pushd docs
start "local server" py -m http.server
start chrome.exe "http://localhost:8000/index.html"
popd 
