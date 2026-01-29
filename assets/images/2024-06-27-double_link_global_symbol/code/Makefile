# 顶层 Makefile

# 子目录列表（可加更多目录）
SUBDIRS := libCommon libA libB exe

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

# 递归构建每个子目录
$(SUBDIRS):
	@$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
	   $(MAKE) -C $$dir clean; \
	done
