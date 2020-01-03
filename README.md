# GOL 
# Satyaa Suresh 

This program plays Conway’s Game of Life.

Conway’s Game of Life is an example of discrete event simulation, where a world of entities live, die, or are born based based on their surrounding neighbors. Each time step simulates another round of living or dying.

This world is represented by a 2-D array of values (0 or 1).

If a grid cell’s value is 1, it represents a live object; if it is 0, it represents a dead object.

At each discrete time step, every cell in the 2-D grid gets a new value based on the current value of its eight neighbors:

A live cell with zero or one live neighbors dies from loneliness.

A live cell with four or more live neighbors dies due to overpopulation.

A dead cell with exactly three live neighbors becomes alive.

All other cells remain in the same state between rounds.

The 2-D world is a torus, meaning every cell in the grid has exactly eight neighbors, even if it is on the edge of the grid. In the torus world, cells on the edge of the grid have neighbors that wrap around to the opposite edge.
