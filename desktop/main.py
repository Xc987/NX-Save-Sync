import socket
import json
import zipfile
import os
import sys
import shutil
from pathlib import Path

def checkConfig():
    appdataPath = os.getenv('LOCALAPPDATA')
    if not appdataPath:
        appdataPath = Path.home() / 'AppData' / 'Local'
    configDir = Path(appdataPath) / 'NX-Save-Sync'
    configDir.mkdir(exist_ok=True)
    configFile = configDir / 'config.json'
    if not configFile.exists():
        with open(configFile, 'w') as f:
            print("\nPlease input the Switch ip")
            hostIp = input("> ")
            json.dump({"host": hostIp}, f, indent=4) 
    return configFile

def downloadZip(host, port, file_name):
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((host, port))
        request = f"GET / HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
        client_socket.sendall(request.encode())
        response = b""
        while True:
            data = client_socket.recv(1024)
            if not data:
                break
            response += data
        if b"200 OK" not in response:
            print("File not found or server error!")
            input("Press enter to exit")
            sys.exit(0)
        header_end = response.find(b"\r\n\r\n")
        if header_end == -1:
            print("Invalid HTTP response!")
            input("Press enter to exit")
            sys.exit(0)
        file_content = response[header_end + 4:]
        with open(file_name, "wb") as file:
            file.write(file_content)
        print(f"File '{file_name}' downloaded successfully.")
        client_socket.close()
        shutdown_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        shutdown_socket.connect((host, port))
        shutdown_socket.sendall(b"SHUTDOWN")
        print("Shuting down server.")
    except Exception as e:
        print(f"{e}")
        input("Press enter to exit")
        sys.exit(0)
    finally:
        if 'client_socket' in locals():
            client_socket.close()
        if 'shutdown_socket' in locals():
            shutdown_socket.close()

if __name__ == "__main__":
    print("Welcome to NX save sync. Select an option by entering 1 or 2.")
    print("1. Pull current save file from switch to pc")
    print("2. Push newer save file from pc to switch (SOON)")
    selected = int(input("> "))
    if (selected == 1):
        configFile = checkConfig()
        if getattr(sys, 'frozen', False):
            scriptDir = os.path.dirname(sys.executable)
        elif __file__:
            scriptDir = os.path.dirname(__file__)
        with open(configFile, 'r') as file:
            data = json.load(file)
            host = data.get("host")
            if host:
                print(f"\nConnecting to host: {host}")
        downloadZip(host, 8080, "temp.zip")
        zipFile = os.path.join(scriptDir, "temp.zip")
        if os.path.exists(zipFile):
            print("Unzipping temp.zip")
        else:
            print("Couldnt find temp.zip!")
            input("Press enter to exit")
            sys.exit(0)
        with zipfile.ZipFile(zipFile, 'r') as zipRef:
            zipRef.extractall(scriptDir)
        tempDir = os.path.join(scriptDir, "temp")
        if os.path.exists(tempDir) and os.path.isdir(tempDir):
            subFolder = [f.name for f in os.scandir(tempDir) if f.is_dir()]
            if len(subFolder) == 1:
                with open(configFile, 'r') as file:
                    data = json.load(file)
                    titleDir = os.path.join(tempDir, subFolder[0])
                    titleFile = os.path.join(titleDir, "Title_Name/")
                    entries = os.listdir(titleFile)
                    files = [entry for entry in entries if os.path.isfile(os.path.join(titleFile, entry))]
                    if len(files) == 1:
                        titleName = files[0]
                        shutil.rmtree(titleFile)
                    if not subFolder[0] in data:
                        print("Please enter the emulator save directory for ", titleName, ". ", "This will erase the entire directory!", sep="")
                        emuPath = (input("> "))
                        with open(configFile, 'r') as file:
                            data = json.load(file)
                            data[subFolder[0]] = [emuPath, titleName]
                        with open(configFile, 'w') as file:
                            json.dump(data, file, indent=4)
            else:
                print("Couldnt find any TID subfolders in /temp/!")
                input("Press enter to exit")
                sys.exit(0)
        else:
            print("Couldnt find temp folder!")
            input("Press enter to exit")
            sys.exit(0)
        with open(configFile, 'r') as file:
            data = json.load(file)
            titleArray = data[subFolder[0]]
            dstDir = titleArray[0]
        srcDir = os.path.join(tempDir, subFolder[0])
        if os.path.exists(dstDir):
            print(f"Deleting any existing save file in {dstDir}")
        else:
            print(f"The directory {dstDir} does not exist!")
            input("Press enter to exit")
            sys.exit(0)
        for item in os.listdir(dstDir):
                itemPath = os.path.join(dstDir, item)
                try:
                    if os.path.isfile(itemPath) or os.path.islink(itemPath):
                        os.unlink(itemPath)
                    elif os.path.isdir(itemPath):
                        shutil.rmtree(itemPath)
                except Exception as e:
                    print(f"Failed to delete {itemPath}. Reason: {e}")
        print(f"Moving save file from {srcDir} to {dstDir}")
        for item in os.listdir(srcDir):
            srcItem = os.path.join(srcDir, item)
            dstItem = os.path.join(dstDir, item)
            if os.path.isfile(srcItem):
                shutil.copy2(srcItem, dstItem)
            elif os.path.isdir(srcItem):
                shutil.copytree(srcItem, dstItem)
        print("Deleteing temp.zip file")
        os.remove(zipFile)
        print("Deleteing temp folder")
        shutil.rmtree(tempDir)
    input("Press enter to exit")