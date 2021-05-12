import matplotlib as mpl
from matplotlib import pyplot
import numpy as np
import sys
import os
import time

# gets grid filename from command line
file_name = sys.argv[1]

# gets rid of anything (e.g. parent folder name) other than the actual file name
# for use in making directories/naming grid images
file_name_for_directory = file_name.split("/")[-1].split(".")[0]

# dpi values to ensure good image quality
pyplot.rcParams['figure.dpi'] = 400
pyplot.rcParams['savefig.dpi'] = 400

# splits grid, which is a list of strings (each string contains digits representing cell values)
# into a list of lists of integer digits, where each inner list represents a row in the grid
def split(grid):
    gridList = []
    lst = []
    for row in grid:
        for char in row:
            lst.append(int(char))
        gridList.append(lst)
        lst = []
    return gridList

listOfGrids = []

# reading from file and converting it into a np array format suitable for plotting
with open(file_name) as file:
    grid = []
    topRow = file.readline().rstrip() # strips \n
    grid.append(topRow)
    dim = len(topRow)-2
    for i in range(dim+1):
        grid.append(file.readline().rstrip())
    listOfGrids.append(np.array(split(grid)))

    grid = []
    file.readline() # \n char
    while True:
        topRow = file.readline().rstrip()
        grid.append(topRow)
        if topRow.startswith("t"): # end of file, where we printed the runtime
            break
        for i in range(dim + 1):
            grid.append(file.readline().rstrip())
        listOfGrids.append(np.array(split(grid)))
        file.readline()  # \n char
        grid = []




# make a color map of fixed colors, corresponding to recommended values in the Shiffrin textbook guide
cmap = mpl.colors.ListedColormap(['lightgreen','black','darkgray','lightgray','white','lightgray','tan','brown','darkgreen','yellow'])


# makes directory to store images
if not os.path.exists(f'images/{file_name_for_directory}'):
    os.makedirs(f'images/{file_name_for_directory}')

start = time.time()

for i in range(0, len(listOfGrids)):
    img = pyplot.imshow(listOfGrids[i], cmap=cmap, aspect='equal')
    # pyplot.show() # use to show rather than save to image
    # grids are by default in time intervals of 50
    pyplot.savefig(f"images/{file_name_for_directory}/{file_name_for_directory}_{i*50}.png")
    
end = time.time()
print(str(end - start) + " s") # timing of the plotting functionality, for future work