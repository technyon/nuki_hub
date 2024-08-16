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

if os.path.exists("src/Config.h"):
    with open("../src/Config.h", "rb") as file_a, open("src/Config.h", "rb") as file_b:
        if file_a.read() != file_b.read():
            shutil.copy2("../src/Config.h", "src/Config.h")
else:
    shutil.copy2("../src/Config.h", "src/Config.h")

regex = r"\#define NUKI_HUB_DATE \"(.*)\""
content_new = ""
file_content = ""

with open ('src/Config.h', 'r' ) as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex, "#define NUKI_HUB_DATE \"" + datetime.now(timezone.utc).strftime("%Y-%m-%d") + "\"", file_content, flags = re.M)

if content_new != file_content:
    with open('src/Config.h', 'w') as writefile:
        writefile.write(content_new)

if os.path.exists("src/main.cpp"):
    with open("../src/main.cpp", "rb") as file_a, open("src/main.cpp", "rb") as file_b:
        if file_a.read() != file_b.read():
            shutil.copy2("../src/main.cpp", "src/main.cpp")
else:
    shutil.copy2("../src/main.cpp", "src/main.cpp")

recursive_purge("managed_components", ".component_hash")

board = env.get('BOARD_MCU')

if os.path.exists("sdkconfig.updater_" + board):
  f1 = 0;
  f2 = 0;
  f3 = 0;
  f4 = os.path.getmtime("sdkconfig.updater_" + board)

  if os.path.exists("sdkconfig.defaults." + board):
    f1 = os.path.getmtime("sdkconfig.defaults." + board)

  if os.path.exists("sdkconfig.release.defaults"):
    f2 = os.path.getmtime("sdkconfig.release.defaults")

  if os.path.exists("sdkconfig.defaults"):
    f3 = os.path.getmtime("sdkconfig.defaults")

  if(f1 > f4 or f2 > f4 or f3 > f4):
    os.remove("sdkconfig.updater_" + board)