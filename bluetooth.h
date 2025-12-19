#ifndef BLUETOOTH_H
#define BLUETOOTH_H
#include <pthread.h>

void init_bluetooth();
void* bluetoothThreadFunc(void* arg);
int is_motor_locked();

#endif // BLUETOOTH_Hnce
