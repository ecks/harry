%{

#include <stdint.h>

#include "json_lexer.h"
#include "debug.h"
#include "routeflow-common.h"
#include "ospf6_lsa.h"

extern int yylex();
extern void yyerror(struct ospf6_header * oh, int get_header, const char * s);

int router_id_index = 0;
u_int32_t * router_id;

int lsa_header_index = 0;
struct ospf6_lsa_header * lsa_header;
%}

%output "json_parser.c"
%defines "json_parser.h"

%parse-param {struct ospf6_header * oh}
%parse-param {int get_header}

%union {
  int ival;
}

%token<ival> T_NUMBER
%token T_OPEN_BRACE T_CLOSE_BRACE T_COMMA T_OPEN_BRACKET T_CLOSE_BRACKET
%token T_HEADER T_ROUTER_ID T_AREA_ID T_VERSION T_TYPE T_LENGTH T_CHECKSUM T_INSTANCE_ID T_RESERVED /* ospf6 header */
%token T_HELLO T_DBDESC
%token T_ROUTER_IDS
%token T_INTERFACE_ID T_PRIORITY T_OPTIONS_ZERO T_OPTIONS_ONE T_OPTIONS_TWO T_HELLO_INTERVAL T_DEAD_INTERVAL T_DROUTER T_BDROUTER /* ospf6 hello */
%token T_RESERVED_ONE T_RESERVED_TWO T_IFMTU T_BITS T_SEQNUM /* ospf6 dbdesc */
%token T_TODO
%token T_LSA_HEADERS T_AGE T_ID T_ADV_ROUTER
%token GET_HEADER GET_BODY
%start line

%%

line: 
    | line statement { printf("line parsed\n"); }
;

statement: T_OPEN_BRACE body T_CLOSE_BRACE { printf("statement parsed\n"); }
;

body: T_HEADER statement T_COMMA T_HELLO T_OPEN_BRACE hello T_CLOSE_BRACE { printf("hello body parsed\n"); }
    | T_HEADER statement T_COMMA T_DBDESC T_OPEN_BRACE dbdesc T_CLOSE_BRACE { printf("dbdesc body parsed\n"); }
    | T_HEADER statement
    | header
;

header: header_expression header_expression header_expression header_expression header_expression header_expression header_expression header_expression { printf("header parsed\n"); }
;

hello: hello_expression hello_expression hello_expression hello_expression hello_expression hello_expression hello_expression hello_expression hello_expression adjacencies_array { printf("hello message parsed\n"); }
;

dbdesc: dbdesc_expression dbdesc_expression dbdesc_expression dbdesc_expression dbdesc_expression dbdesc_expression dbdesc_expression dbdesc_expression lsa_headers_array { printf("dbdesc message parsed\n"); }
;

header_expression: T_ROUTER_ID T_NUMBER T_COMMA { if(get_header) {printf("Router ID parsed: %d\n", $2); oh->router_id = $2; } }  /* ospf6 header */
          | T_ROUTER_ID T_NUMBER { if(get_header) {oh->router_id = $2; } }
          | T_AREA_ID T_NUMBER T_COMMA { if(get_header) {printf("Area ID parsed: %d\n", $2); oh->area_id = $2; } }
          | T_AREA_ID T_NUMBER { if(get_header) {oh->area_id = $2; } }
          | T_VERSION T_NUMBER T_COMMA { if(get_header) {printf("Version parsed: %d\n", $2); oh->version = $2; } }
          | T_VERSION T_NUMBER { if(get_header) {printf("Version parsed: %d\n", $2); oh->version = $2; } }
          | T_TYPE T_NUMBER T_COMMA { if(get_header) {printf("Type parsed: %d\n", $2); oh->type = $2; } }
          | T_TYPE T_NUMBER { if(get_header) {oh->type = $2; } }
          | T_LENGTH T_NUMBER T_COMMA { if(get_header) {printf("Length parsed: %d\n", $2); oh->length = $2; } }
          | T_LENGTH T_NUMBER { if(get_header) {oh->length = $2; } }
          | T_CHECKSUM T_NUMBER T_COMMA { if(get_header) {oh->checksum = $2; } }
          | T_CHECKSUM T_NUMBER { if(get_header) {oh->checksum = $2; } }
          | T_INSTANCE_ID T_NUMBER T_COMMA { if(get_header) {oh->instance_id = $2; } }
          | T_INSTANCE_ID T_NUMBER { if(get_header) {oh->instance_id = $2; } }
          | T_RESERVED T_NUMBER T_COMMA { if(get_header) {oh->reserved = $2; } }
          | T_RESERVED T_NUMBER { if(get_header) {oh->reserved = $2; } }
;

hello_expression: T_INTERFACE_ID T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->interface_id = $2; } }  /* hello */
                | T_INTERFACE_ID T_NUMBER { if(!get_header) { HELLO(oh)->interface_id = $2; } }
                | T_PRIORITY T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->priority = $2; } }
                | T_PRIORITY T_NUMBER { if(!get_header) { HELLO(oh)->priority = $2; } }
                | T_HELLO_INTERVAL T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->hello_interval = $2; } }
                | T_HELLO_INTERVAL T_NUMBER { if(!get_header) { HELLO(oh)->hello_interval = $2; } }
                | T_DEAD_INTERVAL T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->dead_interval = $2; } }
                | T_DROUTER T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->drouter = $2; } }
                | T_DROUTER T_NUMBER { if(!get_header) { HELLO(oh)->drouter = $2; } }
                | T_BDROUTER T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->bdrouter = $2; } }
                | T_BDROUTER T_NUMBER { if(!get_header) { HELLO(oh)->bdrouter = $2; } }
                | T_OPTIONS_ZERO T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->options[0] = $2; } }
                | T_OPTIONS_ZERO T_NUMBER  { if(!get_header) { HELLO(oh)->options[0] = $2; } }
                | T_OPTIONS_ONE T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->options[1] = $2; } }
                | T_OPTIONS_ONE T_NUMBER { if(!get_header) { HELLO(oh)->options[1] = $2; } }
                | T_OPTIONS_TWO T_NUMBER T_COMMA { if(!get_header) { HELLO(oh)->options[2] = $2; } }
                | T_OPTIONS_TWO T_NUMBER { if(!get_header) { HELLO(oh)->options[2] = $2; } }
; 

dbdesc_expression: T_RESERVED_ONE T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->reserved1 = $2; } } /* dbdesc */
                 | T_RESERVED_ONE T_NUMBER { if(!get_header) { DBDESC(oh)->reserved1 = $2; } }
                 | T_RESERVED_TWO T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->reserved2 = $2; } }
                 | T_RESERVED_TWO T_NUMBER { if(!get_header) { DBDESC(oh)->reserved2 = $2; } }
                 | T_IFMTU T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->ifmtu = $2; } }
                 | T_IFMTU T_NUMBER { if(!get_header) { DBDESC(oh)->ifmtu = $2; } }
                 | T_BITS T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->bits = $2; } }
                 | T_BITS T_NUMBER { if(!get_header) { DBDESC(oh)->bits = $2; } }
                 | T_SEQNUM T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->seqnum = $2; } }
                 | T_SEQNUM T_NUMBER { if(!get_header) { DBDESC(oh)->seqnum = $2; } }
                 | T_OPTIONS_ZERO T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->options[0] = $2; } }
                 | T_OPTIONS_ZERO T_NUMBER { if(!get_header) { DBDESC(oh)->options[0] = $2; } }
                 | T_OPTIONS_ONE T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->options[1] = $2; } }
                 | T_OPTIONS_ONE T_NUMBER { if(!get_header) { DBDESC(oh)->options[1] = $2; } }
                 | T_OPTIONS_TWO T_NUMBER T_COMMA { if(!get_header) { DBDESC(oh)->options[2] = $2; } }
                 | T_OPTIONS_TWO T_NUMBER { if(!get_header) { DBDESC(oh)->options[2] = $2; } }
;

array_body: T_OPEN_BRACKET adjacencies T_CLOSE_BRACKET
          | T_OPEN_BRACKET lsa_headers T_CLOSE_BRACKET
          | T_OPEN_BRACKET T_CLOSE_BRACKET
;

adjacencies_array: T_ROUTER_IDS array_body
;

adjacencies: adjacency_body
           | adjacencies adjacency_body
;

adjacency_body: T_OPEN_BRACE adjacency T_CLOSE_BRACE T_COMMA
              | T_OPEN_BRACE adjacency T_CLOSE_BRACE
;

adjacency: T_ROUTER_ID T_NUMBER { if(!get_header) { router_id = HELLO_ROUTER_ID(oh, router_id_index); *router_id = $2; router_id_index++; } }
;

lsa_headers_array: T_LSA_HEADERS array_body
;

lsa_headers: lsa_header_body
           | lsa_headers lsa_header_body
;

lsa_header_body: T_OPEN_BRACE lsa_header T_CLOSE_BRACE T_COMMA
               | T_OPEN_BRACE lsa_header T_CLOSE_BRACE
;

lsa_header: lsa_expression lsa_expression lsa_expression lsa_expression lsa_expression lsa_expression lsa_expression { if(!get_header) { lsa_header_index++; } }
;

lsa_expression: T_AGE T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->age = $2; } }
              | T_AGE T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->age = $2; } }
              | T_TYPE T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->type = $2; } }
              | T_TYPE T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->type = $2; } }
              | T_ID T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->id = $2; } }
              | T_ID T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->id = $2; } }
              | T_ADV_ROUTER T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->adv_router = $2; } }
              | T_ADV_ROUTER T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->adv_router = $2; } }
              | T_SEQNUM T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->seqnum = $2; } }
              | T_SEQNUM T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->seqnum = $2; } }
              | T_CHECKSUM T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->checksum = $2; } }
              | T_CHECKSUM T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->checksum = $2; } }
              | T_LENGTH T_NUMBER T_COMMA { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->length = $2; } }
              | T_LENGTH T_NUMBER { if(!get_header) {lsa_header = DBDESC_LSA_HEADER(oh, lsa_header_index); lsa_header->length = $2; } }
;
%%

void json_parse_header(char * buffer, struct ospf6_header * oh)
{
  YY_BUFFER_STATE bs = yy_scan_bytes(buffer, strlen(buffer));
  yy_switch_to_buffer(bs);

  int get_header = 1;

  if(yyparse(oh, get_header))
  { 
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("Error");
    }
  }

  yy_delete_buffer(bs);
}

void json_parse_body(char * buffer, struct ospf6_header * oh)
{
  YY_BUFFER_STATE bs = yy_scan_bytes(buffer, strlen(buffer));
  yy_switch_to_buffer(bs);

  int get_header = 0;

  if(yyparse(oh, get_header))
  { 
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("Error");
    }
  }

  yy_delete_buffer(bs);

}

void yyerror(struct ospf6_header * oh, int get_header, const char * s)
{
  fprintf(stderr, "Parse error: %s\n", s);
}
