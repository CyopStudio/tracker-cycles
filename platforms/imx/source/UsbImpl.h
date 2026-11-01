#ifndef USBIMPL_H_
#define USBIMPL_H_

#include "Platform.h"
#include "composite.h"
#include "config.h"

extern "C" usb_composite_state_t g_UsbState;

class UsbImpl final : public Usb
{
  public:
    virtual void init() override;
    /**
     * This method should be called whenever any changes in enabled
     * functions are done using SetClassState method to reset USB
     * and allow enumeration of a new set of USB functions
     */
    virtual void reset(void) override;
    /**
     * For not critical USB endpoints this method should be called
     * on regular basis to handle them in non IRQ context i.e. storage and serial
     */
    virtual void update(void) override;
    /**
     * This method is responsible for enabling required USB functions.
     *
     * Midi - enabled by default and should be always enabled
     *
     * AudioIn and AudioOut - enabled by default but should be configurable
     * 	from UI and enabled based on NVM settings
     *
     * Storage - disabled by default unless develop mode is enabled. Should
     * 	be configurable from UI on demand
     *
     * Serial - disabled by default unless develop mode is enabled.
     */
    virtual void setClassState(Usb::ClassType type, bool enable) override;
    /**
     * This method retrieves whether certain USB function is enabled
     */
    virtual bool getClassState(Usb::ClassType type) override;
    /**
     * This method returns true if device is connected to a host and false when not
     */
    virtual bool isAttached() override;
};

#endif /* USBIMPL_H_ */
