#define IGNITION 4

void setup() {
  // put your setup code here, to run once:
  pinMode(IGNITION, OUTPUT);
  digitalWrite(IGNITION, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(IGNITION, HIGH);
  delay(5000); // Review required
  digitalWrite(IGNITION, LOW);
  delay(5000);
}
