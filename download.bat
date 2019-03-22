if not exist "./http-server" mkdir http-server
powershell -Command "(New-Object Net.WebClient).DownloadFile('http://example.com', 'http-server/objectbox-http-server.exe')"
