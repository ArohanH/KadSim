CXX := g++
LD := g++

TARGET_EXEC := kadsim

SRC_DIR := src

BIN_DIR := bin
BUILD_DIR := build

SRCS := $(shell find $(SRC_DIR) -name '*.cc')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CXX_FLAGS := -std=c++17 -Wall -Wpedantic -Werror -g -MMD -MP -O3 -flto=auto
LD_FLAGS := -flto=auto
LIB_FLAGS :=

$(BIN_DIR)/$(TARGET_EXEC): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LD_FLAGS) -o $@ $(OBJS) $(LIB_FLAGS)

$(BUILD_DIR)/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXX_FLAGS) -o $@ $<

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS)
