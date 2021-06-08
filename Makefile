# 此样本文件适用于这样的目录结构
# 源文件和头文件都在src下, .o和.d都在OBJ_DIR下, 生成的可执行文件在build下
# 需要根据实际情况修改的用Makefile标注

OBJ_DIR=obj/
SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)
BUILD_DIR ?= build/
# TODO
BINARY = $(BUILD_DIR)/main

LD=g++

# TODO
INCLUDES = -I $$MHOME/Playground/lib/ubuntu/header
#$(info INCLUDES=${INCLUDES})
CXXFLAGS += -MMD -O0 -Wall -Werror -ggdb3 $(INCLUDES) -std=c++17
LDFLAGS += -L $$MHOME/Playground/lib/ubuntu/lib -l log_c

$(BINARY): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $^ $(LDFLAGS)

$(OBJS): $(OBJ_DIR)/%.o: src/%.cpp
	@echo + CXX $<
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c -o $@ $< 2> gcc_error.txt

-include $(OBJS:.o=.d)
#.d和.o在同一个目录下

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)
