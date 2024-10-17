
# compile options (see README.md for descriptions)
# 0 = disable
# 1 = enable

# ---- STOCK QUANSHENG FERATURES ----
ENABLE_FMRADIO                	?= 0
ENABLE_UART                   	?= 1
ENABLE_AIRCOPY                	?= 1
ENABLE_VOX                    	?= 0
ENABLE_ALARM                  	?= 0
ENABLE_TX1750                 	?= 1
ENABLE_DTMF_CALLING           	?= 0
ENABLE_FLASHLIGHT             	?= 1

# ---- CUSTOM MODS ----
ENABLE_SPECTRUM               	?= 1
ENABLE_BIG_FREQ               	?= 1
ENABLE_SMALL_BOLD             	?= 1
ENABLE_CUSTOM_MENU_LAYOUT     	?= 1
ENABLE_KEEP_MEM_NAME          	?= 1
ENABLE_WIDE_RX                	?= 1
ENABLE_TX_WHEN_AM             	?= 0
ENABLE_F_CAL_MENU             	?= 0
ENABLE_CTCSS_TAIL_PHASE_SHIFT 	?= 0
ENABLE_BOOT_BEEPS             	?= 0
ENABLE_SHOW_CHARGE_LEVEL      	?= 0
ENABLE_REVERSE_BAT_SYMBOL     	?= 0
ENABLE_NO_CODE_SCAN_TIMEOUT   	?= 1
ENABLE_AM_FIX                 	?= 1
ENABLE_SQUELCH_MORE_SENSITIVE 	?= 1
ENABLE_FASTER_CHANNEL_SCAN    	?= 1
ENABLE_RSSI_BAR               	?= 1
ENABLE_AUDIO_BAR              	?= 1
ENABLE_COPY_CHAN_TO_VFO       	?= 1
ENABLE_REDUCE_LOW_MID_TX_POWER	?= 0
ENABLE_BYP_RAW_DEMODULATORS   	?= 0
ENABLE_BLMIN_TMP_OFF          	?= 0
ENABLE_SCAN_RANGES            	?= 1
ENABLE_FEAT_F4HWN             	?= 1
ENABLE_FEAT_F4HWN_SPECTRUM    	?= 1
ENABLE_FEAT_F4HWN_RX_TX_TIMER   ?= 1
ENABLE_FEAT_F4HWN_CHARGING_C    ?= 1
ENABLE_FEAT_F4HWN_SLEEP			?= 1
ENABLE_FEAT_F4HWN_PMR         	?= 0
ENABLE_FEAT_F4HWN_GMRS_FRS_MURS	?= 0
ENABLE_FEAT_F4HWN_CA         	?= 1

# ---- DEBUGGING ----
ENABLE_AM_FIX_SHOW_DATA       	?= 0
ENABLE_AGC_SHOW_DATA          	?= 0
ENABLE_UART_RW_BK_REGS        	?= 0

#############################################################

BIN_DIR := build
SRC_DIR := .
OBJ_DIR := .

TARGET = $(BIN_DIR)/firmware

SRC = $(wildcard $(SRC_DIR)/driver/*.c)
#SRC += $(wildcard $(SRC_DIR)/external/mcufont/fonts/freesans.c)
#SRC += $(wildcard $(SRC_DIR)/external/mcufont/decoder/*.c)
SRC += $(wildcard $(SRC_DIR)/helper/*.c)
SRC += $(wildcard $(SRC_DIR)/ui/*.c)
SRC += $(wildcard $(SRC_DIR)/app/*.c)
SRC += $(wildcard $(SRC_DIR)/*.c)

OBJS = $(OBJ_DIR)/start.o
OBJS += $(OBJ_DIR)/external/printf/printf.o
OBJS += $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

BSP_DEFINITIONS := $(wildcard hardware/*/*.def)
BSP_HEADERS     := $(patsubst hardware/%,bsp/%,$(BSP_DEFINITIONS))
BSP_HEADERS     := $(patsubst %.def,%.h,$(BSP_HEADERS))

ifeq ($(OS), Windows_NT) # windows
    TOP := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
    RM = del /Q
    FixPath = $(subst /,\,$1)
    WHERE = where
    NULL_OUTPUT = nul
else # unix
    TOP := $(shell pwd)
    RM = rm -f
    FixPath = $1
    WHERE = which
    NULL_OUTPUT = /dev/null
endif

AS = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

ifneq (, $(shell $(WHERE) git))
	VERSION_STRING ?= $(shell git describe --tags --exact-match 2>$(NULL_OUTPUT))
	ifeq (, $(VERSION_STRING))
					VERSION_STRING := $(shell git rev-parse --short HEAD)
	endif
endif
# If there is still no VERSION_STRING we need to make one.
# It is needed for the firmware packing script
ifeq (, $(VERSION_STRING))
	VERSION_STRING := NOGIT
endif

AUTHOR_STRING ?= miramir

ASFLAGS = -c -mcpu=cortex-m0
CFLAGS = -Os -Wall -Werror -Wextra -mcpu=cortex-m0 -fshort-enums -fno-delete-null-pointer-checks -flto=auto -std=c2x -MMD

CFLAGS += -DPRINTF_INCLUDE_CONFIG_H
CFLAGS += -DAUTHOR_STRING=\"$(AUTHOR_STRING)\" -DVERSION_STRING=\"$(VERSION_STRING)\"

ifeq ($(ENABLE_SPECTRUM),1)
CFLAGS += -DENABLE_SPECTRUM
endif
ifeq ($(ENABLE_AIRCOPY),1)
	CFLAGS += -DENABLE_AIRCOPY
endif
ifeq ($(ENABLE_FMRADIO),1)
	CFLAGS += -DENABLE_FMRADIO
endif
ifeq ($(ENABLE_UART),1)
	CFLAGS += -DENABLE_UART
endif
ifeq ($(ENABLE_BIG_FREQ),1)
	CFLAGS  += -DENABLE_BIG_FREQ
endif
ifeq ($(ENABLE_SMALL_BOLD),1)
	CFLAGS  += -DENABLE_SMALL_BOLD
endif
ifeq ($(ENABLE_VOX),1)
	CFLAGS  += -DENABLE_VOX
endif
ifeq ($(ENABLE_ALARM),1)
	CFLAGS  += -DENABLE_ALARM
endif
ifeq ($(ENABLE_TX1750),1)
	CFLAGS  += -DENABLE_TX1750
endif
ifeq ($(ENABLE_KEEP_MEM_NAME),1)
	CFLAGS  += -DENABLE_KEEP_MEM_NAME
endif
ifeq ($(ENABLE_WIDE_RX),1)
	CFLAGS  += -DENABLE_WIDE_RX
endif
ifeq ($(ENABLE_TX_WHEN_AM),1)
	CFLAGS  += -DENABLE_TX_WHEN_AM
endif
ifeq ($(ENABLE_F_CAL_MENU),1)
	CFLAGS  += -DENABLE_F_CAL_MENU
endif
ifeq ($(ENABLE_CTCSS_TAIL_PHASE_SHIFT),1)
	CFLAGS  += -DENABLE_CTCSS_TAIL_PHASE_SHIFT
endif
ifeq ($(ENABLE_BOOT_BEEPS),1)
	CFLAGS  += -DENABLE_BOOT_BEEPS
endif
ifeq ($(ENABLE_SHOW_CHARGE_LEVEL),1)
	CFLAGS  += -DENABLE_SHOW_CHARGE_LEVEL
endif
ifeq ($(ENABLE_REVERSE_BAT_SYMBOL),1)
	CFLAGS  += -DENABLE_REVERSE_BAT_SYMBOL
endif
ifeq ($(ENABLE_NO_CODE_SCAN_TIMEOUT),1)
	CFLAGS  += -DENABLE_NO_CODE_SCAN_TIMEOUT
endif
ifeq ($(ENABLE_AM_FIX),1)
	CFLAGS  += -DENABLE_AM_FIX
endif
ifeq ($(ENABLE_AM_FIX_SHOW_DATA),1)
	CFLAGS  += -DENABLE_AM_FIX_SHOW_DATA
endif
ifeq ($(ENABLE_SQUELCH_MORE_SENSITIVE),1)
	CFLAGS  += -DENABLE_SQUELCH_MORE_SENSITIVE
endif
ifeq ($(ENABLE_FASTER_CHANNEL_SCAN),1)
	CFLAGS  += -DENABLE_FASTER_CHANNEL_SCAN
endif
ifeq ($(ENABLE_BACKLIGHT_ON_RX),1)
	CFLAGS  += -DENABLE_BACKLIGHT_ON_RX
endif
ifeq ($(ENABLE_RSSI_BAR),1)
	CFLAGS  += -DENABLE_RSSI_BAR
endif
ifeq ($(ENABLE_AUDIO_BAR),1)
	CFLAGS  += -DENABLE_AUDIO_BAR
endif
ifeq ($(ENABLE_COPY_CHAN_TO_VFO),1)
	CFLAGS  += -DENABLE_COPY_CHAN_TO_VFO
endif
ifeq ($(ENABLE_SINGLE_VFO_CHAN),1)
	CFLAGS  += -DENABLE_SINGLE_VFO_CHAN
endif
ifeq ($(ENABLE_BAND_SCOPE),1)
	CFLAGS += -DENABLE_BAND_SCOPE
endif
ifeq ($(ENABLE_REDUCE_LOW_MID_TX_POWER),1)
	CFLAGS  += -DENABLE_REDUCE_LOW_MID_TX_POWER
endif
ifeq ($(ENABLE_BYP_RAW_DEMODULATORS),1)
	CFLAGS  += -DENABLE_BYP_RAW_DEMODULATORS
endif
ifeq ($(ENABLE_BLMIN_TMP_OFF),1)
	CFLAGS  += -DENABLE_BLMIN_TMP_OFF
endif
ifeq ($(ENABLE_SCAN_RANGES),1)
	CFLAGS  += -DENABLE_SCAN_RANGES
endif
ifeq ($(ENABLE_DTMF_CALLING),1)
	CFLAGS  += -DENABLE_DTMF_CALLING
endif
ifeq ($(ENABLE_AGC_SHOW_DATA),1)
	CFLAGS  += -DENABLE_AGC_SHOW_DATA
endif
ifeq ($(ENABLE_FLASHLIGHT),1)
	CFLAGS  += -DENABLE_FLASHLIGHT
endif
ifeq ($(ENABLE_UART_RW_BK_REGS),1)
	CFLAGS  += -DENABLE_UART_RW_BK_REGS
endif
ifeq ($(ENABLE_CUSTOM_MENU_LAYOUT),1)
	CFLAGS  += -DENABLE_CUSTOM_MENU_LAYOUT
endif
ifeq ($(ENABLE_FEAT_F4HWN),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN
	CFLAGS  += -DALERT_TOT=10
endif
ifeq ($(ENABLE_FEAT_F4HWN_SPECTRUM),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_SPECTRUM
endif
ifeq ($(ENABLE_FEAT_F4HWN_RX_TX_TIMER),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_RX_TX_TIMER
endif
ifeq ($(ENABLE_FEAT_F4HWN_CHARGING_C),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_CHARGING_C
endif
ifeq ($(ENABLE_FEAT_F4HWN_SLEEP),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_SLEEP
endif
ifeq ($(ENABLE_FEAT_F4HWN_PMR),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_PMR
endif
ifeq ($(ENABLE_FEAT_F4HWN_GMRS_FRS_MURS),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_GMRS_FRS_MURS
endif
ifeq ($(ENABLE_FEAT_F4HWN_CA),1)
	CFLAGS  += -DENABLE_FEAT_F4HWN_CA
endif

LDFLAGS = -z noexecstack -mcpu=cortex-m0 -nostartfiles -Wl,-T,firmware.ld -Wl,--gc-sections --specs=nano.specs

ifeq ($(DEBUG),1)
	ASFLAGS += -g
	CFLAGS  += -g
	LDFLAGS += -g
endif

INC =
INC += -I $(TOP)
INC += -I $(TOP)/external/CMSIS_5/CMSIS/Core/Include/
INC += -I $(TOP)/external/CMSIS_5/Device/ARM/ARMCM0/Include

LIBS =

DEPS = $(OBJS:.o=.d)

ifneq (, $(shell $(WHERE) python))
    MY_PYTHON := python
else ifneq (, $(shell $(WHERE) python3))
    MY_PYTHON := python3
endif

ifdef MY_PYTHON
    HAS_CRCMOD := $(shell $(MY_PYTHON) -c "import crcmod" 2>&1)
endif

all: $(TARGET)
	$(OBJCOPY) -O binary $< $<.bin

ifndef MY_PYTHON
	$(info )
	$(info !!!!!!!! PYTHON NOT FOUND, *.PACKED.BIN WON'T BE BUILT)
	$(info )
else ifneq (,$(HAS_CRCMOD))
	$(info )
	$(info !!!!!!!! CRCMOD NOT INSTALLED, *.PACKED.BIN WON'T BE BUILT)
	$(info !!!!!!!! run: pip install crcmod)
	$(info )
else
	-$(MY_PYTHON) fw-pack.py $<.bin $(AUTHOR_STRING) $(VERSION_STRING) $<.packed.bin
endif
	$(SIZE) $<

flash:
	k5prog -b $(TARGET).bin -F -YYY

version.o: .FORCE

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)

bsp/dp32g030/%.h: hardware/dp32g030/%.def

%.o: %.c | $(BSP_HEADERS)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

.FORCE:

-include $(DEPS)

clean:
	$(RM) $(call FixPath, $(TARGET).bin $(TARGET).packed.bin $(TARGET) $(OBJS) $(DEPS))

doxygen:
	doxygen
