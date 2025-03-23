import socket
import json
import zipfile
import os
import shutil

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
            print("File not found or server error.")
            exit()
        header_end = response.find(b"\r\n\r\n")
        if header_end == -1:
            print("Invalid HTTP response.")
            exit()
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
    if (selected == 1) :
        script_dir = os.path.dirname(os.path.abspath(__file__))
        json_file_path = os.path.join(script_dir, 'config.json')
        if not os.path.exists(json_file_path):
            print("config.json file not found!")
            exit() 
        with open(json_file_path, 'r') as file:
            data = json.load(file)
            host = data.get("host")
            if host:
                print(f"Connecting to host: {host}")
            else:
                print("Host key is not found in config.json!")
                exit()
        downloadZip(host, 8080, "temp.zip")
        zip_path = os.path.join(script_dir, "temp.zip")
        if os.path.exists(zip_path):
            print("Unzipping temp.zip")
        else:
            print("Couldnt find temp.zip!")
            exit()
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(script_dir)
        temp_dir = os.path.join(script_dir, "temp")
        if os.path.exists(temp_dir) and os.path.isdir(temp_dir):
            subfolders = [f.name for f in os.scandir(temp_dir) if f.is_dir()]
            if len(subfolders) == 1:
                with open(json_file_path, 'r') as file:
                    data = json.load(file)
                    if subfolders[0] in data:
                        print(f"The key '{subfolders[0]}' already exists.")
                    else:
                        print("Please enter the emulator save directory for TID", subfolders[0])
                        emuPath = (input("> "))
                        with open(json_file_path, 'r') as file:
                            data = json.load(file)
                            data[subfolders[0]] = emuPath
                        with open(json_file_path, 'w') as file:
                            json.dump(data, file, indent=4)
                        print(f"Added key '{subfolders[0]}' to config.json")
            else:
                print("Couldnt find any TID subfolders in /temp/")
                exit()
        else:
            print("Couldnt find temp folder!")
            exit()
        with open(json_file_path, 'r') as file:
            data = json.load(file)
            dst_folder = data.get(subfolders[0])
        src_folder = os.path.join(temp_dir, subfolders[0])
        if os.path.exists(dst_folder):
            print(f"Deleting any existing save file in {dst_folder}")
        else:
            print(f"The directory {dst_folder} does not exist.")
            exit()
        for item in os.listdir(dst_folder):
                item_path = os.path.join(dst_folder, item)
                try:
                    if os.path.isfile(item_path) or os.path.islink(item_path):
                        os.unlink(item_path)
                    elif os.path.isdir(item_path):
                        shutil.rmtree(item_path)
                except Exception as e:
                    print(f"Failed to delete {item_path}. Reason: {e}")
        print(f"Moving save file from /temp/{subfolders[0]} to {dst_folder}")
        for item in os.listdir(src_folder):
            src_item = os.path.join(src_folder, item)
            dst_item = os.path.join(dst_folder, item)
            if os.path.isfile(src_item):
                shutil.copy2(src_item, dst_item)
            elif os.path.isdir(src_item):
                shutil.copytree(src_item, dst_item)
        print("Deleteing temp.zip file")
        os.remove(zip_path)
        print("Deleteing temp folder")
        shutil.rmtree(temp_dir)