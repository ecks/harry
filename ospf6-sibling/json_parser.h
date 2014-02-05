/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_NUMBER = 258,
     T_OPEN_BRACE = 259,
     T_CLOSE_BRACE = 260,
     T_COMMA = 261,
     T_OPEN_BRACKET = 262,
     T_CLOSE_BRACKET = 263,
     T_HEADER = 264,
     T_ROUTER_ID = 265,
     T_AREA_ID = 266,
     T_VERSION = 267,
     T_TYPE = 268,
     T_LENGTH = 269,
     T_CHECKSUM = 270,
     T_INSTANCE_ID = 271,
     T_RESERVED = 272,
     T_HELLO = 273,
     T_DBDESC = 274,
     T_ROUTER_IDS = 275,
     T_INTERFACE_ID = 276,
     T_PRIORITY = 277,
     T_OPTIONS_ZERO = 278,
     T_OPTIONS_ONE = 279,
     T_OPTIONS_TWO = 280,
     T_HELLO_INTERVAL = 281,
     T_DEAD_INTERVAL = 282,
     T_DROUTER = 283,
     T_BDROUTER = 284,
     T_RESERVED_ONE = 285,
     T_RESERVED_TWO = 286,
     T_IFMTU = 287,
     T_BITS = 288,
     T_SEQNUM = 289,
     T_TODO = 290,
     T_LSA_HEADERS = 291,
     T_AGE = 292,
     T_ID = 293,
     T_ADV_ROUTER = 294,
     GET_HEADER = 295,
     GET_BODY = 296
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 26 "json_parser.y"

  int ival;



/* Line 2068 of yacc.c  */
#line 97 "json_parser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


