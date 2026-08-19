import serial
import threading
import tkinter as tk

from tkinter import ttk
from collections import deque
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageTk

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

YELLOW = "#FFD400"
HEADER_TEXT = "#000000"


# ============================================================
# LOGO CONFIGURATION
# ============================================================

BASE_DIR = Path(__file__).resolve().parent
LOGO_PATH = BASE_DIR / "logo.png"

LOGO_WIDTH = 280
LOGO_HEIGHT = 130


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

root.geometry("1400x800")

root.minsize(
    1100,
    700
)

root.configure(
    bg=BLACK
)


# ============================================================
# TTK STYLE
# ============================================================

style = ttk.Style()

style.theme_use("clam")


# ============================================================
# GENERAL FRAME STYLE
# ============================================================

style.configure(
    "TFrame",
    background=BLACK
)


# ============================================================
# LABEL FRAME STYLE
# ============================================================

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

    font=(
        "Arial",
        12,
        "bold"
    )
)


# ============================================================
# GENERAL LABEL STYLE
# ============================================================

style.configure(
    "TLabel",

    background=BLACK,

    foreground=WHITE
)


# ============================================================
# SCROLLBAR STYLE
# ============================================================

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
# ROOT LAYOUT
#
# ROW 0 = FULL WIDTH YELLOW HEADER
# ROW 1 = SERIAL MONITOR + TELEMETRY
# ============================================================

root.rowconfigure(
    0,
    weight=0
)

root.rowconfigure(
    1,
    weight=1
)


# Left side
root.columnconfigure(
    0,
    weight=3
)

# Right side
root.columnconfigure(
    1,
    weight=2
)


# ============================================================
# FULL WIDTH YELLOW HEADER
# ============================================================

header_frame = tk.Frame(
    root,

    bg=YELLOW,

    highlightbackground=YELLOW,
    highlightcolor=YELLOW,

    highlightthickness=2,

    height=125
)


header_frame.grid(
    row=0,
    column=0,

    columnspan=2,

    sticky="nsew",

    padx=8,

    pady=(
        8,
        4
    )
)


header_frame.grid_propagate(
    False
)


# ============================================================
# BRAND CONTAINER
#
# This frame lets TITLE + LOGO remain centered as one group.
# ============================================================

brand_frame = tk.Frame(
    header_frame,

    bg=YELLOW
)


brand_frame.place(
    relx=0.5,
    rely=0.5,

    anchor="center"
)


# ============================================================
# UBCO ROCKETRY TITLE
#
# TITLE FIRST
# ============================================================

title_label = tk.Label(
    brand_frame,

    text="UBCO AEROSPACE",

    bg=YELLOW,

    fg=HEADER_TEXT,

    font=(
        "Arial",
        45,
        "bold"
    ),

    anchor="center",

    justify="center"
)


title_label.grid(
    row=0,
    column=0,

    padx=(
        10,
        25
    ),

    pady=5
)


# ============================================================
# LOGO
#
# LOGO SECOND
# ============================================================

try:

    logo_image = Image.open(
        LOGO_PATH
    ).convert(
        "RGBA"
    )


    logo_image.thumbnail(
        (
            LOGO_WIDTH,
            LOGO_HEIGHT
        ),

        Image.Resampling.LANCZOS
    )


    logo_photo = ImageTk.PhotoImage(
        logo_image
    )


    logo_label = tk.Label(
        brand_frame,

        image=logo_photo,

        bg=YELLOW,

        borderwidth=0,

        highlightthickness=0
    )


    logo_label.grid(
        row=0,
        column=1,

        padx=(
            0,
            10
        ),

        pady=5
    )


    # Keep reference to image
    logo_label.image = logo_photo


except Exception as error:

    logo_label = tk.Label(
        brand_frame,

        text="LOGO",

        bg=YELLOW,

        fg=BLACK,

        font=(
            "Arial",
            30,
            "bold"
        )
    )


    logo_label.grid(
        row=0,
        column=1,

        padx=10,

        pady=5
    )


    print(
        "Logo loading error:",
        error
    )


# ============================================================
# LEFT PANEL
# RAW SERIAL DATA
#
# Starts directly under yellow header
# ============================================================

left_frame = ttk.LabelFrame(
    root,

    text="RAW SERIAL DATA"
)


left_frame.grid(
    row=1,
    column=0,

    sticky="nsew",

    padx=(
        8,
        4
    ),

    pady=(
        4,
        8
    )
)


left_frame.rowconfigure(
    0,
    weight=1
)


left_frame.columnconfigure(
    0,
    weight=1
)


# ============================================================
# RAW SERIAL TEXT
# ============================================================

raw_text = tk.Text(
    left_frame,

    wrap="none",

    font=(
        "Courier",
        16
    ),

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


# ============================================================
# VERTICAL SCROLLBAR
# ============================================================

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


# ============================================================
# HORIZONTAL SCROLLBAR
# ============================================================

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
# RIGHT TELEMETRY PANEL
#
# Also begins directly under yellow header
# ============================================================

right_frame = ttk.Frame(
    root
)


right_frame.grid(
    row=1,
    column=1,

    sticky="nsew",

    padx=(
        4,
        8
    ),

    pady=(
        4,
        8
    )
)


right_frame.columnconfigure(
    0,
    weight=1
)


# Graph gets more vertical space
right_frame.rowconfigure(
    0,
    weight=3
)


# Pressure / servo panel
right_frame.rowconfigure(
    1,
    weight=2
)


# ============================================================
# TEMPERATURE GRAPH FRAME
# ============================================================

temperature_frame = ttk.LabelFrame(
    right_frame,

    text="TEMPERATURE VS TIME"
)


temperature_frame.grid(
    row=0,
    column=0,

    sticky="nsew",

    pady=(
        0,
        6
    )
)


# ============================================================
# MATPLOTLIB FIGURE
# ============================================================

figure = Figure(
    figsize=(
        7,
        4
    ),

    dpi=100,

    facecolor=BLACK
)


ax = figure.add_subplot(
    111
)


ax.set_facecolor(
    DARK
)


# ============================================================
# GRAPH AXIS LABELS
# ============================================================

ax.set_xlabel(
    "Time",

    color=WHITE,

    fontsize=15
)


ax.set_ylabel(
    "Temperature (°C)",

    color=WHITE,

    fontsize=15
)


# ============================================================
# GRAPH TITLE
# ============================================================

ax.set_title(
    "Vent Temperature",

    color=PINK,

    fontsize=16,

    fontweight="bold"
)


# ============================================================
# GRAPH TICKS
# ============================================================

ax.tick_params(
    axis="x",

    colors=WHITE,

    labelsize=9
)


ax.tick_params(
    axis="y",

    colors=WHITE,

    labelsize=9
)


# ============================================================
# TIME FORMAT
# ============================================================

time_locator = mdates.AutoDateLocator()


ax.xaxis.set_major_locator(
    time_locator
)


ax.xaxis.set_major_formatter(
    mdates.DateFormatter(
        "%H:%M:%S"
    )
)


figure.autofmt_xdate()


# ============================================================
# GRAPH BORDER
# ============================================================

for spine in ax.spines.values():

    spine.set_color(
        PINK
    )


# ============================================================
# GRAPH GRID
# ============================================================

ax.grid(
    True,

    color=GRID_GRAY,

    alpha=0.45
)


# ============================================================
# TEMPERATURE LINE
# ============================================================

temperature_line, = ax.plot(
    [],
    [],

    color=PINK,

    linewidth=2.5,

    label="Temperature"
)


# ============================================================
# LEGEND
# ============================================================

legend = ax.legend(
    facecolor=BLACK,

    edgecolor=PINK
)


for text in legend.get_texts():

    text.set_color(
        WHITE
    )


figure.tight_layout()


# ============================================================
# GRAPH CANVAS
# ============================================================

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
# PRESSURE + SERVO PANEL
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


# ============================================================
# STATUS GRID
# ============================================================

for column in range(3):

    status_frame.columnconfigure(
        column,

        weight=1,

        uniform="status_columns"
    )


for row in range(2):

    status_frame.rowconfigure(
        row,

        weight=1,

        uniform="status_rows"
    )


# ============================================================
# CREATE CENTERED TELEMETRY BOX
# ============================================================

def create_data_box(
    parent,
    row,
    column,
    title,
    value,
    unit
):

    frame = tk.Frame(
        parent,

        bg=DARK,

        highlightbackground=PINK,

        highlightcolor=PINK,

        highlightthickness=1
    )


    frame.grid(
        row=row,
        column=column,

        sticky="nsew",

        padx=5,

        pady=5
    )


    # ========================================================
    # CENTER EVERYTHING
    # ========================================================

    frame.columnconfigure(
        0,
        weight=1
    )


    frame.rowconfigure(
        0,
        weight=1
    )


    frame.rowconfigure(
        1,
        weight=1
    )


    frame.rowconfigure(
        2,
        weight=1
    )


    # ========================================================
    # TITLE
    # ========================================================

    title_label = tk.Label(
        frame,

        text=title,

        bg=DARK,

        fg=WHITE,

        font=(
            "Arial",
            13,
            "bold"
        ),

        anchor="center",

        justify="center"
    )


    title_label.grid(
        row=0,
        column=0,

        sticky="nsew"
    )


    # ========================================================
    # VALUE
    # ========================================================

    value_label = tk.Label(
        frame,

        text=str(value),

        bg=DARK,

        fg=PINK,

        font=(
            "Arial",
            26,
            "bold"
        ),

        anchor="center",

        justify="center"
    )


    value_label.grid(
        row=1,
        column=0,

        sticky="nsew"
    )


    # ========================================================
    # UNIT
    # ========================================================

    unit_label = tk.Label(
        frame,

        text=unit,

        bg=DARK,

        fg=WHITE,

        font=(
            "Arial",
            11,
            "bold"
        ),

        anchor="center",

        justify="center"
    )


    unit_label.grid(
        row=2,
        column=0,

        sticky="nsew"
    )


    return value_label


# ============================================================
# PRESSURE BOXES
# ============================================================

pressure0_value = create_data_box(
    status_frame,
    0,
    0,
    "PRESSURE 0",
    0,
    "psi"
)


pressure1_value = create_data_box(
    status_frame,
    0,
    1,
    "PRESSURE 1",
    0,
    "psi"
)


pressure2_value = create_data_box(
    status_frame,
    0,
    2,
    "PRESSURE 2",
    0,
    "psi"
)


# ============================================================
# SERVO BOXES
# ============================================================

servo0_value = create_data_box(
    status_frame,
    1,
    0,
    "SERVO 0",
    0,
    "°"
)


servo1_value = create_data_box(
    status_frame,
    1,
    1,
    "SERVO 1",
    0,
    "°"
)


servo2_value = create_data_box(
    status_frame,
    1,
    2,
    "SERVO 2",
    0,
    "°"
)


# ============================================================
# RAW SERIAL DISPLAY
# ============================================================

def add_raw_line(line):

    timestamp = datetime.now().strftime(
        "%H:%M:%S.%f"
    )[:-3]


    raw_text.insert(
        tk.END,

        f"[{timestamp}] {line}\n"
    )


    raw_text.see(
        tk.END
    )


# ============================================================
# CONVERT UNSIGNED 16-BIT TO SIGNED
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
    # VALIDATE PACKET
    # ========================================================

    if packet[0] != 0xAB:

        return


    if packet[1] != 1:

        return


    if packet[2] != 4:

        return


    if packet[4] != 105:

        return


    if packet[19] != 0xEF:

        return


    # ========================================================
    # SERVO 0
    # ========================================================

    servo0_raw = (
        (packet[5] << 8)
        |
        packet[6]
    )


    servo0 = to_signed_16(
        servo0_raw
    )


    # ========================================================
    # SERVO 1
    # ========================================================

    servo1_raw = (
        (packet[7] << 8)
        |
        packet[8]
    )


    servo1 = to_signed_16(
        servo1_raw
    )


    # ========================================================
    # SERVO 2
    # ========================================================

    servo2_raw = (
        (packet[9] << 8)
        |
        packet[10]
    )


    servo2 = to_signed_16(
        servo2_raw
    )


    # ========================================================
    # PRESSURE 0
    # ========================================================

    pressure0 = (
        (packet[11] << 8)
        |
        packet[12]
    )


    # ========================================================
    # PRESSURE 1
    # ========================================================

    pressure1 = (
        (packet[13] << 8)
        |
        packet[14]
    )


    # ========================================================
    # PRESSURE 2
    # ========================================================

    pressure2 = (
        (packet[15] << 8)
        |
        packet[16]
    )


    # ========================================================
    # TEMPERATURE
    # ========================================================

    temperature = (
        packet[17] - 100
    )


    # ========================================================
    # STORE VALUES
    # ========================================================

    latest_pressure0 = pressure0
    latest_pressure1 = pressure1
    latest_pressure2 = pressure2


    latest_servo0 = servo0
    latest_servo1 = servo1
    latest_servo2 = servo2


    latest_temperature = temperature


    # ========================================================
    # ADD GRAPH POINT
    # ========================================================

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


    ser = None


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
                # RAW SERIAL
                # =================================================

                root.after(
                    0,

                    add_raw_line,

                    line
                )


                # =================================================
                # PARSE DATA
                # =================================================

                parse_packet(
                    line
                )


            except Exception as error:

                if running:

                    root.after(
                        0,

                        add_raw_line,

                        "Serial read error: "
                        + str(error)
                    )


    except Exception as error:

        root.after(
            0,

            add_raw_line,

            "Cannot open serial port: "
            + str(error)
        )


    finally:

        if ser is not None:

            try:

                if ser.is_open:

                    ser.close()


            except Exception:

                pass


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


        ax.relim()

        ax.autoscale_view()


        ax.set_title(
            f"Temperature: "
            f"{latest_temperature} °C",

            color=PINK,

            fontsize=16,

            fontweight="bold"
        )


        canvas.draw_idle()


    # ========================================================
    # PRESSURE
    # ========================================================

    pressure0_value.config(
        text=str(
            latest_pressure0
        )
    )


    pressure1_value.config(
        text=str(
            latest_pressure1
        )
    )


    pressure2_value.config(
        text=str(
            latest_pressure2
        )
    )


    # ========================================================
    # SERVO
    # ========================================================

    servo0_value.config(
        text=str(
            latest_servo0
        )
    )


    servo1_value.config(
        text=str(
            latest_servo1
        )
    )


    servo2_value.config(
        text=str(
            latest_servo2
        )
    )


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