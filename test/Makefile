CC = gcc
CFLAGS = -g -Wall

TEST_UNIT = test_node_key_insert.c
SRC_DIR = ../src
SRC_REQ_SCOPES = core io
TEST_SRC = ./test_node_insert.c
TARGET = test_node_insert

SRCS = $(addprefix $(SRC_DIR)/bptr_, $(addsuffix .c, $(SRC_REQ_SCOPES))) $(TEST_SRC)

all: $(TARGET)

$(TARGET): $(TEST_UNIT) $(SRCS)
	$(CC) $(TEST_UNIT) $(SRCS) $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
