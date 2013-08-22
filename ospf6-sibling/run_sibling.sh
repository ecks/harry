#!/bin/bash
#
# to run type:
# $ qsub -V -t 0-2 -d . run_sibling.sh
#
#PBS -N SIBLING_JOB
#
CONF_FILE_ID="$PBS_ARRAYID"
./ospf6-sibling -i eth3 -f /etc/zebralite/ospf6_sibling_$CONF_FILE_ID.conf
