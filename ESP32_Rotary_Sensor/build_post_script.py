import sys
import shutil
import time
import os
import json
import configparser
from zipfile import ZipFile
Import("env", "projenv")

config = configparser.ConfigParser()
config.read('config.ini')
program = config['DEFAULT']['program']
bin_path = r"./.pio/build/esp32-s3-devkitc-1/firmware.bin"
main_path = r"./src/main.cpp"
version_path = r"./src/Version.h"
newVersion = ''

def increment_json():
    print("Updating JSON")
    _configPath = r".\Releases\OTA_Config.json"
    with open(_configPath) as config_file:
        data=json.load(config_file)
    for item in data:
        for config in data[item]: 
            if config['Config'] == program:
                # newVersion = f'{int(config["Version"][-4:])+1:04d}'
                print(f'Old Version {config["Version"]}')
                newConfig = config['Version'][:-8]+newVersion
                config['Version']=newConfig
                config['URL']=config['URL'][:-12]+newVersion+'.bin'
                print(f'New Version {config["Version"]}')
                with open(_configPath, 'w') as outfile:
                    json.dump(data, outfile, indent= 4)
                outfile.close()
    return

# def increment_json():
#     _configPath = r".\Releases\OTA_Config.json"
#     with open(_configPath, 'r') as file:
#         dataTop = json.load(file)
#     for item in dataTop:
#         data = dataTop[item]
#         if (data['Config'] == program) & (len(newVersion) > 2):
#             url = data["URL"]
#             urlSplit = os.path.split(url)
#             binaryFile = urlSplit[-1]
#             newBinaryFile = program + "_" + newVersion + ".bin"
#             newURL = urlSplit[0] + "/" + newBinaryFile
#             data["Version"] = newVersion
#             data["URL"] = newURL
            
#             with open(_configPath, "w") as write_file:
#                 json.dump(data, write_file, indent=4)
    


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
    for item in os.listdir(".\Releases"):
        if item.endswith(".bin"):
            os.remove(rf".\Releases\{item}")
    shutil.copy(bin_path, f'.\Releases\{program}_{newVersion}.bin')

def backup():
    backup_dir = rf".\backups\{program}_{newVersion}"
    try:
        os.makedirs(backup_dir)
    except:
        pass
    zip_items = ["src","lib","data"]
    with ZipFile(rf"{backup_dir}.zip",'w') as zip_object:
        for item in zip_items:
            for folder_name, sub_folders, file_names in os.walk(rf'./{item}'):
                for filename in file_names:
                    # Create filepath of files in directory
                    file_path = os.path.join(folder_name, filename)
                    # Add files to zip file
                    zip_object.write(file_path, os.path.basename(file_path))
# backup()
increment_version()
increment_json()
env.AddPostAction("buildprog", copy_firmware)
env.AddPostAction("upload", copy_firmware)

print("")
print("#########################################################")