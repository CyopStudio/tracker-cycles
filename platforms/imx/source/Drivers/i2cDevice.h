#ifndef SOURCE_I2CDEVICE_H_
#define SOURCE_I2CDEVICE_H_

class I2cBusController;

class I2cDevice
{
  public:
    virtual bool update() = 0;
    void setController(I2cBusController* busController) { this->i2cBusController = busController; }

  protected:
    I2cBusController* i2cBusController;
};

#endif /* SOURCE_I2CDEVICE_H_ */
