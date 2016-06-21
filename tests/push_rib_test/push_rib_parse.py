#!/usr/bin/python

import string

results_push = []

for i in range(1,21) + range(25, 27):
  f_1 = open(str(i) + '.test', 'r')

  time_start_push = -1.0
  time_end_push = -1.0

  for line in f_1:
    if "Started measurement" in line:
      start_reg_index = string.index(line, "[")
      end_reg_index = string.index(line, "]")
      time_start_push = float(line[start_reg_index + 1:end_reg_index - 1])
      print "start push " + str(time_start_push) + " " + str(i)
    if "Stopped measurement" in line:
      start_unreg_index = string.index(line, "[")
      end_unreg_index = string.index(line, "]")
      time_end_push = float(line[start_unreg_index + 1:end_unreg_index - 1])
      print "end push " + str(time_end_push) + " " + str(i)

 
  if time_start_push == -1.0 or time_end_push == -1.0:
    print "error"
  else:
    result_push = (time_end_push - time_start_push)
    results_push.append(result_push)

results_push.sort()
print "min: " + str(results_push[0])
print "max: " + str(results_push[-1])
print "avg: " + str(sum(results_push) / (float(len(results_push))))
