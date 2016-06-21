import string

results = []
#results_push_not_leader = []

leader = False

f_ss = []

for i in range(1, 31):
       f_c = open("controller.test."+str(i), 'r')

       for j in range(0, 3):
         f_s = open("sibling_"+str(j)+"_term.test."+str(i), 'r')
         f_ss.append(f_s)

       l_c = f_c.readline()
       l_ss = [f_s.readline() for f_s in f_ss]

       got_input = False
       while l_c and all(l_ss) and not got_input:
         print l_c
         print l_ss
         for l_s in l_ss:
           print l_s
        
         time_starts = []
         for l_s in l_ss:
             start_first_index = string.index(l_s, "[")
             end_first_index = string.index(l_s, "]")
             time_start = float(l_s[start_first_index + 1:end_first_index - 1].replace(',','.'))
             time_starts.append(time_start)
         time_start = min(time_starts)

       
         if "forward ospf6 packet: sibling" in l_c:
           start_term_index = string.index(l_c, "[")
           end_term_index = string.index(l_c, "]")
           time_end = float(l_c[start_term_index + 1:end_term_index - 1].replace(',','.'))

           result = (time_end - time_start)
           print result
           results.append(result)

           got_input = True
#           l_c = f_c.readline()
#           l_ss = [l_s.readline() for l_s in f_ss]
         else:
           l_c = f_c.readline()


       f_c.close()
       [f_s.close() for f_s in f_ss]
       del f_ss[:]

       print i
results.sort()
print "min: " + str(results[0])
print "max: " + str(results[-1])
print "avg: " + str(sum(results) / (float(len(results))))
