for i in $(ls /var/log/zebralite/ | grep sibling)
do
  sibling_num=$(echo $i | grep sibling | awk -F '[_.]' '{print $3}') 
  restart_exist=$(sudo cat /var/log/zebralite/$i | grep "starting in restart"  | awk '{print $11}' | tail -n 1 | wc -l)
  if [ $restart_exist -eq "1" ]
  then
    # only need the pid of last restarted app
    pid=$(sudo cat /var/log/zebralite/$i | grep "starting in restart"  | awk '{print $11}' | tail -n 1)
    echo $sibling_num
    echo $pid
    echo "restart $sibling_num $pid" | nc localhost 9999
  fi
done
