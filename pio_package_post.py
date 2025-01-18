""" PlatformIO POST script execution to copy artifacts """

Import("env")
import re, shutil, os
from pathlib import Path

def get_board_name(env):
    board = env.get('BOARD_MCU')
    
    if env.get('BOARD') == 'nuki-esp32solo1':
        board = 'esp32solo1'
    elif env.get('BOARD') == 'nuki-esp32-s3-oct':
        board = 'esp32s3oct'
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

    if "partitions.bin" in file.name:
        shutil.copy(file, f"{target_dir}/nuki_hub.{file.name}")
    elif "firmware" in file.stem:
        shutil.copy(file, f"{target_dir}/nuki_hub_{board}{file.suffix}")
        shutil.copy(f"{project_dir}/updater/release/{board}/updater.bin", f"{target_dir}/nuki_hub_updater_{board}{file.suffix}")
    else:
        shutil.copy(file, f"{target_dir}/{file.name}")

def merge_bin(source, target, env):
    #if not env.get('BUILD_TYPE') in ['release']:
    #    return

    board = get_board_name(env)
    chip = env.get('BOARD_MCU')
    target_dir = create_target_dir(env)
    target_file = f"{target_dir}/webflash_nuki_hub_{board}.bin"

    app_position = "0x10000"
    app_path = target[0].get_abspath()

    flash_args = list()
    flash_args.append(app_position)
    flash_args.append(app_path)

    for position, bin_file in env.get('FLASH_EXTRA_IMAGES'):
        if "boot_app0.bin" in bin_file:
            bin_file = "resources/boot_app0.bin"
        flash_args.append(position)
        flash_args.append(bin_file)

    flash_args.append("0x2B0000")
    flash_args.append(f"{target_dir}/nuki_hub_updater_{board}.bin")

    cmd = f"esptool.py --chip {chip} merge_bin -o {target_file} --flash_mode dio --flash_freq keep --flash_size keep " + " ".join(flash_args)
    env.Execute(cmd)

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

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)