#!/bin/bash

if [ $# -eq 1 ]
then
  num_desired_checkpoints=$1
else
  echo "Provide desired checkpoints necessary before killing sibling"
  exit
fi

for i in {0..29}
do
  ssh hasenov@10.0.0.141 'sudo rm /var/log/zebralite/ospf6_sibling_*.log'

  ssh hasenov@10.0.0.141 'tmux send -t zebra.0 "sudo /home/hasenov/public-sis-is/sis-is/quagga/zebra/zebra -f /etc/quagga/zebra.conf" ENTER'

  sleep 5

  ssh hasenov@10.0.0.141 'tmux send -t sisis.0 "sudo /home/hasenov/public-sis-is/sis-is/quagga/sisisd/sisisd -f /home/hasenov/public-sis-is/sis-is/quagga/sisisd/sisisd.conf.sample"  ENTER'

  sleep 5

  # start up controller
  #ssh hasenov@10.0.0.141 'sudo LD_LIBRARY_PATH=/home/hasenov/etcd-api /home/hasenov/zebralite/controller/controller -f /etc/zebralite/controller.conf > /home/hasenov/zebralite/controller/ctrl_output &'
  #ssh hasenov@10.0.0.141 'qsub -d /home/hasenov/zebralite/controller run_ctrl.sh'
  ssh hasenov@10.0.0.141 'tmux send -t devel.0 "sudo LD_LIBRARY_PATH=/home/hasenov/etcd-api /home/hasenov/zebralite/controller/controller -f /etc/zebralite/controller.conf" ENTER'

  sleep 5

  # start up zebralite
  #sudo /home/hasenov/zebralite/zl/zebralite -f /etc/zebralite/zebralite.conf > zl_output &
  tmux send -t zebralite.0 'sudo /home/hasenov/zebralite/zl/zebralite -f /etc/zebralite/zebralite.conf' ENTER

  sleep 5

  # start up punter
  tmux send -t punter.1 'sudo /home/hasenov/zebralite/punter/punter fe80::20c:29ff:fe12:f995' ENTER

  sleep 5

  # start up external ospf6 process
  ssh hasenov@10.100.4.1 'tmux send -t ospf6d.0 "sudo ~/public-sis-is/sis-is/quagga/ospf6d/ospf6d -f /etc/quagga/ospf6d.conf" ENTER' 

  # start up siblings
  echo "I am about to start the siblings"
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qsub -V -t 0-2 -d /home/hasenov/zebralite/ospf6-sibling run_sibling.sh" ENTER'
#  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "sudo /home/hasenov/zebralite/ospf6-sibling/ospf6-sibling -f /etc/zebralite/ospf6_sibling_\$ID.conf" ENTER'

  num_checkpointed=`echo "num" | nc 10.0.0.141 9999`

  echo $num_checkpointed

  while [ $num_checkpointed -lt $num_desired_checkpoints ]
  do
    # wait until we have enough messages checkpointed
    echo "Not enough checkpoint messages: $num_checkpointed"
    sleep 1
    num_checkpointed=`echo "num" | nc 10.0.0.141 9999`
  done

  # kill sibling 
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qstat | python ~/zebralite/scripts/parse_qstat.py -j 2 | xargs qdel" ENTER'

  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qstat | python ~/zebralite/scripts/parse_qstat.py -c 0 2 | nc localhost 9999" ENTER'
  restarted_sibling_started=`echo "comp get" | nc 10.0.0.141 9999`

  while [ $restarted_sibling_started -lt "1" ]
  do
    echo "Restarted sibling has not started yet: $restarted_sibling_started"
    sleep 1
    ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qstat | python ~/zebralite/scripts/parse_qstat.py -c 0 2 | nc localhost 9999" ENTER'
    restarted_sibling_started=`echo "comp get" | nc 10.0.0.141 9999`
  done

  # wait until the restarted sibling is able to restart
  sleep 10

  ssh hasenov@10.0.0.141 'sudo killall controller'
  sudo killall zebralite
  sudo killall punter
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qdel all" ENTER'
#  ssh hasenov@10.0.0.141 'sudo killall ospf6-sibling'
  ssh hasenov@10.100.4.1 'sudo killall lt-ospf6d'

  # set the I varialble
  echo I=$i | ssh hasenov@10.0.0.141 'cat > /tmp/i'
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "source /tmp/i" ENTER'
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "sudo cat /var/log/zebralite/ospf6_sibling_2.log | grep measurement > ~/zebralite/tests/\$I.test" ENTER'
#  cat /home/hasenov/zebralite/zl/zl_output | grep measurement > $i.test

  ssh hasenov@10.0.0.141 'sudo ~/zebralite/scripts/rm_sisis_addr.bash'
  ssh hasenov@10.0.0.141 '~/zebralite/scripts/reset_etcd.bash'
  ssh hasenov@10.0.0.141 '~/zebralite/scripts/clean_db_all.bash'

  # reset the server
  echo "comp set 0" | nc 10.0.0.141 9999

  ssh hasenov@10.0.0.141 'sudo killall lt-zebra'
  ssh hasenov@10.0.0.141 'sudo killall lt-sisisd'
 done
