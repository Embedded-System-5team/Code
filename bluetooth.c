#include "bluetooth.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

// === 공유 데이터 실체화 ===
pthread_mutex_t auth_mutex;
volatile int auth_user_count = 0; // HC-06은 단일 연결이므로, 1 또는 0으로 사용됨
volatile int admin_override = 0; // 1: 관리자 강제 열림, 0: 일반 제어
// ===

#define UART_DEVICE "/dev/ttyAMA0" // 요청하신 장치 파일
#define BAUD_RATE B115200         // HC-06 기본 보드레이트

static int uart_fd = -1; // 시리얼 포트 파일 디스크립터

/**
 * @brief HC-06 모듈과의 통신을 위한 UART 포트를 초기화합니다.
 */
void init_bluetooth() {
    if (pthread_mutex_init(&auth_mutex, NULL) != 0) {
        fprintf(stderr, ">>> ERROR: Auth Mutex initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    // 1. UART 포트 열기 (읽기/쓰기, 제어 터미널 아님, 논블로킹)
    uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1) {
        perror(">>> ERROR: Unable to open UART device " UART_DEVICE);
        fprintf(stderr, ">>> Check connection, permissions, and 'raspi-config/config.txt' DTO settings.\n");
        exit(EXIT_FAILURE);
    }

    // 2. UART 설정
    struct termios options;
    tcgetattr(uart_fd, &options);

    // 보드레이트 설정
    cfsetispeed(&options, BAUD_RATE);
    cfsetospeed(&options, BAUD_RATE);

    // 8N1 설정 (8비트, No 패리티, 1 Stop 비트)
    options.c_cflag |= (CLOCAL | CREAD); // 로컬 연결, 수신 활성화
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB; // 패리티 비활성화
    options.c_cflag &= ~CSTOPB; // 1 Stop 비트

    // Raw 모드 (입력 처리 비활성화: ICANON, ECHO, ECHOE, ISIG)
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 타임아웃 설정 (100ms 대기)
    options.c_cc[VMIN] = 0; // 최소 읽기 문자 수
    options.c_cc[VTIME] = 1; // 100ms 타임아웃

    // 즉시 적용
    tcsetattr(uart_fd, TCSANOW, &options);

    printf(">>> Bluetooth (HC-06) Initialized on %s at %d bps.\n", UART_DEVICE, BAUD_RATE);
}

/**
 * @brief 서보 모터 잠금 상태를 확인합니다.
 * @return 1이면 잠금 상태(0도), 0이면 열림 상태(90도).
 */
int is_motor_locked() {
    pthread_mutex_lock(&auth_mutex);
    // 관리자 오버라이드가 1이거나, 일반 인증 사용자가 1명 이상이면 (HC-06에서는 1) 열림.
    int locked = (auth_user_count == 0 && admin_override == 0);
    pthread_mutex_unlock(&auth_mutex);
    return locked;
}

/**
 * @brief HC-06을 통해 스마트폰의 인증 및 제어 명령을 처리하는 쓰레드 함수입니다.
 */
void* bluetoothThreadFunc(void* arg) {
    char read_buffer[1024];
    int bytes_read;
    int authenticated = 0; // 0: 로그아웃, 1: 일반 인증, 2: 관리자 인증

    // 초기 메시지 송신
    const char* welcome_msg = "Sentry System: Please enter password (AUTH or ADMIN).\r\n";
    write(uart_fd, welcome_msg, strlen(welcome_msg));

    while (1) {
        // 1. 데이터 읽기
        memset(read_buffer, 0, sizeof(read_buffer));
        bytes_read = read(uart_fd, read_buffer, sizeof(read_buffer) - 1);

        if (bytes_read > 0) {
            // Null 문자 추가 및 개행 문자 제거
            read_buffer[bytes_read] = '\0';
            read_buffer[strcspn(read_buffer, "\r\n")] = 0;

            printf(">>> BT Received: %s (Len: %d)\n", read_buffer, bytes_read);

            // --- 인증 로직 ---
            if (authenticated == 0) {
                if (strncmp(read_buffer, AUTH_PASSWORD, strlen(AUTH_PASSWORD)) == 0) {
                    authenticated = 1;
                    pthread_mutex_lock(&auth_mutex);
                    auth_user_count = 1; // 일반 인증
                    pthread_mutex_unlock(&auth_mutex);

                    const char* res = "Authentication successful. Motor unlocked. Send 'LOGOUT' to lock.\r\n";
                    write(uart_fd, res, strlen(res));
                    printf(">>> BT: User authenticated (Normal).\n");
                }
                // --- 관리자 로직 ---
                else if (strncmp(read_buffer, ADMIN_PASSWORD, strlen(ADMIN_PASSWORD)) == 0) {
                    authenticated = 2; // 관리자 모드
                    const char* res = "Administrator login successful. Send '1' (Open), '0' (Close) or 'LOGOUT'.\r\n";
                    write(uart_fd, res, strlen(res));
                    printf(">>> BT: User authenticated (Admin).\n");
                }
                else {
                    const char* res = "Invalid password. Try again.\r\n";
                    write(uart_fd, res, strlen(res));
                }
            }

            // --- 2. 관리자 제어 (Admin: 2) ---
            else if (authenticated == 2) {
                pthread_mutex_lock(&auth_mutex);
                if (strncmp(read_buffer, "1", 1) == 0) {
                    admin_override = 1;
                    const char* res = "Admin command: Motor forced open (90 degrees).\r\n";
                    write(uart_fd, res, strlen(res));
                }
                else if (strncmp(read_buffer, "0", 1) == 0) {
                    admin_override = 0;
                    const char* res = "Admin command: Motor forced close (0 degrees).\r\n";
                    write(uart_fd, res, strlen(res));
                }
                else if (strncasecmp(read_buffer, "LOGOUT", 6) == 0) {
                    // 로그아웃 시 관리자 오버라이드 해제 (필요하다면)
                    admin_override = 0;
                    authenticated = 0;
                    const char* res = "Logged out from Admin. Enter password to continue.\r\n";
                    write(uart_fd, res, strlen(res));
                }
                else {
                    const char* res = "Invalid admin command. Send '1', '0', or 'LOGOUT'.\r\n";
                    write(uart_fd, res, strlen(res));
                }
                pthread_mutex_unlock(&auth_mutex);
            }

            // --- 3. 일반 사용자 제어 (Auth: 1) ---
            else if (authenticated == 1) {
                if (strncasecmp(read_buffer, "LOGOUT", 6) == 0) {
                    authenticated = 0;
                    pthread_mutex_lock(&auth_mutex);
                    auth_user_count = 0; // 인증 해제 -> 모터 잠금
                    pthread_mutex_unlock(&auth_mutex);
                    const char* res = "Logged out. Motor locked. Enter password to continue.\r\n";
                    write(uart_fd, res, strlen(res));
                }
                else {
                    const char* res = "You are authenticated. Send 'LOGOUT' to lock the motor.\r\n";
                    write(uart_fd, res, strlen(res));
                }
            }
        }

        usleep(50000); // 50ms 대기 후 다시 읽기 (폴링)
    }

    close(uart_fd);
    return NULL;
}