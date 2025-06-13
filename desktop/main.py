import dearpygui.dearpygui as dpg
from pathlib import Path
from io import StringIO
import traceback
import urllib.request
import socket
import json
import zipfile
import os
import sys
import shutil
import threading
import keyboard
import time
import ctypes
ctypes.windll.shcore.SetProcessDpiAwareness(2)
output_buffer = StringIO()

if getattr(sys, 'frozen', False):
    scriptDir = os.path.dirname(sys.executable)
elif __file__:
    scriptDir = os.path.dirname(__file__)
selected = 0
selected_items = set()
tempDir = os.path.join(scriptDir, "temp")
zipPath = os.path.join(scriptDir, "temp.zip")
input_text = None
output_widget = None
pressed_enter = False
titles = []
keys = []
paths = []
server_thread = None
themesel = 1

class OutputWindow:
    def __init__(self):
        self.buffer = StringIO()
        self.setup_redirection()
        self.setup_exception_handler()
        
    def setup_redirection(self):
        sys.stdout = self.OutputRedirector(self.buffer, self.update_window)
        sys.stderr = self.OutputRedirector(self.buffer, self.update_window)
    
    def setup_exception_handler(self):
        sys.excepthook = self.handle_exception
    
    class OutputRedirector:
        def __init__(self, buffer, update_callback):
            self.buffer = buffer
            self.update_callback = update_callback
            
        def write(self, text):
            self.buffer.write(text)
            self.update_callback()
            
        def flush(self):
            pass
    
    def handle_exception(self, exc_type, exc_value, exc_traceback):
        exception_only = f"{exc_type.__name__}({exc_value.args[0]}, '{exc_value.args[1]}')" if exc_value.args else f"{exc_type.__name__}()"
        traceback_text = "".join(traceback.format_exception(exc_type, exc_value, exc_traceback))
        full_output = f"{exception_only}\n\nTraceback:\n{traceback_text}"
        self.buffer.write(full_output + "\n")
        self.update_window()
        sys.__stderr__.write(full_output + "\n")
    
    def update_window(self):
        if dpg.does_item_exist("output_window"):
            dpg.set_value("output_text", self.buffer.getvalue())
    
    def create_window(self):
        if dpg.does_item_exist("output_window"):
            dpg.focus_item("output_window")
            return
        with dpg.window(tag="output_window", label="Debug Output", width=300, height=250):
            with dpg.menu_bar():
                dpg.add_menu_item(label="Copy All", callback=lambda: dpg.set_clipboard_text(self.buffer.getvalue()))
            with dpg.child_window(tag="output_scroll", height=-1):
                dpg.add_input_text(tag="output_text", default_value=self.buffer.getvalue(), multiline=True, readonly=False, width=-1, height=-1, tab_input=True)
    def show_context_menu(self):
        if dpg.does_item_exist("output_text"):
            with dpg.window(tag="context_menu", popup=True, no_title_bar=True, no_move=True):
                dpg.add_menu_item(label="Copy All", callback=lambda: dpg.set_clipboard_text(self.buffer.getvalue()))

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
    
def changeHost(device):
    global pressed_enter
    configFile = checkConfig()
    if not configFile.exists():
        with open(configFile, 'w') as f:
            with dpg.window(tag="input2", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
                if device == 0:
                    dpg.add_text("Input switch IP")
                elif device == 1:
                    dpg.add_text("Input secondary switch IP")
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
            if device == 0:
                json.dump({"host": hostIp}, f, indent=4) 
            elif device == 1:
                json.dump({"shost": hostIp}, f, indent=4) 
            
    else:
        config = {}
        with open(configFile, 'r') as f:
            config = json.load(f)
        with dpg.window(tag="input2", label="Input Window", pos=(125, 125), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=100):
            if device == 0:
                dpg.add_text("Input switch IP")
            elif device == 1:
                dpg.add_text("Input secondary switch IP")
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
        if device == 0:
            config["host"] = hostIp
        elif device == 1:
            config["shost"] = hostIp
        with open(configFile, 'w') as f:
            json.dump(config, f, indent=4)
    if device == 0:
        dpg.set_value("current_ip", f"Switch IP: {hostIp}")
        dpg.set_item_label("current_ip_button", "Change switch IP")
    elif device == 1:
        dpg.set_value("current_ip2", f"Secondary switch IP: {hostIp}")
        dpg.set_item_label("current_ip_button2", "Change Secondary switch IP")
    
def downloadZip(host, port, file_name):
    try:
        printToWidget("Downloading temp.zip.\n")
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
        global titles, keys, paths
        try:
            urllib.request.urlopen('https://www.google.com', timeout=5)
        except:
            printToWidget2("Device is not connected to the internet!\n")
            printToWidget2("Process ended with an error!\n")
            titles = []
            keys = []
            paths = []
            return
        IPAddr = socket.gethostbyname(socket.gethostname())
        printToWidget2(f"PC IP: {IPAddr}\n")
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
            printToWidget2("Process ended successfully!\n")

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

def inputStringPath(titleName):
    with dpg.window(tag="input", label="Input Window", pos=(125, 115), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=125):
        dpg.add_text("Input emulator save file path for title:")
        dpg.add_text(titleName)
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

def changeTheme(sender, app_data):
    global themesel
    appdataPath = os.getenv('LOCALAPPDATA')
    if not appdataPath:
        appdataPath = Path.home() / 'AppData' / 'Local'
    configDir = Path(appdataPath) / 'NX-Save-Sync'
    configDir.mkdir(exist_ok=True)
    configFile = configDir / 'config.json'
    if dpg.get_value(sender):
        config = {}
        with open(configFile, 'r') as f:
            config = json.load(f)
        theme = "dark"
        config["theme"] = theme
        with open(configFile, 'w') as f:
            json.dump(config, f, indent=4)
        themesel = 1
        setTheme()
    else:
        config = {}
        with open(configFile, 'r') as f:
            config = json.load(f)
        theme = "light"
        config["theme"] = theme
        with open(configFile, 'w') as f:
            json.dump(config, f, indent=4)
        themesel = 2
        setTheme()

def setTheme():
    if (themesel == 1):
        with dpg.theme() as appTheme:
            with dpg.theme_component(dpg.mvAll):
                dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (25, 25, 25))
                dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (37, 37, 38))
                dpg.add_theme_color(dpg.mvThemeCol_Text, (230, 230, 230))
                dpg.add_theme_color(dpg.mvThemeCol_Button, (60, 60, 60))
                dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (80, 80, 80))
                dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (40, 40, 40))
                dpg.add_theme_color(dpg.mvThemeCol_Tab, (40, 40, 40))
                dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (37, 37, 38), category=dpg.mvThemeCat_Core)
                dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 5)
            with dpg.theme_component(dpg.mvListbox):
                dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (37, 37, 38, 255), category=dpg.mvThemeCat_Core)
    elif (themesel == 2):
        with dpg.theme() as appTheme:
            with dpg.theme_component(dpg.mvAll):
                dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (240, 240, 240))
                dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, (240, 240, 240))
                dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (255, 255, 255))
                dpg.add_theme_color(dpg.mvThemeCol_Text, (30, 30, 30))
                dpg.add_theme_color(dpg.mvThemeCol_Button, (180, 180, 180))
                dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (200, 200, 200))
                dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (160, 160, 160))
                dpg.add_theme_color(dpg.mvThemeCol_Tab, (220, 220, 220))
                dpg.add_theme_color(dpg.mvNodeCol_TitleBar, (255, 255, 255))
                dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 5)
                dpg.add_theme_color(dpg.mvThemeCol_TitleBg, (95, 166, 216), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, (95, 166, 216), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_TitleBgCollapsed, (95, 166, 216), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (255, 255, 255), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, (180, 180, 180), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, (150, 150, 150), category=dpg.mvThemeCat_Core)
                dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, (120, 120, 120), category=dpg.mvThemeCat_Core)
            with dpg.theme_component(dpg.mvCheckbox):
                dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (180, 180, 180))
                dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, (200, 200, 200))
                dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, (160, 160, 160))
            with dpg.theme_component(dpg.mvInputText):
                dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (180, 180, 180))
                dpg.add_theme_color(dpg.mvThemeCol_Border, (200, 200, 200))
                dpg.add_theme_color(dpg.mvThemeCol_BorderShadow, (240, 240, 240))
            with dpg.theme_component(dpg.mvProgressBar):
                dpg.add_theme_color(dpg.mvThemeCol_PlotHistogram, (95, 166, 216))
                dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (180, 180, 180))
            with dpg.theme_component(dpg.mvListbox):
                dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (255, 255, 255, 255), category=dpg.mvThemeCat_Core)

    dpg.bind_theme(appTheme)

def button_callback(output_window):
    output_window.create_window()
    dpg.split_frame()

def createWindow():
    global output_widget, output_widget2, themesel
    output_window = OutputWindow()
    dpg.create_context()
    with dpg.font_registry():
        default_font = dpg.add_font("C:/Windows/Fonts/arial.ttf", 16*2)
    configFile = checkConfig()
    if configFile != 0:
        with open(configFile, 'r') as file:
            data = json.load(file)
            theme = data.get("theme")
            if (theme == "dark"):
                themesel = 1
                setTheme()
            elif (theme == "light"):
                themesel = 2
                setTheme()
            elif (theme == None):
                configFile = checkConfig()
                config = {}
                with open(configFile, 'r') as f:
                    config = json.load(f)
                config["theme"] = "dark"
                with open(configFile, 'w') as f:
                    json.dump(config, f, indent=4)
                themesel = 1
                setTheme()
    else:
        appdataPath = os.getenv('LOCALAPPDATA')
        if not appdataPath:
            appdataPath = Path.home() / 'AppData' / 'Local'
        configDir = Path(appdataPath) / 'NX-Save-Sync'
        configDir.mkdir(exist_ok=True)
        configFile = configDir / 'config.json'
        config = {}
        with open(configFile, 'w') as f:
            theme = "dark"
            json.dump({"theme": theme}, f, indent=4)
        themesel = 1
        setTheme()
    with dpg.window(tag="Primary Window", label="Main Window", no_title_bar=True, no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=False, width=800, height=600):
        dpg.bind_font(default_font)
        dpg.set_global_font_scale(0.57)
        with dpg.tab_bar():
            with dpg.tab(label="Receive"):
                dpg.add_text("Receive save file from primary switch or secondary switch")
                with dpg.group(horizontal=True):
                    dpg.add_button(label="Connect to switch", width=150, height=30, callback=lambda: startPull(0), tag="connect_button")
                    dpg.add_progress_bar(label="Temp", default_value=0, width=200, tag="progress_bar")
                    dpg.add_text("", tag="progress_info")
                    dpg.hide_item("progress_bar")
                    dpg.hide_item("progress_info")
                dpg.add_button(label="Connect to secondary switch", width=250, height=30, callback=lambda: startPull(1), tag="connect_button2")
                output_widget = dpg.add_input_text(multiline=True, readonly=True, width=570, height=210, tab_input=True)
                dpg.hide_item(output_widget)
            with dpg.tab(label="Send"):
                dpg.add_text("Send save file to primary switch or secondary switch")
                dpg.add_button(label="Start server", width=150, height=30, callback=startPush, tag="start_button")
                output_widget2 = dpg.add_input_text(multiline=True, readonly=True, width=570, height=240, tab_input=True)
                dpg.hide_item(output_widget2)
            with dpg.tab(label="Titles"):
                with dpg.group(horizontal=True):
                    dpg.add_button(label="List all titles", width=150, height=30, callback=listTitles, tag="list_titles_button")
                    dpg.add_text("No valid entries found in config.json!", tag="no_titles_warn")
                    dpg.hide_item("no_titles_warn")
                with dpg.group(horizontal=True):
                    dpg.add_button(label="Delete title", width=150, height=30, callback=deleteTitle, tag="list_titles_button2")
                    dpg.add_text("No valid entries found in config.json!", tag="no_titles_warn2")
                    dpg.hide_item("no_titles_warn2")
                dpg.add_button(label="Add new title", width=150, height=30, callback=addTitle, tag="start_butt1on")
            with dpg.tab(label="Config"):
                configFile = checkConfig()
                if configFile != 0:
                    with open(configFile, 'r') as file:
                        data = json.load(file)
                        host = data.get("host")
                        if host:
                            dpg.add_text(f"Switch IP: {host}", tag="current_ip")
                        else:
                            dpg.add_text("Switch IP not set!", tag="current_ip")
                        host = data.get("shost")
                        if host:
                            dpg.add_text(f"Secondary switch IP: {host}", tag="current_ip2")
                        else:
                            dpg.add_text("Secondary switch IP not set!", tag="current_ip2")
                        host = data.get("host")
                        if host:
                            dpg.add_button(label="Change switch IP", width=150, height=30, tag="current_ip_button", callback=lambda: changeHost(0))
                        else:
                            dpg.add_button(label="Set switch IP", width=150, height=30, tag="current_ip_button", callback=lambda: changeHost(0))
                        host = data.get("shost")
                        if host:
                            dpg.add_button(label="Change secondary switch IP", width=250, height=30, tag="current_ip_button2", callback=lambda: changeHost(1))
                        else:
                            dpg.add_button(label="Set secondary switch IP", width=250, height=30, tag="current_ip_button2", callback=lambda: changeHost(1))
                else:
                    dpg.add_text("Switch IP not set!", tag="current_ip")
                    dpg.add_button(label="Set switch IP", width=150, height=30, tag="current_ip_button", callback=lambda: changeHost(0))
                    dpg.add_button(label="Set secondary switch IP", width=250, height=30, tag="current_ip_button2", callback=lambda: changeHost(1))
                dpg.add_button(label="Show debug", width=150, height=30, callback=lambda: button_callback(output_window))
                configFile = checkConfig()
                if configFile != 0:
                    with open(configFile, 'r') as file:
                        data = json.load(file)
                        theme = data.get("theme")
                        if (theme == "dark"):
                            dpg.add_checkbox(label="Dark mode", tag="theme_toggle", default_value=True, callback=changeTheme)
                        elif (theme == "light"):
                            dpg.add_checkbox(label="Dark mode", tag="theme_toggle", default_value=False, callback=changeTheme)
    dpg.create_viewport(title='NX-Save-Sync', small_icon='include/icon.ico', large_icon='include/icon.ico', width=600, height=400, min_width=600, min_height=400, max_width=600, max_height=400)
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.set_primary_window("Primary Window", True)
    dpg.start_dearpygui()
    try:
        while dpg.is_dearpygui_running():
            dpg.render_dearpygui_frame()
    except Exception as e:
        output_window.handle_exception(type(e), e, e.__traceback__)
    finally:
        dpg.destroy_context()
        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__
    server = uploadZip()
    server.shutdown_flag.set()
    dpg.destroy_context()

def closeTitleWindow():
    global titles, keys, paths
    dpg.delete_item("titles")
    dpg.delete_item("array_listbox")
    titles = []
    keys = []
    paths = []

def listTitles():
    configFile = checkConfig()
    if configFile == 0:
        dpg.show_item(output_widget2)
        return 0
    with open(configFile, 'r') as file:
        config = json.load(file)
    for key, value in config.items():
        if key != "host" and isinstance(value, list) and len(value) >= 2:
            keys.append(key)
            paths.append(value[0])
            titles.append(value[1])
    if not titles:
        dpg.show_item("no_titles_warn")
        return 0
    else:
        dpg.hide_item("no_titles_warn")
        with dpg.window(tag="titles", label="Titles Window", on_close=closeTitleWindow, pos=(40, 50), no_resize=True, no_collapse=True, no_move=True, modal=True, width=500, height=275):
            dpg.add_text("All saved titles")
            with dpg.child_window(border=False):
                dpg.add_listbox(tag="array_listbox",items=titles,num_items=10,width=-1)

def on_listbox_select(sender, app_data, user_data):
    global titles, keys, paths
    selected_item = dpg.get_value(sender)
    num = titles.index(selected_item)
    configFile = checkConfig()
    with open(configFile, 'r') as file:
        data = json.load(file)
    if keys[num] in data:
        del data[keys[num]]
    else:
        print(f"Key '{keys[num]}' not found in the JSON file.")
    json.dump(data, open(configFile,'w'), indent=4)
    closeTitleWindow()

def deleteTitle():
    configFile = checkConfig()
    if configFile == 0:
        dpg.show_item(output_widget2)
        return 0
    with open(configFile, 'r') as file:
        config = json.load(file)
    for key, value in config.items():
        if key != "host" and isinstance(value, list) and len(value) >= 2:
            keys.append(key)
            paths.append(value[0])
            titles.append(value[1])
    if not titles:
        dpg.show_item("no_titles_warn2")
        return 0
    else:
        dpg.hide_item("no_titles_warn2")
        with dpg.window(tag="titles", label="Titles Window", on_close=closeTitleWindow, pos=(40, 50), no_resize=True, no_collapse=True, no_move=True, modal=True, width=500, height=275):
            dpg.add_text("Select a title to delete")
            with dpg.child_window(border=False):
                dpg.add_listbox(tag="array_listbox",items=titles,num_items=10,width=-1, callback=on_listbox_select)

def confirmWrite(tid_value, path_value, name_value):
    configFile = checkConfig()
    with open(configFile, 'r') as file:
        data = json.load(file)
        data[tid_value] = [path_value, name_value]
    with open(configFile, 'w') as file:
        json.dump(data, file, indent=4)
    cancelWrite()
    
def cancelWrite():
    dpg.delete_item("warning")
    dpg.delete_item("warn_owerwrite1")
    dpg.delete_item("warn_owerwrite2")
    dpg.delete_item("confirm_button")
    dpg.delete_item("cancel_button")
    
def addTitle():
    with dpg.window(tag="input", label="Input Window", pos=(125, 55), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=210):
        dpg.add_text("Title ID")
        dpg.add_input_text(width=250, tag="input_widget_tid")
        dpg.add_text("Title name")
        dpg.add_input_text(width=250, tag="input_widget_name")
        dpg.add_text("Emulator save file path")
        dpg.add_input_text(width=250, tag="input_widget_path")
    global pressed_enter
    input_entered = True
    while input_entered:
        keyboard.on_press(checkInput)
        tid_value = dpg.get_value("input_widget_tid")
        name_value = dpg.get_value("input_widget_name")
        path_value = dpg.get_value("input_widget_path")
        if tid_value != "" and name_value != "" and path_value != "" and pressed_enter:
            dpg.delete_item("input")
            dpg.delete_item("input_widget_tid")
            dpg.delete_item("input_widget_name")
            dpg.delete_item("input_widget_path")
            input_entered = False
        time.sleep(0.1) 
    keyboard.unhook_all()
    pressed_enter = False
    configFile = checkConfig()
    with open(configFile, 'r') as file:
        data = json.load(file)
        if tid_value not in data:
            data[tid_value] = [path_value, name_value]
        else:
            with dpg.window(tag="warning", label="Warning Window", pos=(125, 75), no_resize=True, no_collapse=True, no_close=True, no_move=True, modal=True, width=300, height=160):
                dpg.add_text("This TID is already saved", tag="warn_owerwrite1")
                dpg.add_text("The data will be overwritten!", tag="warn_owerwrite2")
                dpg.add_button(label="Ok", width=150, height=30, tag="confirm_button", callback=lambda: confirmWrite(tid_value, path_value, name_value))
                dpg.add_button(label="Cancel", width=150, height=30, tag="cancel_button", callback=cancelWrite)
    with open(configFile, 'w') as file:
        json.dump(data, file, indent=4)
    
def cleanUp():
    printToWidget("Deleteing temp.zip file.\n")
    os.remove(os.path.join(scriptDir, "temp.zip"))
    printToWidget("Deleteing temp folder.\n")
    shutil.rmtree(tempDir)

def printToWidget(message):
    global output_widget
    if output_widget is not None:
        dpg.set_value(output_widget, dpg.get_value(output_widget) + message)

def printToWidget2(message):
    global output_widget2
    if output_widget2 is not None:
        dpg.set_value(output_widget2, dpg.get_value(output_widget2) + message)

def checkForTemp():
    if os.path.isfile(zipPath):
        os.remove(os.path.join(scriptDir, "temp.zip"))
    if os.path.isdir(tempDir):
        shutil.rmtree(tempDir)

def changeSelection(sender, app_data, user_data):
    item = user_data
    if app_data:
        dpg.configure_item(f"{item}_text", color=(0, 135, 215))
        selected_items.add(item)
    else:
        if (themesel == 1):
            dpg.configure_item(f"{item}_text", color=(255, 255, 255))
        else:
            dpg.configure_item(f"{item}_text", color=(30, 30, 30))
        selected_items.discard(item)
    if (len(selected_items) == 0):
        dpg.hide_item("start_push_button")
    else:
        dpg.show_item("start_push_button")
    if len(selected_items) == 1:
        dpg.set_item_label("start_push_button", f"Send {len(selected_items)} title")
    else:
        dpg.set_item_label("start_push_button", f"Send {len(selected_items)} titles")
    

def selectAllTitles():
    global selected_items
    selected_items = set(titles)
    push()

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
            dpg.add_text("Select a title(s) you want to send save file")
            with dpg.child_window(height=175, width=-1, border=False):
                for item in titles:
                    with dpg.group(horizontal=True):
                        dpg.add_checkbox(callback=changeSelection, user_data=item, tag=item)
                        dpg.add_text(f"{item}", tag=f"{item}_text")
            with dpg.group(horizontal=True):
                dpg.add_button(label="Send 0 titles", callback=push, tag="start_push_button")
                dpg.add_button(label="Send all titles", callback=selectAllTitles)
            dpg.hide_item("start_push_button")

def startPush():
    global selected
    selected = 2
    result = selectTitle()
    if (result == 0):
        printToWidget2("Process ended with an error!\n")
        
def startPull(device):
    global selected
    selected = 1
    result = pull(device)
    if (result == 0):
        printToWidget("Process ended with an error!\n")
    elif (result == 1):
        printToWidget("Process ended successfully!\n")

def pull(device):
    global output_widget
    dpg.set_value(output_widget, "")
    dpg.show_item(output_widget)
    checkForTemp()
    configFile = checkConfig()
    if configFile == 0:
        printToWidget("Config file doesnt exist!\n")
        return 0
    with open(configFile, 'r') as file:
        data = json.load(file)
        if device == 0:
            host = data.get("host")
        elif device == 1:
            host = data.get("shost")
        if host:
            printToWidget(f"Connecting to host: {host}\n")
        else:
            if device == 0:
                printToWidget("Switch IP not set!\n")
            elif device == 1:
                printToWidget("Secondary switch IP not set!\n")
            return 0
    if downloadZip(host, 8080, "temp.zip") == 0:
       return 0
    if os.path.exists(os.path.join(scriptDir, "temp.zip")):
        printToWidget("Unzipping temp.zip\n")
    else:
        printToWidget("Couldnt find temp.zip!\n")
        return 0
    with zipfile.ZipFile(zipPath, 'r') as zip_ref:
        zip_ref.extractall(scriptDir)
    foundTitles = False
    for entry in os.listdir(tempDir):
        foundTitles = True
        full_path = os.path.join(tempDir, entry)
        if os.path.isdir(full_path):
            if os.path.exists(tempDir) and os.path.isdir(tempDir):
                subFolder = entry
                with open(configFile, 'r') as file:
                    data = json.load(file)
                    titleDir = os.path.join(tempDir, subFolder)
                    titleFile = os.path.join(titleDir, "Title_Name/")
                    titleFiletxt = os.path.join(titleFile, "title.txt")
                    with open(titleFiletxt,'r', encoding='utf-8') as f:
                        titleName = f.readlines()[0]
                    if len(titleName) >= 1:
                        shutil.rmtree(titleFile)
                    if not subFolder in data:
                        printToWidget(f"Please enter the emulator save directory for {titleName}\n")
                        emuPath = inputStringPath(titleName)
                        with open(configFile, 'r') as file:
                            data = json.load(file)
                            data[subFolder] = [emuPath, titleName]
                        with open(configFile, 'w') as file:
                            json.dump(data, file, indent=4)
            else:
                printToWidget("Couldnt find temp folder!\n")
                return 0
            with open(configFile, 'r') as file:
                data = json.load(file)
                titleArray = data[subFolder]
                dstDir = titleArray[0]
            srcDir = os.path.join(tempDir, subFolder)
            if os.path.exists(dstDir):
                printToWidget(f"Deleting any existing save file in {dstDir}\n")
            else:
                printToWidget(f"The directory {dstDir} does not exist!\n")
                cleanUp()
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
                    cleanUp()
                    return 0
            printToWidget("Moving save file.\n")
            for item in os.listdir(srcDir):
                srcItem = os.path.join(srcDir, item)
                dstItem = os.path.join(dstDir, item)
                if os.path.isfile(srcItem):
                    shutil.copy2(srcItem, dstItem)
                elif os.path.isdir(srcItem):
                    shutil.copytree(srcItem, dstItem)
    if (foundTitles == False):
        printToWidget("Couldnt find any TID subfolders in /temp/!\n")
        cleanUp()
        return 0
    printToWidget("Deleteing temp.zip file.\n")
    os.remove(os.path.join(scriptDir, "temp.zip"))
    printToWidget("Deleteing temp folder.\n")
    shutil.rmtree(tempDir)
    return 1

def push():
    dpg.delete_item("titles")
    global titles, keys, paths, selected_items
    dpg.set_value(output_widget2, "")
    dpg.show_item(output_widget2)
    checkForTemp()
    os.mkdir(tempDir)
    for i in range(0, len(selected_items)):
        selectedTitle = titles.index(list(selected_items)[i])
        printToWidget2(f"Selected title: {titles[selectedTitle]}\n")
        printToWidget2(f"Selected TID: {keys[selectedTitle]}\n")
        printToWidget2(f"Title save data path: {paths[selectedTitle]}\n")
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
    titles = []
    keys = []
    paths = []
    selected_items = set()
    printToWidget2("Zipping /temp/ folder!\n")
    server = uploadZip()
    server_thread = threading.Thread(target=server.run)
    server_thread.start()

if __name__ == "__main__":
    createWindow()