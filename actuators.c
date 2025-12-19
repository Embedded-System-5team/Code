#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <wiringPiSPI.h>
#include <softTone.h>
#include "config.h"     // 핀 번호(BUZZER_PIN)와 모드(MODE_...) 정의 가져옴
#include "actuators.h"  // 함수 원형

// --- SPI 설정 ---
#define SPI_CH 0
#define SPI_SPEED 1000000 // 1MHz

// --- MAX7219 레지스터 주소 맵 ---
#define REG_NOOP        0x00
#define REG_DIGIT0      0x01
#define REG_DECODE_MODE 0x09
#define REG_INTENSITY   0x0A
#define REG_SCAN_LIMIT  0x0B
#define REG_SHUTDOWN    0x0C
#define REG_DISPLAYTEST 0x0F

// --- 아이콘 데이터 (비트맵) ---

// 0. [CLEAR] 화면 지우기
uint8_t ICON_CLEAR[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// 1. [SAFE] (왼쪽: 자물쇠 / 오른쪽: 웃음)
uint8_t ICON_LOCK[8]  = {0x3C, 0x42, 0x42, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
uint8_t ICON_SMILE[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};

// 2. [WARNING] (왼쪽: 주의 / 오른쪽: 느낌표)
uint8_t ICON_WARN_TRIANGLE[8] = {0x18, 0x3C, 0x7E, 0xDB, 0x99, 0x18, 0x18, 0x18};
uint8_t ICON_EXCLAMATION[8]   = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00};

// 3. [DANGER] (왼쪽: 해골 / 오른쪽: X표)
uint8_t ICON_SKULL[8] = {0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0xC3, 0x3C, 0x3C};
uint8_t ICON_X[8]     = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};


// --- 내부 헬퍼 함수 (SPI 통신) ---

// 데이터 전송 (오른쪽 모듈 -> 왼쪽 모듈 Daisy Chain)
void write_MAX7219(uint8_t reg, uint8_t data_left, uint8_t data_right) {
    uint8_t buf[4];
    // 순서: [칩2(Left) 주소] [칩2 데이터] -> [칩1(Right) 주소] [칩1 데이터]
    buf[0] = reg; 
    buf[1] = data_right;   
    buf[2] = reg; 
    buf[3] = data_left;
    wiringPiSPIDataRW(SPI_CH, buf, 4);
}

// 화면 렌더링 함수
void render_dual(uint8_t *left_icon, uint8_t *right_icon) {
    for (int row = 0; row < 8; row++) {
        // MAX7219 레지스터는 1부터 시작, 배열 인덱스는 0부터 시작
        write_MAX7219(row + 1, left_icon[row], right_icon[row]);
    }
}

// MAX7219 초기화
void initMax7219() {
    write_MAX7219(REG_DISPLAYTEST, 0x00, 0x00); // 테스트 모드 끔
    write_MAX7219(REG_SHUTDOWN, 0x01, 0x01);    // 셧다운 해제 (켜기)
    write_MAX7219(REG_SCAN_LIMIT, 0x07, 0x07);  // 8줄 모두 사용
    write_MAX7219(REG_DECODE_MODE, 0x00, 0x00); // 매트릭스 모드 (No Decode)
    write_MAX7219(REG_INTENSITY, 0x01, 0x01);   // 밝기 중간 (0~15)
    render_dual(ICON_CLEAR, ICON_CLEAR);        // 화면 지우기
}


// --- 외부 공개 함수 (actuators.h에 선언된 함수들) ---

void init_actuators() {
    // 1. 부저 초기화 (SoftTone)
    if (softToneCreate(BUZZER_PIN) != 0) {
        printf("[Error] SoftTone Create Failed! Check Pin %d\n", BUZZER_PIN);
    } else {
        printf("[Info] Buzzer Initialized on GPIO %d\n", BUZZER_PIN);
    }

    // 2. 닷매트릭스(SPI) 초기화
    initMax7219();
    printf("[Info] Dot Matrix Initialized\n");
}

void cleanup_actuators() {
    softToneWrite(BUZZER_PIN, 0); // 소리 끄기
    render_dual(ICON_CLEAR, ICON_CLEAR); // 화면 끄기
    printf("[Info] Actuators Cleaned up\n");
}


// --- 쓰레드 함수 구현 ---

// [쓰레드 1] 디스플레이 제어
void* displayThreadFunc(void* arg) {
    int local_mode;
    while (1) {
        // [LOCK] 현재 모드 읽기
        pthread_mutex_lock(&mode_mutex);
        local_mode = current_mode;
        pthread_mutex_unlock(&mode_mutex);

        if (local_mode == MODE_EXIT) break;

        switch (local_mode) {
            case MODE_SAFE:
                render_dual(ICON_LOCK, ICON_SMILE);
                delay(200); 
                break;

            case MODE_WARN:
                render_dual(ICON_WARN_TRIANGLE, ICON_EXCLAMATION);
                delay(200);
                break;

            case MODE_DANGER:
                // 위험 모드는 깜빡임 효과 (Animation)
                render_dual(ICON_SKULL, ICON_X);
                delay(200); 
                render_dual(ICON_CLEAR, ICON_CLEAR); // 껐다
                delay(200); 
                break;

            case MODE_CLEAR:
            default:
                render_dual(ICON_CLEAR, ICON_CLEAR);
                delay(200);
                break;
        }
    }
    return NULL;
}

// [쓰레드 2] 부저 제어
void* buzzerThreadFunc(void* arg) {
    int local_mode;
    while (1) {
        // [LOCK] 현재 모드 읽기
        pthread_mutex_lock(&mode_mutex);
        local_mode = current_mode;
        pthread_mutex_unlock(&mode_mutex);

        if (local_mode == MODE_EXIT) break;

        switch (local_mode) {
            
            // [경고] 1초 간격 "삑... 삑..."
            case MODE_WARN:
                softToneWrite(BUZZER_PIN, 1000); // 1000Hz 켜기
                delay(200); 
                softToneWrite(BUZZER_PIN, 0);    // 끄기
                delay(800);
                break;

            // [위험] 경찰차 사이렌 (Frequency Sweep)
            case MODE_DANGER:
                // 주파수 상승 (500 -> 1500)
                for (int freq = 500; freq < 1500; freq += 20) {
                    // 루프 중간 모드 변경 확인
                    pthread_mutex_lock(&mode_mutex);
                    if (current_mode != MODE_DANGER) {
                         pthread_mutex_unlock(&mode_mutex);
                         goto exit_danger_loop; // 위험 모드를 빠져나감
                    }
                    pthread_mutex_unlock(&mode_mutex);
                    softToneWrite(BUZZER_PIN, freq);
                    delay(5); 
                }
                
                // 주파수 하강 (1500 -> 500)
                for (int freq = 1500; freq > 500; freq -= 20) {
                    pthread_mutex_lock(&mode_mutex);
                    if (current_mode != MODE_DANGER) {
                        pthread_mutex_unlock(&mode_mutex);
                        goto exit_danger_loop;
                    }
                    pthread_mutex_unlock(&mode_mutex);
                    softToneWrite(BUZZER_PIN, freq);
                    delay(5);
                }
                break;

            // [안전/대기] 소리 끔
            case MODE_SAFE:
            case MODE_CLEAR:
            default:
                softToneWrite(BUZZER_PIN, 0);
                delay(100); 
                break;
        }

        exit_danger_loop:; // goto 레이블
    }
    
    // 쓰레드 종료 시 확실하게 끄기
    softToneWrite(BUZZER_PIN, 0);
    return NULL;
}