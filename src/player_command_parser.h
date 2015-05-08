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
     RCSS_PCOM_INT = 258,
     RCSS_PCOM_REAL = 259,
     RCSS_PCOM_STR = 260,
     RCSS_PCOM_LP = 261,
     RCSS_PCOM_RP = 262,
     RCSS_PCOM_DASH = 263,
     RCSS_PCOM_POS = 264,
     RCSS_PCOM_TURN = 265,
     RCSS_PCOM_TURN_NECK = 266,
     RCSS_PCOM_KICK = 267,
     RCSS_PCOM_LONG_KICK = 268,
     RCSS_PCOM_CATCH = 269,
     RCSS_PCOM_SAY = 270,
     RCSS_PCOM_UNQ_SAY = 271,
     RCSS_PCOM_SENSE_BODY = 272,
     RCSS_PCOM_SCORE = 273,
     RCSS_PCOM_MOVE = 274,
     RCSS_PCOM_CHANGE_VIEW = 275,
     RCSS_PCOM_COMPRESSION = 276,
     RCSS_PCOM_BYE = 277,
     RCSS_PCOM_DONE = 278,
     RCSS_PCOM_POINTTO = 279,
     RCSS_PCOM_ATTENTIONTO = 280,
     RCSS_PCOM_TACKLE = 281,
     RCSS_PCOM_CLANG = 282,
     RCSS_PCOM_EAR = 283,
     RCSS_PCOM_SYNCH_SEE = 284,
     RCSS_PCOM_VIEW_WIDTH_NARROW = 285,
     RCSS_PCOM_VIEW_WIDTH_NORMAL = 286,
     RCSS_PCOM_VIEW_WIDTH_WIDE = 287,
     RCSS_PCOM_VIEW_QUALITY_LOW = 288,
     RCSS_PCOM_VIEW_QUALITY_HIGH = 289,
     RCSS_PCOM_ON = 290,
     RCSS_PCOM_OFF = 291,
     RCSS_PCOM_TRUE = 292,
     RCSS_PCOM_FALSE = 293,
     RCSS_PCOM_OUR = 294,
     RCSS_PCOM_OPP = 295,
     RCSS_PCOM_LEFT = 296,
     RCSS_PCOM_RIGHT = 297,
     RCSS_PCOM_EAR_PARTIAL = 298,
     RCSS_PCOM_EAR_COMPLETE = 299,
     RCSS_PCOM_CLANG_VERSION = 300,
     RCSS_PCOM_ERROR = 301
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




