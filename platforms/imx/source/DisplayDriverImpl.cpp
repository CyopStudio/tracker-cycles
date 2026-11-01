#include "DisplayDriverImpl.h"
#include "FT812.h"
#include "pin_mux.h"

uint8_t DisplayDriverImpl::streamUpdateStep = 0;

void dataTransferComplite();

void DisplayDriverImpl::init(SpiBus* spi)
{
    this->spi = spi;
    FT812_Init(BOARD_INITPINS_DISPLAY_PD_GPIO,
               BOARD_INITPINS_DISPLAY_PD_GPIO_PIN,
               BOARD_INITPINS_DISPLAY_CS_GPIO,
               BOARD_INITPINS_DISPLAY_CS_GPIO_PIN,
               BOARD_INITPINS_DISPLAY_RESET_GPIO,
               BOARD_INITPINS_DISPLAY_RESET_GPIO_PIN,
               spi,
               &kConfig800_480,
               false);
    FT812_setCMDOffsetPtr(&cmdOffset);
}

void DisplayDriverImpl::setPowerState(bool state)
{
    if (state)
    {
        MCU_set_runMode();
    }
    else
    {
        MCU_set_sleepMode();
    }
}

void DisplayDriverImpl::setBrightness(int value)
{
    EVE_MemWrite8(REG_PWM_DUTY, value);
}

void DisplayDriverImpl::setRotation(int value)
{
    EVE_MemWrite8(REG_ROTATE, value);
}

void DisplayDriverImpl::getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height)
{
    uint16_t Reg_Cmd_Write_Offset;
    Reg_Cmd_Write_Offset = EVE_MemRead16(REG_CMD_WRITE);

    uint32_t ParameterAddr = ((Reg_Cmd_Write_Offset - 12) & 4095); // 12 bytes back
    addr = EVE_MemRead32((RAM_CMD + ParameterAddr));

    ParameterAddr = ((Reg_Cmd_Write_Offset - 8) & 4095); // 8 bytes back
    width = EVE_MemRead32((RAM_CMD + ParameterAddr));

    ParameterAddr = ((Reg_Cmd_Write_Offset - 4) & 4095); // 4 bytes back
    height = EVE_MemRead32((RAM_CMD + ParameterAddr));
}

uint32_t DisplayDriverImpl::getDisplayListOffset()
{
    return EVE_MemRead32(REG_CMD_DL);
}

void DisplayDriverImpl::IncCMDOffset(uint16_t value)
{
    cmdOffset = EVE_IncCMDOffset(cmdOffset, value);
}

void DisplayDriverImpl::WriteData32(uint32_t data)
{
    EVE_Write32(data);
}

void DisplayDriverImpl::beginCoProList()
{
    API_LIB_BeginCoProList();
}

void DisplayDriverImpl::beginCoProListNoCheck()
{
    API_LIB_BeginCoProListNoCheck();
}

void DisplayDriverImpl::endCoProList()
{
    API_LIB_EndCoProList();
}

void DisplayDriverImpl::awaitCoProEmpty()
{
    API_LIB_AwaitCoProEmpty();
}

uint8_t DisplayDriverImpl::isCoProEmpty()
{
    return API_LIB_IsCoProEmpty();
}

void DisplayDriverImpl::writeDataRAMG(const uint8_t* imgData,
                                      uint32_t dataSize,
                                      uint32_t destAddress)
{
    API_LIB_WriteDataRAMG(imgData, dataSize, destAddress);
}

void DisplayDriverImpl::writeFontRAMG(const uint8_t* imgData,
                                      uint32_t dataSize,
                                      uint32_t destAddress)
{
    API_LIB_WriteFontRAMG(imgData, dataSize, destAddress);
}

void DisplayDriverImpl::readDataRAMG(uint8_t* imgData, uint32_t dataSize, uint32_t sourceAddress)
{
    API_LIB_ReadDataRAMG(imgData, dataSize, sourceAddress);
}

void DisplayDriverImpl::readBackDataRAMG(uint8_t* imgData,
                                         uint32_t dataSize,
                                         uint32_t sourceAddress)
{
    API_LIB_ReadBackDataRAMG(imgData, dataSize, sourceAddress);
}

uint8_t DisplayDriverImpl::sendString(const char* string, uint32_t length)
{
    return API_SendString(string, length);
}

void DisplayDriverImpl::writeDataToCMD(const uint8_t* imgData, uint32_t totalDataSize)
{
    API_LIB_WriteDataToCMD(imgData, totalDataSize);
}

//========================================================================================================
void DisplayDriverImpl::initStreamDma(uint16_t byteResolution,
                                      uint16_t streamWidth,
                                      uint16_t streamHeight,
                                      uint8_t* streamBuf,
                                      uint32_t destMemAddress)
{
    setUpdateDmaStep(0);

    if (streamInitialized)
    {
        return;
    }

    streamBufDestMemAddress = destMemAddress;
    screenBuffSize = byteResolution * streamWidth * streamHeight;
    screenBuff = streamBuf;

    spi->initDma(dataTransferComplite, screenBuff, screenBuffSize);
    streamInitialized = true;
}

void DisplayDriverImpl::stopStreamDma()
{
    spi->stopDma();
    MCU_CShigh();
    streamInitialized = false;
}

bool DisplayDriverImpl::updateStreamDma(bool lastFrame)
{
    if (!streamInitialized)
        return true;

    if (streamUpdateStep == 0 && !lastFrame)
        return false;

    bool refreshScreenNow = false;

    switch (streamUpdateStep)
    {
        case 0: // DMA send start
        {
            sendDataDma();
            break;
        }
        case 1: // sending in progress
        {
            break;
        }
        case 2: // DMA transfer finished - time to show frame
        {
            refreshScreenNow = true;
            setUpdateDmaStep(3);
            break;
        }
        case 3:
        {
            setUpdateDmaStep(0);
            break;
        }
    }

    return refreshScreenNow;
}

void DisplayDriverImpl::sendDataDma()
{
    MCU_CSlow(); // CS low begins SPI transaction
    EVE_AddrForWr(streamBufDestMemAddress);

    spi->transferDmaBuff();

    setUpdateDmaStep(1);
}

void dataTransferComplite()
{
    MCU_CShigh();

    DisplayDriverImpl::setUpdateDmaStep(2);
}
