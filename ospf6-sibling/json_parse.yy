%x BEGIN_JSON
%x ROUTER_ID
%x AREA_ID
%x VERSION
%x TYPE
%x LENGTH
%x CHECKSUM
%x INSTANCE_ID
%x RESERVED
%{
  enum yytokentype {
    NUMBER = 258
  };
  int yylval;
%}

%%
"{"                       { BEGIN(BEGIN_JSON); }
<BEGIN_JSON>"\"router_id\": "  { BEGIN(ROUTER_ID); }
<BEGIN_JSON>"\"area_id\": "  { BEGIN(AREA_ID); }
<BEGIN_JSON>"\"version\": "  { BEGIN(VERSION); }
<BEGIN_JSON>"\"type\": "  { BEGIN(TYPE); }
<BEGIN_JSON>"\"length\": "  { BEGIN(LENGTH); }
<BEGIN_JSON>"\"checksum\": "  { BEGIN(CHECKSUM); }
<BEGIN_JSON>"\"instance_id\": "  { BEGIN(INSTANCE_ID); }
<BEGIN_JSON>"\"reserved\": "  { BEGIN(RESERVED); }
<ROUTER_ID>[0-9]+ {yylval = atoi(yytext); return NUMBER; }
<ROUTER_ID>"}"            { BEGIN(INITIAL); }
<ROUTER_ID>","            { BEGIN(BEGIN_JSON); }
<AREA_ID>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<AREA_ID>"}"            { BEGIN(INITIAL); }
<AREA_ID>","            { BEGIN(BEGIN_JSON); }
<VERSION>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<VERSION>"}"            { BEGIN(INITIAL); }
<VERSION>","            { BEGIN(BEGIN_JSON); }
<TYPE>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<TYPE>"}"            { BEGIN(INITIAL); }
<TYPE>","            { BEGIN(BEGIN_JSON); }
<LENGTH>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<LENGTH>"}"            { BEGIN(INITIAL); }
<LENGTH>","            { BEGIN(BEGIN_JSON); }
<CHECKSUM>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<CHECKSUM>"}"            { BEGIN(INITIAL); }
<CHECKSUM>","            { BEGIN(BEGIN_JSON); }
<INSTANCE_ID>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<INSTANCE_ID>"}"            { BEGIN(INITIAL); }
<INSTANCE_ID>","            { BEGIN(BEGIN_JSON); }
<RESERVED>[0-9]+           {yylval = atoi(yytext); return NUMBER; }
<RESERVED>"}"            { BEGIN(INITIAL); }
<RESERVED>","            { BEGIN(BEGIN_JSON); }
[ \t]  { /* ignore whitespace */ }
.      { printf("Mystery character %c\n", *yytext); }
%%
void json_parse(char * buffer, struct ospf6_header * oh)
{
  int tok;

  oh_g = oh;
  YY_BUFFER_STATE bs = yy_scan_bytes(buffer, strlen(buffer));
  yy_switch_to_buffer(bs);

  while(tok = yylex())
  {
    printf("%d\n", tok);
    if(tok == NUMBER) printf(" = %d\n", yylval);
    else printf("\n");
  }

  yy_delete_buffer(bs);
}
