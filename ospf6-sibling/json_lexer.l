%{
#define YY_DECL int yylex()

#include "json_parser.h"
%}

%option outfile="json_lexer.c" header-file="json_lexer.h"

%%
[0-9]+ { yylval.ival = atoi(yytext); printf("Number: %d\n", yylval.ival); return T_NUMBER; }
"{"                { printf("Open brace\n"); return T_OPEN_BRACE; }
"}"                { printf("Close brace\n"); return T_CLOSE_BRACE; }
","                { printf("Comma\n"); return T_COMMA; }
"["                { printf("Open bracket\n"); return T_OPEN_BRACKET; }
"]"                { printf("Close bracket\n"); return T_CLOSE_BRACKET; }
"header: "         { printf("Header\n"); return T_HEADER; }
"msg: "            { printf("Msg\n"); return T_MSG; }
"router_id: "      { return T_ROUTER_ID; }
"area_id: "        { printf("Area ID\n"); return T_AREA_ID; }
"version: "        { printf("Version\n"); return T_VERSION; }
"type: "           { printf("Type\n"); return T_TYPE; }
"length: "         { printf("Length\n"); return T_LENGTH; }
"checksum: "       { printf("Checksum\n"); return T_CHECKSUM; }
"instance_id: "    { printf("Instance ID\n"); return T_INSTANCE_ID; }
"reserved: "       { printf("Reserved\n"); return T_RESERVED; }
"interface_id: "   { printf("Interface ID\n"); return T_TODO; }
"priority: "       { printf("Priority\n"); return T_TODO; }
"options_0: "      { printf("Options_0\n"); return T_TODO; }
"options_1: "      { printf("Options_1\n"); return T_TODO; }
"options_2: "      { printf("Options_2\n"); return T_TODO; }
"hello_interval: " { printf("Hello Interval\n"); return T_TODO; }
"dead_interval: "  { printf("Dead Interval\n"); return T_TODO; }
"drouter: "        { printf("DRouter\n"); return T_TODO; }
"bdrouter: "       { printf("BDRouter\n"); return T_TODO; }
"router_ids: "     { printf("Router IDs\n"); return T_ROUTER_IDS; }
[ \t]  { /* ignore whitespace */ }
.      { printf("Mystery character %c\n", *yytext); }
%%
