import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('2022-05-28/TEST__12-38-39.633912.csv')

print(df.head(5))

df.plot(x='Time', y='EMG')
plt.show()
