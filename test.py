from picamera2 import Picamera2, Preview
import signal

picam2 = Picamera2()
picam2.configure(picam2.create_preview_configuration(main={"size": (1280, 720)}))

picam2.start_preview(Preview.QTGL)   # 데스크톱 미리보기(보통 이거)
picam2.start()

signal.pause()

