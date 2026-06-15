
// ---------- Helper: clamp an angle to [-MAX_ANGLE_DEG, +MAX_ANGLE_DEG]
static inline float clampAngle(float a) {      // 25) Function header (fast inline)
  if (a >  MAX_ANGLE_DEG) return  MAX_ANGLE_DEG;// 26) If above +180, clamp down to +180
  if (a < -MAX_ANGLE_DEG) return -MAX_ANGLE_DEG;// 27) If below -180, clamp up to -180
  return a;                                    // 28) Otherwise return unchanged
}

// ---------- Helper: convert angle (deg) -> ticks (can be negative)
static inline long angleToTicks(float angleDeg) { // 29) Function header
  angleDeg = clampAngle(angleDeg);             // 30) Ensure requested angle is within -180..+180
  return lroundf(angleDeg * TICKS_PER_DEG);    // 31) Multiply by ticks/deg and round to nearest long
}

// ---------- Helper: stop motor
static inline void motorStop() {
  if (!motorReady) return;
  ledcWrite(RPWM, 0);
  ledcWrite(LPWM, 0);
}

// ---------- Helper: drive motor based on sign of error (your requested logic)
static inline void driveByError(long error, int speed) {
  speed = constrain(speed, 0, 255);

  if (error > 0) {              // need to move one way
    ledcWrite(LPWM, 0);
    ledcWrite(RPWM, speed);
  }
  else if (error < 0) {         // need to move the other way
    ledcWrite(RPWM, 0);
    ledcWrite(LPWM, speed);
  }
  else {                        // error == 0 -> stop
    ledcWrite(RPWM, 0);
    ledcWrite(LPWM, 0);
  }
}

//-------------------------DEBUGGING---------------------------------------------------------
void reportFaultIfAny() { 
  if (!motorFault) {
    faultPrinted = false;   // reset latch when fault clears naturally (e.g., new turning)
    return;
  }

  if (faultPrinted) return; // already reported this fault once

  Serial.print("ERROR: Motor fault detected -> ");

  if (motorFaultCode == FAULT_TIMEOUT) {
    Serial.println("TIMEOUT: motor could not reach target before TURN_TIMEOUT_MS.");
  } 
  else if (motorFaultCode == FAULT_STALL) {
    Serial.println("STALL: encoder ticks stopped changing while still far from target.");
  }
  else {
    Serial.println("UNKNOWN FAULT.");
  }

  // Optional extra debug info:
  Serial.print("  mode=IDLE, ticks=");
  Serial.print(myEnc.read());
  Serial.print(", targetTicks=");
  Serial.println(targetTicks);

  faultPrinted = true;
}
//---------------------------------------------------------------------


// ====================== MOTION UPDATE (CALL IN LOOP) ======================

void updateMotion() {                          // 54) Function header
  long currentTicks = myEnc.read();            // 55) Read current encoder ticks
  // ---------- TURNING MODE ----------
  if (mode == TURNING) {                       // 56) If we're turning toward a target
    unsigned long now = millis();

  // ===== TIMEOUT (add) ===== FOR WHATEVER REASON, AFTER 6S IT TILL AUTOMATICALLY TURN OFF = TIME OUT STATUS
    if (now - turnStartTime > TURN_TIMEOUT_MS) {
      motorStop();
      mode = IDLE;
      motorFault = true;
      motorFaultCode = FAULT_TIMEOUT;
      Serial.println(lastTurnCheckTime);
      Serial.println(now);
      return;
    }

    long error = targetTicks - currentTicks;   // 57) Compute how far from target
    if (labs(error) <= POS_TOL_TICKS) {        // 58) If close enough to target
      motorStop();                             // 59) Stop motor
      mode = IDLE;                             // 60) Go idle
      return;                                  // 61) Exit updateMotion
    }
    if (now - lastTurnCheckTime >= TURN_STALL_CHECK_MS) {
      long moved = labs(currentTicks - lastTurnCheckTicks);
    // if still far from target but not moving -> stall
      if (moved <= TURN_STALL_MIN_DELTA_TICKS && labs(error) > (POS_TOL_TICKS * 4)) {
        motorStop();
        mode = IDLE;
        motorFault = true;
        motorFaultCode = FAULT_STALL;
        return;
      }

      lastTurnCheckTicks = currentTicks;
      lastTurnCheckTime = now;
  }

    long dist = labs(error);                   // 62) Distance from target in ticks (positive number)

    dist = constrain(dist, 0L, 400L);          // 63) Limit distance used for speed mapping
    int speed = map((int)dist, 0, 400, SPEED_MIN, SPEED_MAX); // 64) Farther -> faster
    speed = constrain(speed, SPEED_MIN, SPEED_MAX);           // 65) Enforce safe speed bounds

    driveByError(error, speed);                // 66) Apply direction & duty using your rule
    return;                                    // 67) Done for this loop cycle
  }

  // ---------- CALIBRATING MODE ----------
  if (mode == CALIBRATING) {                   // 68) If we're homing
    if (homeDir == HOME_CW) {                  // 69) If chosen home direction is CW
      ledcWrite(LPWM, 0);                      // 70) Ensure opposite direction off
      ledcWrite(RPWM, CAL_SPEED);              // 71) Drive slowly toward hard stop
    } else {                                   // 72) Else home direction is CCW
      ledcWrite(RPWM, 0);                      // 73) Ensure opposite direction off
      ledcWrite(LPWM, CAL_SPEED);              // 74) Drive slowly toward hard stop
    }

    unsigned long now = millis();              // 75) Get current time

    if (now - lastSampleTime >= STALL_WINDOW_MS) { // 76) If time to sample encoder movement
      long delta = labs(currentTicks - lastTicksSample); // 77) How much encoder changed since last sample

      if (delta <= STALL_DELTA_TICKS) {        // 78) If essentially no movement -> possible hard stop
        if (!stallTiming) {                    // 79) If we just entered possible stall
          stallTiming = true;                  // 80) Start stall timing
          stallStartTime = now;                // 81) Mark when it started
        } else {                               // 82) Already timing a stall
          if (now - stallStartTime >= STALL_HOLD_MS) { // 83) If stall lasted long enough
            motorStop();                       // 84) Stop motor at hard stop
            myEnc.write(0);                    // 85) Reset encoder count to 0 = "home"
            mode = IDLE;                       // 86) Exit calibration
            stallTiming = false;               // 87) Reset stall state
            return;                            // 88) Done
          }
        }
      } else {                                 // 89) Encoder moved normally
        stallTiming = false;                   // 90) Not stalled; clear stall timer
      }

      lastTicksSample = currentTicks;          // 91) Store current ticks as the new sample baseline
      lastSampleTime = now;                    // 92) Store time of this sample
    }

    return;                                    // 93) Done for this loop cycle
  }

  // ---------- IDLE MODE ----------
  // 94) If mode == IDLE, do nothing (motor should already be stopped)
}

// ====================== ARDUINO STANDARD ======================


void loop() {                                  // 103) Runs repeatedly forever
  handleSerialCommands();  
  updateMotion();                              // 104) Keep executing turning/calibration state machine
  reportFaultIfAny(); 
}
