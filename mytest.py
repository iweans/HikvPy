import time
import cv2
# ----------------------------------------
from core import Device
# --------------------------------------------------


camera01 = Device('cap01', '192.168.0.65', 8000, 'admin', 'a1234567')
ret_flag, error_code = camera01.login()
if not ret_flag:
    print(f'{camera01.id} login failed: {error_code} !')
camera02 = Device('cap02', '192.168.0.66', 8000, 'admin', 'a1234567')
ret_flag, error_code = camera02.login()
if not ret_flag:
    print(f'{camera02.id} login failed: {error_code} !')


ret_flag01, error_code01 = camera01.open()
ret_flag02, error_code02 = camera02.open()
if not ret_flag01:
    print(f'{camera01.id} open failed: {error_code01} !')
    exit()
if not ret_flag02:
    print(f'{camera02.id} open failed: {error_code02} !')
    exit()

cv2.namedWindow(camera01.id)
cv2.namedWindow(camera02.id)

count = 0
while True:
    ret_flag01, frame01 = camera01.read()
    ret_flag02, frame02 = camera02.read()
    if ret_flag01:
        count += 1
        frame01 = cv2.resize(frame01, (640, 360))
        cv2.imshow(camera01.id, frame01)
        cv2.waitKey(1)
    if ret_flag02:
        count += 1
        frame02 = cv2.resize(frame02, (640, 360))
        cv2.imshow(camera02.id, frame02)
        cv2.waitKey(1)

    if count > 500:
        count = 0
        cv2.destroyWindow(camera01.id)
        cv2.destroyWindow(camera02.id)
        ret_flag01, error_code01 = camera01.close()
        ret_flag02, error_code02 = camera02.close()
        if not ret_flag01:
            print(f'{camera01.id} close failed: {error_code01}')
            exit()
        if not ret_flag02:
            print(f'{camera02.id} close failed: {error_code02}')
            exit()

        time.sleep(2)

        time.sleep(2)
        ret_flag01, error_code01 = camera01.open()
        ret_flag02, error_code02 = camera02.open()
        if not ret_flag01:
            print(f'{camera01.id} open failed: {error_code01} !')
            exit()
        if not ret_flag02:
            print(f'{camera02.id} open failed: {error_code02} !')
            exit()





