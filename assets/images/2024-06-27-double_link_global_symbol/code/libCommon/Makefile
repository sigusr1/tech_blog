# 静态库名
TARGET = libCommon.a

# 查找本目录下所有 cpp 文件
SRCS := $(wildcard *.cpp)

# C++ 编译器和归档工具
CXX = g++
AR = ar
ARFLAGS = rcs
CXXFLAGS += -fPIC  -I.

# 生成对应的 .o 文件名
OBJS := $(SRCS:.cpp=.o)

# 默认目标
all: $(TARGET)

# 规则：生成静态库
$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

# 规则：编译 .cpp 为 .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
