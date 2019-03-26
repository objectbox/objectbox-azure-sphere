if not exist "./http-server" mkdir http-server
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://github.com/objectbox/objectbox-azure-sphere/releases/download/V1.0.0/objectbox-http-server.exe', 'http-server/objectbox-http-server.exe')"
