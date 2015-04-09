#!/usr/bin/python

import sys, getopt

def get_job(num):
    jobs = []
    for line in sys.stdin:
      if "SIBLING_JOB" in line:
        jobs.append((line.split()[0]).split('.')[0])

    return jobs[num]

def get_all_jobs():
    jobs = []
    for line in sys.stdin:
      if "SIBLING_JOB" in line:
        jobs.append((line.split()[0]).split('.')[0])

    return jobs


def job_compare_group(job_a_id, job_b_id):
    job_a_group_id = job_a_id.split('-')[0]
    job_b_group_id = job_b_id.split('-')[0]
    if job_a_group_id == job_b_group_id:
        return "0"
    else:
        # if different group sibling has started, check to see what it is
        return "1"
try:
    optlist, args = getopt.getopt(sys.argv[1:], "jc")
except:
    print "Arguments required"
    sys.exit()

for o,a in optlist:
    if o == "-j":
      print get_job(int(args[0]))
    elif o == "-c":
        jobs = get_all_jobs()

        job_0 = int(args[0])
        job_1 = int(args[1])

        if len(jobs) <= (job_0 or job_1):
          print "comp set 0"
        else:
          print "comp set " + job_compare_group(jobs[job_0], jobs[job_1])
