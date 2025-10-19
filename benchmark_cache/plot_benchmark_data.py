import matplotlib.pyplot as plt
import numpy as np


data_file = "benchmark_plot_data.csv"
data_N = []
data_time = []
with open(data_file, "r") as f:
	for line in f:
		if line:
			split_data = line.strip().split(",")
			data_N.append(split_data[0])
			data_time.append(split_data[1])

	
plt.scatter(data_N, data_time)
plt.show()