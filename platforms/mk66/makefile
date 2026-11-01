# Variables to provide (example values):
#
# TOOLCHAIN=/usr/local/bin/arm-none-eabi-
# TRACKER_DIR=../Tracker/
# TARGET_POSTFIX=mybuild
# DEBUG=1 (any value turns on)

# Target configuration
PRODUCT		?= ORIGINAL
TRACKER_DIR	?= ../../core
-include ../../version.mk
-include ../version.mk
TARGET_POSTFIX	?= $(DEFAULT_TARGET_POSTFIX)
BUILD_ID_STR	?= $(TARGET_POSTFIX)
TARGET		:= PolyendTracker

# Build environment setup
.DEFAULT_GOAL := build

BUILD_DIR	:= build
OBJ_DIR 	:= $(BUILD_DIR)/obj/
RM 			:= rm -rf

CPP_SRCS 	+= $(shell find ./3rdparty/ -name *.cpp)
CPP_SRCS 	+= $(shell find ./source/ -name *.cpp)
C_SRCS 	 	+= $(shell find ./3rdparty/ -name *.c)
ASM_SRCS 	+= $(shell find ./3rdparty/ -name *.S)

_CPP_OBJS	= $(patsubst ./%.cpp, %.o, $(CPP_SRCS))
_C_OBJS		= $(patsubst ./%.c, %.o, $(C_SRCS))
_ASM_OBJS	= $(patsubst ./%.S, %.o, $(ASM_SRCS))
OBJS		+= $(addprefix $(OBJ_DIR)/, $(_CPP_OBJS))
OBJS		+= $(addprefix $(OBJ_DIR)/, $(_C_OBJS))
OBJS		+= $(addprefix $(OBJ_DIR)/, $(_ASM_OBJS))

OBJ_DIRS	:= $(dir $(OBJS))
DEPS		:= $(OBJS:.o=.d)

NOW := $(shell date +"%Y%m%d_%H%M")

INCLUDES=-I./source -I./config -I./3rdparty/Audio -I./3rdparty/Encoder -I./3rdparty/Buttons -I./3rdparty/BlinkLed -I./3rdparty/radio -I./3rdparty/FastCRC -I./3rdparty/MIDI -I./3rdparty/I2C -I./3rdparty/Leds -I./3rdparty/Battery -I./3rdparty/teensy_core -I./3rdparty/SDRAM -I./3rdparty/Snooze -I./3rdparty/SPI -I./3rdparty/FT812 -I./3rdparty/EEPROM -I./3rdparty/SD -I./3rdparty/SD/drivers -I./3rdparty/SD/fatfs/source -I./3rdparty/SD/fatfs/source/fsl_sd_disk -I./3rdparty/SD/fatfs/source/fsl_ram_disk -I./3rdparty/SD/sdmmc/inc -I${TRACKER_DIR}/include
LIBS=-lTracker -larm_cortexM4lf_math

DEFINES=-D__NEWLIB__ -D__MK66FX1M0__ -DF_CPU=180000000 -DUSB_MIDI -DTEENSYDUINO=142 -DARDUINO=10805 -DLAYOUT_US_ENGLISH -DCPU_MK66FX1M0VMD18 -DBUILD_ID=\"$(BUILD_ID_STR)\"

COMPILER_OPTIONS=-fno-common -g -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -fstack-usage -specs=nano.specs -Wall -Wextra -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin 
ifdef DEBUG
COMPILER_OPTIONS+=-Og -DDEBUG
else
COMPILER_OPTIONS+=-O2 -DNDEBUG
endif

ifeq ($(PRODUCT), MINI)
DEFINES+=-DTRACKER_MINI
else ifeq ($(PRODUCT), ORIGINAL)
else 
$(error Invalid PRODUCT value $(PRODUCT))
endif

CPP_OPTIONS=-std=gnu++17 -felide-constructors -includecstdint -fno-rtti -fno-exceptions 
CC_OPTIONS=

# Build rules

$(OBJ_DIR)/%.o: %.cpp
	$(TOOLCHAIN)c++ -MMD -MP $(DEFINES) $(INCLUDES) $(CPP_OPTIONS) $(COMPILER_OPTIONS) -o "$@" "$<"

$(OBJ_DIR)/%.o: %.c
	$(TOOLCHAIN)gcc -MMD -MP $(DEFINES) $(INCLUDES) $(CC_OPTIONS) $(COMPILER_OPTIONS) -o "$@" "$<"

$(OBJ_DIR)/%.o: %.S
	$(TOOLCHAIN)gcc -c -x assembler-with-cpp $(DEFINES) $(INCLUDES) -g -o "$@" "$<"
 
$(TARGET): $(OBJS)
	$(TOOLCHAIN)c++ -L ./3rdparty/teensy_core -L ./3rdparty/Audio -L ${TRACKER_DIR}/build-MK66-$(PRODUCT) -Xlinker --defsym=__rtc_localtime=0 -Xlinker -print-memory-usage -Xlinker --gc-sections -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -T mk66fx1m0.ld -o "$(BUILD_DIR)/$(TARGET).axf" $(OBJS) $(LIBS) 
	$(MAKE) --no-print-directory post-build
	
# Main rules

build:
	$(MAKE) --no-print-directory pre-build
	$(MAKE) --no-print-directory main-build

main-build: $(TARGET)

pre-build:
	$(foreach dir,$(OBJ_DIRS),mkdir -p $(dir);)

post-build: 
	cp "$(BUILD_DIR)/$(TARGET).axf" "$(BUILD_DIR)/$(TARGET)_$(NOW)_$(TARGET_POSTFIX).axf"
	$(TOOLCHAIN)objcopy --gap-fill=0xFF -v -O ihex "$(BUILD_DIR)/$(TARGET).axf" "$(BUILD_DIR)/$(TARGET)_$(TARGET_POSTFIX).ptf" 
	$(TOOLCHAIN)objdump -S -C "$(BUILD_DIR)/$(TARGET).axf" > "$(BUILD_DIR)/$(TARGET)_$(NOW)_$(TARGET_POSTFIX).lss" 
	$(TOOLCHAIN)size "$(BUILD_DIR)/$(TARGET).axf"

clean:
	-$(RM) $(BUILD_DIR)

-include $(DEPS)
	
.PHONY: build
