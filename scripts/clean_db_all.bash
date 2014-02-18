#!/bin/bash

for i in 0 1 2
do
  /home/hasenov/zebralite/scripts/clean_db -b $i 
done
