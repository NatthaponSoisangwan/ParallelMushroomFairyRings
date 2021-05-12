# Makefile for game of life
CC_SEQ= pgcc -Minfo=opt
CC_ACC_M= pgcc -fast -ta=tesla:cc75,managed -Minfo=accel -lcurand -Mcuda

#$(CC_ACC) -lcurand diffusion_acc.c fillRand.o -Mcuda -o diffusion_acc

# will use managed memory instead of managing it ourselves with this:
CC_ACC= pgcc -fast -ta=tesla:cc75 -Minfo=accel

#profiling for sequential
SNVPROF1=nvprof --cpu-profiling on -f -o
SNVPROF2=nvprof --cpu-profiling-mode top-down -i #--cpu-profiling-scope instruction -i

#could try: --cpu-profiling-scope instruction

#profiling for multicore  and GPU
NVPROF1 = nvprof -f -o
NVPROF2 = nvprof --cpu-profiling off --openacc-profiling on -i 

# NUM_ITERS=1024

######### sequential
gol_seq: gol_seq.c
	${CC_SEQ} -o gol_seq gol_seq.c

gol_seq_prof: gol_seq
	${SNVPROF1} gol_seq.prof ./gol_seq
	${SNVPROF2} gol_seq.prof


fillRand.o: fillRand.cu
	nvcc -c fillRand.cu -o fillRand.o

gol_acc: gol_acc.c fillRand.o
	${CC_ACC_M} -o gol_acc gol_acc.c fillRand.o

gol_acc_prof: gol_acc
	${NVPROF1} gol_acc.prof ./gol_acc
	${NVPROF2} gol_acc.prof


####### clean
clean:
	rm -f   gol_seq   gol_acc gol_acc.o fillRand.o *.prof
