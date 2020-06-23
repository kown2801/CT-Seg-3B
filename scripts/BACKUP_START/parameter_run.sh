#SBATCH --ntasks-per-node=48
#SBATCH --nodes=2
#SBATCH --mem-per-cpu=1G
#SBATCH --account=rrg-tremblay-ac

module reset
module load gcc/7.3.0 nixpkgs boost scipy-stack openmpi/3.1.2
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${HOME}/local/lib:${HOME}/local/lib/openblas
