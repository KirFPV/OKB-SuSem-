from capture_of_qr_code import capture
from skan_qr_code import scan_qrcode


######################################################################
##################### Обработка Serial порт ##########################
######################################################################

'''
Таблица g-кодов

S - коды - отправка сообщений espP -> rpi
G - коды - отправка сообщений rpi -> espP
M - коды - отправка сообщений espP <-> espM по BLE

S2 - ранее отправленая команда выполнена
S8 - нажата кнопка инициализации, начать процедуру перезагрузки

G1 Hx - движение к этажу, x - этаж цель
G3 - передать команду на машинку "начать движение"
G4 - передать команду на машинку "обратное движение"
G28 - движение каретки в нулевое положение, калибровка

M2 - машинка закончила движение
M3 - команда машинке "начать движение"
M4 - команда машинке "обратное движение"
'''

import serial
import logging
import time

ser = ""
car_slot = [0, 0, 0, 0, 0, 0]

def log_init():
    logging.basicConfig(
        filename='log.txt',
        level=logging.DEBUG,
        format='%(asctime)s -%(levelname)s - %(message)s',
        encoding= "utf-8",
        filemode= "w"
    )
    logging.info(f'Начало логирования в файл')
log_init()

def log_print(msg_log, type_msg):
    if type_msg == 1:
        logging.info(f"{msg_log}")
    elif type_msg == 2:
        logging.debug(f"{msg_log}")
    elif type_msg == 3:
        logging.warning(f"{msg_log}")
        

def uart_init():
    print(';(uart init..)')
    serials = serial.Serial('COM4', 9600)
    log_print("Serial порт активирован", 1)
    return serials

ser = uart_init()

def uart_print(msg_out):
    print(f';({msg_out} >> UART)')
    msg_out += '\n'
    ser.write(msg_out.encode('utf-8'))
    log_print(f'Отправлен G-код на парковку {msg_out}', 2)

def uart_read():
    msg_in_raw = ser.readline()
    msg_in_data = msg_in_raw.decode('utf-8')
    if (";(" in str(msg_in_data)):
        debag_mes = msg_in_data[2:]
        print(';(', debag_mes)
        log_print(f"Получено отладочное сообщение: {msg_in_data}", 2)
        
    elif ("S" in str(msg_in_data)):
        code_in = int(msg_in_data[1:])
        print(';(', code_in, ')', sep = "")
        log_print(f"Получен G-код: {msg_in_data}", 2)
        return code_in


######################################################################
################## Обработка основных функций кода ###################
######################################################################    

def reboot():
    global car_slot
    log_print(f"Начало перезагрузки", 2)
    print(';(reboot)')
    uart_print("G28")
    log_init()
    car_slot = [0, 0, 0, 0, 0, 0]
    print(wait_finish_task())
    main()

def wait_finish_task():
    code_in = uart_read()
    while code_in != 2:
        time.sleep(1)
        code_in = uart_read()
        if code_in == 2:
            return "task_finish"
        
def start_cycle():
    code_in = uart_read()
    while code_in != 8:
        time.sleep(1)
        code_in = uart_read()
        if code_in == 8:
            return "task_finish"
        
def skan_qr():
    print(';(Capture img and scan for QR)')
    filename = f'qr.png'
    print(filename)

    capture(filename)
    data = scan_qrcode(filename)
    if (not data):
        print(f';(Cant found QR code. Try again..)')
        log_print(f"QR код не найден", 1)
        return "no qr"
    else:
        print(";(Successfully found and read QR code!)")
        print(f";(Found '{data}' item)")
        log_print(f"Найден QR код {data}", 1)
        return data

def work_cycle():
    global car_slot
    print(start_cycle())
    log_print(f"Начало основного цикла", 2)
    current_floor_task = skan_qr()

    if current_floor_task == "no qr":  #qr нет, машинка вне зоны сканирования
        log_print(f"QR код не найден, движение машинки на 1 шаг вперед, повторный запуск основного цикла", 1)
        uart_print("G3")
        print(";(", wait_finish_task(), ")", sep = "")
        current_floor_task = skan_qr()
        if uart_read() == 2:
            work_cycle()
    
    if car_slot[int(current_floor_task) - 1] == 0: #qr принадлежит пустой ячейке, в зоне сканирования машинка
        log_print(f"Найденый QR принадлежит свободной ячейке, QR находится на машинке, начало выполнения цикла подъема машинки в ячеку {current_floor_task}", 1)
        
        uart_print("G3")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Машинка в зоне подъемника", 1)

        uart_print(f'G1 H{int(current_floor_task)}')
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Подъемник поднят на этаж {current_floor_task}", 1)

        uart_print("G3")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Машинка в ячейке", 1)

        uart_print("G1 H1")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Подъемник в зоне выгрузки", 1)
        
        car_slot[int(current_floor_task) - 1] = 1
        log_print(f"Текущая загруженость парковки {car_slot}", 1)
        work_cycle()

    elif car_slot[int(current_floor_task) - 1] == 1: #qr принадлежит занятой ячейке, qr в руках
        log_print(f"Найденый QR принадлежит занятой ячейке, QR находится в руках пользователя, начало выполнение цикла спуска машинки в зону выгрузки {current_floor_task}", 1)

        uart_print(f'G1 H{int(current_floor_task)}')
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Подъемник поднят на этаж {current_floor_task}", 1)

        uart_print("G4")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Машинка в зоне подъемника", 1)

        uart_print("G1 H1")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Подъемник в зоне выгрузки", 1)

        uart_print("G4")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Машинка в зоне сканирования", 1)

        uart_print("G4")
        print(";(", wait_finish_task(), ")", sep = "")
        log_print(f"Машинка вне зоны парковки", 1)

        car_slot[int(current_floor_task) - 1] = 0
        log_print(f"Текущая загруженость парковки {car_slot}", 1)
        work_cycle()


def main(): 
    global ser, car_slot
    start_cycle()
    log_init()
    #print(";(", check_BLE(), ")", sep = "")  

    uart_print("G28")
    print(";(", wait_finish_task(), ")", sep = "")
    
    time.sleep(3)

    print(";(", work_cycle(), ")", sep = "")

    #if code_in == 8:
        #reboot()



main()


######################################################################
######################## Сканирование qr кода ########################
######################################################################

