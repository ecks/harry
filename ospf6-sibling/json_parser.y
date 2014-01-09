%{

#include <stdint.h>

#include "json_lexer.h"
#include "routeflow-common.h"

extern int yylex();
extern void yyerror(struct ospf6_header * oh, const char * s);

%}

%output "json_parser.c"
%defines "json_parser.h"

%parse-param {struct ospf6_header * oh}

%union {
  int ival;
}

%token<ival> T_NUMBER
%token T_OPEN_BRACE T_CLOSE_BRACE T_COMMA T_OPEN_BRACKET T_CLOSE_BRACKET
%token T_HEADER T_ROUTER_ID T_AREA_ID T_VERSION T_TYPE T_LENGTH T_CHECKSUM T_INSTANCE_ID T_RESERVED
%token T_MSG
%token T_ROUTER_IDS
%token T_TODO

%start line

%%

line: 
    | line statement { printf("line parsed\n"); }
;

statement: T_OPEN_BRACE body T_CLOSE_BRACE { printf("statement parsed\n"); }
;

body: T_HEADER statement T_COMMA T_MSG statement { printf("body parsed\n"); }
    | T_HEADER statement
    | header
    | msg
;

header: expression
      | expression expression expression expression expression expression expression expression { printf("header parsed\n"); }
;

msg: expression expression expression expression expression expression expression expression expression adjacencies_array { printf("message parsed\n"); }
;

expression: T_ROUTER_ID T_NUMBER T_COMMA { printf("Router ID parsed: %d\n", $2); oh->router_id = $2; }
          | T_ROUTER_ID T_NUMBER { oh->router_id = $2; }
          | T_AREA_ID T_NUMBER T_COMMA { printf("Area ID parsed: %d\n", $2); oh->area_id = $2; }
          | T_AREA_ID T_NUMBER { oh->area_id = $2; }
          | T_VERSION T_NUMBER T_COMMA { printf("Version parsed: %d\n", $2); oh->version = $2; }
          | T_VERSION T_NUMBER { printf("Version parsed: %d\n", $2); oh->version = $2; }
          | T_TYPE T_NUMBER T_COMMA { printf("Type parsed: %d\n", $2); oh->type = $2; }
          | T_TYPE T_NUMBER { oh->type = $2; }
          | T_LENGTH T_NUMBER T_COMMA { printf("Length parsed: %d\n", $2); oh->length = $2; }
          | T_LENGTH T_NUMBER { oh->length = $2; }
          | T_CHECKSUM T_NUMBER T_COMMA { oh->checksum = $2; }
          | T_CHECKSUM T_NUMBER { oh->checksum = $2; }
          | T_INSTANCE_ID T_NUMBER T_COMMA { oh->instance_id = $2; }
          | T_INSTANCE_ID T_NUMBER { oh->instance_id = $2; }
          | T_RESERVED T_NUMBER T_COMMA { oh->reserved = $2; }
          | T_RESERVED T_NUMBER { oh->reserved = $2; }
          | T_TODO T_NUMBER T_COMMA /* do nothing for now */
          | T_TODO T_NUMBER         /* do nothing for now */
;

adjacencies_array: T_ROUTER_IDS adjacencies_body
;

adjacencies_body: T_OPEN_BRACKET adjacencies T_CLOSE_BRACKET
;

adjacencies: adjacency
           | adjacencies adjacency
;

adjacency: T_ROUTER_ID T_NUMBER T_COMMA /* do nothing for now */
         | T_ROUTER_ID T_NUMBER         /* do nothing for now */
;
%%

void json_parse(char * buffer, struct ospf6_header * oh)
{
  YY_BUFFER_STATE bs = yy_scan_bytes(buffer, strlen(buffer));
  yy_switch_to_buffer(bs);

  int arg;

  if(yyparse(oh))
  { 
    printf("Error\n");
  }

  yy_delete_buffer(bs);
}

void yyerror(struct ospf6_header * oh, const char * s)
{
  fprintf(stderr, "Parse error: %s\n", s);
}
