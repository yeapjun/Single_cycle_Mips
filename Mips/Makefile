# ============================================================
#  Makefile — Single-cycle MIPS Simulator
#  컴파일: make
#  실행:   ./mips_sim [-v] <binary.bin>
#  정리:   make clean
# ============================================================

CC      = gcc
TARGET  = mips_sim

# 경고 최대, C99, 최적화 O2
CFLAGS  = -Wall -Wextra -std=c99 -O2

# 디버그 빌드: make DEBUG=1
ifeq ($(DEBUG), 1)
    CFLAGS += -g -DDEBUG -O0
endif

SRCS    = main.c     \
          memory.c   \
          decode.c   \
          alu.c      \
          cpu.c

OBJS    = $(SRCS:.c=.o)

HDRS    = mips.h    \
          memory.h  \
          decode.h  \
          alu.h     \
          cpu.h

# ---- 기본 타겟 -----------------------------------------------
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful: $(TARGET)"

# ---- 헤더 의존성 포함 ----------------------------------------
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- 정리 ----------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned."