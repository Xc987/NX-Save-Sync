import socket
import json
import zipfile
import os
import sys
import shutil
from pathlib import Path
import msvcrt
import socket
import threading

def get_key():
    while True:
        if msvcrt.kbhit():
            ch = msvcrt.getch()
            if ch == b'\xe0':
                ch = msvcrt.getch()
                if ch == b'H':
                    return 'up'
                elif ch == b'P':
                    return 'down'
            elif ch == b'\r':
                return 'enter'
            else:
                return ch.decode('ascii')

def clear_lines(n=1):
    for _ in range(n):
        sys.stdout.write('\033[F')
        sys.stdout.write('\033[K')

def checkConfig():
    appdataPath = os.getenv('LOCALAPPDATA')
    if not appdataPath:
        appdataPath = Path.home() / 'AppData' / 'Local'
    configDir = Path(appdataPath) / 'NX-Save-Sync'
    configDir.mkdir(exist_ok=True)
    configFile = configDir / 'config.json'
    if not configFile.exists():
        with open(configFile, 'w') as f:
            print("\n\033[35m[LOAD]\033[0m Please input the Switch ip")
            hostIp = input("\033[35m[LOAD]\033[0m > ")
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
            print("\033[31m[FAIL] File not found or server error!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        header_end = response.find(b"\r\n\r\n")
        if header_end == -1:
            print("\033[31m[FAIL] Invalid HTTP response!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        file_content = response[header_end + 4:]
        with open(file_name, "wb") as file:
            file.write(file_content)
        print(f"\033[32m[ OK ]\033[0m File {file_name} downloaded successfully.")
        client_socket.close()
        shutdown_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        shutdown_socket.connect((host, port))
        shutdown_socket.sendall(b"SHUTDOWN")
        print("\033[32m[ OK ]\033[0m Shuting down server.")
    except Exception as e:
        print(f"\033[31m[FAIL] {e}\033[0m")
        input("Press enter to exit")
        sys.exit(0)
    finally:
        if 'client_socket' in locals():
            client_socket.close()
        if 'shutdown_socket' in locals():
            shutdown_socket.close()

class uploadZip:
    def __init__(self):
        self.shutdown_flag = threading.Event()
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
    def handle_client(self, client_socket):
        try:
            request = client_socket.recv(1024)
            if not request:
                return            
            if b"SHUTDOWN" in request:
                client_socket.send(b"HTTP/1.1 200 OK\r\n\r\nServer shutting down")
                client_socket.close()
                self.shutdown_flag.set()
                try:
                    wake_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    wake_socket.connect(('0.0.0.0', 8080))
                    wake_socket.close()
                except:
                    pass
                return
            if not os.path.exists('temp.zip'):
                response = "HTTP/1.1 404 Not Found\r\n\r\nFile not found"
                client_socket.send(response.encode())
                client_socket.close()
                return
            
            file_size = os.path.getsize('temp.zip')
            headers = (
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/zip\r\n"
                f"Content-Disposition: attachment; filename=\"{os.path.basename('temp.zip')}\"\r\n"
                f"Content-Length: {file_size}\r\n"
                "Connection: close\r\n\r\n"
            )
            client_socket.send(headers.encode())
            
            with open('temp.zip', 'rb') as f:
                while True:
                    try:
                        data = f.read(4096)
                        if not data:
                            break
                        client_socket.send(data)
                    except ConnectionError:
                        break
                        
        except Exception as e:
            print(f"Client handling error: {str(e)}")
        finally:
            try:
                client_socket.close()
            except:
                pass

    def run(self):
        self.server_socket.bind(('0.0.0.0', 8080))
        self.server_socket.listen(5)
        self.server_socket.settimeout(1)
        print("\033[32m[ OK ]\033[0m Server is now running")
        try:
            while not self.shutdown_flag.is_set():
                try:
                    client_socket, addr = self.server_socket.accept()
                    threading.Thread(target=self.handle_client, args=(client_socket,)).start()
                except socket.timeout:
                    continue
                except OSError as e:
                    if not self.shutdown_flag.is_set():
                        print(f"Accept error: {e}")
        finally:
            self.server_socket.close()
            print("\033[32m[ OK ]\033[0m Shutting down server")

if __name__ == "__main__":
    os.system("")
    print("Welcome to NX save sync. Select an option.")
    lines = ["Pull current save file from switch to pc", 
             "Push newer save file from pc to switch"]
    selected = 1
    print(f"\033[44m{lines[0]}\033[0m")
    print(f"{lines[1]}")
    while True:
        key = get_key()
        if key == 'up':
            if (selected != 1):
                clear_lines(2)
                selected = 1
                print(f"\033[44m{lines[0]}\033[0m")
                print(f"{lines[1]}")
        elif key == 'down':
            if (selected != 2):
                clear_lines(2)
                selected = 2
                print(f"{lines[0]}")
                print(f"\033[44m{lines[1]}\033[0m")
        elif key == 'enter':
            break
    configFile = checkConfig()
    if getattr(sys, 'frozen', False):
        scriptDir = os.path.dirname(sys.executable)
    elif __file__:
        scriptDir = os.path.dirname(__file__)
    if (selected == 1):
        with open(configFile, 'r') as file:
            data = json.load(file)
            host = data.get("host")
            if host:
                print(f"\n\033[32m[ OK ]\033[0m Connecting to host: {host}")
        downloadZip(host, 8080, "temp.zip")
        if os.path.exists(os.path.join(scriptDir, "temp.zip")):
            print("\033[33m[WAIT]\033[0m Unzipping temp.zip")
        else:
            print("\033[31m[FAIL] Couldnt find temp.zip!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        with zipfile.ZipFile(os.path.join(scriptDir, "temp.zip"), 'r') as zipRef:
            zipRef.extractall(scriptDir)
        clear_lines(1)
        print("\033[32m[ OK ]\033[0m Unzipping temp.zip")
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
                        print("\033[35m[LOAD]\033[0m Please enter the emulator save directory for", titleName)
                        emuPath = (input("\033[35m[LOAD]\033[0m > "))
                        with open(configFile, 'r') as file:
                            data = json.load(file)
                            data[subFolder[0]] = [emuPath, titleName]
                        with open(configFile, 'w') as file:
                            json.dump(data, file, indent=4)
            else:
                print("\033[31m[FAIL] Couldnt find any TID subfolders in /temp/!\033[0m")
                input("Press enter to exit")
                sys.exit(0)
        else:
            print("\033[31m[FAIL] Couldnt find temp folder!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        with open(configFile, 'r') as file:
            data = json.load(file)
            titleArray = data[subFolder[0]]
            dstDir = titleArray[0]
        srcDir = os.path.join(tempDir, subFolder[0])
        if os.path.exists(dstDir):
            print(f"\033[32m[ OK ]\033[0m Deleting any existing save file in {dstDir}")
        else:
            print(f"\033[31m[FAIL] The directory {dstDir} does not exist!\033[0m")
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
                    print(f"\033[31m[FAIL] Failed to delete {itemPath}!\033[0m")
        print("\033[32m[ OK ]\033[0m Moving save file")
        for item in os.listdir(srcDir):
            srcItem = os.path.join(srcDir, item)
            dstItem = os.path.join(dstDir, item)
            if os.path.isfile(srcItem):
                shutil.copy2(srcItem, dstItem)
            elif os.path.isdir(srcItem):
                shutil.copytree(srcItem, dstItem)
        print("\033[32m[ OK ]\033[0m Deleteing temp.zip file")
        os.remove(os.path.join(scriptDir, "temp.zip"))
        print("\033[32m[ OK ]\033[0m Deleteing temp folder")
        shutil.rmtree(tempDir)
    elif (selected == 2):
        print("\nSelect a title")
        with open(configFile, 'r') as file:
            config = json.load(file)
        keys = []
        paths = []
        titles = []
        for key, value in config.items():
            if key != "host" and isinstance(value, list) and len(value) >= 2:
                keys.append(key)
                paths.append(value[0])
                titles.append(value[1])
        if not titles:
            print("\033[31m[FAIL] No valid entries found in config.json!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        selected = 0
        visible_lines = min(10, len(titles))
        while True:
            start_idx = max(0, selected - visible_lines // 2)
            end_idx = min(len(titles), start_idx + visible_lines)
            start_idx = max(0, end_idx - visible_lines)
            for i in range(start_idx, end_idx):
                prefix = "\033[44m" if i == selected else ""
                print(f"{prefix}{titles[i]}\033[0m")
            key = get_key()
            if key == 'up':
                selected = max(0, selected - 1)
            elif key == 'down':
                selected = min(len(titles) - 1, selected + 1)
            elif key == 'enter':
                break
            if (len(titles) > 10):
                clear_lines(10)
            else:
                clear_lines(len(titles)) 
        print(f"\n\033[96m[INFO]\033[0m Selected title: {titles[selected]}")
        print(f"\033[96m[INFO]\033[0m Selected TID: {keys[selected]}")
        print(f"\033[96m[INFO]\033[0m Title save data path: {paths[selected]}")
        tempDir = os.path.join(scriptDir, "temp")
        os.mkdir(tempDir)
        subFolder = os.path.join(tempDir, keys[selected])
        os.mkdir(subFolder)
        print("\033[33m[WAIT]\033[0m Exporting save data")
        if os.path.exists(paths[selected]) and os.path.isdir(paths[selected]):
            shutil.copytree(paths[selected], subFolder, dirs_exist_ok=True)
        else:
            print("\033[31m[FAIL] Couldnt find the save data folder!\033[0m")
            input("Press enter to exit")
            sys.exit(0)
        clear_lines(1)
        print("\033[32m[ OK ]\033[0m Exporting save data")
        print("\033[33m[WAIT]\033[0m Zipping /temp/ folder")
        
        with zipfile.ZipFile('temp.zip', 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk('temp'):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.relpath(file_path, start='temp')
                    zipf.write(file_path, arcname=os.path.join('temp', arcname))
        clear_lines(1)
        print("\033[32m[ OK ]\033[0m Zipping /temp/ folder")
        IPAddr = socket.gethostbyname(socket.gethostname())
        print(f"\033[96m[INFO]\033[0m PC IP: {IPAddr}")
        server = uploadZip()
        server.run()
        print("\033[32m[ OK ]\033[0m Deleteing temp.zip file")
        os.remove(os.path.join(scriptDir, "temp.zip"))
        print("\033[32m[ OK ]\033[0m Deleteing temp folder")
        shutil.rmtree(tempDir)
    input("Press enter to exit")