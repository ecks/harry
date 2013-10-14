#!/bin/bash

addrs=$(ip addr show dev lo | grep fcff | perl -ne 'm/inet6\s(.*)\//; print "$1\n"')
for addr in $addrs
do
  echo "Removing addr $addr"
  sudo ip addr del $addr dev lo
done
