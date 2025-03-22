import socket
import sys
import json
import os

def read_host_from_json(file_path, value):
    with open(file_path, 'r') as file:
        data = json.load(file)
        host = data.get(value)
        return host
def download_file(host, port, file_name):
    try:
        # Create a socket and connect to the server
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((host, port))

        # Send an HTTP GET request to download the file
        request = f"GET / HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
        client_socket.sendall(request.encode())
        print("GET request sent.")

        # Receive the HTTP response
        response = b""
        while True:
            data = client_socket.recv(1024)
            if not data:
                break
            response += data

        # Check if the response is valid
        if b"200 OK" not in response:
            print("Error: File not found or server error.")
            return

        # Extract the file content from the response
        header_end = response.find(b"\r\n\r\n")
        if header_end == -1:
            print("Error: Invalid HTTP response.")
            return

        file_content = response[header_end + 4:]

        # Save the file
        with open(file_name, "wb") as file:
            file.write(file_content)
        print(f"File '{file_name}' downloaded successfully.")

        # Close the first connection
        client_socket.close()

        # Create a new connection to send the shutdown signal
        print("Sending shutdown signal...")
        shutdown_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        shutdown_socket.connect((host, port))
        shutdown_socket.sendall(b"SHUTDOWN")
        print("Shutdown signal sent.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Close the socket
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
        host = read_host_from_json(json_file_path, "host")
        download_file(host, 8080, "temp.zip")