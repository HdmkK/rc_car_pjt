# Makefile

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -pedantic
LDFLAGS :=
LDLIBS  := -pthread

TARGETS := ctrl_tx_tcp drive_tx_udp socketReceiver udpReceiver driveRecvAndCanTx

.PHONY: all clean
all: $(TARGETS)

# ---- Executables ----
ctrl_tx_tcp: ctrl_tx_tcp.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

drive_tx_udp: drive_tx_udp.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

socketReceiver: socketReceiver.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

udpReceiver: udpReceiver.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

driveRecvAndCanTx: driveRecvAndCanTx.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)
# ---- Object build rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Header dependencies ----
ctrl_tx_tcp.o: ctrl_tx_tcp.h ctrl_protocol.h
drive_tx_udp.o: drive_tx_udp.h ctrl_protocol.h
socketReceiver.o: ctrl_protocol.h
# udpReceiver.o: (no deps)

clean:
	rm -f $(TARGETS) *.o *.a *.so *.d
