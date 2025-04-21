#!/bin/bash
#SBATCH --job-name=plague-sim
#SBATCH --output=plague5.out
#SBATCH --error=plague5.err
#SBATCH --nodes=1
#SBATCH --ntasks=2
#SBATCH --ntasks-per-node=2
#SBATCH --partition=el8
#SBATCH --time=00:15:00

module purge
module load xl_r spectrum-mpi gcc

mpirun -np $SLURM_NTASKS ./plagueInc --synch=1 --end=1000