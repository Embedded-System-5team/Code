#include "motor.h"
#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <softPwm.h>
#include <wiringPi.h>

// 50Hz (20ms) �ֱ� SoftPWM ���� ���� �޽� ��
#define SERVO_PULSE_0_DEG 5  // 0.5ms �޽�
#define SERVO_PULSE_90_DEG 25 // 1.5ms �޽�

void init_motor() {
    // ���� ���� �ʱ�ȭ (SoftPWM)
    pinMode(SERVO_PIN, OUTPUT);

    // SoftPWM �ֱ⸦ 50Hz (20ms)�� ���� (���� 200)
    if (softPwmCreate(SERVO_PIN, 0, 200) != 0) {
        fprintf(stderr, ">>> ERROR: SoftPWM for Servo failed. Check library installation.\n");
        return;
    }

    set_motor_state(1); // �ʱ� ����: ��� (0��)
    printf(">>> Servo Motor Module Initialized on Pin %d.\n", SERVO_PIN);
}

void cleanup_motor() {
    // ���� ���� ����: �޽��� ���� ���� LOW�� ����
    softPwmWrite(SERVO_PIN, 0);
    digitalWrite(SERVO_PIN, LOW);
    printf(">>> Servo Motor Module Cleaned Up.\n");
}

void set_motor_state(int is_locked) {
    if (is_locked) {
        softPwmWrite(SERVO_PIN, SERVO_PULSE_0_DEG); // 0�� (���)
        // printf(">>> Motor: Locked (0 degrees)\n"); // Main���� ����ϹǷ� ����
    }
    else {
        softPwmWrite(SERVO_PIN, SERVO_PULSE_90_DEG); // 90�� (����)
        // printf(">>> Motor: Unlocked (90 degrees)\n"); // Main���� ����ϹǷ� ����
    }
}