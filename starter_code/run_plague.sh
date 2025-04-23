#!/bin/bash
#SBATCH --job-name=plague-sim
#SBATCH --output=plague_%j.out
#SBATCH --error=plague_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=2
#SBATCH --time=00:03:00
#SBATCH --partition=el8
#SBATCH --gres=gpu:4
#SBATCH --exclusive 
#SBATCH --mail-type=END,FAIL
#SBATCH --mail-user=kruppd@rpi.edu

echo "Running on $(hostname) at $(date)"
echo "ROSS path: $ROSS_DIR"

# Load modules
module purge
module load xl_r
module load spectrum-mpi
module load cuda/11.2

# Set required env vars for Spectrum MPI
export OMPI_MCA_btl=^openib
export OMPI_MCA_orte_base_help_aggregate=0  # optional: shows all MPI error messages

# Run the simulation (no --mpi needed with Spectrum)
mpirun -np 2 ./plagueInc --synch=3 --end=1000