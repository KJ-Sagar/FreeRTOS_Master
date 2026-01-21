# =========================================================
# Toolchain
# =========================================================
CC   = arm-none-eabi-gcc
SIZE = arm-none-eabi-size

BIN       := freertos_tcp_mps2_demo.axf
BUILD_DIR := build

# =========================================================
# FreeRTOS Paths
# =========================================================
FREERTOS_DIR_REL := ../../../FreeRTOS
FREERTOS_DIR     := $(abspath $(FREERTOS_DIR_REL))
KERNEL_DIR       := $(FREERTOS_DIR)/Source

FREERTOS_PLUS_DIR_REL := ../../../FreeRTOS-Plus
FREERTOS_PLUS_DIR     := $(abspath $(FREERTOS_PLUS_DIR_REL))
FREERTOS_TCP          := $(FREERTOS_PLUS_DIR)/Source/FreeRTOS-Plus-TCP

# =========================================================
# Include Paths
# =========================================================
INCLUDE_DIRS += \
    -I. \
    -Iconfig \
    -Iplatform \
    -Iapp \
    -Iapp/demos \
    -ICMSIS \
    -I$(KERNEL_DIR)/include \
    -I$(KERNEL_DIR)/portable/GCC/ARM_CM3 \
    -I$(FREERTOS_TCP)/source/include \
    -I$(FREERTOS_TCP)/source/portable/Compiler/GCC \
    -I$(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/ether_lan9118

# =========================================================
# Source Files
# =========================================================

# --- Startup / system ---
SOURCE_FILES += \
    startup.c \
    syscalls.c

# --- Platform (entry + networking) ---
SOURCE_FILES += \
    platform/main.c \
    platform/main_networking.c

# --- Application core ---
SOURCE_FILES += \
    app/app_main.c \
    app/network_role.c \
    app/demo_selector.c

# --- Application demos ---
SOURCE_FILES += \
    app/demos/demo_echo.c \
    app/demos/demo_echo_server.c \
    app/demos/demo_heartbeat.c

# --- Existing FreeRTOS TCP demos reused ---
SOURCE_FILES += \
    TCPEchoClient_SingleTasks.c \
    tcp_heartbeat_demo.c

# =========================================================
# FreeRTOS Kernel
# =========================================================
SOURCE_FILES += \
    $(KERNEL_DIR)/tasks.c \
    $(KERNEL_DIR)/list.c \
    $(KERNEL_DIR)/queue.c \
    $(KERNEL_DIR)/event_groups.c \
    $(KERNEL_DIR)/portable/GCC/ARM_CM3/port.c \
    $(KERNEL_DIR)/portable/MemMang/heap_3.c

# =========================================================
# FreeRTOS+TCP Stack
# =========================================================
SOURCE_FILES += \
    $(wildcard $(FREERTOS_PLUS_DIR)/Source/FreeRTOS-Plus-TCP/source/*.c) \
    $(FREERTOS_TCP)/source/portable/BufferManagement/BufferAllocation_2.c \
    $(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/NetworkInterface.c \
    $(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/ether_lan9118/smsc9220_eth_drv.c

# =========================================================
# Compiler Flags
# =========================================================
CPPFLAGS += -DHEAP3

CFLAGS += \
    -mthumb \
    -mcpu=cortex-m3 \
    -Wall -Wextra -Wshadow \
    -ffunction-sections -fdata-sections \
    -MMD \
    $(INCLUDE_DIRS)

ifeq ($(DEBUG),1)
    CFLAGS += -g3 -Og
else
    CFLAGS += -Os
endif

# =========================================================
# Linker Flags
# =========================================================
LDFLAGS += \
    -T mps2_m3.ld \
    -Xlinker --gc-sections \
    -Xlinker -Map=$(BUILD_DIR)/output.map \
    -nostartfiles \
    -specs=nano.specs \
    -specs=nosys.specs \
    -specs=rdimon.specs

# =========================================================
# Build Rules
# =========================================================
OBJ_FILES := $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/$(BIN): $(OBJ_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ_FILES:.o=.d)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
