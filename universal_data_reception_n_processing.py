'''
  Протокол отправки:
  данные с датчиков: $<целое_число0>,<целое_число1>..<целое_числоN>;
  данные состояния устройства: #<строка_символов>;

  Для управления микроконтроллером отправляются однозначные числа 3..7
'''
from PyQt5 import QtWidgets, uic
from PyQt5.QtSerialPort import QSerialPort, QSerialPortInfo
from PyQt5.QtCore import QIODevice
import time
import csv
import datetime
from datetime import date
import os

receive_data_flag = True
app = QtWidgets.QApplication([])
ui = uic.loadUi('D:/PyProjects/Muscle_Controller/get_save_data.ui')
ui.setWindowTitle('Data Collection by Injenus')

DIRECTORY = ''
FILENAME = ''
folder = ''  # dont change!!! it's just for convenience
header_list = ['extenSig', 'flexSig', 'extenRaw', 'flexRaw', 'accX', 'accY',
               'accZ', 'deltaTime(us from MC)', 'Time(us from PC by MC data)']
num_add = 1  # кол-во столбцов, добавляемых на ПК

''' 
#На случай вычисления времени от ПК без данных МК
init_time = 0
pause_time = 0
delta_time = 0
'''
timePC = 0  # время от ПК по данным МК

isRecording = False
isWaitingStatus = 4 * [True]  # for Start, for Pause, for Continue, for Stop

flag_collection = False  # условие для формирования строки данных (пакета) из Порта
clear_rxs = ''  # строка, в которую формируется (наполняется посимвольно) пакет переданных данных


def refresh_serial_list():
    ui.serial_combobox.clear()
    port_list = []
    ports = QSerialPortInfo().availablePorts()
    for port in ports:
        port_list.append((port.portName()))
    ui.serial_combobox.addItems(port_list)


def open_port():
    serial.setPortName(ui.serial_combobox.currentText())
    serial.open(QIODevice.ReadWrite)


def ready_data():
    transmit_data(3)


def close_port():
    serial.close()
    ui.text_status_serial.setText('The COM-port was closed.')
    ui.data_combobox.setEnabled(False)
    ui.start_data.setEnabled(False)
    ui.ready_send_data.setEnabled(False)


def receive_data():
    global flag_collection, clear_rxs
    rx = serial.readAll()
    # print(rx)
    rxs = ''
    for el in rx:
        rxs += el.decode('UTF-8')

    for symb in rxs:
        if symb == "#" or symb == "$":
            flag_collection = True
        if symb == ';':
            flag_collection = False
            parsing(clear_rxs)
            clear_rxs = ''
        if flag_collection:
            clear_rxs += symb


def parsing(stroka):
    global isRecording, header_list, init_time, delta_time, delta_time, pause_time, pause_time, isWaitingStatus, timePC
    print(stroka)
    if '#' in stroka:
        data_stroka = stroka[1:]
        if data_stroka == 'All SYSTEMS ONLINE':
            ui.ready_send_data.setEnabled(True)
            ui.text_status_serial.setText(
                'The COM-port is open. Click on "Ready" to test the connection.')
        elif data_stroka == 'READY':
            ui.text_status_serial.setText('Connected.')
            ui.start_data.setEnabled(True)
            ui.data_combobox.setEnabled(True)
        elif data_stroka == 'START':
            timePC = 0
            init_time = time.time()
            delta_time = 0
            ui.text_status_data.setText('Recording...')
            ui.start_data.setEnabled(False)
            ui.ready_send_data.setEnabled(False)
            ui.pause_data.setEnabled(True)
            ui.stop_data.setEnabled(True)
        elif data_stroka == 'PAUSE':
            pause_time = time.time()
            ui.ready_send_data.setEnabled(False)
            ui.continue_data.setEnabled(True)
            ui.pause_data.setEnabled(False)
            ui.close_serial.setEnabled(True)
            ui.text_status_data.setText(
                'Recording paused. Press "Continue" to continue.')
        elif data_stroka == 'CONTINUE':
            isWaitingStatus[2] = True
            delta_time += time.time() - pause_time
            ui.ready_send_data.setEnabled(False)
            ui.continue_data.setEnabled(False)
            ui.pause_data.setEnabled(True)
            ui.close_serial.setEnabled(False)
            ui.text_status_data.setText('Recording...')
        elif data_stroka == 'STOP':
            isWaitingStatus[3] = True
            ui.ready_send_data.setEnabled(True)
            ui.start_data.setEnabled(True)
            ui.continue_data.setEnabled(False)
            ui.pause_data.setEnabled(False)
            ui.stop_data.setEnabled(False)
            ui.close_serial.setEnabled(True)
            ui.text_status_data.setText(
                'Recording saved. To start a new one, select the type of gesture and press "Start".')
            ui.data_combobox.setEnabled(True)
            isRecording = False
        elif data_stroka == 'ERROR: CHECK MPU6050':
            ui.text_status_serial.setText('MPU-6050 connection error.')
        else:
            print('ERROR # MESSAGE:', data_stroka)
            ui.text_status_serial.setText(
                'An error occurred, please try again.')
        # print(data_stroka)
    elif '$' in stroka:
        try:
            arr_data = [int(x) for x in stroka[1:].split(',')]
        except:
            arr_data = [-1 for i in range(len(header_list))]
            print('НЕКОРРЕКТНЫЕ ДАННЫЕ!')
            ui.text_status_data.setText('ERROR: Incorrect data!')
        if len(arr_data) + num_add > len(header_list):
            arr_data = [-1 for i in range(len(header_list))]
            print('ОЖИДАЛОСЬ МЕНЬШЕ ДАННЫХ!')
            ui.text_status_data.setText('ERROR: Less data expected!')
        elif len(arr_data) + num_add < len(header_list):
            arr_data = [-1 for i in range(len(header_list))]
            print('ОЖИДАЛОСЬ БОЛЬШЕ ДАННЫХ!')
            ui.text_status_data.setText('ERROR: More data expected!')

        if not isWaitingStatus[0]:
            timePC = -arr_data[-1]
            isWaitingStatus[0] = True
        if not isWaitingStatus[1]:
            pause_record()
        if not isWaitingStatus[2]:
            # continue_record()
            pass
        if not isWaitingStatus[3]:
            stop_record()

        print(arr_data)

        with open(DIRECTORY + folder + FILENAME + '.csv', 'a',
                  newline="") as csv_file:
            writer = csv.writer(csv_file)
            '''
            writer.writerow(
                arr_data + [round(time.time() - init_time - delta_time, 6)])'''
            timePC += arr_data[-1]
            writer.writerow(arr_data + [timePC])

        analysis(arr_data)


def analysis(data):
    pass


def transmit_data(send_command):
    txs = str(send_command)
    serial.write(txs.encode())
    print(txs)


def start_record():
    global isRecording, FILENAME, folder, header_list, isWaitingStatus
    transmit_data(4)
    isWaitingStatus[0] = False
    FILENAME = ui.data_combobox.currentText()
    if not isRecording:
        isRecording = True
        ui.data_combobox.setEnabled(False)

        folder = str(date.today()) + '/'
        if not os.path.exists(DIRECTORY + folder):
            os.makedirs(DIRECTORY + folder)

        FILENAME = FILENAME.replace(' ', '_') + '__' + str(
            (datetime.datetime.now().time())).replace(':', '-')

        with open(DIRECTORY + folder + FILENAME + '.csv', "w",
                  newline="") as file:
            writer = csv.writer(file)
            writer.writerows([header_list])


def continue_record():
    global isWaitingStatus
    transmit_data(6)
    # isWaitingStatus[2] = False


def pause_record():
    global isWaitingStatus
    transmit_data(5)
    # isWaitingStatus[1] = False


def stop_record():
    global isWaitingStatus
    transmit_data(7)
    # isWaitingStatus[3] = False


#
serial = QSerialPort()
serial.setBaudRate(1000000)
refresh_serial_list()
#
ui.refresh_serial.clicked.connect(refresh_serial_list)
ui.open_serial.clicked.connect(open_port)
ui.ready_send_data.clicked.connect(ready_data)
ui.close_serial.clicked.connect(close_port)
#
serial.readyRead.connect(receive_data)
###
ui.text_status_serial.setReadOnly(1)
ui.text_status_serial.setText('The COM-port is closed. Please, open one.')
ui.text_status_data.setReadOnly(1)
ui.text_status_data.setText(
    'To start recording, select the type of gesture and press "Start".')
ui.ready_send_data.setEnabled(False)
ui.start_data.setEnabled(False)
ui.continue_data.setEnabled(False)
ui.pause_data.setEnabled(False)
ui.stop_data.setEnabled(False)
ui.data_combobox.setEnabled(False)
#
ui.start_data.clicked.connect(start_record)
ui.continue_data.clicked.connect(continue_record)
ui.pause_data.clicked.connect(pause_record)
ui.stop_data.clicked.connect(stop_record)
#
ui.data_combobox.addItems(
    ['Rock sign', 'Phone call', 'OK', 'V sign', 'Spider-Man', 'Thumbs up',
     'TEST'])

#
ui.show()
app.exec()
