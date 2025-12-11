from playsound3 import playsound
# import pymavlink.dialects.v20.all as dialect
from pymavlink import mavutil
from batfilestart import start_bat_file

from PyQt6 import uic
from PyQt6.QtWidgets import QApplication
import time
import threading




# Запуск MavProxy
bat_thread = threading.Thread(target=start_bat_file)
bat_thread.daemon = True
bat_thread.start()


Form, Window = uic.loadUiType("dialog.ui")

app = QApplication([])

window = Window()
form = Form()
form.setupUi(window)
window.show()


def do_connect(connect):
# def do_connect(connect='COM16', baud = 57600):
    vehicle = mavutil.mavlink_connection(connect)
    vehicle.wait_heartbeat()
    return vehicle

master = do_connect('udp:127.0.0.1:14551')

tar1 = 0
tar2 = 0
tar3 = 0
tar4 = 0
start_time1 = start_time2 = start_time3 = start_time4 = time.time()

def accept():
    print("ACCEPTED")

def rejected():
    print("REJECTED")

def set_new_text_label():
    global tar1, tar2, tar3, tar4
    form.label1.setText(str(tar1))
    form.label2.setText(str(tar2))
    form.label3.setText(str(tar3))
    form.label4.setText(str(tar4))

def set_mode(number_of_target, mode_num):
     master.mav.command_long_send(number_of_target, master.target_component, mavutil.mavlink.MAV_CMD_DO_SET_MODE, 0, 1, mode_num, 0, 0, 0, 0, 0)


def change_color_red(number_of_button:int):
    match number_of_button:
        case 1:
            form.pushButton1.setStyleSheet("background-color: red")
        case 2:
            form.pushButton2.setStyleSheet("background-color: red")
        case 3:
            form.pushButton3.setStyleSheet("background-color: red")
        case 4:
            form.pushButton4.setStyleSheet("background-color: red")
        case _:
            pass

    playsound("bung.mp3")

def change_color_green(number_of_button:int):
    match number_of_button:
        case 1:
            form.pushButton1.setStyleSheet("background-color: green")
        case 2:
            form.pushButton2.setStyleSheet("background-color: green")
        case 3:
            form.pushButton3.setStyleSheet("background-color: green")
        case 4:
            form.pushButton4.setStyleSheet("background-color: green")
        case _:
            pass

def relay_trigger(number_of_target:int):
    print("TRIGGER")
    change_color_green(number_of_target)
    # command = dialect.MAVLink_command_long_message(number_of_target,
    #                                                target_component=master.target_component,
    #                                                command=dialect.MAV_CMD_DO_SET_RELAY,
    #                                                confirmation=0,
    #                                                param1=0,  # relay pin instance number, starts from 0, 0 = first relay
    #                                                param2=1,  # 1 = ON, 0 = OFF
    #                                                param3=0,
    #                                                param4=0,
    #                                                param5=0,
    #                                                param6=0,
    #                                                param7=0)

    master.mav.command_long_send(number_of_target, master.target_component, mavutil.mavlink.MAV_CMD_DO_SET_RELAY,  # Команда
        0,  # confirmation
        0,  # param1 (Номер реле)
        1,  # param2 (1 - ON)
        0, 0, 0, 0, 0  # Остальные параметры
    )

    # master.mav.send(command)
    time.sleep(0.1)
    # command = dialect.MAVLink_command_long_message(number_of_target,
    #                                                target_component=master.target_component,
    #                                                command=dialect.MAV_CMD_DO_SET_RELAY,
    #                                                confirmation=0,
    #                                                param1=0,
    #                                                # relay pin instance number, starts from 0, 0 = first relay
    #                                                param2=0,  # 1 = ON, 0 = OFF
    #                                                param3=0,
    #                                                param4=0,
    #                                                param5=0,
    #                                                param6=0,
    #                                                param7=0)

    master.mav.command_long_send(number_of_target, master.target_component, mavutil.mavlink.MAV_CMD_DO_SET_RELAY,  # Команда
                                 0,  # confirmation
                                 0,  # param1 (Номер реле)
                                 0,  # param2 (1 - ON)
                                 0, 0, 0, 0, 0  # Остальные параметры
                                 )

    # master.mav.send(command)
    time.sleep(0.5)

    #перевод в режим MANUAL/AUTO (0- manual, 10 - auto)
    set_mode(number_of_target, 10)

def get_telemetry():
    global tar1, tar2, tar3, tar4, start_time1, start_time2, start_time3, start_time4
    while True:
        try:
            msg = master.recv_match(blocking=True, timeout=1)
            if msg is not None:
                if msg.get_type() in ['STATUSTEXT']:
                    message = msg.to_dict()
                    print(message)
                    mavtext = message['text']
                    match mavtext:
                        case 'SET HOLD MODE1':
                            if time.time() - start_time1>4:
                                tar1 = tar1 + 1
                                print(tar1)
                                print(f"Мишень №1 поражена")
                                change_color_red(1)
                                set_new_text_label()
                                start_time1 = time.time()
                                time.sleep(0.5)
                                relay_trigger(1)
                        case 'SET HOLD MODE2':
                            if time.time() - start_time2 > 4:
                                tar2 = tar2 + 1
                                print(tar2)
                                print(f"Мишень №2 поражена")
                                change_color_red(2)
                                set_new_text_label()
                                start_time2 = time.time()
                        case 'SET HOLD MODE3':
                            if time.time() - start_time3 > 4:
                                tar3 = tar3 + 1
                                print(tar3)
                                print(f"Мишень №3 поражена")
                                change_color_red(3)
                                set_new_text_label()
                                start_time3 = time.time()
                        case 'SET HOLD MODE4':
                            if time.time() - start_time4 > 4:
                                tar4 = tar4 + 1
                                print(tar4)
                                print(f"Мишень №4 поражена")
                                change_color_red(4)
                                set_new_text_label()
                                start_time4 = time.time()
                        case _:
                            print("Что это было?")

        except Exception as e:
            print(f'Потеряно подключение телеметрии {e}')


telemetry_thread = threading.Thread(target=get_telemetry)
telemetry_thread.daemon = True
telemetry_thread.start()

form.buttonBox.accepted.connect(accept)
form.buttonBox.rejected.connect(rejected)

form.label1.setText(str(tar1))
form.label2.setText(str(tar2))
form.label3.setText(str(tar3))
form.label4.setText(str(tar4))



form.pushButton1.clicked.connect(lambda: relay_trigger(1))
form.pushButton2.clicked.connect(lambda: relay_trigger(2))
form.pushButton3.clicked.connect(lambda: relay_trigger(3))
form.pushButton4.clicked.connect(lambda: relay_trigger(4))
form.pushButtonAct.clicked.connect(lambda: change_color_red(4))


app.exec()

