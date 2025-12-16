# Makefile

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -pedantic
LDFLAGS :=
LDLIBS  := -pthread

# Output directories
BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj

# Program names (keep as before)
TARGETS := ctrl_tx_tcp drive_tx_udp socketReceiver udpReceiver driveRecvAndCanTx gateway

# Actual built binaries go to build/bin
BIN_TARGETS := $(addprefix $(BIN_DIR)/,$(TARGETS))

.PHONY: all clean $(TARGETS)
all: $(BIN_TARGETS)

# ---- Phony wrappers (so `make gateway` still works) ----
$(TARGETS): %: $(BIN_DIR)/%

# ---- Link rules (binaries in build/bin) ----
$(BIN_DIR)/ctrl_tx_tcp: $(OBJ_DIR)/ctrl_tx_tcp.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/drive_tx_udp: $(OBJ_DIR)/drive_tx_udp.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/socketReceiver: $(OBJ_DIR)/socketReceiver.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/udpReceiver: $(OBJ_DIR)/udpReceiver.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/driveRecvAndCanTx: $(OBJ_DIR)/driveRecvAndCanTx.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/gateway: $(OBJ_DIR)/gateway.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ---- Object build rule (objects in build/obj) ----
# Also generate dependency files (.d)
CFLAGS += -MMD -MP

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Ensure directories exist ----
$(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@

# ---- Header dependencies (optional; .d handles most of it) ----
$(OBJ_DIR)/ctrl_tx_tcp.o: ctrl_tx_tcp.h ctrl_protocol.h
$(OBJ_DIR)/drive_tx_udp.o: drive_tx_udp.h ctrl_protocol.h
$(OBJ_DIR)/socketReceiver.o: ctrl_protocol.h
$(OBJ_DIR)/gateway.o: ctrl_protocol.h
# udpReceiver / driveRecvAndCanTx deps are handled by auto .d

# ---- Include auto-generated dependency files ----
-include $(wildcard $(OBJ_DIR)/*.d)

clean:
	rm -rf $(BUILD_DIR)
