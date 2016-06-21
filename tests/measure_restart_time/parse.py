import string

results_push_leader = []
#results_push_not_leader = []

leader = False

for i in range(2, 12)+range(13,20)+range(21,31):
    for j in range(0, 3):
        f_1 = open("sibling_"+str(j)+"_term." + 'test.'+str(i), 'r')
        f_2 = open("sibling_"+str(j)+"_first." + 'test.'+str(i), 'r')

        l1 = f_1.readline()
        l2 = f_2.readline()

        while l1 and l2:
          print l1
          print l2
        
          start_term_index = string.index(l1, "[")
          end_term_index = string.index(l1, "]")
          time_start_push = float(l1[start_term_index + 1:end_term_index - 1])
          if "true" in l1:
              print "leader"
              leader = True
          else:
              print "not leader"
              leader = False

          start_first_index = string.index(l2, "[")
          end_first_index = string.index(l2, "]")
          time_end_push = float(l2[start_first_index + 1:end_first_index - 1])

          result_push = (time_end_push - time_start_push)
          print result_push
#          if leader:
          results_push_leader.append(result_push)
#          else:
#            results_push_not_leader.append(result_push)

          l1 = f_1.readline()
          l2 = f_2.readline()

        f_1.close()
        f_2.close()

    print i
    results_push_leader.sort()
#    results_push_not_leader.sort()
    if len(results_push_leader) != 0:
        print "Leader"
        print "min: " + str(results_push_leader[0])
        print "max: " + str(results_push_leader[-1])
        print "avg: " + str(sum(results_push_leader) / (float(len(results_push_leader))))

#    if len(results_push_not_leader) != 0:
#        print "Non Leader"
#        print "min: " + str(results_push_not_leader[0])
#        print "max: " + str(results_push_not_leader[-1])
#        print "avg: " + str(sum(results_push_not_leader) / (float(len(results_push_not_leader))))

