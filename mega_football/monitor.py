import serial
import threading
import time
import tkinter as tk

from tkinter import ttk
from collections import deque
from datetime import datetime

from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.dates as mdates

# ============================================================
# SERIAL CONFIGURATION
# ============================================================

PORT = "/dev/ttyACM0"
BAUD = 115200


# ============================================================
# UI COLORS
# ============================================================

BLACK = "#090909"
DARK = "#151515"
DARKER = "#101010"

PINK = "#FF4FA3"
LIGHT_PINK = "#FF9DCC"

WHITE = "#FFFFFF"
GRAY = "#BFBFBF"
GRID_GRAY = "#555555"


# ============================================================
# DATA STORAGE
# ============================================================

MAX_POINTS = 300

time_data = deque(maxlen=MAX_POINTS)
temperature_data = deque(maxlen=MAX_POINTS)


latest_servo0 = 0
latest_servo1 = 0
latest_servo2 = 0

latest_pressure0 = 0
latest_pressure1 = 0
latest_pressure2 = 0

latest_temperature = 0

running = True


# ============================================================
# MAIN WINDOW
# ============================================================

root = tk.Tk()

root.title("Mega Football Telemetry")
root.geometry("1200x700")

root.configure(bg=BLACK)


# ============================================================
# TTK STYLE
# ============================================================

style = ttk.Style()

# Required on Linux for custom ttk colours
style.theme_use("clam")


# General frames
style.configure(
    "TFrame",
    background=BLACK
)


# Label frames
style.configure(
    "TLabelframe",
    background=BLACK,
    bordercolor=PINK,
    borderwidth=2,
    relief="solid"
)

style.configure(
    "TLabelframe.Label",
    background=BLACK,
    foreground=PINK,
    font=("Arial", 12, "bold")
)


# General labels
style.configure(
    "TLabel",
    background=BLACK,
    foreground=WHITE
)


style.configure(
    "Data.TLabel",
    background=DARK,
    foreground=WHITE,
    font=("Arial", 20, "bold"),
    anchor="center",
    justify="center",
    padding=10
)


# Scrollbars
style.configure(
    "Vertical.TScrollbar",
    background=PINK,
    troughcolor=DARK,
    bordercolor=BLACK,
    arrowcolor=WHITE
)

style.configure(
    "Horizontal.TScrollbar",
    background=PINK,
    troughcolor=DARK,
    bordercolor=BLACK,
    arrowcolor=WHITE
)


# ============================================================
# ROOT GRID
# ============================================================

# Left side = 1 part
# Right side = 2 parts
root.columnconfigure(0, weight=1)
root.columnconfigure(1, weight=2)

root.rowconfigure(0, weight=1)


# ============================================================
# LEFT PANEL - RAW SERIAL DATA
# ============================================================

left_frame = ttk.LabelFrame(
    root,
    text="RAW SERIAL DATA"
)

left_frame.grid(
    row=0,
    column=0,
    sticky="nsew",
    padx=8,
    pady=8
)

left_frame.rowconfigure(0, weight=1)
left_frame.columnconfigure(0, weight=1)


raw_text = tk.Text(
    left_frame,
    wrap="none",
    font=("Courier", 17),

    bg=DARKER,
    fg=WHITE,

    insertbackground=PINK,

    selectbackground=PINK,
    selectforeground=BLACK,

    relief="flat",
    borderwidth=0,

    padx=8,
    pady=8
)

raw_text.grid(
    row=0,
    column=0,
    sticky="nsew"
)


# Vertical scrollbar
scrollbar_y = ttk.Scrollbar(
    left_frame,
    orient="vertical",
    command=raw_text.yview
)

scrollbar_y.grid(
    row=0,
    column=1,
    sticky="ns"
)


# Horizontal scrollbar
scrollbar_x = ttk.Scrollbar(
    left_frame,
    orient="horizontal",
    command=raw_text.xview
)

scrollbar_x.grid(
    row=1,
    column=0,
    sticky="ew"
)


raw_text.configure(
    yscrollcommand=scrollbar_y.set,
    xscrollcommand=scrollbar_x.set
)


# ============================================================
# RIGHT SIDE
# ============================================================

right_frame = ttk.Frame(root)

right_frame.grid(
    row=0,
    column=1,
    sticky="nsew",
    padx=8,
    pady=8
)

right_frame.columnconfigure(0, weight=1)

# Temperature graph gets more space
right_frame.rowconfigure(0, weight=3)

# Pressure/servo panel
right_frame.rowconfigure(1, weight=2)


# ============================================================
# RIGHT TOP - TEMPERATURE VS TIME
# ============================================================

temperature_frame = ttk.LabelFrame(
    right_frame,
    text="TEMPERATURE VS TIME"
)

temperature_frame.grid(
    row=0,
    column=0,
    sticky="nsew",
    pady=(0, 8)
)


# ============================================================
# MATPLOTLIB GRAPH
# ============================================================

figure = Figure(
    figsize=(7, 4),
    dpi=100,
    facecolor=BLACK
)

ax = figure.add_subplot(111)

ax.set_facecolor(DARK)


ax.set_xlabel(
    "Time",
    color=WHITE,
    fontsize=20)

ax.set_ylabel(
    "Temperature (°C)",
    color=WHITE,
    fontsize=20
)


ax.set_title(
    "Manifold Temperature",
    color=PINK,
    fontsize=15,
    fontweight="bold"
)


ax.tick_params(
    axis="x",
    colors=WHITE
)
# Format X axis as actual timestamps
time_locator = mdates.AutoDateLocator()

ax.xaxis.set_major_locator(
    time_locator
)

ax.xaxis.set_major_formatter(
    mdates.DateFormatter("%H:%M:%S")
)

figure.autofmt_xdate()
ax.tick_params(
    axis="y",
    colors=WHITE
)


# Pink graph border
for spine in ax.spines.values():
    spine.set_color(PINK)


# Grid
ax.grid(
    True,
    color=GRID_GRAY,
    alpha=0.45
)


# Temperature line
temperature_line, = ax.plot(
    [],
    [],
    color=PINK,
    linewidth=2.5,
    label="Temperature"
)


# Legend
legend = ax.legend(
    facecolor=BLACK,
    edgecolor=PINK
)

for text in legend.get_texts():
    text.set_color(WHITE)


figure.tight_layout()


canvas = FigureCanvasTkAgg(
    figure,
    master=temperature_frame
)

canvas.get_tk_widget().pack(
    fill=tk.BOTH,
    expand=True,
    padx=5,
    pady=5
)


# ============================================================
# RIGHT BOTTOM - PRESSURE AND SERVO VALUES
# ============================================================

status_frame = ttk.LabelFrame(
    right_frame,
    text="PRESSURE AND SERVO POSITIONS"
)

status_frame.grid(
    row=1,
    column=0,
    sticky="nsew"
)


for column in range(3):

    status_frame.columnconfigure(
        column,
        weight=1
    )


status_frame.rowconfigure(
    0,
    weight=1
)

status_frame.rowconfigure(
    1,
    weight=1
)


# ============================================================
# PRESSURE 0
# ============================================================

pressure0_label = ttk.Label(
    status_frame,
    text="Pressure 0\n0 psi",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

pressure0_label.grid(
    row=0,
    column=0,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# PRESSURE 1
# ============================================================

pressure1_label = ttk.Label(
    status_frame,
    text="Pressure 1\n0 psi",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

pressure1_label.grid(
    row=0,
    column=1,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# PRESSURE 2
# ============================================================

pressure2_label = ttk.Label(
    status_frame,
    text="Pressure 2\n0 psi",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

pressure2_label.grid(
    row=0,
    column=2,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# SERVO 0
# ============================================================

servo0_label = ttk.Label(
    status_frame,
    text="Servo 0\n0°",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

servo0_label.grid(
    row=1,
    column=0,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# SERVO 1
# ============================================================

servo1_label = ttk.Label(
    status_frame,
    text="Servo 1\n0°",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

servo1_label.grid(
    row=1,
    column=1,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# SERVO 2
# ============================================================

servo2_label = ttk.Label(
    status_frame,
    text="Servo 2\n0°",
    style="Data.TLabel",
    anchor="center",
    justify="center"
)

servo2_label.grid(
    row=1,
    column=2,
    sticky="nsew",
    padx=6,
    pady=6
)


# ============================================================
# RAW SERIAL DISPLAY
# ============================================================

def add_raw_line(line):

    # Timestamp generated by PC when line is received
    timestamp = datetime.now().strftime(
        "%H:%M:%S.%f"
    )[:-3]

    raw_text.insert(
        tk.END,
        f"[{timestamp}] {line}\n"
    )

    # Automatically scroll to newest data
    raw_text.see(tk.END)


# ============================================================
# CONVERT UNSIGNED 16-BIT VALUE TO SIGNED
# ============================================================

def to_signed_16(value):

    if value >= 32768:
        return value - 65536

    return value


# ============================================================
# PARSE TELEMETRY PACKET
# ============================================================

def parse_packet(line):

    global latest_servo0
    global latest_servo1
    global latest_servo2

    global latest_pressure0
    global latest_pressure1
    global latest_pressure2

    global latest_temperature


    # Mega Football printPacket() produces
    # space-separated decimal numbers
    parts = line.split()


    # ========================================================
    # TELEMETRY PACKET = 20 BYTES
    # ========================================================
    #
    # Byte 0       Header
    # Byte 1       Version
    # Byte 2       Destination
    # Byte 3       Source
    # Byte 4       Command
    #
    # Byte 5-6     Servo 0
    # Byte 7-8     Servo 1
    # Byte 9-10    Servo 2
    #
    # Byte 11-12   Pressure 0
    # Byte 13-14   Pressure 1
    # Byte 15-16   Pressure 2
    #
    # Byte 17      Temperature + 100
    # Byte 18      CRC8
    # Byte 19      Footer
    #
    # ========================================================


    if len(parts) != 20:
        return


    try:

        packet = [
            int(value)
            for value in parts
        ]

    except ValueError:

        return


    # ========================================================
    # VALIDATE TELEMETRY
    # ========================================================

    # HEADER = 0xAB = 171
    if packet[0] != 0xAB:
        return


    # VERSION
    if packet[1] != 1:
        return


    # Destination should be Mega Football = 4
    if packet[2] != 4:
        return


    # Command 105 = telemetry
    if packet[4] != 105:
        return


    # FOOTER = 0xEF = 239
    if packet[19] != 0xEF:
        return


    # ========================================================
    # DECODE SERVO 0
    # ============================================================

    servo0_raw = (
        (packet[5] << 8)
        | packet[6]
    )

    servo0 = to_signed_16(
        servo0_raw
    )


    # ========================================================
    # DECODE SERVO 1
    # ============================================================

    servo1_raw = (
        (packet[7] << 8)
        | packet[8]
    )

    servo1 = to_signed_16(
        servo1_raw
    )


    # ========================================================
    # DECODE SERVO 2
    # ============================================================

    servo2_raw = (
        (packet[9] << 8)
        | packet[10]
    )

    servo2 = to_signed_16(
        servo2_raw
    )


    # Examples:
    #
    # 0     -> 0°
    # 90    -> 90°
    # 65535 -> -1°
    # 65534 -> -2°


    # ========================================================
    # DECODE PRESSURE 0
    # ============================================================

    pressure0 = (
        (packet[11] << 8)
        | packet[12]
    )


    # ========================================================
    # DECODE PRESSURE 1
    # ============================================================

    pressure1 = (
        (packet[13] << 8)
        | packet[14]
    )


    # ========================================================
    # DECODE PRESSURE 2
    # ============================================================

    pressure2 = (
        (packet[15] << 8)
        | packet[16]
    )


    # ========================================================
    # DECODE TEMPERATURE
    # ============================================================

    temperature = (
        packet[17] - 100
    )


    # ========================================================
    # STORE LATEST VALUES
    # ============================================================

    latest_pressure0 = pressure0
    latest_pressure1 = pressure1
    latest_pressure2 = pressure2


    latest_servo0 = servo0
    latest_servo1 = servo1
    latest_servo2 = servo2


    latest_temperature = temperature


    # ========================================================
    # ADD TEMPERATURE POINT TO GRAPH
    # ============================================================

    current_time = datetime.now()

    time_data.append(
    current_time
)

    temperature_data.append(
    temperature
)


# ============================================================
# SERIAL READER THREAD
# ============================================================

def serial_reader():

    global running


    try:

        ser = serial.Serial(
            PORT,
            BAUD,
            timeout=1
        )


        root.after(
            0,
            add_raw_line,
            "Connected to Mega Football: "
            + PORT
        )


        while running:

            try:

                raw = ser.readline()


                if not raw:
                    continue


                line = raw.decode(
                    "utf-8",
                    errors="ignore"
                ).strip()


                if not line:
                    continue


                # =================================================
                # RAW SERIAL PANEL
                # =================================================

                root.after(
                    0,
                    add_raw_line,
                    line
                )


                # =================================================
                # PARSE SAME LINE FOR GUI VALUES
                # =================================================

                parse_packet(
                    line
                )


            except Exception as error:

                root.after(
                    0,
                    add_raw_line,
                    "Serial read error: "
                    + str(error)
                )


        ser.close()


    except Exception as error:

        root.after(
            0,
            add_raw_line,
            "Cannot open serial port: "
            + str(error)
        )


# ============================================================
# UPDATE GUI
# ============================================================

def update_gui():

    # ========================================================
    # TEMPERATURE GRAPH
    # ========================================================

    if len(time_data) > 1:

        temperature_line.set_data(
            list(time_data),
            list(temperature_data)
        )


        # Automatically change graph limits
        ax.relim()
        ax.autoscale_view()


        # Keep pink title when value changes
        ax.set_title(
            f"Temperature: "
            f"{latest_temperature} °C",
            color=PINK,
            fontsize=15,
            fontweight="bold"
        )


        canvas.draw_idle()


    # ========================================================
    # PRESSURE VALUES
    # ========================================================

    pressure0_label.config(
        text=
        f"Pressure 0\n"
        f"{latest_pressure0} psi"
    )


    pressure1_label.config(
        text=
        f"Pressure 1\n"
        f"{latest_pressure1} psi"
    )


    pressure2_label.config(
        text=
        f"Pressure 2\n"
        f"{latest_pressure2} psi"
    )


    # ========================================================
    # SERVO VALUES
    # ========================================================

    servo0_label.config(
        text=
        f"Servo 0\n"
        f"{latest_servo0}°"
    )


    servo1_label.config(
        text=
        f"Servo 1\n"
        f"{latest_servo1}°"
    )


    servo2_label.config(
        text=
        f"Servo 2\n"
        f"{latest_servo2}°"
    )


    # Refresh GUI every 100 ms
    root.after(
        100,
        update_gui
    )


# ============================================================
# CLEAN EXIT
# ============================================================

def close_program():

    global running

    running = False

    root.destroy()


root.protocol(
    "WM_DELETE_WINDOW",
    close_program
)


# ============================================================
# START SERIAL THREAD
# ============================================================

thread = threading.Thread(
    target=serial_reader,
    daemon=True
)

thread.start()


# ============================================================
# START GUI UPDATES
# ============================================================

update_gui()


# ============================================================
# RUN PROGRAM
# ============================================================

root.mainloop()