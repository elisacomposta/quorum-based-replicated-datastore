import matplotlib.pyplot as plt
import numpy as np
import csv

# Open file
file = open('results/client/client_operations.csv', 'r')
file.readline()

# Create lists 
clients = []
gets = []
puts = []
refused = []

# Split columns
for a, b, c, d in csv.reader(file, delimiter=';'):
    clients.append(a)
    gets.append(int(b))
    puts.append(int(c))
    refused.append(int(d))

# Plot
x_axis = np.arange(len(clients))
y_axis = np.arange(max(max(gets), max(puts), max(refused)) + 1)

plt.bar(x_axis - 0.3, list(gets), 0.2, align='center', label='get', color='seagreen')
plt.bar(x_axis - 0.1, puts, 0.2, align='center', label='put', color='lightseagreen')
plt.bar(x_axis + 0.1, refused, 0.2, align='center', label='refused', color='tomato')
plt.xticks(x_axis, clients)
plt.yticks(y_axis, y_axis)
plt.title('Operations per client')
lgd = plt.legend(loc='upper left', bbox_to_anchor=(1, 1))
plt.savefig('results/plots/client_operations.png', bbox_extra_artists=(lgd,), bbox_inches='tight')
