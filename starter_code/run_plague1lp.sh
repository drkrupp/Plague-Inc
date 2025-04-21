#!/bin/bash
#SBATCH --job-name=plague-sim
#SBATCH --output=plague_%j.out
#SBATCH --error=plague_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=2
#SBATCH --time=00:30:00
#SBATCH --partition=el8
#SBATCH --gres=gpu:4

echo "Running on $(hostname) at $(date)"
echo "ROSS path: $ROSS_DIR"

# Load necessary modules
module purge
module load xl_r
module load spectrum-mpi
module load cuda/11.2

# Spectrum MPI settings
export OMPI_MCA_btl=^openib
export OMPI_MCA_orte_base_help_aggregate=0

# Run the ROSS simulation (MPI launch)
mpirun -np 2 ./plague1lp --synch=3 --end=1000
