AV-031 - LoRa GSE Code Overview
APRIL 2025

Note: This explanation was written for code Full_GSE_RevP4_30Mar2026.

The system is essentially a chain of four devices that relay information across UART buses and radio. There are two physical locations in which the devices may be based:
	Station - The electronics directly beside the rocket during the launch. Must be operated remotely. Controls valves, provides fueling system data and other telemetry.
	Football - The electronics located at a safe distance from the rocket, where they are operated by the user. Consists of a control panel for changing valve states and displays telemetry and error data.

The microcontrollers are listed below in the order of the chain of communication:
	mega_station - An Elegoo Mega 2560 board on the Station
	lora_station - An ESP Heltec Wifi LoRa V3 board, capable of radio communication
	lora_football - An ESP Heltec Wifi LoRa V3 board, capable of radio communication
	mega_station - An Elegoo Mega 2560 board on the Football

Note: The Station may also be referred to as the "Manifold" (1 and 2). As a liquid engine requires equipment for the oxidizer and the fuel, the infrastructure is in development. The hardware and software will be identical for each manifold.

mega_station
The mega_station is located on the Station, beside the rocket. It handles servos (for valves) and pressure sensors, with room for future sensors.

setup() - The setup loop initializes the pressure sensors with their calibration constants and begins reading the servo encoders. The UART bus connecting to the lora_station is started.
loop() - The loop runs as often as possible and begins by updating each servo. This must be done in order to stop the servos when the encoders reach a specified value. It stores the position of the servo, compares it to the target destination, and if it is supposed to be moving. If at target, it will stop. If timed out, it will raise an error.

Next, it runs handleLoraInput(). This looks to see if any commands have been sent over the UART bus from the lora_station. It checks if available, runs a validity check, and confirms the DEST_ID field matches the mega_station. Then it will execute the command and send an acknowledgment.

Lastly, every specified interval (currently 100ms), the device will log and send telemetry data. It builds a packet with the new servo and pressure data, and forwards it to the lora_station (destined for the mega_football). This telemetry can be sent as fast as desired; however, it should be noted that since the lora radios are one directional, (and the UART buses), it is a bad idea to spam packets that are not expected to have transient behaviour.



lora_station
The next link on the chain is the lora_station, with radio capabilities. As the mega_station is used for two manifolds, and it was desired to have both identical (hardware and software), the lora_station completes non-repeated functions. This is operating the radio and controlling ignition. 

setup() handles the same ideas as the previous. It initializes ignition (a relay module), the radio, and the pin used for writing ignition.

In loop(), the lora_station runs handleMegaInput(), which stores telemetry packets (if valid) for sending during the next interval. If it is a non-telemetry packet, such as command acknowledgement or an error, then it will forward the message immediately.

It also supports a second manifold with handleSecondInput(). This is configured on a second UART bus, to avoid packet collisions. Should this be uncommented, the sendSelector() function must also be used, so that alternating packets will get forwarded (and not always just the same device).

sendNextPacket() takes the last valid telemetry packet and radios it to the Football.
handleLoraReceive() checks for any commands sent over the radio. It parses, validates, and logs them. If the command is for the lora_station, they get executed; otherwise, they are forwarded to the UART bus to the mega_station.


lora_football
This device runs nearly identically to the lora_station; however, it currently has no commands it can execute. It simply acts as a relay between the lora_station and the mega_football.


mega_football
This is what the user controls are connected to. It is in charge of taking that input, creating the appropriate commands, and sending them to the lora_football. The setup() initializes the various tiles that it is connected to; the pressure displays, ignition and error, switches control panel, and more as added. It has datalogging and the UART bus to the lora_football.

In the loop(), it uses handleLoraInput() for updating the telemetry data. It checks if it is valid and if it is the correct device, then will update the pressure displays, error lights, and valve position lights accordingly.

checkIgnition() is used to monitor the arm key and the ignition button. If the button is pressed, the ignition signal will be sent.

checkControlPanel() runs every interval, and checks all the arm / valve switches. It works by reading all current states into an array and updates all arm lights accordingly. Then, the valve switch states are compared to the last known state. If they are different (and the valve is armed), a command to change valve state will be sent, and the last known state updated. If they are not armed, the variables will update, but no command will be sent.
