from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO, emit 
import json
import time
import threading 
import random 
import serial   


serial_port = None
port = 'COM3'

# initialize flask app with html template which is by default in templates without specifying
app = Flask(__name__)
app.config["SECRET KEY"] = "FootballDashboard"

# initialize socketIO for bidirectional real-time communication 
socketio = SocketIO(app, cors_allowed_origins="*")


def read_serial_data(): 
    global serial_port
    while True: 
        try: 
            #check for data waiting to be read
            if serial_port.in_waiting > 0:
            
                # recieve raw line of bytes from serial port and decode into string  
                raw_line = serial_port.readline().decode("utf-8").strip()
                
                
               # print(f"[DEBUG] RAW PACKET: {raw_line}")
                
                #take raw strings and convert into python dictionary
                serial_data = json.loads(raw_line)
               
                socketio.emit('serial_data', serial_data)
                
        except Exception as e: 
            print(f'[DEBUG] ERROR ON SERIAL READING THREAD...ERROR: {e}')
 
 
def connect_device(): 
    global serial_port, port
    
    try: 
        #connect to COM4 serial port with 115200 baud rate and timeout of 1 second (must match esp baudrate)
        serial_port = serial.Serial(port, 115200, timeout=1)
        print(f'[SERIAL] CONNECTED TO {port}')
        
        # Start background thread to read from serial port
        reading_thread = threading.Thread(target=read_serial_data, daemon=True)
        reading_thread.start()
        print("[DEBUG] READER THREAD STARTED")

    except Exception as e: 
        print(f'[DEBUG] SERIAL CONNECTION TO {port} FAILED...ERROR: {e}')    
        

@app.route('/')
def home():
    return render_template('index.html')

# @app.route('/api/data')

#     return jsonify(data) #converts dictionary into json for web browser

if __name__ == '__main__':
    #attempt to connect to ESP32
    connect_device()
    
    # if connection success thread is running in background and server may start
    print("[DEBUG] Launching Football Dashboard...")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, use_reloader = False)
    
    
# def get_gse_data(): 

#     sensor_readings = {
#         "Pressure": random.randint(100, 500),
#         "Temperature": random.randint(20,100),
#         "valveState": False
#     }
#     return sensor_readings
