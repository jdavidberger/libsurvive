import libsurvive
import sys

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from mpl_toolkits.mplot3d import Axes3D

actx = libsurvive.SimpleContext(sys.argv)

for obj in actx.Objects():
    print(obj.Name())

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
line, = ax.plot([], [], [], color='b')
line.axes.axis([-5, 5, -5, 5])
line.axes.set_zlim3d([-5, 5])

x = []
y = []
z = []

def update(*args):
    updated = actx.NextUpdated()
    while updated:
        x.append(updated.Pose()[0].Pos[0])
        y.append(updated.Pose()[0].Pos[1])
        z.append(updated.Pose()[0].Pos[2])
        line.set_data(x, y)
        line.set_3d_properties(z)
        updated = actx.NextUpdated()
    return [ line ]

def gen_function():
    frames=0
    while actx.Running():
        frames = frames + 30
        yield frames
    print frames
    return

ani = animation.FuncAnimation(fig, update, gen_function, init_func=update, interval=30, blit=True)
plt.show()
        
