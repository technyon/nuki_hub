Import("env")
import re, shutil, os
from datetime import datetime, timezone

def recursive_purge(dir, pattern):
    if os.path.isdir(dir):
        for f in os.listdir(dir):
            if os.path.isdir(os.path.join(dir, f)):
                recursive_purge(os.path.join(dir, f), pattern)
            elif re.search(pattern, os.path.join(dir, f)):
                os.remove(os.path.join(dir, f))

regex = r"\#define NUKI_HUB_DATE \"(.*)\""
content_new = ""

with open ('../src/Config.h', 'r' ) as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex, "#define NUKI_HUB_DATE \"" + datetime.now(timezone.utc).strftime("%Y-%m-%d") + "\"", file_content, flags = re.M)

with open('../src/Config.h', 'w') as writefile:
    writefile.write(content_new)

shutil.copy("../src/main.cpp", "src/main.cpp")
recursive_purge("managed_components", ".component_hash")

if env.get('BOARD_MCU') == "esp32":
  board = "esp32dev"
else:
  board = env.get('BOARD_MCU')

if os.path.exists("sdkconfig.updater_" + board):
  os.remove("sdkconfig." + board)