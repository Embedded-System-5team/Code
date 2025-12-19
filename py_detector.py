import cv2
import time
import os
import numpy as np
from picamera2 import Picamera2 

# 경로 정의
FIFO_PATH = "/tmp/opencv_fifo"
TRIGGER_PATH = "/tmp/trigger_capture"  # [추가] C에서 보내는 촬영 신호 파일
SAVE_PATH = "danger_capture.jpg"       # [추가] 저장될 사진 파일명
THRESHOLD_VAL = 25
MIN_CONTOUR_AREA = 500

def run_motion_detector():
    print("[Python] Initializing Picamera2...")
    
    # Picamera2 초기화
    picam2 = Picamera2()
    
    # 설정: 비디오 분석용(LoRes)와 사진 촬영용(Main) 동시 구성
    config = picam2.create_preview_configuration(
        main={"size": (1920, 1080), "format": "BGR888"}, # 사진용 고화질
        lores={"size": (640, 480), "format": "YUV420"},  # 분석용 저화질
        queue=False
    )
    picam2.configure(config)
    picam2.start()
    
    print("[Python] Picamera2 started successfully.")

    # FIFO 파일 열기
    try:
        fifo_fd = os.open(FIFO_PATH, os.O_WRONLY) 
        print(f"[Python] Opened FIFO pipe: {FIFO_PATH}")
    except Exception as e:
        print(f"[Python] ERROR opening pipe: {e}")
        picam2.stop()
        return

    time.sleep(2) 
    
    # 초기 프레임 (분석용 lores 스트림 사용)
    prev_frame = picam2.capture_array("lores")
    gray_prev = cv2.cvtColor(prev_frame, cv2.COLOR_YUV2GRAY_I420)
    
    print("[Python] Motion Detector Running... (Waiting for capture trigger)")

    try:
        while True:
           # === [수정된 로직] 촬영 트리거 파일 확인 ===
            if os.path.exists(TRIGGER_PATH):
                print(">>> [Python] Capture Trigger received! Taking photo...")
                
                # [추가됨] 기존 사진 파일이 있다면 먼저 삭제 (권한 충돌 방지)
                if os.path.exists(SAVE_PATH):
                    try:
                        os.remove(SAVE_PATH)
                        print(">>> [Python] Old file removed.")
                    except Exception as e:
                        print(f">>> [Python] Failed to remove old file: {e}")

                try:
                    # 사진 촬영 및 저장
                    picam2.capture_file(SAVE_PATH) 
                    print(f">>> [Python] Saved to {SAVE_PATH}")
                    
                    # [추가됨] 저장된 파일의 권한을 '누구나 읽기/쓰기 가능'하게 변경 (chmod 666)
                    os.chmod(SAVE_PATH, 0o666)
                    
                except Exception as e:
                    print(f">>> [Python] Capture failed: {e}")
                
                # 트리거 파일 삭제
                try:
                    os.remove(TRIGGER_PATH)
                except:
                    pass
            # ==========================================

            # 실시간 움직임 감지 (lores 스트림 사용)
            current_frame = picam2.capture_array("lores")
            gray_current = cv2.cvtColor(current_frame, cv2.COLOR_YUV2GRAY_I420)
            
            diff = cv2.absdiff(gray_prev, gray_current)
            _, thresh = cv2.threshold(diff, THRESHOLD_VAL, 255, cv2.THRESH_BINARY)
            thresh = cv2.dilate(thresh, None, iterations=2)
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

            motion_detected = False
            for contour in contours:
                if cv2.contourArea(contour) > MIN_CONTOUR_AREA:
                    motion_detected = True
                    break

            if motion_detected:
                os.write(fifo_fd, b'1')
            else:
                os.write(fifo_fd, b'0')

            gray_prev = gray_current
            time.sleep(0.05) 
            
    except KeyboardInterrupt:
        print("\n[Python] Detector stopped.")
    finally:
        picam2.stop()
        os.close(fifo_fd)

if __name__ == "__main__":
    run_motion_detector()