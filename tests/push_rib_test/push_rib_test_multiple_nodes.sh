#!/bin/bash

for i in {24..30}
do
  route_check=`sudo ip -6 route | grep "2001:db8:beef:2::/64" | wc -l` 
  if [ $route_check -eq 1 ] 
  then
    sudo ip -6 route del "2001:db8:beef:2::/64"
  fi  

  default_route_check=`sudo ip -6 route | grep "2001:db8:beef:2::/64" | wc -l` 
  if [ $default_route_check -eq 1 ] 
  then 
    sudo ip -6 route del default
  fi  

  routes=zebra_routes(k+1)
  sed 's/ipv6 nd prefix 2001:db8:beef:2::\/64/ipv6 nd prefix 2001:db8:beef:2::\/64\n $routes/g' /etc/quagga/zebra.conf

  ssh hasenov@10.0.0.141 'sudo ~/zebralite/scripts/rm_sisis_addr.bash'
  ssh hasenov@10.0.0.141 '~/zebralite/scripts/reset_etcd.bash'
  ssh hasenov@10.0.0.141 '~/zebralite/scripts/clean_db_all.bash'

  # start up controller
  #ssh hasenov@10.0.0.141 'sudo LD_LIBRARY_PATH=/home/hasenov/etcd-api /home/hasenov/zebralite/controller/controller -f /etc/zebralite/controller.conf > /home/hasenov/zebralite/controller/ctrl_output &'
  #ssh hasenov@10.0.0.141 'qsub -d /home/hasenov/zebralite/controller run_ctrl.sh'
  ssh hasenov@10.0.0.141 'tmux send -t devel.0 "sudo LD_LIBRARY_PATH=/home/hasenov/etcd-api /home/hasenov/zebralite/controller/controller -f /etc/zebralite/controller.conf" ENTER'

  sleep 5

  # start up zebralite
  #sudo /home/hasenov/zebralite/zl/zebralite -f /etc/zebralite/zebralite.conf > zl_output &
  tmux send -t zebralite.0 'sudo /home/hasenov/zebralite/zl/zebralite -f /etc/zebralite/zebralite.conf > zl_output' ENTER

  sleep 5

  # start up punter
  tmux send -t punter.0 'sudo /home/hasenov/zebralite/punter/punter fe80::20c:29ff:fe12:f995' ENTER

  sleep 5

  # start up external ospf6 process
  ssh hasenov@10.100.4.1 'tmux send -t ospf6d.0 "sudo ~/public-sis-is/sis-is/quagga/ospf6d/ospf6d -f /etc/quagga/ospf6d.conf" ENTER' 

  # start up siblings
  echo "I am about to start the siblings"
  #ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qsub -V -t 0-2 -d /home/hasenov/zebralite/ospf6-sibling run_sibling.sh" ENTER'
  ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "sudo /home/hasenov/zebralite/ospf6-sibling/ospf6-sibling -f /etc/zebralite/ospf6_sibling_\$ID.conf" ENTER'

  # wait until the route is installed
  route_check=`sudo ip -6 route | grep "2001:db8:beef:2::/64" | wc -l`
  while [ $route_check -lt 1 ]
  do
    route_check=`sudo ip -6 route | grep "2001:db8:beef:2::/64" | wc -l`
    sleep 5
  done

  ssh hasenov@10.0.0.141 'sudo killall controller'
  sudo killall zebralite
  sudo killall punter
  #ssh hasenov@10.0.0.141 'tmux send -t sblngs.0 "qdel all" ENTER'
  ssh hasenov@10.0.0.141 'sudo killall ospf6-sibling'
  ssh hasenov@10.100.4.1 'sudo killall lt-ospf6d'

  cat /home/hasenov/zebralite/zl/zl_output | grep measurement > $i.test
done

zebra_routes($n)
{
  "ipv6 address 2001:db8:beef:$n::1/64\n ipv6 nd prefix 2001:db8:beef:$n::/64"
}
