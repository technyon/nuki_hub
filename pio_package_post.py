""" PlatformIO POST script execution to copy artifacts """

Import("env")
import re, shutil, os
from pathlib import Path

def get_board_name(env):
    board = env.get('BOARD_MCU')
    
    if env.get('BOARD') == 'nuki-esp32solo1':
        board = 'esp32solo1'
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
    project_dir = env.get('PROJECT_DIR')
    board = get_board_name(env)

    if "firmware" in file.stem:
        shutil.copy(file, f"{target_dir}/nuki_hub_{board}{file.suffix}")
        shutil.copy(f"{project_dir}/updater/release/{board}/updater.bin", f"{target_dir}/nuki_hub_updater_{board}{file.suffix}")
    elif "bootloader" in file.stem:
        shutil.copy(file, f"{target_dir}/nuki_hub_bootloader_{board}{file.suffix}")
    elif "partitions" in file.stem:
        shutil.copy(file, f"{target_dir}/nuki_hub_partitions_{board}{file.suffix}")
    else:
        shutil.copy(file, f"{target_dir}/{file.name}")

def package_last_files(source, target, env):
    files = ["resources/boot_app0.bin", "resources/how-to-flash.txt"]

    target_dir = create_target_dir(env)
    for file in files:
        file = Path(file)
        shutil.copy(file, f"{target_dir}/{file.name}")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_files)
env.AddPostAction("$PROJECT_DIR/updater/release/" + get_board_name(env) + "/updater.bin", copy_files)
env.AddPostAction("$BUILD_DIR/firmware.bin", package_last_files)
env.AddPostAction("$BUILD_DIR/partitions.bin", copy_files)
env.AddPostAction("$BUILD_DIR/bootloader.bin", copy_files)
env.AddPostAction("$BUILD_DIR/firmware.elf", copy_files)