from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import socket
import threading

app = Flask(__name__)
socketio = SocketIO(app)

def tcp_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(("0.0.0.0", 1000))  # TCP port where ESP32 connects
    server_socket.listen(1)
    print("TCP Server Listening on port 1000...")

    while True:
        conn, addr = server_socket.accept()
        print(f"New connection from {addr}")

        # Handle this connection in a separate thread
        threading.Thread(target=handle_client, args=(conn,), daemon=True).start()

def handle_client(conn):
    with conn:
        while True:
            data = conn.recv(1024)
            if not data:
                print("Client disconnected")
                break

            socketio.emit('audio', data)

@app.route('/')
def index():
    # return "<p>Hello</p>"
    return render_template('index.html')

if __name__ == '__main__':
    threading.Thread(target=tcp_server, daemon=False).start()
    socketio.run(app, host='0.0.0.0', port=2000)