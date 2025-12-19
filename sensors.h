#ifndef SENSORS_H
#define SENSORS_H

void init_sensors();
int check_pir();       // 움직임 감지 시 1 반환 (PIR)
double get_distance(); // 거리(cm) 반환 (초음파)
int check_opencv_motion(); // OpenCV 움직임 감지 결과 반환 (전역 변수 읽기)
int capture_image();   // 카메라 캡처 및 성공 시 0 반환

// IPC 통신 쓰레드 원형
void* opencvPipeReadThread(void* arg); 

// Python Detector 자동 실행 함수 원형
void start_python_detector(); 

#endif