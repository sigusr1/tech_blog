# 动态库名
TARGET = libA.so

# 源文件
SRCS := A.cpp

# 本目录下的目标对象文件
OBJS := $(SRCS:.cpp=.o)

# 路径设置
LIBCOMMON_DIR := ../libCommon
STATIC_LIBS := $(LIBCOMMON_DIR)/libCommon.a

# 编译器参数
CXX = g++
CXXFLAGS = -fPIC -I$(LIBCOMMON_DIR)

# 链接参数
LDFLAGS = -shared

all: $(TARGET)

$(TARGET): $(OBJS) $(STATIC_LIBS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(STATIC_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
