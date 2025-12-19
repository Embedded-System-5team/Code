#ifndef CONFIG_H
#define CONFIG_H

#include <wiringPi.h>
#include <pthread.h> 

// --- 핀 번호 설정 (BCM GPIO 기준) ---
#define PIR_PIN     27  // PIR 센서
#define TRIG_PIN    4  // 초음파 Trig
#define ECHO_PIN    5  // 초음파 Echo
#define BUZZER_PIN  12  // 부저 (SoftTone)
#define SERVO_PIN   13  // 서보 모터 (SoftPWM) - *새로 추가됨*

// --- 시스템 모드 정의 ---
#define MODE_CLEAR   0
#define MODE_SAFE    1  // 평소 (감시 중)
#define MODE_WARN    2  // 접근 (경고)
#define MODE_DANGER  3  // 초근접 (위험/방어)
#define MODE_EXIT    99

// --- 거리 임계값 (cm) ---
#define DIST_WARN    100.0 // 1m 이내 진입 시 경고
#define DIST_DANGER  50.0  // 50cm 이내 진입 시 위험

// --- 네트워크 및 블루투스 설정 ---
#define WIFI_SERVER_PORT 8080
#define MAX_CLIENTS      5
#define AUTH_PASSWORD    "1234"  // 일반 사용자 비밀번호
#define ADMIN_PASSWORD   "9999"  // 관리자 비밀번호

// --- 카메라 및 OpenCV 설정 ---
#define CAMERA_COMMAND "rpicam-still"
#define CAPTURE_FILE_NAME "danger_capture.jpg"
#define CAPTURE_OPTIONS " -o " CAPTURE_FILE_NAME " -t 1 --autofocus-mode auto"

// === FIFO 경로 (IPC) ===
#define FIFO_PATH "/tmp/opencv_fifo" 

// 전역 변수 공유 (extern)
extern volatile int current_mode;
extern volatile int opencv_motion_detected;
extern pthread_mutex_t mode_mutex;

#endif // CONFIG_H