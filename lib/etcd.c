#include "stdio.h"
#include "etcd-api.h"
 
#define ETCD_PROXY "127.0.0.1:8080"

int etcd_get_unique_xid()
{
  etcd_session sess; 
  char * key = "counter";
  int xid;


  sess = etcd_open_str(ETCD_PROXY);
  if(!sess) 
  {
    fprintf(stderr, "etcd_open failed\n");
    return -1;
  }

  if((xid = etcd_get_and_set(sess, key)) < 0)
  {
    // retry again until successfull 
    etcd_get_unique_xid();
  }

  return xid;
}

// return: xid to use next
int etcd_get_and_set(etcd_session sess, char * key)
{
  char * value;
  int curr_xid;
  unsigned long ttl_num = 0; // dont need ttl for now
  char new_value[10]; // array of size 10 in char

  value = etcd_get(sess, key);
  if(!value)
  {
    fprintf(stderr, "etcd_get failed\n");
    return -1;
  }

  printf("got value: %s\n", value);

  curr_xid = (int)strtol(value, NULL, 10);
  curr_xid++; // increment one more from the currently read value

  sprintf(new_value, "%i", curr_xid);

  if(etcd_set(sess, key, new_value, value, ttl_num) != ETCD_OK)
  {
    fprintf(stderr, "etcd_set failed\n");
    return -1;
  }

  // successful at acquiring a unique xid
  return curr_xid;
}
