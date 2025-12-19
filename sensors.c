#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h> 
#include <pthread.h> 

#include "config.h"
#include "sensors.h"

// =========================================================
// 센서 초기화 및 PIR/초음파 함수
// =========================================================

void init_sensors() {
    pinMode(PIR_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pullUpDnControl(PIR_PIN, PUD_DOWN);
}

int check_pir() {
    /*
    return digitalRead(PIR_PIN);
    */

    static unsigned long last_pir_time = 0; // 마지막 감지 시간을 기억하는 변수 (static)
    const unsigned long PIR_HOLD_TIME = 10000; // 유지 시간: 10초 (원하는 대로 조절 가능)
    
    // 1. 실제 센서값 읽기
    int current_state = digitalRead(PIR_PIN);

    // 2. 움직임이 감지되면(HIGH), 타이머 갱신
    if (current_state == 1) {
        last_pir_time = millis(); // 현재 시간(ms) 저장
        return 1; // 감지됨
    }

    // 3. 지금은 LOW지만, 마지막 감지로부터 10초가 안 지났다면?
    if ((millis() - last_pir_time) < PIR_HOLD_TIME) {
        // 아직 사람이 있다고 "거짓말"을 함 (유지 상태)
        return 1; 
    }

    // 4. 10초도 지났고 센서도 LOW라면 -> 진짜 없음
    return 0;
}

double get_distance() {
    struct timeval start, end;
    long timeout = 0;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    while(digitalRead(ECHO_PIN) == LOW) {
        if(timeout++ > 30000) return -1;
    }
    gettimeofday(&start, NULL);

    timeout = 0;
    while(digitalRead(ECHO_PIN) == HIGH) {
        if(timeout++ > 30000) return -1;
    }
    gettimeofday(&end, NULL);

    long seconds = end.tv_sec - start.tv_sec;
    long micros = end.tv_usec - start.tv_usec;
    double elapsed = seconds + micros / 1000000.0;
    
    return elapsed * 34000 / 2;
}

// =========================================================
// OpenCV 움직임 감지 결과 읽기 (뮤텍스 적용)
// =========================================================

int check_opencv_motion() {
    int detected;
    pthread_mutex_lock(&mode_mutex);
    detected = opencv_motion_detected;
    pthread_mutex_unlock(&mode_mutex);
    return detected; 
}

// =========================================================
// [IPC] OpenCV 결과 읽기 쓰레드 (FIFO/Named Pipe)
// =========================================================

void* opencvPipeReadThread(void* arg) {
    char buffer[2];
    int fd;
    
    // 1. FIFO(Named Pipe) 파일 생성
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("[FIFO] mkfifo failed");
        return NULL;
    }

    printf("[FIFO] Waiting for Python Detector to start and open pipe (%s)...\n", FIFO_PATH);
    
    // 2. 파이프 열기 (파이썬이 쓸 때까지 대기)
    fd = open(FIFO_PATH, O_RDONLY); 
    if (fd == -1) {
        perror("[FIFO] open failed");
        return NULL;
    }
    
    printf("[FIFO] Pipe opened. Reading motion status...\n");

    // 3. 데이터 수신 루프 (0 또는 1을 읽음)
    while (current_mode != MODE_EXIT) {
        if (read(fd, buffer, 1) > 0) { // 데이터 수신 시 전역 변수 업데이트 (동기화 필요)
            pthread_mutex_lock(&mode_mutex);
            if (buffer[0] == '1') {
                opencv_motion_detected = 1;
            } else if (buffer[0] == '0') {
                opencv_motion_detected = 0;
            }
            pthread_mutex_unlock(&mode_mutex);
        }
        usleep(10000); 
    }
    
    close(fd);
    unlink(FIFO_PATH); 
    return NULL;
}

// =========================================================
// Python Detector 백그라운드 실행 함수 (작동 안됨)
// =========================================================
//source venv/bin/activate
//python3 py_detector.py

// void start_python_detector() {
//     const char *command = "/home/pi/Projects/github/Embedded_System_5team/Code/Embedded_System/venv/bin/python3 /home/pi/Projects/github/Embedded_System_5team/Code/Embedded_System/py_detector.py > /tmp/py_log.txt 2>&1 &";
    
//     printf("[Auto Start] Executing Python Detector: %s\n", command);

//     int result = system(command);

//     if (result == -1) {
//         perror("[Auto Start] system() function call failed");
//     } else {
//         printf("[Auto Start] Python Detector started.\n");
//     }
// }


// =========================================================
// 카메라 캡처 함수 (Python 트리거 방식)
// =========================================================

int capture_image() {
    // Python에게 촬영 신호를 보내기 위해 파일을 생성합니다.
    const char *trigger_cmd = "touch /tmp/trigger_capture";

    printf("[Camera] Requesting capture via Python trigger...\n");

    int result = system(trigger_cmd);

    if (result == -1) {
        perror("[Camera] Failed to create trigger file");
        return -1;
    } 
    
    printf("✅ [Camera] Capture request sent.\n");
    return 0;
}