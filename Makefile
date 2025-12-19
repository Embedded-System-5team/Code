CC = gcc
CFLAGS = -Wall -pthread
# 라즈베리파이 5의 경우 wiringPi 라이브러리 이름 확인 필요 (보통 -lwiringPi)
LIBS = -lwiringPi

# 타겟 정의
TARGET_MAIN = sentry_system
TARGET_TEST = camera_test

# 오브젝트 파일 정의
OBJS_MAIN = main.o sensors.o actuators.o motor.o bluetooth.o network.o
OBJS_TEST = camera_test_only_ipc.o sensors.o

# [명령어: make all] 메인 시스템과 테스트 프로그램 모두 컴파일
all: $(TARGET_MAIN) $(TARGET_TEST)

# 1. 메인 시스템 빌드
$(TARGET_MAIN): $(OBJS_MAIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# .c 파일을 .o 파일로 컴파일하는 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# 정리 (make clean)
clean:
	rm -f *.o $(TARGET_MAIN) $(TARGET_TEST)