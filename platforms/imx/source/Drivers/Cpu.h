/*
 * Cpu.h
 *
 *  Created on: Sep 14, 2023
 *      Author: radekp
 */

#ifndef CPU_H_
#define CPU_H_

class Cpu
{
  public:
    Cpu();
    virtual ~Cpu();

    void setVoltage(uint32_t Voltage);
    uint32_t getVoltage();

    void setFrequency(uint32_t Frequency);
    uint32_t getFrequency();

    uint32_t getTemperature();

    void update();

  private:
    uint32_t SetArmClock(uint32_t Frequency);
};

#endif /* CPU_H_ */
