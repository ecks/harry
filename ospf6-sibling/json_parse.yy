%x begin_json 
%x router_id_state
%x area_id_state
%x version_state
%x type_state
%x length_state
%x checksum_state
%x instance_id_state
%x reserved_state
%{
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "routeflow-common.h"

  enum yytokentype {
    ROUTER_ID = 258,
    AREA_ID = 259,
    VERSION = 260,
    TYPE = 261,
    LENGTH = 262,
    CHECKSUM = 263,
    INSTANCE_ID = 264,
    RESERVED = 265
  };
  int yylval;
%}

%%
"{"                       { BEGIN(begin_json); }
<begin_json>"router_id: "  { BEGIN(router_id_state); }
<begin_json>"area_id: "  { BEGIN(area_id_state); }
<begin_json>"version: "  { BEGIN(version_state); }
<begin_json>"type: "  { BEGIN(type_state); }
<begin_json>"length: "  { BEGIN(length_state); }
<begin_json>"checksum: "  { BEGIN(checksum_state); }
<begin_json>"instance_id: "  { BEGIN(instance_id_state); }
<begin_json>"reserved: "  { BEGIN(reserved_state); }
<router_id_state>[0-9]+ { yylval = strtol(yytext, NULL, 10); return ROUTER_ID; }
<router_id_state>"}"            { BEGIN(INITIAL); }
<router_id_state>","            { BEGIN(begin_json); }
<area_id_state>[0-9]+           {yylval = atoi(yytext); return AREA_ID; }
<area_id_state>"}"            { BEGIN(INITIAL); }
<area_id_state>","            { BEGIN(begin_json); }
<version_state>[0-9]+           {yylval = atoi(yytext); return VERSION; }
<version_state>"}"            { BEGIN(INITIAL); }
<version_state>","            { BEGIN(begin_json); }
<type_state>[0-9]+           {yylval = atoi(yytext); return TYPE; }
<type_state>"}"            { BEGIN(INITIAL); }
<type_state>","            { BEGIN(begin_json); }
<length_state>[0-9]+           {yylval = atoi(yytext); return LENGTH; }
<length_state>"}"            { BEGIN(INITIAL); }
<length_state>","            { BEGIN(begin_json); }
<checksum_state>[0-9]+           {yylval = atoi(yytext); return CHECKSUM; }
<checksum_state>"}"            { BEGIN(INITIAL); }
<checksum_state>","            { BEGIN(begin_json); }
<instance_id_state>[0-9]+           {yylval = atoi(yytext); return INSTANCE_ID; }
<instance_id_state>"}"            { BEGIN(INITIAL); }
<instance_id_state>","            { BEGIN(begin_json); }
<reserved_state>[0-9]+           {yylval = atoi(yytext); return RESERVED; }
<reserved_state>"}"            { BEGIN(INITIAL); }
<reserved_state>","            { BEGIN(begin_json); }
[ \t]  { /* ignore whitespace */ }
.      { printf("Mystery character %c\n", *yytext); }
%%

#include <stdint.h>

void json_parse(char * buffer, struct ospf6_header * oh)
{
  int tok;

  YY_BUFFER_STATE bs = yy_scan_bytes(buffer, strlen(buffer));
  yy_switch_to_buffer(bs);


  while(tok = yylex())
  {
    printf("%d\n", tok);
    switch(tok)
   {
     case ROUTER_ID:
       oh->router_id = yylval;
       break;

     case AREA_ID:
       oh->area_id = yylval;
       break;

     case  VERSION:
       oh->version = yylval;
       break;

     case  TYPE:
       oh->type = yylval;
       break;

     case LENGTH:
       oh->length = yylval;
       break;

     case CHECKSUM:
       oh->checksum = yylval;
       break;
 
     case INSTANCE_ID:
       oh->instance_id = yylval;
       break;
     
     case RESERVED:
       oh->reserved = yylval;
       break;
    }
    printf(" = %d\n", yylval);
  }

  yy_delete_buffer(bs);
}
