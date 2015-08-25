#!/bin/bash

routes=$(ip -6 route | grep zebra | perl -ne 'm/^(.*) via/; print "$1\n"')
for route in $routes
do
  echo "Removing route $route"
  sudo ip -6 route del $route
done
