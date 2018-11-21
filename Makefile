# paths
OBJ_DIR := obj
SRC_DIR := src

# flags
CFLAGS := -O0 -g -Werror -Wall -Wextra -Wno-unused-parameter
LDFLAGS :=

# libs and include
pkg_configs := wayland-server \
               wlroots

LIBS := $(shell pkg-config --libs   ${pkg_configs})
INCS := $(shell pkg-config --cflags ${pkg_configs})

CFLAGS += -I${SRC_DIR} ${INCS}
LDFLAGS += ${LIBS}

# enable unstable wlroots features
CFLAGS += -DWLR_USE_UNSTABLE

# source and objects
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# targets
TARGET := jwc

all: $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	${CC} -o $@ -c $< ${CFLAGS}

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET) $(OBJ)
