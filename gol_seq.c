#include <stdio.h>
#include <stdlib.h>
#include <omp.h>   // to use openmp timing functions
#include <ctype.h>
#include <unistd.h>

#define SRAND_VALUE 1985

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

// number of game steps (default = 16)
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
          } else if (isprint (optopt)) {
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
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

int transition(int cells[10]) {
    int self = cells[0];

    if (self == INERT) {
        return INERT;
    }

    else if (self == DEAD2) {
        return EMPTY;
    }

    else if (self == SPORE) {
        float nextProb = (float)rand()/(float)RAND_MAX;
        if (nextProb < probSporeToHyphae) {
            return YOUNG;
        }
        return SPORE;
    }

   else if (self == MATURING) {
        float nextProb = (float)rand()/(float)RAND_MAX;
        if (nextProb < probMushroom) {
            return MUSHROOMS;
        }
        return OLDER;
    }

    else if (self == EMPTY) {
        int count = 0;

        for (int i = 1; i < 9; i++) { // count the number of YOUNG neighbors
            if (cells[i] == YOUNG)
            count++;
        }

        // printf("count: %d\n", count);

        if (count == 0) {
            return EMPTY;
        }

        float nextProb = (float)rand()/(float)RAND_MAX;
        float probSpreadFinal = binomialProbability(count);
        // printf("nextProb: %.6f\n", probSpreadFinal);
        // printf("probSpreadFinal: %.6f\n", probSpreadFinal);

        if (nextProb < probSpreadFinal) {
            return YOUNG;
        }
        else {
            return EMPTY;
        }        
    }

    else {
        return self += 1; // for the YOUNG, MUSHROOMS, OLDER, DECAYING, DEAD1 case, go to next state
    }
}


void apply_rules(int *grid, int *newGrid) {
    int i,j;

    // iterate over the grid
    for (i = 1; i <= dim; i++) {
        for (j = 1; j <= dim; j++) {
            int id = i*(dim+2) + j;

            // apply the game rules
            int cells[9] = {grid[id], grid[id+(dim+2)], grid[id-(dim+2)], grid[id+1],
                grid[id-1], grid[id+(dim+3)], grid[id-(dim+3)], grid[id-(dim+1)], grid[id+(dim+1)]}; // construct array of neighbors

            newGrid[id] = transition(cells);
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
 
void gol(int *grid, int *newGrid)
{
    
    init_boundary(grid, newGrid);
    
    apply_rules(grid, newGrid);
    
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

    for(it = 0; it < itEnd; it++) {
        gol( grid, newGrid );
        if (it % 50 == 0) {
            printGridOutput(grid);
        }
    }

    double runtime = omp_get_wtime() - st;
 
    printf("total time: %f s\n", runtime);

    for (i = 1; i <= dim; i++) {
        for (j = 1; j <= dim; j++) {
            counts[grid[i*(dim+2) + j]]++;
        }
    }

    // Uncomment below to print out final cell counts of each type
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
