# # COMP 445 final project

This is the code and data repository for my capstone project for COMP 445 (Parallel/Distributed Processing). It is based on the Mushroom Fairy Ring cellular automata project described by Shiflet and Shiflet. Refer to the [report](https://docs.google.com/document/d/1YLiJuwb21qcQkeGn1GAO4gfDVacFW7doOG1oLX8fHWs/edit?usp=sharing) for background information and details on the parallel (OpenAcc manycore) implementation.

# How to compile

Compile the code using `make` followed by the version to be compiled, like this:

        make gol_seq
        make gol_acc
        
To remove compiled code, run `make clean`

# How to run

Run the code using the filename followed by flags, followed by a redirect to text file output, like this:

        ./gol_seq (-n) [DIMENSION] (-t) [NUM_TIMESTEP] (-b) [BOARD_CASE] > board0.txt

For example,

        ./gol_seq -n 5000 -t 200 -b 1 > board0.txt

Yields a 5000 x 5000 grid, 200 time steps, board 1, output stored in board0.txt.

Running without redirect will print the boards to the console, which will not work with large grids.

Flags are optional, and can be run without them, like this

        ./gol_seq
        ./gol_acc

Default (no flags used) values are dimension = 1500, num_timesteps = 1024, board_case = 0.

Board case 0 is one spore, one in each the center of each quadrant.
Board case 1 are four spores, one in each the center of each quadrant.
Board case 2 are spores randomly dispersed. Spore density by default is 1% - see `probSpore = 0.01`. Change manually in the C code.

Users are encouraged to experiment with different spore densities in board 2,  such as the 4% density used in the report for figure 5.

# How to profile

Compile the code using `make` followed by the version to be compiled with a prof suffix, like this:

        make gol_seq_prof
        make gol_acc_prof

The profiling uses the defaults (1500 x 1500, 1024 time steps, board 0).

# How to visualize

Setup by installing required package matplotlib. A virtual environment is recommended.

Run using `python3 visualize.py [file_name]`, such as:

        python3 visualize.py board0.txt
        
Which will take some time to process and produce .png images in the images/[file_name] folder.
As the grids are by default printed out every 50 time steps, the images are named grid_0, grid_50, and so on.

Images from sample runs are saved in this images folder for reference.
If you wish to visualize from already generated boards, use, for example,

        python3 visualize.py ./example_board_output/board0.txt
