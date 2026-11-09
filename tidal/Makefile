# Tracker Cycles host build: shared core, tests, editor prototype.
#   make            core + tests + prototype (headless)
#   make test       run the tests
#   make proto SDL=1 SDL_PREFIX=/usr   prototype with an SDL2 window
#   make arm        cross-compile the core for the MK66 (size check only)
#   make run        build (SDL) and open the prototype window (run_proto.sh)
#   make golden     recapture golden/expected.txt + types.txt from real Tidal
#   make bench      engine timing and arena use on heavy patterns

CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -g -Wall -Wextra -Wno-missing-field-initializers -Wno-format-truncation
BUILD     := build
CORE_SRC  := $(wildcard core/*.cpp)
CORE_OBJ  := $(CORE_SRC:%.cpp=$(BUILD)/%.o)
TEST_SRC  := $(filter-out tests/bench.cpp,$(wildcard tests/*.cpp))
TEST_OBJ  := $(TEST_SRC:%.cpp=$(BUILD)/%.o)

# The prototype draws with the firmware's own fonts (core/libs/fonts).
FW_LIBS   := ../core/libs
FONT_SRC  := $(FW_LIBS)/fonts/RobotoMono-Regular_14_L4.cpp \
             $(FW_LIBS)/fonts/RobotoMono-Bold_17_L4.cpp \
             $(FW_LIBS)/fonts/LiberationSans-Regular_12_L4.cpp \
             $(FW_LIBS)/NewDisplay/CompressionRLE.cpp
FONT_OBJ  := $(patsubst $(FW_LIBS)/%.cpp,$(BUILD)/fw/%.o,$(FONT_SRC))
PROTO_SRC := $(filter-out proto/sdl_window.cpp,$(wildcard proto/*.cpp))
PROTO_OBJ := $(PROTO_SRC:%.cpp=$(BUILD)/%.o)
PROTO_LIBS := -pthread

ifeq ($(SDL),1)
SDL_PREFIX ?= /usr
PROTO_OBJ  += $(BUILD)/proto/sdl_window.o
CXXFLAGS_SDL := -DTC_HAVE_SDL -I$(SDL_PREFIX)/include -I$(SDL_PREFIX)/include/SDL2 \
                -I$(SDL_PREFIX)/include/x86_64-linux-gnu
PROTO_LIBS += -L$(SDL_PREFIX)/lib/x86_64-linux-gnu -L$(SDL_PREFIX)/lib -lSDL2
endif

.PHONY: all test smoke proto arm clean run golden bench
all: $(BUILD)/run_tests $(BUILD)/tidal_proto

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

# Scripted editor sessions: tests/proto/NAME.keys must print NAME.expected.
smoke: $(BUILD)/tidal_proto
	tests/proto/run_smoke.sh $(BUILD)/tidal_proto

proto: $(BUILD)/tidal_proto

run:
	./run_proto.sh

# Needs GHC + Tidal 1.10.3 (docs/host_setup.md); ghcup's env is sourced if present.
golden:
	. $$HOME/.ghcup/env 2>/dev/null; python3 golden/capture.py

bench: $(BUILD)/bench
	./$(BUILD)/bench

$(BUILD)/bench: $(CORE_OBJ) $(BUILD)/tests/bench.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/run_tests: $(CORE_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/tidal_proto: $(CORE_OBJ) $(PROTO_OBJ) $(FONT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(PROTO_LIBS)

$(BUILD)/core/%.o: core/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Icore -MMD -MP -c -o $@ $<

$(BUILD)/proto/%.o: proto/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_SDL) -Icore -I$(FW_LIBS) -MMD -MP -c -o $@ $<

$(BUILD)/fw/%.o: $(FW_LIBS)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(FW_LIBS) -w -c -o $@ $<

# Size check for the device: same flags family as the MK66 firmware build.
ARM_CXX   ?= arm-none-eabi-g++
ARM_FLAGS := -std=gnu++17 -O2 -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb \
             -ffunction-sections -fdata-sections -fno-rtti -fno-exceptions -specs=nano.specs
arm:
	@mkdir -p $(BUILD)/arm
	@for f in $(CORE_SRC); do $(ARM_CXX) $(ARM_FLAGS) -c -o $(BUILD)/arm/$$(basename $$f .cpp).o $$f || exit 1; done
	arm-none-eabi-size -t $(BUILD)/arm/*.o

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
