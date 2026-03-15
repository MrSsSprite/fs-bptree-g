CC = gcc
CFLAGS = -g -Wall

SRC_DIR = ../src
SRC_REQ_SCOPES = core io
TEST_SRC = ./test_node_insert.c

TEST_UNITS = test_node_key_insert.c test_node_val_insert.c test_node_kv_insert.c
TARGETS = $(TEST_UNITS:.c=)

SHARED_SRCS = $(addprefix $(SRC_DIR)/bptr_, $(addsuffix .c, $(SRC_REQ_SCOPES))) $(TEST_SRC)

all: $(TARGETS)

$(TARGETS): %: %.c $(SHARED_SRCS)
	$(CC) $< $(SHARED_SRCS) $(CFLAGS) -o $@

clean:
	rm -f $(TARGETS)
