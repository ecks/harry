total="0"

for i in {1..30}
do
  curr=$(cat $i.test)
  echo $curr
  total=$(echo "$curr+$total"|bc)
  echo $total
done
