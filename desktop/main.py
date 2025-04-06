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
selected = 0
selectedTitle = 0
tempDir = os.path.join(scriptDir, "temp")
zipPath = os.path.join(scriptDir, "temp.zip")
input_text = None
output_widget = None
pressed_enter = False
titles = []
keys = []
paths = []
server_thread = None

def checkConfig():
    appdataPath = os.getenv('LOCALAPPDATA')
    if not appdataPath:
        appdataPath = Path.home() / 'AppData' / 'Local'
    configDir = Path(appdataPath) / 'NX-Save-Sync'
    configDir.mkdir(exist_ok=True)
    configFile = configDir / 'config.json'
    if not configFile.exists():
        return 0
    else:
        return configFile
    
def changeHost():
    global pressed_enter
    appdataPath = os.getenv('LOCALAPPDATA')
    if not appdataPath:
        appdataPath = Path.home() / 'AppData' / 'Local'
    configDir = Path(appdataPath) / 'NX-Save-Sync'
    configDir.mkdir(exist_ok=True)
    configFile = configDir / 'config.json'
    if not configFile.exists():
        with open(configFile, 'w') as f:
            with dpg.window(tag="input2", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
                dpg.add_text("Input switch IP")
                dpg.add_input_text(width=250, tag="input_widget2")
                input_entered = True
                while input_entered:
                    keyboard.on_press(checkInput)
                    input_value = dpg.get_value("input_widget2")
                    if input_value != "" and pressed_enter:
                        dpg.delete_item("input_widget2")
                        dpg.delete_item("input2")
                        input_entered = False
                    time.sleep(0.1) 
                keyboard.unhook_all()
                pressed_enter = False
                hostIp = input_value
            json.dump({"host": hostIp}, f, indent=4) 
    else:
        config = {}
        with open(configFile, 'r') as f:
            config = json.load(f)
        with dpg.window(tag="input2", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
            dpg.add_text("Input switch IP")
            dpg.add_input_text(width=250, tag="input_widget2")
            input_entered = True
            while input_entered:
                keyboard.on_press(checkInput)
                input_value = dpg.get_value("input_widget2")
                if input_value != "" and pressed_enter:
                    dpg.delete_item("input_widget2")
                    dpg.delete_item("input2")
                    input_entered = False
                time.sleep(0.1) 
            keyboard.unhook_all()
            pressed_enter = False
        hostIp = input_value
        config["host"] = hostIp
        with open(configFile, 'w') as f:
            json.dump(config, f, indent=4)
    dpg.set_value("current_ip", f"Switch IP: {hostIp}")
    dpg.set_item_label("current_ip_button", "Change switch IP")

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
            return 0
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
            return 0
        printToWidget(f"File {file_name} downloaded successfully.\n")
        dpg.hide_item("progress_bar")
        dpg.hide_item("progress_info")
        shutdown_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        shutdown_socket.connect((host, port))
        shutdown_socket.sendall(b"SHUTDOWN")
        shutdown_socket.close()
        printToWidget("Shutting down server.\n")
        return 1
    except Exception as e:
        printToWidget(f"Error: {e}\n")
        return 0
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
            printToWidget2(f"Client handling error: {str(e)}\n")
        finally:
            try:
                client_socket.close()
            except:
                pass
    def run(self):
        self.server_socket.bind(('0.0.0.0', 8080))
        self.server_socket.listen(5)
        self.server_socket.settimeout(1)
        printToWidget2("Server is now running\n")
        try:
            while not self.shutdown_flag.is_set():
                try:
                    client_socket, addr = self.server_socket.accept()
                    threading.Thread(target=self.handle_client, args=(client_socket,)).start()
                except socket.timeout:
                    continue
                except OSError as e:
                    if not self.shutdown_flag.is_set():
                        printToWidget2(f"Accept error: {e}\n")
        finally:
            printToWidget2("Shutting down server\n")
            self.server_socket.close()
            printToWidget2("Deleteing temp.zip file\n")
            os.remove(os.path.join(scriptDir, "temp.zip"))
            printToWidget2("Deleteing temp folder\n")
            shutil.rmtree(tempDir)
            
def checkInput(event):
    global pressed_enter
    if event.name == 'enter':
        pressed_enter = True

def inputString():
    with dpg.window(tag="input", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
        dpg.add_text("Input emulator save file path")
        dpg.add_input_text(width=250, tag="input_widget")
    global pressed_enter
    input_entered = True
    while input_entered:
        keyboard.on_press(checkInput)
        input_value = dpg.get_value("input_widget")
        if input_value != "" and pressed_enter:
            dpg.delete_item("input_widget")
            dpg.delete_item("input")
            input_entered = False
        time.sleep(0.1) 
    keyboard.unhook_all()
    pressed_enter = False
    return input_value

def createWindow():
    global output_widget, output_widget2
    dpg.create_context()
    with dpg.font_registry():
        default_font = dpg.add_font("C:/Windows/Fonts/arial.ttf", 16)
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
                    dpg.add_button(label="Connect to switch", width=150, height=30, callback=startPull, tag="connect_button")
                    dpg.add_progress_bar(label="Temp", default_value=0, width=200, tag="progress_bar")
                    dpg.add_text("", tag="progress_info")
                    dpg.hide_item("progress_bar")
                    dpg.hide_item("progress_info")
                output_widget = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget)
            with dpg.tab(label="Push"):
                dpg.add_text("Push newer save file from pc to switch")
                dpg.add_button(label="Start server", width=150, height=30, callback=startPush, tag="start_button")
                output_widget2 = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget2)
            with dpg.tab(label="Config"):
                configFile = checkConfig()
                if configFile != 0:
                    with open(configFile, 'r') as file:
                        data = json.load(file)
                        host = data.get("host")
                        if host:
                            dpg.add_text(f"Switch IP: {host}", tag="current_ip")
                            dpg.add_button(label="Change switch IP", width=150, height=30, tag="current_ip_button", callback=changeHost)   
                else:
                    dpg.add_text("Switch IP not set!", tag="current_ip")
                    dpg.add_button(label="Set switch IP", width=150, height=30, tag="current_ip_button", callback=changeHost)
    
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

def getSelectedTitle(sender, app_data):
    global selectedTitle, titles, keys, paths
    selectedTitle = titles.index(app_data)
    dpg.delete_item("titles")
    result = push()
    if (result == 0):
        printToWidget2("Process ended with an error!\n")
    elif (result == 1):
        printToWidget2("Process ended successfully!\n")
    titles = []
    keys = []
    paths = []

def selectTitle():
    configFile = checkConfig()
    if configFile == 0:
        dpg.show_item(output_widget2)
        printToWidget2("Config file doesnt exist!\n")
        return 0
    with open(configFile, 'r') as file:
        config = json.load(file)
    for key, value in config.items():
        if key != "host" and isinstance(value, list) and len(value) >= 2:
            keys.append(key)
            paths.append(value[0])
            titles.append(value[1])
    if not titles:
        dpg.show_item(output_widget2)
        printToWidget2("No valid entries found in config.json!\n")
        return 0
    else:
        with dpg.window(tag="titles", label="Title selection Window", pos=(40, 50), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=500, height=275):
            dpg.add_text("Select a title you want to push save file")
            with dpg.child_window(border=False):
                dpg.add_listbox(tag="array_listbox",items=titles,num_items=10,width=-1, callback=getSelectedTitle)

def startPush():
    global selected
    selected = 2
    result = selectTitle()
    if (result == 0):
        printToWidget2("Process ended with an error!\n")
    elif (result == 1):
        printToWidget2("Process ended successfully!\n")

def startPull():
    global selected
    selected = 1
    result = pull()
    if (result == 0):
        printToWidget("Process ended with an error!\n")
    elif (result == 1):
        printToWidget("Process ended successfully!\n")

def pull():
    global output_widget
    dpg.set_value(output_widget, "")
    dpg.show_item(output_widget)
    configFile = checkConfig()
    if configFile == 0:
        printToWidget("Config file doesnt exist!\n")
        return 0
    with open(configFile, 'r') as file:
        data = json.load(file)
        host = data.get("host")
        if host:
            printToWidget(f"Connecting to host: {host}\n")
    if downloadZip(host, 8080, "temp.zip") == 0:
        return 0
    if os.path.exists(os.path.join(scriptDir, "temp.zip")):
        printToWidget("Unzipping temp.zip\n")
    else:
        printToWidget("Couldnt find temp.zip!\n")
        return 0
    
    with zipfile.ZipFile(zipPath, 'r') as zip_ref:
        zip_ref.extractall(scriptDir)
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
            return 0
    else:
        printToWidget("Couldnt find temp folder!\n")
        return 0
    with open(configFile, 'r') as file:
        data = json.load(file)
        titleArray = data[subFolder[0]]
        dstDir = titleArray[0]
    srcDir = os.path.join(tempDir, subFolder[0])
    if os.path.exists(dstDir):
        printToWidget(f"Deleting any existing save file in {dstDir}\n")
    else:
        printToWidget(f"The directory {dstDir} does not exist!\n")
        return 0
    for item in os.listdir(dstDir):
        itemPath = os.path.join(dstDir, item)
        try:
            if os.path.isfile(itemPath) or os.path.islink(itemPath):
                os.unlink(itemPath)
            elif os.path.isdir(itemPath):
                shutil.rmtree(itemPath)
        except Exception as e:
            printToWidget(f"Failed to delete {itemPath}!\n")
            return 0
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
    return 1

def push():
    dpg.set_value(output_widget2, "")
    dpg.show_item(output_widget2)
    printToWidget2(f"Selected title: {titles[selectedTitle]}\n")
    printToWidget2(f"Selected TID: {keys[selectedTitle]}\n")
    printToWidget2(f"Title save data path: {paths[selectedTitle]}\n")
    os.mkdir(tempDir)
    subFolder = os.path.join(tempDir, keys[selectedTitle])
    os.mkdir(subFolder)
    printToWidget2("Exporting save data\n")
    if os.path.exists(paths[selectedTitle]) and os.path.isdir(paths[selectedTitle]):
        shutil.copytree(paths[selectedTitle], subFolder, dirs_exist_ok=True)
    else:
        printToWidget2("Couldnt find the save data folder!\n")
        shutil.rmtree("temp")
        return 0
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
    createWindow()