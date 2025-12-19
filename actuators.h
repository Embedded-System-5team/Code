#ifndef ACTUATORS_H
#define ACTUATORS_H

void init_actuators();
void cleanup_actuators();

// 쓰레드 함수들
void* displayThreadFunc(void* arg);
void* buzzerThreadFunc(void* arg);

#endif