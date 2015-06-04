#ifndef OSPF6_SPF_H
#define OSPF6_SPF_H 

/* Transit Vertex */
struct ospf6_vertex
{
  /* type of this vertex */
  u_int8_t type;

  /* Vertex Identifier */
  struct prefix vertex_id;

  /* Identifier String */
  char name[128];

  /* Associated Area */
  struct ospf6_area *area;

  /* Associated LSA */
  struct ospf6_lsa *lsa;

  /* Distance from Root (i.e. Cost) */
  u_int32_t cost;

  /* Router hops to this node */
  u_char hops;

  /* nexthops to this node */
  struct ospf6_nexthop nexthop[OSPF6_MULTI_PATH_LIMIT];

  /* capability bits */
  u_char capability;
                      
  /* Optional capabilities */
  u_char options[3];

  /* For tree display */
  struct ospf6_vertex *parent;
  struct alt_list * child_list;
};

#define OSPF6_VERTEX_TYPE_ROUTER  0x01
#define OSPF6_VERTEX_TYPE_NETWORK 0x02
#define VERTEX_IS_TYPE(t, v) \
  ((v)->type == OSPF6_VERTEX_TYPE_ ## t ? 1 : 0)

extern void ospf6_spf_schedule(struct ospf6_area * oa, unsigned int hostnum);

#endif
