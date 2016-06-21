#!/bin/bash

if [ $# -eq 1 ]
then
  i=$1
else
  i=0
fi

sudo grep "Attempting to send the message" /var/log/zebralite/ospf6_sibling_0.log > ~/zebralite/tests/measure_voter_responsiveness/sibling_0_term.test.$i
sudo grep "Attempting to send the message" /var/log/zebralite/ospf6_sibling_1.log > ~/zebralite/tests/measure_voter_responsiveness/sibling_1_term.test.$i
sudo grep "Attempting to send the message" /var/log/zebralite/ospf6_sibling_2.log > ~/zebralite/tests/measure_voter_responsiveness/sibling_2_term.test.$i
sudo grep "forward ospf6 packet" /var/log/zebralite/controller.log > ~/zebralite/tests/measure_voter_responsiveness/controller.test.$i
