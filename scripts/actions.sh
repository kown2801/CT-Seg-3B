#!/bin/bash
#SBATCH --time=0-00:10:00
#SBATCH --job-name="actions"
#SBATCH --ntasks=1
#SBATCH --mem-per-cpu=1G
#SBATCH --account=rrg-tremblay-ac
#SBATCH --error=occupation-log.out
#SBATCH --output=occupation-log.out

source ./export.sh ; python actions.py $@
