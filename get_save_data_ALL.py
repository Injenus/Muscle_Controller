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

init_time = 0
pause_time = 0
delta_time = 0

isRecording = False
# df = pd.DataFrame(columns=['EMG', 'AngA', 'AngH', 'Time'])
buff, buff2 = [], []


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
    transmit_data([7, 1])


def close_port():
    serial.close()
    ui.text_status_serial.setText('The COM-port was closed.')


def receive_data():
    global receive_data_flag, df, init_time, delta_time, buff, buff2
    receive_data_flag = True
    rx = str(serial.readAll(), 'utf-8')
    rx = rx.replace('\r\n', '_')
    rx = rx.replace('\r', '')
    rx = rx.replace('\n', '')
    arr = rx.split('_')

    print(rx)
    # print(arr)
    arr = [value for value in arr if value != '']
    print('Массив', arr)

    for i, el in enumerate(arr):
        if len(el) > 0:
            if el[0] == ',':
                arr[i] = el[1:]
            elif el[-1] == ',':
                arr[i] = el[:-1]

    if len(arr):
        # check first el
        first_data = arr[0].split(',')
        if len(first_data) <= 4 and first_data[0].isdigit():
            if len(buff):  # если буфер непустой
                if len(buff + first_data) == 4:
                    data = buff + first_data
                else:
                    data = buff[:-1] + [buff[-1] + first_data[0]] + first_data[
                                                                    1:]
                    if len(data) > 4:
                        # не работает эта проверка, проще выкинуть брак
                        data1 = data[:3] + [str(data[3])[:-len(data[:-1]) + 1]]
                        print('Текущие данные first:', data)
                        parsing(data1)
                        data2 = [str(data[3])[len(data[:-1]) - 1:]] + data[4:]
                        print('Текущие данные first:', data)
                        parsing(data2)
                        data = data2
                        pass

                print('Текущие данные first 0:', data)
                parsing(data)


            else:  # если буфер пустой
                data = first_data
                print('Текущие данные first 1:', data)
                parsing(data)

            # if len(data) < 4:
            #     if len(buff2 + data) == 4:
            #         data = buff2 + data
            #     else:
            #         data = buff2[:-1] + [buff2[-1] + data[0]] + data[1:]
            #     buff2 = []
            buff = []


        else:
            if not first_data[0].isdigit():
                data = arr[0].split()
                print('Текущие данные first 2:', data)
                parsing(data)
            else:
                print('ERROR_0_!!!')

        for el in arr[1:-1]:
            if len(el) > 0:
                el_arr = el.split(',')
                if len(el_arr) < 4:
                    if not el_arr[0].isdigit():
                        data = el.split()
                        print('Текущие данные 3:', data)
                        parsing(data)
                    else:
                        print('ERROR_1_!!!')
                else:
                    if len(el_arr) == 4:
                        data = el_arr
                        print('Текущие данные 4:', data)
                        parsing(data)
                    else:
                        print('ERROR_2_!!!')

        # check last el
        if len(arr) > 1:
            last_data = arr[-1].split(',')
            if len(last_data) < 4 and last_data[0].isdigit():
                buff = last_data
            else:
                if len(last_data) == 4 and int(last_data[3]) <= int(
                        arr[-2].split(',')[
                            -1]):
                    buff = last_data
                elif not last_data[0].isdigit():
                    data = arr[-1].split()
                    print('Текущие данные last 5:', data)
                    parsing(data)
                elif len(last_data) == 4:
                    data = last_data
                    print('Текущие данные last 6:', data)
                    parsing(data)
                else:
                    print('ERROR_3_!!!')
        else:
            if first_data[0].isdigit():
                buff2 = first_data


def parsing(data):
    print('Arduino says:', data, time.time() - init_time - delta_time)
    if data[0] == 'READY' or data[0] == 'READY (setup)':
        ui.text_status_serial.setText(
            'The COM-port is open. All ready to work!')
    elif data[0] == 'START' or data[0] == 'CONTINUE':
        pass
    elif data[0].isdigit():
        with open(DIRECTORY + folder + FILENAME + '.csv', 'a',
                  newline="") as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow(data)

        # df.loc[len(df.index)] = data
        # df.to_csv(folder + FILENAME + '.csv', encoding='utf-8', index=True)

        analysis()
    # elif rxs == 'PAUSE' and rxs == 'STOP':
    #     pass


def analysis():
    pass


def transmit_data(data):
    global receive_data_flag
    # if receive_data_flag or data == [7, 1]:
    txs = ''
    for val in data:
        txs += str(val) + ','
    txs = txs[:-1]
    serial.write(txs.encode())
    print(txs)
    receive_data_flag = False


def start_record():
    transmit_data([4])
    global isRecording, df, FILENAME, folder, init_time, delta_time
    init_time = time.time()
    delta_time = 0
    FILENAME = ui.data_combobox.currentText()
    ui.text_status_data.setText('Recording...')
    ui.start_data.setEnabled(False)
    ui.continue_data.setEnabled(False)
    ui.pause_data.setEnabled(True)
    ui.stop_data.setEnabled(True)
    if not isRecording:
        isRecording = True
        ui.data_combobox.setEnabled(False)

        # df = pd.DataFrame(columns=['EMG', 'AngA', 'AngH', 'Time'])

        folder = str(date.today()) + '/'
        if not os.path.exists(DIRECTORY + folder):
            os.makedirs(DIRECTORY + folder)

        # FILENAME = FILENAME.replace(' ', '_')
        # if os.path.isfile(DIRECTORY + folder + FILENAME + '.csv'):
        #     FILENAME += '__' + str((datetime.datetime.now().time())).replace(
        #         ':', '-')

        FILENAME = FILENAME.replace(' ', '_') + '__' + str(
            (datetime.datetime.now().time())).replace(
            ':', '-')

        with open(DIRECTORY + folder + FILENAME + '.csv', "w",
                  newline="") as file:
            writer = csv.writer(file)
            writer.writerows([['EMG', 'AngA', 'AngH', 'Time']])


def continue_record():
    transmit_data([6])
    global delta_time, pause_time
    delta_time += time.time() - pause_time
    ui.continue_data.setEnabled(False)
    ui.pause_data.setEnabled(True)
    ui.text_status_data.setText('Recording...')


def pause_record():
    transmit_data([5])
    global pause_time
    pause_time = time.time()
    ui.continue_data.setEnabled(True)
    ui.pause_data.setEnabled(False)
    ui.text_status_data.setText(
        'Recording paused. Press "Continue" to continue.')


def stop_record():
    transmit_data([8])
    global isRecording, folder, FILENAME
    ui.start_data.setEnabled(True)
    ui.continue_data.setEnabled(False)
    ui.pause_data.setEnabled(False)
    ui.stop_data.setEnabled(False)
    ui.text_status_data.setText(
        'Recording saved. To start a new one, select the type of gesture and press "Start".')
    ui.data_combobox.setEnabled(True)
    isRecording = False
    # df.to_csv(folder + FILENAME + '.csv', encoding='utf-8', index=True)
    # print(df.shape)


#
serial = QSerialPort()
serial.setBaudRate(115200)
refresh_serial_list()
#
ui.refresh_serial.clicked.connect(refresh_serial_list)
ui.open_serial.clicked.connect(open_port)
ui.close_serial.clicked.connect(close_port)
#
serial.readyRead.connect(receive_data)
###
ui.text_status_serial.setReadOnly(1)
ui.text_status_serial.setText('The COM-port is closed. Please, open one.')
ui.text_status_data.setReadOnly(1)
ui.text_status_data.setText(
    'To start recording, select the type of gesture and press "Start".')
ui.continue_data.setEnabled(False)
ui.pause_data.setEnabled(False)
ui.stop_data.setEnabled(False)
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
