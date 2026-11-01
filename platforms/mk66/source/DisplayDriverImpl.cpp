#include "DisplayDriverImpl.h"
#include "DMAChannel.h"
#include "FT812.h"
#include "wiring.h"

static DMAChannel dmatx;
DMASetting* dmasettings = nullptr;

uint8_t DisplayDriverImpl::streamUpdateStep = 0;

void dataTransferComplite();

void DisplayDriverImpl::init()
{
    FT812_Init();
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

    uint32_t SCREEN_BUFF_SIZE = byteResolution * streamWidth * streamHeight;
    uint32_t SCREEN_DMA_MAX_SIZE = 0xD000 / 2; // Divided by 2 because for 8 bit BITER and CITER
                                               // have to be in 15-bits range and this is limitation
    uint32_t SCREEN_DMA_NUM_SETTINGS = (((uint32_t)(SCREEN_BUFF_SIZE / SCREEN_DMA_MAX_SIZE)) + 1);

    dmasettings = new DMASetting[SCREEN_DMA_NUM_SETTINGS];

    streamBufDestMemAddress = destMemAddress;

    const uint32_t bytesPerLine = streamWidth * byteResolution;
    const uint32_t maxLines = (SCREEN_DMA_MAX_SIZE / bytesPerLine);
    uint32_t transactionIdx = 0, sumLines = 0, lines;
    do
    {
        // Source:
        lines = min(maxLines, streamHeight - sumLines);
        int32_t currentTransactionLength = lines * bytesPerLine;
        dmasettings[transactionIdx].TCD->CSR = 0;
        dmasettings[transactionIdx].TCD->SADDR =
          &streamBuf[sumLines * streamWidth * byteResolution];

        dmasettings[transactionIdx].TCD->SOFF = 1;
        dmasettings[transactionIdx].TCD->ATTR_SRC = 0;
        dmasettings[transactionIdx].TCD->NBYTES = 1;
        dmasettings[transactionIdx].TCD->SLAST = -currentTransactionLength;
        dmasettings[transactionIdx].TCD->BITER = currentTransactionLength;
        dmasettings[transactionIdx].TCD->CITER = currentTransactionLength;

        // Destination:
        dmasettings[transactionIdx].TCD->DADDR = &SPI2_PUSHR;
        dmasettings[transactionIdx].TCD->DOFF = 0;
        dmasettings[transactionIdx].TCD->ATTR_DST = 0;
        dmasettings[transactionIdx].TCD->DLASTSGA = 0;

        dmasettings[transactionIdx].replaceSettingsOnCompletion(dmasettings[transactionIdx + 1]);
        // dmasettings[transactionIdx].interruptAtCompletion();
        sumLines += lines;

    } while (++transactionIdx < SCREEN_DMA_NUM_SETTINGS);

    dmasettings[SCREEN_DMA_NUM_SETTINGS - 1].replaceSettingsOnCompletion(dmasettings[0]);
    dmasettings[SCREEN_DMA_NUM_SETTINGS - 1].interruptAtCompletion();
    dmasettings[SCREEN_DMA_NUM_SETTINGS - 1].disableOnCompletion();

    dmatx.begin(false);
    dmatx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI2_TX);
    dmatx = dmasettings[0];

    dmatx.attachInterrupt(dataTransferComplite);

    streamInitialized = true;
}

void DisplayDriverImpl::stopStreamDma()
{
    dmatx.disable();
    dmatx.clearInterrupt();

    SPI2_MCR |= SPI_MCR_HALT;

    MCU_CShigh();

    (*(volatile uint16_t*)((int)&SPI2_PUSHR + 2)) = 0;
    SPI2_RSER &= ~SPI_RSER_TFFF_DIRS;
    SPI2_RSER &= ~SPI_RSER_TFFF_RE;
    SPI2_MCR |= (SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF);
    SPI2_SR |= (SPI_SR_TFFF | SPI_SR_TCF | SPI_SR_EOQF | SPI_SR_RFDF | SPI_SR_RFOF | SPI_SR_TFUF |
                SPI_SR_TXRXS);

    SPI2_MCR &= ~SPI_MCR_HALT;

    setUpdateDmaStep(0);
}

bool DisplayDriverImpl::updateStreamDma(bool lastFrame)
{
    if (!streamInitialized)
        return false;

    if (streamUpdateStep == 0 && !lastFrame)
        return false;

    bool refreshScreenNow = false;

    switch (streamUpdateStep)
    {
        case 0: // DMA send start
        {
            sendDataDma();
            setUpdateDmaStep(1);
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

    SPI2_MCR |= SPI_MCR_HALT;

    //(*(volatile uint16_t *)((int)&SPI2_PUSHR + 2)) = (SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT) >> 16;
    SPI2_RSER |= SPI_RSER_TFFF_DIRS | SPI_RSER_TFFF_RE; // Set ILI_DMA Interrupt Request Select and
                                                        // Enable register

    SPI2_MCR &= ~SPI_MCR_HALT; // Start transfers.

    dmatx.enable();
}

void dataTransferComplite()
{
    dmatx.disable();
    dmatx.clearInterrupt();

    SPI2_MCR |= SPI_MCR_HALT;

    MCU_CShigh();

    (*(volatile uint16_t*)((int)&SPI2_PUSHR + 2)) = 0;
    SPI2_RSER &= ~SPI_RSER_TFFF_DIRS;
    SPI2_RSER &= ~SPI_RSER_TFFF_RE;
    SPI2_MCR |= (SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF);
    SPI2_SR |= (SPI_SR_TFFF | SPI_SR_TCF | SPI_SR_EOQF | SPI_SR_RFDF | SPI_SR_RFOF | SPI_SR_TFUF |
                SPI_SR_TXRXS);

    SPI2_MCR &= ~SPI_MCR_HALT;

    DisplayDriverImpl::setUpdateDmaStep(2);
}
