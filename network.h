#ifndef NETWORK_H
#define NETWORK_H

#include <pthread.h>

void init_network();
void* wifiServerThreadFunc(void* arg);
void send_alert(int mode);

#endif // NETWORK_H
