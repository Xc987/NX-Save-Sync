import socket
import sys

# Configuration
HOST = "192.168.8.8"  # Replace with your Nintendo Switch's IP address
PORT = 8080
FILE_NAME = "temp.zip"
SHUTDOWN_SIGNAL = b"SHUTDOWN"

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
        shutdown_socket.sendall(SHUTDOWN_SIGNAL)
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
    if len(sys.argv) > 1:
        HOST = sys.argv[1]  # Allow the host to be passed as a command-line argument

    download_file(HOST, PORT, FILE_NAME)