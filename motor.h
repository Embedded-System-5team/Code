#ifndef MOTOR_H
#define MOTOR_H

// 서보 모터 초기화
void init_motor();

// 서보 모터 정리
void cleanup_motor();

// 서보 모터 상태 설정 함수
// is_locked: 1 (잠금, 0도), 0 (열림, 90도)
void set_motor_state(int is_locked);

#endif // MOTOR_Hce
