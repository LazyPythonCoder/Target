import threading
import time
import cv2
from pymavlink import mavutil

# --- НАСТРОЙКИ ---
CAMERA_INDEX = 0  # 0 — основная веб-камера, 1 — следующая подключенная USB-камера
FRAME_WIDTH = 1280  # Желаемая ширина кадра
FRAME_HEIGHT = 720  # Желаемая высота кадра

COM_PORT = "COM16"  # Укажите ваш COM-порт наземного блока HM30 (см. Диспетчер устройств)
BAUD_RATE = 57600

# Словарь для хранения актуальной телеметрии
telemetry = {
    "voltage": 0.0,
    "alt": 0.0,
    "speed": 0.0,
    "sats": 0,
    "mode": "UNKNOWN",
    "armed": False,
    "mavlink_ok": False,
}


def mavlink_worker():
    """Фоновый поток для чтения данных MAVLink с COM-порта."""
    print(f"Подключение к MAVLink на {COM_PORT}...")
    while True:
        try:
            master = mavutil.mavlink_connection(COM_PORT, baud=BAUD_RATE)
            master.wait_heartbeat(timeout=5)
            print("Heartbeat получен! Телеметрия активна.")
            telemetry["mavlink_ok"] = True

            # Запрашиваем поток данных у ArduPilot (4 Гц)
            master.mav.request_data_stream_send(
                master.target_system,
                master.target_component,
                mavutil.mavlink.MAV_DATA_STREAM_ALL,
                4,
                1,
            )

            while True:
                msg = master.recv_match(blocking=True, timeout=2.0)
                if not msg:
                    telemetry["mavlink_ok"] = False
                    continue

                telemetry["mavlink_ok"] = True
                msg_type = msg.get_type()

                if msg_type == "SYS_STATUS":
                    telemetry["voltage"] = msg.voltage_battery / 1000.0

                elif msg_type == "VFR_HUD":
                    telemetry["alt"] = msg.alt
                    telemetry["speed"] = msg.groundspeed

                elif msg_type == "GPS_RAW_INT":
                    telemetry["sats"] = msg.satellites_visible

                elif msg_type == "HEARTBEAT":
                    telemetry["mode"] = mavutil.mode_string_v10(msg)
                    telemetry["armed"] = bool(
                        msg.base_mode
                        & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED
                    )

        except Exception as e:
            telemetry["mavlink_ok"] = False
            print(f"Ожидание MAVLink ({e})...")
            time.sleep(2.0)


def draw_text(img, text, pos, color=(0, 255, 0), scale=0.7):
    """Отрисовка текста с черной обводкой для читаемости на любом фоне."""
    font = cv2.FONT_HERSHEY_SIMPLEX
    x, y = pos
    cv2.putText(img, text, (x, y), font, scale, (0, 0, 0), 4, cv2.LINE_AA)
    cv2.putText(img, text, (x, y), font, scale, color, 2, cv2.LINE_AA)


def open_camera(index):
    """Открытие веб-камеры через DirectShow (оптимально для Windows)."""
    cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    return cap


def main():
    # Запуск фонового потока телеметрии
    t = threading.Thread(target=mavlink_worker, daemon=True)
    t.start()

    print(f"Открытие камеры с индексом {CAMERA_INDEX}...")
    cap = open_camera(CAMERA_INDEX)

    if not cap.isOpened():
        print("Ошибка: не удалось открыть камеру. Проверьте CAMERA_INDEX.")
        return

    window_name = "MAVLink Webcam OSD"
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Потеря сигнала с камеры, переподключение...")
            cap.release()
            time.sleep(0.5)
            cap = open_camera(CAMERA_INDEX)
            continue

        h, w, _ = frame.shape

        # --- ОТРИСОВКА ЭЛЕМЕНТОВ OSD ---
        # Верхний левый угол: Батарея и спутники
        bat_color = (0, 255, 0) if telemetry["voltage"] > 14.5 else (0, 0, 255)
        draw_text(frame, f"BAT: {telemetry['voltage']:.1f} V", (20, 40), bat_color)
        draw_text(frame, f"SAT: {telemetry['sats']}", (20, 75))

        # Верхний правый угол: Режим полета и статус ARM
        arm_text = "ARMED" if telemetry["armed"] else "DISARMED"
        arm_color = (0, 0, 255) if telemetry["armed"] else (200, 200, 200)
        draw_text(frame, f"MODE: {telemetry['mode']}", (w - 260, 40))
        draw_text(frame, arm_text, (w - 260, 75), arm_color)

        # Нижний левый угол: Высота и скорость
        draw_text(frame, f"ALT: {telemetry['alt']:.1f} m", (20, h - 60))
        draw_text(frame, f"SPD: {telemetry['speed']:.1f} m/s", (20, h - 25))

        # Предупреждение, если нет связи с полетным контроллером
        if not telemetry["mavlink_ok"]:
            draw_text(
                frame, "NO MAVLINK TELEMETRY", (w // 2 - 150, 40), (0, 0, 255)
            )

        # Перекрестие по центру экрана
        # cx, cy = w // 2, h // 2
        # cv2.line(frame, (cx - 15, cy), (cx + 15, cy), (0, 255, 0), 2)
        # cv2.line(frame, (cx, cy - 15), (cx, cy + 15), (0, 255, 0), 2)

        cv2.imshow(window_name, frame)

        # Управление клавишами:
        # 'q' или 'Esc' — выход
        # 'f' — переключить полноэкранный режим
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q") or key == 27:
            break
        elif key == ord("f"):
            prop = cv2.getWindowProperty(window_name, cv2.WND_PROP_FULLSCREEN)
            new_mode = (
                cv2.WINDOW_NORMAL
                if prop == cv2.WINDOW_FULLSCREEN
                else cv2.WINDOW_FULLSCREEN
            )
            cv2.setWindowProperty(
                window_name, cv2.WND_PROP_FULLSCREEN, new_mode
            )

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()





# # Для RTSP потока
#
# import os
# import threading
# import time
# import cv2
# from pymavlink import mavutil
#
# # --- НАСТРОЙКИ ---
# RTSP_URL = "rtsp://admin:123456@192.168.144.30:554/Streaming/Channels/101"
# COM_PORT = "COM3"  # Укажите ваш COM-порт наземного блока HM30
# BAUD_RATE = 57600
#
# # Принудительно используем TCP для RTSP, чтобы не было артефактов по радиоканалу
# os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp"
#
# # Словарь для хранения актуальной телеметрии
# telemetry = {
#     "voltage": 0.0,
#     "alt": 0.0,
#     "speed": 0.0,
#     "sats": 0,
#     "mode": "UNKNOWN",
#     "armed": False,
# }
#
#
# def mavlink_worker():
#     """Фоновый поток для чтения данных MAVLink с COM-порта."""
#     print(f"Подключение к MAVLink на {COM_PORT}...")
#     try:
#         master = mavutil.mavlink_connection(COM_PORT, baud=BAUD_RATE)
#         master.wait_heartbeat()
#         print("Heartbeat получен! Телеметрия активна.")
#
#         # Запрашиваем поток данных у ArduPilot (4 Гц)
#         master.mav.request_data_stream_send(
#             master.target_system,
#             master.target_component,
#             mavutil.mavlink.MAV_DATA_STREAM_ALL,
#             4,
#             1,
#         )
#
#         while True:
#             msg = master.recv_match(blocking=True, timeout=1.0)
#             if not msg:
#                 continue
#
#             msg_type = msg.get_type()
#
#             if msg_type == "SYS_STATUS":
#                 telemetry["voltage"] = msg.voltage_battery / 1000.0  # мВ в Вольты
#
#             elif msg_type == "VFR_HUD":
#                 telemetry["alt"] = msg.alt
#                 telemetry["speed"] = msg.groundspeed
#
#             elif msg_type == "GPS_RAW_INT":
#                 telemetry["sats"] = msg.satellites_visible
#
#             elif msg_type == "HEARTBEAT":
#                 telemetry["mode"] = mavutil.mode_string_v10(msg)
#                 telemetry["armed"] = bool(
#                     msg.base_mode & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED
#                 )
#
#     except Exception as e:
#         print(f"Ошибка MAVLink: {e}")
#
#
# def draw_text(img, text, pos, color=(0, 255, 0), scale=0.7):
#     """Отрисовка текста с черной обводкой для читаемости на любом фоне."""
#     font = cv2.FONT_HERSHEY_SIMPLEX
#     x, y = pos
#     cv2.putText(img, text, (x, y), font, scale, (0, 0, 0), 4, cv2.LINE_AA)
#     cv2.putText(img, text, (x, y), font, scale, color, 2, cv2.LINE_AA)
#
#
# def main():
#     # Запуск потока телеметрии
#     t = threading.Thread(target=mavlink_worker, daemon=True)
#     t.start()
#
#     print("Подключение к RTSP потоку камеры...")
#     cap = cv2.VideoCapture(RTSP_URL, cv2.CAP_FFMPEG)
#     cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)  # Минимальный буфер для снижения задержки
#
#     if not cap.isOpened():
#         print("Не удалось открыть RTSP поток. Проверьте IP и порт.")
#         return
#
#     window_name = "SIYI HM30 OSD"
#     cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
#
#     while True:
#         ret, frame = cap.read()
#         if not ret:
#             print("Потеря кадра, переподключение...")
#             time.sleep(0.5)
#             cap = cv2.VideoCapture(RTSP_URL, cv2.CAP_FFMPEG)
#             continue
#
#         h, w, _ = frame.shape
#
#         # --- ОТРИСОВКА ЭЛЕМЕНТОВ OSD ---
#         # Верхний левый угол: Батарея и спутники
#         bat_color = (0, 255, 0) if telemetry["voltage"] > 14.5 else (0, 0, 255)
#         draw_text(frame, f"BAT: {telemetry['voltage']:.1f} V", (20, 40), bat_color)
#         draw_text(frame, f"SAT: {telemetry['sats']}", (20, 75))
# 
#         # Верхний правый угол: Режим полета и статус ARM
#         arm_text = "ARMED" if telemetry["armed"] else "DISARMED"
#         arm_color = (0, 0, 255) if telemetry["armed"] else (200, 200, 200)
#         draw_text(frame, f"MODE: {telemetry['mode']}", (w - 250, 40))
#         draw_text(frame, arm_text, (w - 250, 75), arm_color)
#
#         # Нижний левый угол: Высота и скорость
#         draw_text(frame, f"ALT: {telemetry['alt']:.1f} m", (20, h - 60))
#         draw_text(frame, f"SPD: {telemetry['speed']:.1f} m/s", (20, h - 25))
#
#         # Перекрестие по центру экрана
#         cx, cy = w // 2, h // 2
#         cv2.line(frame, (cx - 15, cy), (cx + 15, cy), (0, 255, 0), 2)
#         cv2.line(frame, (cx, cy - 15), (cx, cy + 15), (0, 255, 0), 2)
#
#         cv2.imshow(window_name, frame)
#
#         # Выход по клавише 'q' или 'Esc'
#         key = cv2.waitKey(1) & 0xFF
#         if key == ord("q") or key == 27:
#             break
#
#     cap.release()
#     cv2.destroyAllWindows()
#
#
# if __name__ == "__main__":
#     main()
#
