import sys
import shutil
import time
import os
import json
Import("env", "projenv")

updateJson = True
platform = "Sprayer"
program = "Sprayer_Fold_Controller"
source_path = rf"C:\Users\NolteS\Documents\Github\{platform}\{program}\.pio\build\esp32-s3-devkitc-1\firmware.bin"
dest_path = rf"C:\Users\NolteS\Documents\Github\{platform}"
backup_path = rf"C:\Users\NolteS\Documents\Github\{platform}\Backups"
main_path=rf"C:\Users\NolteS\Documents\Github\{platform}\{program}\src\definitions.h"
config_path = rf"C:\Users\NolteS\Documents\Github\{platform}\OTA_Config.json"
newVersion = ''

def incrementJson():
    print("Updating JSON")
    with open(config_path) as config_file:
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
                with open(config_path, 'w') as outfile:
                    json.dump(data, outfile)
    return

def deleteOld():
    for file in os.listdir(dest_path):
        if program in file:
            if '.bin' in file:
                print(file)
                try:
                    shutil.move(os.path.join(dest_path,file),backup_path)
                except:
                    pass

def increment_version():
    global newVersion
    start=0
    stop=0
    with open(main_path) as file:
        lines = file.readlines()

    for i in range(len(lines)):
        if "#define VERSION" in lines[i]:
            oldVersion = lines[i].replace("#define VERSION ","").replace('"','')
            vars = oldVersion.split('.')
            varNew=int(vars[-1])+1
            vars[-1] = f"{varNew:04d}"
            print(vars)
            newVersion = f'{vars[0].strip()}.{vars[1].strip()}.{vars[2]}'
            newVersion_line=f'#define VERSION "{newVersion}"\n'
            lines[i] = newVersion_line

    with open(main_path, 'w') as file: 
        file.writelines(lines)



def copy_firmware(source, target, env):
    time.sleep(1)
    print("after_upload")
    shutil.copy(source_path, os.path.join(dest_path,f'{program}_{newVersion}.bin'))
    if updateJson:
        incrementJson()
    # do some actions

increment_version()
deleteOld()
env.AddPostAction("buildprog", copy_firmware)
print("#########################################################")