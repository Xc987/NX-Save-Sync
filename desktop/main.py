import dearpygui.dearpygui as dpg
from pathlib import Path
import socket
import json
import zipfile
import os
import sys
import shutil
import threading
import keyboard
import time

if getattr(sys, 'frozen', False):
    scriptDir = os.path.dirname(sys.executable)
elif __file__:
    scriptDir = os.path.dirname(__file__)

tempDir = os.path.join(scriptDir, "temp")
input_text = None
output_widget = None
pressed_enter = False
titles = []
keys = []
paths = []
selected = 0
server_thread = None

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
        total_received = 0
        content_length = 0
        last_update = 0
        while not header_complete:
            data = client_socket.recv(64 * 1024)
            if not data:
                break
            header += data
            if b"\r\n\r\n" in header:
                header_end = header.find(b"\r\n\r\n")
                headers = header[:header_end].decode('utf-8', errors='ignore')
                for line in headers.split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        content_length = int(line.split(':')[1].strip())
                        break
                file_handle.write(header[header_end + 4:])
                total_received = len(header[header_end + 4:])
                header_complete = True
        if not content_length:
            printToWidget("Could not determine file size from headers\n")
            return
        dpg.show_item("progress_bar")
        dpg.show_item("progress_info")
        while True:
            data = client_socket.recv(64 * 1024)
            if not data:
                break
            file_handle.write(data)
            total_received += len(data)
            if time.time() - last_update > 0.1:
                progress = total_received / content_length
                dpg.set_value("progress_bar", progress)
                downloaded_mb = total_received / (1024 * 1024)
                total_mb = content_length / (1024 * 1024)
                dpg.configure_item("progress_info", default_value=f"{downloaded_mb:.1f}MB / {total_mb:.1f}MB")
                last_update = time.time()
        file_handle.close()
        if b"200 OK" not in header:
            printToWidget("Server error or file not found!\n")
            return
        printToWidget(f"File {file_name} downloaded successfully.\n")
        dpg.hide_item("progress_bar")
        dpg.hide_item("progress_info")
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
            printToWidget2("Deleteing temp.zip file\n")
            os.remove(os.path.join(scriptDir, "temp.zip"))
            printToWidget2("Deleteing temp folder\n")
            shutil.rmtree(tempDir)
               
def on_enter_press(event):
    global pressed_enter
    if event.name == 'enter':
        pressed_enter = True

def inputString():
    with dpg.window(tag="input", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
        dpg.add_text("Input emulator save file path")
        dpg.add_input_text(width=250, tag="input_widget")
    global input_value, input_entered, pressed_enter
    input_entered = True
    dpg.show_item("input_widget")
    while input_entered:
        keyboard.on_press(on_enter_press)
        input_value = dpg.get_value("input_widget")
        if input_value != "" and pressed_enter:
            input_entered = True
            dpg.hide_item("input_widget")
            dpg.hide_item("input")
            input_entered = False
        time.sleep(0.1) 
    keyboard.unhook_all()
    return input_value

def window():
    global output_widget, output_widget2
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
                    dpg.add_progress_bar(label="Temp", default_value=0, width=200, tag="progress_bar")
                    dpg.add_text("", tag="progress_info")
                    dpg.hide_item("progress_bar")
                    dpg.hide_item("progress_info")
                output_widget = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget)
            with dpg.tab(label="Push"):
                dpg.add_text("Push newer save file from pc to switch")
                dpg.add_button(label="Start server", width=150, height=30, callback=selectTitle)
                output_widget2 = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget2)
            with dpg.tab(label="Config"):
                dpg.add_text("This is the content of Tab 3", indent=20)
    
    dpg.create_viewport(title='NX-Save-Sync', width=600, height=400, min_width=600, min_height=400, max_width=600, max_height=400)
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.start_dearpygui()
    while dpg.is_dearpygui_running():
        dpg.render_dearpygui_frame()
    server = uploadZip()
    server.shutdown_flag.set()
    dpg.destroy_context()

def printToWidget(message):
    global output_widget
    if output_widget is not None:
        dpg.set_value(output_widget, dpg.get_value(output_widget) + message)

def printToWidget2(message):
    global output_widget2
    if output_widget2 is not None:
        dpg.set_value(output_widget2, dpg.get_value(output_widget2) + message)

def find_first_position(sender, app_data):
    global selected
    selected = titles.index(app_data)
    dpg.hide_item("titles")
    push()

def selectTitle():
    configFile = checkConfig()
    print("\nSelect a title")
    with open(configFile, 'r') as file:
        config = json.load(file)
    for key, value in config.items():
        if key != "host" and isinstance(value, list) and len(value) >= 2:
            keys.append(key)
            paths.append(value[0])
            titles.append(value[1])
    if not titles:
        print("\033[31m[FAIL] No valid entries found in config.json!\033[0m")
        
    
    with dpg.window(tag="titles", label="Title selection Window", pos=(40, 50), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=500, height=275):
        dpg.add_text("Select a title you want to push save file")
        with dpg.child_window(border=False):
            dpg.add_listbox(tag="array_listbox",items=titles,num_items=10,width=-1, callback=find_first_position)

def pull():
    global output_widget
    dpg.set_value(output_widget, "")
    dpg.show_item(output_widget)
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

def push():
    dpg.show_item(output_widget2)
    printToWidget2(f"Selected title: {titles[selected]}\n")
    printToWidget2(f"Selected TID: {keys[selected]}\n")
    printToWidget2(f"Title save data path: {paths[selected]}\n")
    
    os.mkdir(tempDir)
    subFolder = os.path.join(tempDir, keys[selected])
    os.mkdir(subFolder)
    printToWidget2("Exporting save data\n")
    if os.path.exists(paths[selected]) and os.path.isdir(paths[selected]):
        shutil.copytree(paths[selected], subFolder, dirs_exist_ok=True)
    else:
        printToWidget2("Couldnt find the save data folder!\n")
    with zipfile.ZipFile('temp.zip', 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk('temp'):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, start='temp')
                zipf.write(file_path, arcname=os.path.join('temp', arcname))
    printToWidget2("Zipping /temp/ folder!\n")
    IPAddr = socket.gethostbyname(socket.gethostname())
    printToWidget2(f"PC IP: {IPAddr}\n")
    server = uploadZip()
    server_thread = threading.Thread(target=server.run)
    server_thread.start()

if __name__ == "__main__":
    window()