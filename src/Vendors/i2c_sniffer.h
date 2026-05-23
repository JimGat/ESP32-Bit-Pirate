/**
 * @AUTHOR Ákos Szabó (Whitehawk Tailor) - aaszabo@gmail.com
 * 
 * This is an I2C sniffer that logs traffic on I2C BUS.
 * 
 * It is not part of the I2C BUS. It is neither a Master, nor a Slave and puts no data to the lines.
 * It just listens and logs the communication.
 * 
 * Two pins as input are attached to SDC and SDA lines.
 * Since the I2C communications runs on 400kHz so,
 * the tool that runs this program should be fast.
 * This was tested on an ESP32 bord Heltec WiFi Lora32 v2
 * ESP32 core runs on 240MHz.
 * It means there are 600 ESP32 cycles during one I2C clock tick.
 *
 * 
 * The program uses interrupts to detect
 * the raise edge of the SCL - bit transfer 
 * the falling edge of SDA if SCL is HIGH- START
 * the raise edge of SDA if SCL id HIGH - STOP 
 * 
 * In the interrupt routines there is just a few line of code
 * that mainly sets the status and stores the incoming bits.
 * Otherwise the program gets timeout panic in interrupt handler and
 * restart the CPU.
 *                   https://github.com/WhitehawkTailor/I2C-sniffer/
 */
#include <Arduino.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void i2c_sniffer_begin(uint8_t scl, uint8_t sda);
bool i2c_sniffer_setup();
void i2c_sniffer_stop();
void i2c_sniffer_release();
bool i2c_sniffer_available();
char i2c_sniffer_read();
void i2c_sniffer_reset_buffer();

#ifdef __cplusplus
}
#endif
