import re, shutil, os
from datetime import datetime, timezone

regex = r"\#define NUKI_HUB_DATE \"(.*)\""
content_new = ""

with open ('src/Config.h', 'r' ) as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex, "#define NUKI_HUB_DATE \"" + datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S") + "\"", file_content, flags = re.M)

with open('src/Config.h', 'w') as writefile:
    writefile.write(content_new)