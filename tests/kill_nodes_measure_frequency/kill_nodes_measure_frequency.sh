#!/bin/bash

for i in {1..1}
do
  ssh hasenov@hctrl-hsa01 'tmux send -t zebra.0 "cd /home/hasenov/public-sis-is/sis-is/quagga/zebra; sudo ./zebra -f /etc/quagga/zebra.conf" ENTER'

  sleep 5

  ssh hasenov@hctrl-hsa01 'tmux send -t sisis.0 "cd /home/hasenov/public-sis-is/sis-is/quagga/sisisd/; sudo ./sisisd -f sisisd.conf.sample"  ENTER'

  sleep 5

  # start up controller
  ssh hasenov@hctrl-hsa01 'tmux send -t devel.0 "cd /home/hasenov/zebralite/controller/; sudo ./controller -f /etc/zebralite/controller.conf" ENTER'

  sleep 5

  # start up zebralite
  tmux send -t zebralite.0 'cd /home/hasenov/zebralite/zl/; sudo ./zebralite -f /etc/zebralite/zebralite.conf > zl_output' ENTER

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

  read input

  sleep 5

  # kill a sibling 
  echo "kill" | nc hctrl-hsa01 9999

#  read input

  cur_xid=$(cat /home/hasenov/zebralite/zl/zl_output | grep "Forwarding OSPF6 traffic" | tail -n 1 | awk '{print $9}')
  echo $cur_xid
  xid_at_siblings=$(echo "egress_xid" | nc hctrl-hsa01 9999)
  echo $xid_at_siblings

  xid_diff=$(echo "a=$cur_xid-$xid_at_siblings/1;if(0>a)a*=-1;a"|bc)

  sleep_period=10

  while [ $xid_diff -lt 2 ]
  do
    echo "We are still operational with sleep period $sleep_period"

    # repopulate the sibling data
    ssh hasenov@hctrl-hsa01 'tmux send -t parse_log.0 "cd /home/hasenov/zebralite/scripts/; ./parse_sibling_log_restart.bash" ENTER'

    sleep $sleep_period

    # kill a sibling 
    echo "kill" | nc hctrl-hsa01 9999

    cur_xid=$(cat /home/hasenov/zebralite/zl/zl_output | grep "Forwarding OSPF6 traffic" | tail -n 1 | awk '{print $9}')
    echo $cur_xid
    xid_at_siblings=$(echo "egress_xid" | nc hctrl-hsa01 9999)
    echo $xid_at_siblings

    xid_diff=$(echo "a=$cur_xid-$xid_at_siblings/1;if(0>a)a*=-1;a"|bc)

    if [ $sleep_period -gt 1 ]
    then
      sleep_period=$(echo "$sleep_period-1" | bc)
    fi
  done

  echo "stalled by $xid_diff"

#  echo $sleep_period >  $i.test
  read input

  # delete sibling 
  ssh hasenov@hctrl-hsa01 'tmux send -t sblngs.0 "cd /home/hasenov/zebralite/marathon/; ./delete.sh" ENTER'

  # kill event subscriber
  ssh hasenov@hctrl-hsa01 'sudo killall python'

  ssh hasenov@hctrl-hsa01 'sudo killall controller'
  sudo killall zebralite

  ssh hasenov@hctrl-hsa01 'sudo killall lt-zebra'
  ssh hasenov@hctrl-hsa01 'sudo killall lt-sisisd'
  ssh hasenov@hctrl-hsa01 'sudo rm /home/hasenov/zebralite/marathon/out'
  rm /home/hasenov/zebralite/zl/zl_output

  # reset the sisis addresses, clear the database
  ssh hasenov@hctrl-hsa01 'tmux send -t devel.0 "reset" ENTER'

  # reset the kill server
  echo "reset" | nc hctrl-hsa01 9999
done
