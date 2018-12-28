# paths
OBJ_DIR := obj
SRC_DIR := src
PROTOCOLS_DIR := protocols

# flags
CFLAGS := -O0 -g -Werror -Wall -Wextra -Wno-unused-parameter
LDFLAGS := -L/usr/lib -L/usr/lib/x86_64-linux-gnu

# libs and include
pkg_configs := wayland-server \
               wlroots \
               xkbcommon

LIBS := $(shell pkg-config --libs   ${pkg_configs})
INCS := $(shell pkg-config --cflags ${pkg_configs})

CFLAGS += -I${SRC_DIR} -I${PROTOCOLS_DIR} ${INCS}
LDFLAGS += ${LIBS}

# enable unstable wlroots features
CFLAGS += -DWLR_USE_UNSTABLE

# source and objects
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# wayland protocols
WAYLAND_PROTOCOLS := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_XML := $(WAYLAND_PROTOCOLS)/unstable/xdg-shell/xdg-shell-unstable-v6.xml
WAYLAND_HEADER := $(PROTOCOLS_DIR)/xdg-shell-unstable-v6-protocol.h

# targets
TARGET := jwc

all: $(TARGET)

$(WAYLAND_HEADER):
	wayland-scanner server-header $(WAYLAND_XML) $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR) $(WAYLAND_HEADER)
	${CC} -o $@ -c $< ${CFLAGS}

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET) $(OBJ) $(WAYLAND_HEADER)
