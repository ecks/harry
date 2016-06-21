#!/bin/bash

if [ $# -eq 1 ]
then
  num_desired_checkpoints=$1
elif [ $# -eq 2 ]
then
  num_desired_checkpoints=$1
  voter_type=$2
  echo $voter_type
  echo $2
else
  echo "Provide desired checkpoints necessary before killing sibling"
  exit
fi

for i in {1..30}
do
  ssh hasenov@hctrl-hsa01 'tmux send -t zebra.0 "cd /home/hasenov/public-sis-is/sis-is/quagga/zebra; sudo ./zebra -f /etc/quagga/zebra.conf" ENTER'

  sleep 5

  ssh hasenov@hctrl-hsa01 'tmux send -t sisis.0 "cd /home/hasenov/public-sis-is/sis-is/quagga/sisisd/; sudo ./sisisd -f sisisd.conf.sample"  ENTER'

  sleep 5

  # debug controller temporarily
#  echo "start the controller manually"
#  read input
  # start up controller
  ssh hasenov@hctrl-hsa01 'tmux send -t devel.0 "cd /home/hasenov/zebralite/controller/; sudo ./controller -f /etc/zebralite/controller.conf" ENTER'

  sleep 5

  # start up zebralite
  tmux send -t zebralite.0 'cd /home/hasenov/zebralite/zl/; sudo ./zebralite -f /etc/zebralite/zebralite.conf > zl_output' ENTER

  sleep 5

  # start up punter
  tmux send -t punter.0 'cd /home/hasenov/punter; sudo ./punter fe80::20c:29ff:fe12:f995' ENTER

  sleep 5

  # start up event subscriber
  ssh hasenov@hctrl-hsa01 'tmux send -t event_subscriber.0 "cd /home/hasenov/zebralite/marathon/; sudo python event_subscriber.py 8081 > out" ENTER'

  sleep 2

  # start up siblings
  ssh hasenov@hctrl-hsa01 'tmux send -t sblngs.0 "cd /home/hasenov/zebralite/marathon/; ./post.sh" ENTER'

  sleep 15
  
  # populate the sibling data
  ssh hasenov@hctrl-hsa01 'tmux send -t parse_log.0 "cd /home/hasenov/zebralite/scripts/; ./parse_sibling_log_normal.bash" ENTER'

  echo "done with initialization"

  sleep 5

  if [ $num_desired_checkpoints -eq 0 ]
  then
    # dont bother starting external ospf neighbor
    echo "no external ospf6 neighbor"
  else
    # start up external ospf6 process
    ssh hasenov@hext-hsa01 'tmux send -t ospf6d.0 "sudo ~/public-sis-is/sis-is/quagga/ospf6d/ospf6d -f /etc/quagga/ospf6d.conf" ENTER'
  fi

  sleep 5

  num_checkpointed=`echo "num" | nc hctrl-hsa01 9998`

  echo $num_checkpointed

  while [ $num_checkpointed -lt $num_desired_checkpoints ]
  do
    # wait until we have enough messages checkpointed
    echo "Not enough checkpoint messages: $num_checkpointed"
    sleep 1
    num_checkpointed=`echo "num" | nc hctrl-hsa01 9998`
  done

  if [ $num_desired_checkpoints -eq 0 ]
  then
    # no ospf6 neighbor to kill
    echo "no external ospf6 neighbor"
  else
    # now we are going to kill the external ospf6 process
    ssh hasenov@hext-hsa01 'sudo killall lt-ospf6d'
  fi

  sleep 5

  echo $voter_type

  if [ $voter_type == "any" ]
  then
    echo "waiting on forwarding"
   # wait until first message is received from zebralite
    forwarding=$(cat /home/hasenov/zebralite/zl/zl_output | grep "Forwarding OSPF6 traffic with length" | wc -l)
    while [ $forwarding -lt 1 ]
    do
      forwarding=$(cat /home/hasenov/zebralite/zl/zl_output | grep "Forwarding OSPF6 traffic with length" | wc -l)
    done
  fi


  # dont kill any siblings 
  sleep_period=10

#  iterations=1

#  while [ $iterations -gt 0 ]
#  do
#    echo "We are at iteration $iterations"

    # repopulate the sibling data
#    ssh hasenov@hctrl-hsa01 'tmux send -t parse_log.0 "cd /home/hasenov/zebralite/scripts/; ./parse_sibling_log_restart.bash" ENTER'

#    sleep $sleep_period

    # kill a sibling 
#    able_to_kill=$(echo "kill" | nc hctrl-hsa01 9999)

#    echo "able to kill: $able_to_kill"

#    iterations=$(echo "$iterations-1" | bc)

#  done

  # set the i variable
  echo I=$i | ssh hasenov@hctrl-hsa01 'cat > /tmp/i' 
  ssh hasenov@hctrl-hsa01 'tmux send -t sblngs.0 "source /tmp/i" ENTER'
  ssh hasenov@hctrl-hsa01 'tmux send -t sblngs.0 "sudo /home/hasenov/zebralite/tests/measure_voter_responsiveness/filter_logs.bash \$I" ENTER'
  
#  echo $sleep_period >  $i.test
#  read input

  # delete sibling 
  ssh hasenov@hctrl-hsa01 'tmux send -t sblngs.0 "cd /home/hasenov/zebralite/marathon/; ./delete.sh" ENTER'

  # kill event subscriber
  ssh hasenov@hctrl-hsa01 'sudo killall python'
  ssh hasenov@hctrl-hsa01 'cd /home/hasenov/zebralite/marathon/; rm out'

  ssh hasenov@hctrl-hsa01 'sudo killall controller'
  sudo killall zebralite
  sudo killall punter

  ssh hasenov@hctrl-hsa01 'sudo killall lt-zebra'
  ssh hasenov@hctrl-hsa01 'sudo killall lt-sisisd'
  ssh hasenov@hctrl-hsa01 'sudo rm /home/hasenov/zebralite/marathon/out'
  rm /home/hasenov/zebralite/zl/zl_output

  # reset the sisis addresses, clear the database
  ssh hasenov@hctrl-hsa01 'tmux send -t devel.0 "reset" ENTER'

  # reset the kill server
  echo "reset" | nc hctrl-hsa01 9999
done
