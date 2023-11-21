import serial
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
import numpy as np

# input() name of file==sampleName
# read from serial
# save to csv
# draw graph plt

filename = input("Enter sample name: ")
portname = input("Enter USB port name: ")
print(f"Params: {filename}, {portname}")

ser = serial.Serial(portname, 9600, timeout=1)
time.sleep(2)

data = []

def print_serial():
    while True:
        line = ser.readline().decode()
        #print(line)

def plot_static():
    for i in range(50):
        line = ser.readline()   # read a byte string
        if line:
            string = line.decode()  # convert the byte string to a unicode string
            num = int(string) # convert the unicode string to an int
            print(num)
            data.append(num) # add int to data list
    ser.close()

    # build the plot
    plt.plot(data)
    plt.xlabel('Time')
    plt.ylabel('Potentiometer Reading')
    plt.title('Potentiometer Reading vs. Time')
    plt.show()