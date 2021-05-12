#include <stdio.h>
#include <stdlib.h>
#include <omp.h>   // to use openmp timing functions
#include <ctype.h>
#include "openacc.h"
#include <curand.h>
#include <unistd.h>
#include <time.h>

#define SRAND_VALUE 1985
 
curandGenerator_t setup_prng(void *stream, unsigned long long seed);
void gen_rand_nums(curandGenerator_t gen, float *d_buffer, int num, void *stream);
void rand_cleanup( curandGenerator_t gen );

// Cell Values with Associated Constants
#define EMPTY 0
#define SPORE 1
#define YOUNG 2
#define MATURING 3
#define MUSHROOMS 4
#define OLDER 5
#define DECAYING 6
#define DEAD1 7
#define DEAD2 8
#define INERT 9

// Probability values
#define probSpore 0.01 // initial SPORE density
#define probSporeToHyphae 0.5 // SPORE to YOUNG
#define probMushroom 0.8 // MATURING to MUSHROOMS
#define probSpread 0.5 // Probability of a EMPTY neighbor going EMPTY to YOUNG for a YOUNG cell

// default grid dimension (1500 x 1500)
int dim = 1500;

// number of game steps (default = 1024)
int itEnd = 1 << 10;

// default board = 0
int board = 0;

void getArguments(int argc, char *argv[], int * dim, int * time, int * board) {
    int c;        // result from getopt calls

    char *dimvalue;
    char *tvalue;
    char *boardvalue;

    while ((c = getopt (argc, argv, "n:t:b:")) != -1) {
      switch (c)
        {
        case 'n':
          dimvalue = optarg;
          *dim = atoi(dimvalue);
          break;
        case 't':
          tvalue = optarg;
          *time = atoi(tvalue);
          break;
        case 'b':
          boardvalue = optarg;
          *board = atoi(boardvalue);
          if ((*board > 2) || (*board < 0)) {
            fprintf (stderr,
                     "Only boards 0, 1, and 2 are allowed");
            exit(EXIT_FAILURE);
          }
          break;
        case '?':
          if (optopt == 'n' || optopt == 't') {
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            exit(EXIT_FAILURE);
          } else if (isprint (optopt)) {
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            exit(EXIT_FAILURE);
          } else {
            fprintf (stderr,
                     "Unknown option character `\\x%x'.\n",
                     optopt);
            exit(EXIT_FAILURE);
          }

        }
    }
}

void init_boundary(int *grid, int *newGrid) {
    // makes boundaries (outer perimeter) into INERT to make decaying boundary condition

    int i;

    // boundary rows
    for (i = 0; i <= dim; i++) {
      // make bottom boundary row INERT
      grid[(dim+2)*(dim+1)+i] = INERT;
      
      // make top boundary row INERT
      grid[i] = INERT;
    }

    // boundary columns
    for (i = 0; i <= dim+1; i++) {
      // make right most boundary column INERT
      grid[i*(dim+2)+dim+1] = INERT;
      
      // make left most ghost column INERT
      grid[i*(dim+2)] = INERT;
    }
}

float binomialProbability(int count) { // calculates probability of repeated probSpread trials (for each count)
    float result = 0;
    for (int i = 0; i < count; i++) {
        result += (1.0-result) * probSpread;
        // printf("iter\n");
    }
    // printf("result: %d\n", result);
    return result;
}


void apply_rules(int *grid, int *newGrid, curandGenerator_t cuda_gen, float *arrayRN, int length) {
    int i,j;

    float nextProb;

    // Query OpenACC for CUDA stream
    // OpenACC has a queue for stream of data to be generated-here we
    // match that to what the CUDA code will create.
    void *stream = acc_get_cuda_stream(acc_async_sync);

    // get a new set of random numbers to use for probability testing
    // NVIDIA curand will create our initial random data.
    // see: https://developer.nvidia.com/blog/3-versatile-openacc-interoperability-techniques/
    // Notice that when using host_data you must give a list of 
    // the device arrays with the use_device clause.
    #pragma acc host_data use_device(arrayRN)
    {
        gen_rand_nums(cuda_gen, arrayRN, length, stream);
    }

    // iterate over the grid
    #pragma acc kernels
    {
        #pragma acc loop independent
        for (i = 1; i <= dim; i++) {
            #pragma acc loop independent gang vector(128)

            for (j = 1; j <= dim; j++) {
                int id = i*(dim+2) + j;

                // construct array of neighbors
                int cells[9] = {grid[id], grid[id+(dim+2)], grid[id-(dim+2)], grid[id+1],
                    grid[id-1], grid[id+(dim+3)], grid[id-(dim+3)], grid[id-(dim+1)], grid[id+(dim+1)]};

                int self = cells[0];
                

                if (self == INERT) {
                    newGrid[id] = INERT;
                }

                else if (self == DEAD2) {
                    newGrid[id] = EMPTY;
                }

                else if (self == SPORE) {
                    nextProb = arrayRN[id];
                    if (nextProb < probSporeToHyphae) {
                        newGrid[id] = YOUNG;
                    }
                    else {
                        newGrid[id] = SPORE;
                    }
                }

                else if (self == MATURING) {
                    nextProb = arrayRN[id];
                    if (nextProb < probMushroom) {
                        newGrid[id] = MUSHROOMS;
                    }
                    else {
                        newGrid[id] = OLDER;
                    }
                }

                else if (self == EMPTY) {
                    int count = 0;

                    for (int i = 1; i < 9; i++) { // count the number of YOUNG neighbors
                        if (cells[i] == YOUNG)
                        count++;
                    }

                    // printf("count: %d\n", count);

                    if (count == 0) {
                        newGrid[id] = EMPTY;
                    }
                    else {
                        nextProb = arrayRN[id];
                        float probSpreadFinal = binomialProbability(count);
                        // printf("nextProb: %.6f\n", probSpreadFinal);
                        // printf("probSpreadFinal: %.6f\n", probSpreadFinal);

                        if (nextProb < probSpreadFinal) {
                            newGrid[id] = YOUNG;
                        }
                        else {
                            newGrid[id] = EMPTY;
                        }
                    }        
                }

                else {
                    newGrid[id] = self += 1; // for the YOUNG, MUSHROOMS, OLDER, DECAYING, DEAD1 case, go to next state
                }
            }
        }
    }
}



void update_grid(int *grid, int *newGrid) {
    int i,j;

    // copy new grid over, as pointers cannot be switched on the device
    for(i = 1; i <= dim; i++) {
        for(j = 1; j <= dim; j++) {
            int id = i*(dim+2) + j;
            grid[id] = newGrid[id];
        }
    }
}
 
void gol(int *grid, int *newGrid, curandGenerator_t cuda_gen, float *arrayRN, int length) {
    init_boundary(grid, newGrid);
    
    apply_rules(grid, newGrid, cuda_gen, arrayRN, length);
    
    update_grid(grid, newGrid);     
}

void printGrid(int *grid) {
    int i, j;

    for (i = 0; i <= dim+1; i++) {
        for (j = 0; j <= dim+1; j++) {
            if (grid[i*(dim+2) + j] == MUSHROOMS) {
                printf("■"); // VISUALIZE MUSHROOMS
            }
            else {
                printf("%d", (grid[i*(dim+2) + j]));
            }
        }
        printf("\n");
    }
    printf("\n");
}

void printGridOutput(int *grid) {
    int i, j;

    for (i = 0; i <= dim+1; i++) {
        for (j = 0; j <= dim+1; j++) {
            printf("%d", (grid[i*(dim+2) + j]));
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char* argv[])
{
    int i, j;
    
    // Board 0 is the single spore in the center
    // Board 1 is the four spores in the center of each quadrant
    // Board 2 is a randomly generated grid (initial spore density = 0.01), or 1%.

    if(argc > 1){ // if flags are set
        getArguments(argc, argv, &dim, &itEnd, &board);
    }
    
    // grid array with dimension dim + boundary columns and rows
    int    arraySize = (dim+2) * (dim+2);
    size_t bytes     = arraySize * sizeof(int);
    int    *grid     = (int*)malloc(bytes);
 
    // allocate result grid
    int */*restrict*/newGrid = (int*) malloc(bytes);
 
    // assign initial population to empty board if case 2
    srand(SRAND_VALUE);
    if (board == 2) {
        for(i = 1; i <= dim; i++) {
            for(j = 1; j <= dim; j++) {
                if (((float) rand()/RAND_MAX) < probSpore) {
                    grid[i*(dim+2)+j] = SPORE;
                }
                else {
                    grid[i*(dim+2)+j] = EMPTY;
                }
            }
        }
    }

    else { // otherwise initialize empty board
        for(i = 1; i <= dim; i++) {
            for(j = 1; j <= dim; j++) {
                grid[i*(dim+2)+j] = EMPTY;
            }
        }
    }
    switch (board) {
        case (0):
            grid[dim/2 * dim + dim/2] = SPORE; // approximately centered one spore
            break;
        case (1):
            grid[(int) (dim * 0.25 * dim + dim * 0.25)] = SPORE; // four spores, one in each quadrant
            grid[(int) (dim * 0.25 * dim + dim * 0.75)] = SPORE;
            grid[(int) (dim * 0.75 * dim + dim * 0.25)] = SPORE;
            grid[(int) (dim * 0.75 * dim + dim * 0.75)] = SPORE;
            break;
    }

    int counts[10] = {0,0,0,0,0,0,0,0,0,0}; // sum up cell counts

    int it;
    double st = omp_get_wtime(); // start timing

    void *stream = acc_get_cuda_stream(acc_async_sync);

    int length = (dim+2)*(dim+2);
    curandGenerator_t cuda_gen;
    float *restrict arrayRN = (float*)malloc(length*sizeof(float));

    // use CUDA library functions to initialize a generator
    unsigned long long seed = time(NULL);
    cuda_gen = setup_prng(stream, seed);


    for(it = 0; it < itEnd; it++) {
        gol( grid, newGrid, cuda_gen, arrayRN, length );
        if (it % 50 == 0) {
            printGridOutput(grid);
        }
    }

    // tear down CUDA random number generation
    rand_cleanup(cuda_gen);

    double runtime = omp_get_wtime() - st;
 
    printf("total time: %f s\n", runtime);

    for (i = 1; i <= dim; i++) {
        for (j = 1; j <= dim; j++) {
            counts[grid[i*(dim+2) + j]]++;
        }
    }
    // printf("Total EMPTY: %d\n", counts[0]);
    // printf("Total SPORE: %d\n", counts[1]);
    // printf("Total YOUNG: %d\n", counts[2]);
    // printf("Total MATURING: %d\n", counts[3]);
    // printf("Total MUSHROOMS: %d\n", counts[4]);
    // printf("Total OLDER: %d\n", counts[5]);
    // printf("Total DECAYING: %d\n", counts[6]);
    // printf("Total DEAD1: %d\n", counts[7]);
    // printf("Total DEAD2: %d\n", counts[8]);
    // printf("Total INERT: %d\n", counts[9]);




    // printGrid(grid); //to debug; only use with small grids

    free(grid);
    free(newGrid);
 
    return 0;
}
