/*
    BonaDroneFC.ino : Arduino sketch to perform altitude estimation using
    BonaDrone's FC sensors and VL53L1X Rangefinder

    Author: Juan Gallostra Acín

    Additional libraries required (under your Arduino/libraries folder):

      https://github.com/simondlevy/VL53L1X
      https://github.com/simondlevy/LPS22HB
      https://github.com/simondlevy/LSM6DSM
      https://github.com/simondlevy/CrossPlatformDataBus


    Copyright (c) 2018 Juan Gallostra

    This file is part of the Arduino Range-Baro-AltitudeEstimation library.

    The Arduino Range-Baro-AltitudeEstimation library is free software:
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License as published by the Free Software Foundation,
    either version 3 of the License, or (at your option) any later version.

    The Arduino Range-Baro-AltitudeEstimation library is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
    <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <Wire.h>
// IMU
#include <LSM6DSM.h>
// Barometer
#include <LPS22HB.h>
// Rangefinder
#include <VL53L1X.h>
// altitude estimator
#include "estimator.h"

uint8_t LED_PIN = 38;
// --- IMU related variables and functions ---
// LSM6DSM full-scale settings
static const LSM6DSM::Ascale_t Ascale = LSM6DSM::AFS_2G;
static const LSM6DSM::Gscale_t Gscale = LSM6DSM::GFS_2000DPS;
static const LSM6DSM::Rate_t   AODR   = LSM6DSM::ODR_1660Hz;
static const LSM6DSM::Rate_t   GODR   = LSM6DSM::ODR_1660Hz;

// Biases computed by Kris
float ACCEL_BIAS[3] = {0.0, 0.0, 0.0};
float GYRO_BIAS[3]  = {0.0, 0.0, 0.0};


LSM6DSM lsm6dsm = LSM6DSM(Ascale, Gscale, AODR, GODR, ACCEL_BIAS, GYRO_BIAS);

static void imuRead(float gyro[3], float accel[3])
{
    if (lsm6dsm.checkNewData()) {
        float _ax, _ay, _az, _gx, _gy, _gz;
        lsm6dsm.readData(_ax, _ay, _az, _gx, _gy, _gz);

        // Negate to support board orientation
        _ax = -_ax;
        _gy = -_gy;
        _gz = -_gz;

        // Copy gyro values back out in rad/sec
        gyro[0] = _gx * M_PI / 180.0f;
        gyro[1] = _gy * M_PI / 180.0f;
        gyro[2] = _gz * M_PI / 180.0f;
        // and acceleration values
        accel[0] = _ax;
        accel[1] = _ay;
        accel[2] = _az;

    } 
}

// --- Barometer related variables and functions ---
// Pressure and temperature oversample rate
static LPS22HB::Rate_t ODR = LPS22HB::P_75Hz;     
static LPS22HB lps22hb = LPS22HB(ODR);

// --- Rangefinder related variables and functions ---
static VL53L1X distanceSensor;

// --- Altitude estimator ---
static cp::AltitudeEstimator altitude = cp::AltitudeEstimator(20.0);

unsigned long pastTime;
// -- Sensor and communication protocols initialization ---  
void setup(void)
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    // Start I^2C
    Wire.begin(TWI_PINS_20_21);
    Wire.setClock(400000); // I2C frequency at 400 kHz
    delay(1000);

    // initialize sensors
    lsm6dsm.begin();
    lsm6dsm.calibrate(GYRO_BIAS, ACCEL_BIAS);
    lps22hb.begin();
    if (distanceSensor.begin() == false) {
        while (true) {
            Serial.println("Sensor offline!");
            delay(200);
        }
    }
    // initialize the estimator
    altitude.init();

    // Begin serial comms
    Serial.begin(115200);
    // initialize timer
    pastTime = millis();
    digitalWrite(LED_PIN, HIGH);
}

// --- Main loop to be executed ---
void loop(void)
{
      unsigned long currentTime = millis();
      if ((currentTime - pastTime) > 50)
      {
          // read sensors
          float pressure = lps22hb.readPressure();
          float rangeHeight = (float)distanceSensor.getDistance() / 1000.0f;
          float accelData[3];
          float gyroData[3];
          imuRead(gyroData, accelData);
          // update estimation
          altitude.estimate(accelData, gyroData, rangeHeight, pressure);

          // Send results through serial
          Serial.print(altitude.range.getAltitude());
          Serial.print(",");
          Serial.print(altitude.baro.getAltitude());
          Serial.print(",");
          Serial.println(altitude.getAltitude());
        
          pastTime = currentTime;
      }
}
