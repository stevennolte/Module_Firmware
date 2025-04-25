import sys
import shutil
import time
import os
import json
import configparser
from zipfile import ZipFile
Import("env", "projenv")

version_path = r"./include/Version.h"
bin_path = r"./.pio/build/esp32-s3-devkitc-1/firmware.bin"
main_path = r"./src/main.cpp"
program = ""



#
def getInfo():
    with open(version_path, 'r',encoding='utf-8',) as file:
        lines = file.readlines()
    for i in range(len(lines)):
        if lines[i].startswith("uint8_t VERSION[3] ="):
            version = lines[i].replace("uint8_t VERSION[3] = {","").replace("};","").replace(" ","").replace("\n","").split(",")
            # version = [int(i) for i in version]
            # return version
    for i in range(len(lines)):
        global program
        if lines[i].startswith("#define NAME "):
            program = lines[i].replace("#define NAME ","").replace('"','').replace("\n","")
            print(program)
    return

def increment_version():
    global newVersion
    start=0
    stop=0
    print(os.path.isfile(main_path))
    lines = []
    with open(version_path, 'r',encoding='utf-8',) as file:
        lines = file.readlines()
    print("imported")
    for i in range(len(lines)):
        print(lines[i])
        if "#define VERSION" in lines[i]:
            oldVersion = lines[i].replace("#define VERSION ","").replace('"','')
            vars = oldVersion.split('.')
            varNew=int(vars[-1])+1
            if varNew >= 255:
                vars[-1] = f"{0:04d}"
                vars[-2] = f"{int(vars[-2])+1}"
            else:
                vars[-1] = f"{varNew:04d}"
            print(vars)
            newVersion = f'{vars[0].strip()}.{vars[1].strip()}.{vars[2]}'
            newVersion_line=f'#define VERSION "{newVersion}"\n'
            print(newVersion_line)
            lines[i] = newVersion_line
            break

    with open(version_path, 'w', encoding='utf-8') as file: 
        file.writelines(lines)
    backup_dir = rf".\backups\{program}_{newVersion}"
    
    zip_items = ["src","lib","data"]
    with ZipFile(rf"{backup_dir}.zip",'w') as zip_object:
        for item in zip_items:
            for folder_name, sub_folders, file_names in os.walk(rf'./{item}'):
                for filename in file_names:
                    # Create filepath of files in directory
                    file_path = os.path.join(folder_name, filename)
                    # Add files to zip file
                    zip_object.write(file_path, os.path.basename(file_path))

def copy_firmware(source, target, env):
    # for item in os.listdir(".\Releases"):
    #     if item.endswith(".bin"):
    #         pass
            # os.remove(rf".\{item}")
    print("Copying firmware to data folder")
    shutil.copy(bin_path, f'.\{program}.bin')

# def backup():
#     backup_dir = rf".\backups\{program}_{newVersion}"
#     try:
#         os.makedirs(backup_dir)
#     except:
#         pass
#     zip_items = ["src","lib","data"]
#     with ZipFile(rf"{backup_dir}.zip",'w') as zip_object:
#         for item in zip_items:
#             for folder_name, sub_folders, file_names in os.walk(rf'./{item}'):
#                 for filename in file_names:
#                     # Create filepath of files in directory
#                     file_path = os.path.join(folder_name, filename)
#                     # Add files to zip file
#                     zip_object.write(file_path, os.path.basename(file_path))
# backup()
print(getInfo())
# increment_version()
# increment_json()
env.AddPostAction("buildprog", copy_firmware)
env.AddPostAction("upload", copy_firmware)

print("")
print("#########################################################")