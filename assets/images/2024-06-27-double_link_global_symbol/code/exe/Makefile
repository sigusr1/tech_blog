# 编译器
CXX = g++
CXXFLAGS = -I../libA -I../libB

# 依赖的 so 库路径
LDFLAGS = -L../libA -L../libB

# 要链接的 so
LDLIBS = -lA -lB

# 输出文件
TARGET = main

# 源文件和对象文件
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# 编译
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
