#ifndef SOURCE_DRIVERS_EXTERNALSDRAM_H_
#define SOURCE_DRIVERS_EXTERNALSDRAM_H_

class ExternalSDRAM
{
  public:
    void init();
    void update();
    void setLowPowerMode(bool lowPowerMode);
    bool test();

  private:
    static constexpr int kRamSize = 1024 * 1024 * 4;
    bool testRamBlock(int blockIdx, int* const startAddress);
};

extern ExternalSDRAM externalRAM;

#endif /* SOURCE_DRIVERS_EXTERNALSDRAM_H_ */
