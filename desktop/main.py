import dearpygui.dearpygui as dpg
from pathlib import Path
import socket
import json
import zipfile
import os
import sys
import shutil
import msvcrt
import threading
import keyboard
import time

input_text = None
output_widget = None
pressed_enter = False

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
        print("\n\033[31m[FAIL] Config file doesnt exist!\033[0m")
        
    return configFile

def changeHost():
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
    else:
        config = {}
        with open(configFile, 'r') as f:
            config = json.load(f)
        print("\n\033[35m[LOAD]\033[0m Please input the Switch IP")
        hostIp = input("\033[35m[LOAD]\033[0m > ")
        config["host"] = hostIp
        with open(configFile, 'w') as f:
            json.dump(config, f, indent=4)

def downloadZip(host, port, file_name):
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 256 * 1024)
        client_socket.connect((host, port))
        request = f"GET / HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
        client_socket.sendall(request.encode())
        header = b""
        file_handle = open(file_name, "wb")
        header_complete = False
        while True:
            data = client_socket.recv(64 * 1024)
            if not data:
                break
            if not header_complete:
                header += data
                if b"\r\n\r\n" in header:
                    header_end = header.find(b"\r\n\r\n")
                    file_handle.write(header[header_end + 4:])
                    header_complete = True
            else:
                file_handle.write(data)

        file_handle.close()
        if b"200 OK" not in header:
            printToWidget("Server error or file not found!\n")
            return
        printToWidget(f"File {file_name} downloaded successfully.\n")
        shutdown_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        shutdown_socket.connect((host, port))
        shutdown_socket.sendall(b"SHUTDOWN")
        shutdown_socket.close()
        printToWidget("Shutting down server.\n")
    except Exception as e:
        printToWidget(f"Error: {e}\n")
    finally:
        client_socket.close()

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

def on_enter_press(event):
    global pressed_enter
    if event.name == 'enter':
        pressed_enter = True

def inputString():
    global input_value, input_entered, pressed_enter
    input_entered = True
    dpg.show_item("input_widget")
    while input_entered:
        keyboard.on_press(on_enter_press)
        input_value = dpg.get_value("input_widget")
        if input_value != "" and pressed_enter:
            input_entered = True
            dpg.hide_item("input_widget")
            input_entered = False
        time.sleep(0.1) 
    keyboard.unhook_all()
    return input_value

def window():
    global output_widget
    dpg.create_context()
    with dpg.font_registry():
        try:
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        font_path = os.path.join(base_path, os.path.join("res", "font.ttf"))
        default_font = dpg.add_font(font_path, 16)
    with dpg.theme() as dark_theme:
        with dpg.theme_component(dpg.mvAll):
            dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (25, 25, 25))
            dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (20, 20, 20))
            dpg.add_theme_color(dpg.mvThemeCol_Text, (230, 230, 230))
            dpg.add_theme_color(dpg.mvThemeCol_Button, (60, 60, 60))
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (80, 80, 80))
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (40, 40, 40))
            dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 5)
    with dpg.window(tag="Primary Window", label="Main Window", no_resize=True, no_collapse=True, no_close=True, no_move=True, width=800, height=600):
        dpg.bind_font(default_font)
        dpg.bind_theme(dark_theme)
        with dpg.tab_bar():
            with dpg.tab(label="Pull"):
                dpg.add_text("Pull current save file from switch to pc")
                with dpg.group(horizontal=True):
                    dpg.add_button(label="Connect to switch", width=150, height=30, callback=pull)
                    dpg.add_input_text(label="Emulator save file path", width=150, tag="input_widget")
                    dpg.hide_item("input_widget")
                output_widget = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget)
            with dpg.tab(label="Push"):
                dpg.add_text("Push newer save file from pc to switch")
                dpg.add_button(label="Start server", width=150, height=30)
            with dpg.tab(label="Config"):
                dpg.add_text("This is the content of Tab 3", indent=20)
                dpg.add_radio_button(
                    label="Options", 
                    items=["Option A", "Option B", "Option C"],
                    horizontal=True
                )
                dpg.add_progress_bar(label="Progress", default_value=0.5, width=200)
    
    dpg.create_viewport(title='NX-Save-Sync', width=600, height=400, min_width=600, min_height=400, max_width=600, max_height=400)
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.start_dearpygui()
    dpg.destroy_context()

def printToWidget(message):
    global output_widget
    if output_widget is not None:
        dpg.set_value(output_widget, dpg.get_value(output_widget) + message)

def pull():
    global output_widget
    dpg.set_value(output_widget, "")
    dpg.show_item(output_widget)
    if getattr(sys, 'frozen', False):
        scriptDir = os.path.dirname(sys.executable)
    elif __file__:
        scriptDir = os.path.dirname(__file__)
    configFile = checkConfig()
    with open(configFile, 'r') as file:
        data = json.load(file)
        host = data.get("host")
        if host:
            printToWidget(f"Connecting to host: {host}\n")
    downloadZip(host, 8080, "temp.zip")
    if os.path.exists(os.path.join(scriptDir, "temp.zip")):
        printToWidget("Unzipping temp.zip\n")
    else:
        printToWidget("Couldnt find temp.zip!\n")
    zip_path = os.path.join(scriptDir, "temp.zip")
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(scriptDir)
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
                    printToWidget(f"Please enter the emulator save directory for {titleName}\n")
                    emuPath = inputString()
                    with open(configFile, 'r') as file:
                        data = json.load(file)
                        data[subFolder[0]] = [emuPath, titleName]
                    with open(configFile, 'w') as file:
                        json.dump(data, file, indent=4)
        else:
            printToWidget("Couldnt find any TID subfolders in /temp/!\n")
            
    else:
        printToWidget("Couldnt find temp folder!\n")
        
    with open(configFile, 'r') as file:
        data = json.load(file)
        titleArray = data[subFolder[0]]
        dstDir = titleArray[0]
    srcDir = os.path.join(tempDir, subFolder[0])
    if os.path.exists(dstDir):
        printToWidget(f"Deleting any existing save file in {dstDir}\n")
    else:
        printToWidget(f"The directory {dstDir} does not exist!\n")
        
    for item in os.listdir(dstDir):
        itemPath = os.path.join(dstDir, item)
        try:
            if os.path.isfile(itemPath) or os.path.islink(itemPath):
                os.unlink(itemPath)
            elif os.path.isdir(itemPath):
                shutil.rmtree(itemPath)
        except Exception as e:
            printToWidget(f"Failed to delete {itemPath}!\n")
    printToWidget("Moving save file.\n")
    for item in os.listdir(srcDir):
        srcItem = os.path.join(srcDir, item)
        dstItem = os.path.join(dstDir, item)
        if os.path.isfile(srcItem):
            shutil.copy2(srcItem, dstItem)
        elif os.path.isdir(srcItem):
            shutil.copytree(srcItem, dstItem)
    printToWidget("Deleteing temp.zip file.\n")
    os.remove(os.path.join(scriptDir, "temp.zip"))
    printToWidget("Deleteing temp folder.\n")
    shutil.rmtree(tempDir)

if __name__ == "__main__":
    window()
    if getattr(sys, 'frozen', False):
        scriptDir = os.path.dirname(sys.executable)
    elif __file__:
        scriptDir = os.path.dirname(__file__)
    selected = 1
    if (selected == 3):
        changeHost()
        sys.exit(0)
    elif (selected == 2):
        configFile = checkConfig()
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
