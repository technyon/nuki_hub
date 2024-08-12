""" PlatformIO POST script execution to copy updater """

Import("env")
import glob, re, shutil, os
from pathlib import Path

def get_board_name(env):
    board = env.get('BOARD_MCU')
    if env.get('BOARD') == 'esp32-solo1':
        board = env.get('BOARD').replace('-', '')
    return board

def create_target_dir(env):
    board = get_board_name(env)
    target_dir = env.GetProjectOption("custom_build") + '/' + board
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    return target_dir

def copy_files(source, target, env):
    file = Path(target[0].get_abspath())
    target_dir = create_target_dir(env)
    board = get_board_name(env)

    if "firmware" in file.stem:
        shutil.copy(file, f"{target_dir}/updater.bin")

def remove_files(source, target, env):
    for f in glob.glob("src/*.cpp"):
        os.remove(f)
        
    for f in glob.glob("src/*.h"):
        os.remove(f)
        
    for f in glob.glob("src/networkDevices/*.cpp"):
        os.remove(f)
        
    for f in glob.glob("src/networkDevices/*.h"):
        os.remove(f)

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_files)
env.AddPostAction("$BUILD_DIR/firmware.bin", remove_files)

regex = r"\#define NUKI_HUB_DATE \"(.*)\""
content_new = ""

with open ('../src/Config.h', 'r' ) as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex, "#define NUKI_HUB_DATE \"unknownbuilddate\"", file_content, flags = re.M)

with open('../src/Config.h', 'w') as writefile:
    writefile.write(content_new)
