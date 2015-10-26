for i in $(ls /var/log/zebralite/ | grep sibling)
do
  sibling_num=$(echo $i | grep sibling | awk -F '[_.]' '{print $3}') 
  pid=$(sudo cat /var/log/zebralite/$i | grep "normal" | awk '{print $11}')
  leader=$(sudo cat /var/log/zebralite/$i | grep "am the oldest"  | wc -l)
  echo $sibling_num
  echo $pid
  echo $leader
  echo "sibling $sibling_num $pid $leader" | nc localhost 9999
done
