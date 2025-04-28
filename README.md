# ESP32 Real-Time Audio Streaming System

## Overview

This project implements a real-time audio streaming system using an ESP32 microcontroller. The ESP32 samples microphone audio via its ADC peripheral, then streams the audio data over a WiFi TCP connection. A Python server receives the audio data and relays it to a web browser client using WebSocket, enabling live audio playback.

The system architecture demonstrates multi-tasking using FreeRTOS, producer-consumer patterns with inter-task queues, peripheral interfacing, and real-time network communication.

---

## System Architecture

- **Mic Sampling Task**: Samples audio data from the microphone using the ESP32 ADC and enqueues it into a FreeRTOS queue.
- **TCP Transmission Task**: Dequeues sampled data and transmits it over a persistent TCP socket connection.
- **Python Server**: Listens for TCP connections, receives audio packets, and broadcasts them over a WebSocket.
- **Browser Client**: Receives streamed audio packets and plays them in real time using the Web Audio API.

---

## Features

- **FreeRTOS**:
  - FreeRTOS multi-tasking: Separate tasks for ADC sampling and TCP communication.
  - Inter-task communication using FreeRTOS queues (producer-consumer model).
- **Networking**:
  - TCP client implementation on ESP32.
  - WiFi management and reconnection handling.
- **Peripheral Control**:
  - Microphone audio acquisition using ADC.
- **Streaming**:
  - Python server relays data using Flask and Socket.IO.
  - Browser frontend plays audio with minimal latency using the Web Audio API.

---

## Requirements

### Hardware

- ESP32 Development Board
- Electret microphone breakout board (analog output)

### Software

- ESP-IDF (ESP32 development framework)
- Python 3 (for the server)
  - Flask
  - Flask-SocketIO

### Browser

- Modern browser (Chrome, Firefox) with Web Audio API support

---

## Setup Instructions

### 1. ESP32 Firmware

- Configure WiFi credentials inside `wifi_manager.c`.
- Set port number inside `tcp_client.c`.
- Build and flash the firmware using ESP-IDF:
  ```bash
  idf.py build flash monitor
  ```

### 2. Python Server

- Install Python dependencies:
  ```bash
  pip install flask flask-socketio gevent
  ```
- Run the server:
  ```bash
  python3 server.py
  ```

### 3. Browser Client

- Open `http://localhost:2000` in your browser.
- Click the **Start Listening** button to begin receiving live audio.

---

## Future Improvements

- Implement basic audio compression
- Support OTA firmware updates.


