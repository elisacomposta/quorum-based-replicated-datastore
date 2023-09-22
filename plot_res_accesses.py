import matplotlib.pyplot as plt
import numpy as np
import csv
import sys

# Open file
param = sys.argv[1]
filename = 'results/replica/accesses/replica_' + param + '.csv'
file = open(filename, 'r')
file.readline()

# Create lists
resources = []
accesses = []

# Split columns
for res, acc in csv.reader(file, delimiter=';'):
    resources.append(res)
    accesses.append(int(acc))

# Plot
x_axis = np.arange(len(resources))
y_axis = np.arange(max(accesses) + 1)

plt.bar(x_axis, list(accesses), 0.2, align='center')
plt.xticks(x_axis, resources)
plt.yticks(y_axis, y_axis)
plt.title('Operations per resource')
img_name = 'results/plots/res_accesses_' + param + '.png'
plt.savefig(img_name)
