#!/bin/bash

if [ $# -eq 1 ]
then
  i=$1
else
  i=0
fi

sudo grep "Terminate" /var/log/zebralite/ospf6_sibling_0.log > ~/zebralite/tests/measure_restart_time/sibling_0_term.test.$i
sudo grep "Terminate" /var/log/zebralite/ospf6_sibling_1.log > ~/zebralite/tests/measure_restart_time/sibling_1_term.test.$i
sudo grep "Terminate" /var/log/zebralite/ospf6_sibling_2.log > ~/zebralite/tests/measure_restart_time/sibling_2_term.test.$i
sudo grep "About to send first msg" /var/log/zebralite/ospf6_sibling_0.log > ~/zebralite/tests/measure_restart_time/sibling_0_first.test.$i
sudo grep "About to send first msg" /var/log/zebralite/ospf6_sibling_1.log > ~/zebralite/tests/measure_restart_time/sibling_1_first.test.$i
sudo grep "About to send first msg" /var/log/zebralite/ospf6_sibling_2.log > ~/zebralite/tests/measure_restart_time/sibling_2_first.test.$i
