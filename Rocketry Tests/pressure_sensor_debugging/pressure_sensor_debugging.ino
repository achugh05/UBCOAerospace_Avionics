// Pin assignments
const int sensorPins[3] = {A0, A1, A2};

// Variables to hold readings
int sensorValues[3];
float sensorVoltage[3];
float sensorPressure[3];

void setup() {
  Serial.begin(9600); // Start serial communication
}

void loop() {
  // Read each sensor
  for (int i = 0; i < 3; i++) {
    sensorValues[i] = analogRead(sensorPins[i]);         // ADC value (0-1023)
    sensorVoltage[i] = sensorValues[i] * (5.0 / 1023);  // Convert ADC to voltage
    sensorPressure[i] = (sensorVoltage[i] - 0.5) * 400; // Convert voltage to PSI
  }

  // Print voltages
  Serial.print("Voltages (V): ");
  for (int i = 0; i < 3; i++) {
    Serial.print(sensorVoltage[i], 2);
    if (i < 2) Serial.print(", ");
  }

  // Print pressures
  Serial.print(" | Pressures (PSI): ");
  for (int i = 0; i < 3; i++) {
    Serial.print(sensorPressure[i], 1);
    if (i < 2) Serial.print(", ");
  }

  Serial.println();
  delay(500); // Half-second delay between readings
}