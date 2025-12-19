#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <signal.h> 

#include "config.h"
#include "sensors.h"
#include "actuators.h"
#include "motor.h"
#include "bluetooth.h"
#include "network.h"

// 전역 변수 실체화 (공유 자원)
volatile int current_mode = MODE_SAFE;
volatile int opencv_motion_detected = 0;
pthread_mutex_t mode_mutex; 

// Ctrl+C가 눌리면 이 함수가 소환됩니다.
void emergency_shutdown(int sig) {
    printf("\n>>> Force Shutdown Detected! Cleaning up...\n");
    
    // 1. 장치들 끄기 (여기에 다 몰아넣으세요)
    cleanup_actuators();
    cleanup_motor();
    
    // 2. 파이썬 카메라 끄기
    system("pkill -f py_detector.py");
    
    // 3. 프로그램 진짜 종료
    exit(0);
}

int main() {
    signal(SIGINT, emergency_shutdown);

    // 1. 라이브러리 초기화
    if (wiringPiSetupGpio() == -1) return 1;
    if (wiringPiSPISetup(0, 1000000) == -1) {
        fprintf(stderr, ">>> ERROR: Unable to open SPI device /dev/spidev0.0.\n");
        return 1;
    }

    // 1-1. 뮤텍스 초기화
    if (pthread_mutex_init(&mode_mutex, NULL) != 0) {
        fprintf(stderr, ">>> ERROR: Mutex initialization failed.\n");
        return 1;
    }

    // 2. 모듈별 초기화
    init_sensors();
    init_actuators();
    init_motor();     
    init_bluetooth(); 
    init_network();   

    // === Python Detector 자동 실행 (IPC 시작) ===
    start_python_detector();

    // 3. 쓰레드 시작
    pthread_t th_disp, th_buzz, th_pipe_reader;
    pthread_t th_bt, th_wifi; 

    pthread_create(&th_disp, NULL, displayThreadFunc, NULL);
    pthread_create(&th_buzz, NULL, buzzerThreadFunc, NULL);
    pthread_create(&th_pipe_reader, NULL, opencvPipeReadThread, NULL);
    pthread_create(&th_bt, NULL, bluetoothThreadFunc, NULL);
    pthread_create(&th_wifi, NULL, wifiServerThreadFunc, NULL);

    printf(">>> Sentry System Started (Full Integration) <<<\n");
    printf("State: SAFE (Monitoring Camera Motion OR PIR...)\n");

    int camera_triggered = 0;
    int capture_done = 0;
    int local_mode;
    int opencv_detected;
    int last_alert_mode = MODE_SAFE;

    // 4. 메인 루프 (수정된 시나리오: Cam+PIR 필수 -> 이후 거리 측정)
    while (1) {
        // [모터 제어]
        int locked = is_motor_locked();
        set_motor_state(locked);

        // [LOCK] OpenCV 감지 결과 읽기
        pthread_mutex_lock(&mode_mutex);
        opencv_detected = opencv_motion_detected;
        pthread_mutex_unlock(&mode_mutex);

        // --- 1. PIR 센서 값 읽기 ---
        int pir_detected = check_pir();

        // --- 2. 시나리오 판단 시작 ---
        
        // [조건 1] 카메라와 PIR이 '둘 다' 감지되었는가? (AND 조건)
        if (opencv_detected == 1 && pir_detected == 1) {
            
            // 1차 조건 만족! 이제야 비로소 거리를 측정합니다.
            static double safe_dist = 0; 
            double raw_dist = get_distance();
            if (raw_dist != -1) {
                safe_dist = raw_dist;
            }
            double dist = safe_dist;

            // [조건 2] 거리가 위험 수준인가?
            if (dist > 0 && dist < DIST_DANGER) {
                // -> MODE_DANGER (침입자가 확실하고, 거리도 가까움)
                pthread_mutex_lock(&mode_mutex);
                local_mode = current_mode;

                if (local_mode != MODE_DANGER) {
                    printf("!!! DANGER: Target Verified & Close (%.1f cm) !!!\n", dist);
                    if (last_alert_mode != MODE_DANGER) {
                        send_alert(MODE_DANGER);
                        last_alert_mode = MODE_DANGER;
                    }
                }
                current_mode = MODE_DANGER;
                pthread_mutex_unlock(&mode_mutex);

                // 사진 캡처
                if (capture_done == 0) {
                    capture_image();
                    capture_done = 1;
                }
            }
            else {
                // -> MODE_WARN (침입자는 맞는데, 아직 거리는 멂)
                pthread_mutex_lock(&mode_mutex);
                local_mode = current_mode;

                if (local_mode != MODE_WARN) {
                    printf("--- Warning: Target Verified (Cam + PIR) ---\n");
                    if (last_alert_mode != MODE_WARN) {
                        send_alert(MODE_WARN);
                        last_alert_mode = MODE_WARN;
                    }
                }
                current_mode = MODE_WARN;
                capture_done = 0; // WARN 상태에서는 캡처 플래그 초기화
                pthread_mutex_unlock(&mode_mutex);
            }
        }
        // [조건 불만족] 카메라나 PIR 중 하나라도 감지 안 되면 -> SAFE
        else {
            pthread_mutex_lock(&mode_mutex);
            if (current_mode != MODE_SAFE) {
                printf(">>> Condition not met (Cam:%d, PIR:%d). Safe Mode.\n", opencv_detected, pir_detected);
                current_mode = MODE_SAFE;
                last_alert_mode = MODE_SAFE;
            }
            pthread_mutex_unlock(&mode_mutex);
        }

        delay(50); // 루프 주기
    }

    // 종료 처리
    pthread_mutex_lock(&mode_mutex);
    current_mode = MODE_EXIT;
    pthread_mutex_unlock(&mode_mutex);

    pthread_join(th_pipe_reader, NULL);
    pthread_join(th_disp, NULL);
    pthread_join(th_buzz, NULL);
    pthread_join(th_bt, NULL);
    pthread_join(th_wifi, NULL);

    pthread_mutex_destroy(&mode_mutex);

    return 0;
}