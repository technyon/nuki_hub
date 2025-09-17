""" PlatformIO POST script execution to copy updater """

Import("env")
import glob, re, shutil, os
from pathlib import Path

def get_board_name(env):
    board = env.get('BOARD_MCU')
    if env.get('BOARD') == 'nuki-esp32solo1':
        board = 'esp32solo1'
    elif env.get('BOARD') == 'nuki-esp32-p4-c5':
        board = 'esp32p4c5'        
    elif env.get('BOARD') == 'nuki-esp32gls10':
        board = 'esp32gls10'
    elif env.get('BOARD') == 'nuki-esp32-s3-oct':
        board = 'esp32s3oct'
    elif env.get('BOARD') == 'nuki-esp32-s3-nopsram':
        board = 'esp32s3nopsram'
    elif env.get('BOARD') == 'nuki-esp32dev-nopsram':
        board = 'esp32nopsram'        
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

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_files)