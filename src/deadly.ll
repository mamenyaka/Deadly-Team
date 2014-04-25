/*
 * deadly.ll
 *
 * It is used in our Programming laboratory exercises
 * at the University of Debrecen. For details, see the books "Bátfai
 * Norbert: Programozó Páternoszter újratöltve: C, C++, Java, Python és
 * AspectJ esettanulmányok"
 * http://www.inf.unideb.hu/~nbatfai/konyvek/PROP/prop.book.xml.pdf
 * and "Bátfai Norbert: Mesterséges intelligencia a gyakorlatban: bevezetés
 * a  robotfoci programozásba"
 * http://www.inf.unideb.hu/~nbatfai/konyvek/MIRC/mirc.book.xml.pdf
 *
 * Norbert Bátfai, PhD
 * batfai.norbert@inf.unideb.hu, nbatfai@gmail.com
 * IT Dept, University of Debrecen
 *
 * Mamenyák András, BSc, University of Debrecen
 * 
 * I would like to thank Komzsik János, BSc, University of Debrecen for his assistance and logo.
 */

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * http://www.gnu.org/licenses/gpl.html
 */

/*
 * References
 * Felhasznált és ajánlott irodalom:
 *
 * Don Blaheta: Project 2: Lexer
 * http://torvalds.cs.longwood.edu/courses/cmsc445/s12/proj2.pdf
 *
 * John R. Levine: flex & bison, O’Reilly, 2009
 *
 * Lexical Analysis With Flex
 * http://flex.sourceforge.net/manual
 *
 * Bison - GNU parser generator
 * www.gnu.org/software/bison/manual/
 * A Complete C++ Example
 * http://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html#A-Complete-C_002b_002b-Example
 * http://www.gnu.org/software/bison/manual/html_node/C_002b_002b-Parsers.html#C_002b_002b-Parsers
 */

%option c++
%option noyywrap

%{
#define YY_DECL int DeadlyLexer::yylex()
#include "deadly.h"
#include <cstdio>
%}

/*
 *  // See the figure entitled "Figure 4.2: The fags and lines in the simulation."
 *  // in the RCSS manual, version "February 11, 2003" pp. 47. at http://sourceforge.net/projects/sserver/files/rcssmanual/9-20030211/
 */

SEE	"(see"

BODY	"(sense_body"

HEAD	"(head_angle"

LIGHT	"(light"

INIT	"(init"

BALL	"((b)"

BIGBALL	"((B)"

PLYR	"((p"

BIGPLYR	"((P)"

HEAR	"(hear"

STAMINA	"(stamina"

VIEW	"(view_mode"

PASS	"pass"
DASH    "dash"

FC	"((f c)"
FCT	"((f c t)"
FCB	"((f c b)"

GRT	"((f g r t)"
GR	"((g r)"
GRB	"((f g r b)"

GLT	"((f g l t)"
GL	"((g l)"
GLB	"((f g l b)"

PRT	"((f p r t)"
PR	"((f p r c)"
PRB	"((f p r b)"

PLT	"((f p l t)"
PL	"((f p l c)"
PLB	"((f p l b)"

FRT	"((f r t)"
FRB	"((f r b)"

FLT	"((f l t)"
FLB	"((f l b)"

FRB30	"((f r b 30)"
FRB20	"((f r b 20)"
FRB10	"((f r b 10)"
FR0	"((f r 0)"
FRT10	"((f r t 10)"
FRT20	"((f r t 20)"
FRT30	"((f r t 30)"

FLB30	"((f l b 30)"
FLB20	"((f l b 20)"
FLB10	"((f l b 10)"
FL0	"((f l 0)"
FLT10	"((f l t 10)"
FLT20	"((f l t 20)"
FLT30	"((f l t 30)"

FTR50	"((f t r 50)"
FTR40	"((f t r 40)"
FTR30	"((f t r 30)"
FTR20	"((f t r 20)"
FTR10	"((f t r 10)"
FT0	"((f t 0)"
FTL50	"((f t l 50)"
FTL40	"((f t l 40)"
FTL30	"((f t l 30)"
FTL20	"((f t l 20)"
FTL10	"((f t l 10)"

FBR50	"((f b r 50)"
FBR40	"((f b r 40)"
FBR30	"((f b r 30)"
FBR20	"((f b r 20)"
FBR10	"((f b r 10)"
FB0	"((f b 0)"
FBL50	"((f b l 50)"
FBL40	"((f b l 40)"
FBL30	"((f b l 30)"
FBL20	"((f b l 20)"
FBL10	"((f b l 10)"

WS	[ \t]*
WORD	[^-:\n \t()]{2,}
INT2	[0123456789]{1,2}
INT4	[0123456789]{1,4}
FLOAT	[-.0123456789]+

%%
{INIT}{WS}("l"|"r"){WS}{INT2}{WS}{WORD}	{
    std::sscanf(yytext, "(init %c %d %s", &lr, &squad_number, hear_buffer);
    if (! std::strncmp(hear_buffer, "before_kick_off", 15))
        play_mode = before_kick_off;
    if(lr == 'l')
        side = 1;
    else
        side = -1;
}
{BODY}{WS}{INT4}				{
    std::sscanf(yytext, "(sense_body %d", &time);
}
{HEAD}{WS}{FLOAT}				{
    std::sscanf(yytext, "(head_angle %lf", &estha);
}
{STAMINA}{WS}{INT4}				{
    std::sscanf(yytext, "(stamina %d", &stamina);
}
{VIEW}{WS}{WORD}{WS}{WORD}")"			{
    std::sscanf(yytext, "(view_mode %s %s)", quality_buffer, width_buffer);
    if (! std::strncmp(quality_buffer, "high", 4))
        quality = 1;
    if (! std::strncmp(quality_buffer, "low", 3))
        quality = 0;
    if (! std::strncmp(width_buffer, "narrow", 6))
        width = -1;
    if (! std::strncmp(width_buffer, "normal", 6))
        width = 0;
    if (! std::strncmp(width_buffer, "wide", 4))
        width = 1;
}
{SEE}{WS}{INT4}					{
    std::sscanf(yytext, "(see %d", &time);
}
{LIGHT}{WS}{FLOAT}{WS}{FLOAT}{WS}{FLOAT}	{
    std::sscanf(yytext, "(light %lf %lf %lf", &estx, &esty, &esta);
}
{FC}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f c) %lf %lf", &dist, &ang);
    flags[Flag::FC].set_distang(time, dist, ang);
}
{FCT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f c t) %lf %lf", &dist, &ang);
    flags[Flag::FCT].set_distang(time, dist, ang);
}
{FCB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f c b) %lf %lf", &dist, &ang);
    flags[Flag::FCB].set_distang(time, dist, ang);
}
{GRT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f g r t) %lf %lf", &dist, &ang);
    flags[Flag::GRT].set_distang(time, dist, ang);
}
{GR}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((g r) %lf %lf", &dist, &ang);
    flags[Flag::GR].set_distang(time, dist, ang);
}
{GRB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f g r b) %lf %lf", &dist, &ang);
    flags[Flag::GRB].set_distang(time, dist, ang);
}
{GLT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f g l t) %lf %lf", &dist, &ang);
    flags[Flag::GLT].set_distang(time, dist, ang);
}
{GL}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((g l) %lf %lf", &dist, &ang);
    flags[Flag::GL].set_distang(time, dist, ang);
}
{GLB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f g l b) %lf %lf", &dist, &ang);
    flags[Flag::GLB].set_distang(time, dist, ang);
}
{PRT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p r t) %lf %lf", &dist, &ang);
    flags[Flag::PRT].set_distang(time, dist, ang);
}
{PR}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p r) %lf %lf", &dist, &ang);
    flags[Flag::PR].set_distang(time, dist, ang);
}
{PRB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p r b) %lf %lf", &dist, &ang);
    flags[Flag::PRB].set_distang(time, dist, ang);
}
{PLT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p l t) %lf %lf", &dist, &ang);
    flags[Flag::PLT].set_distang(time, dist, ang);
}
{PL}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p l) %lf %lf", &dist, &ang);
    flags[Flag::PL].set_distang(time, dist, ang);
}
{PLB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f p l b) %lf %lf", &dist, &ang);
    flags[Flag::PLB].set_distang(time, dist, ang);
}
{FRT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r t) %lf %lf", &dist, &ang);
    flags[Flag::RT].set_distang(time, dist, ang);
}
{FRB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r b) %lf %lf", &dist, &ang);
    flags[Flag::RB].set_distang(time, dist, ang);
}
{FLT}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l t) %lf %lf", &dist, &ang);
    flags[Flag::LT].set_distang(time, dist, ang);
}
{FLB}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l b) %lf %lf", &dist, &ang);
    flags[Flag::LB].set_distang(time, dist, ang);
}
{FRB30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r b 30) %lf %lf", &dist, &ang);
    flags[Flag::FRB30].set_distang(time, dist, ang);
}
{FRB20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r b 20) %lf %lf", &dist, &ang);
    flags[Flag::FRB20].set_distang(time, dist, ang);
}
{FRB10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r b 10) %lf %lf", &dist, &ang);
    flags[Flag::FRB10].set_distang(time, dist, ang);
}
{FR0}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r 0) %lf %lf", &dist, &ang);
    flags[Flag::FR0].set_distang(time, dist, ang);
}
{FRT30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r t 30) %lf %lf", &dist, &ang);
    flags[Flag::FRT30].set_distang(time, dist, ang);
}
{FRT20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r t 20) %lf %lf", &dist, &ang);
    flags[Flag::FRT20].set_distang(time, dist, ang);
}
{FRT10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f r t 10) %lf %lf", &dist, &ang);
    flags[Flag::FRT10].set_distang(time, dist, ang);
}
{FLB30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l b 30) %lf %lf", &dist, &ang);
    flags[Flag::FLB30].set_distang(time, dist, ang);
}
{FLB20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l b 20) %lf %lf", &dist, &ang);
    flags[Flag::FLB20].set_distang(time, dist, ang);
}
{FLB10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l b 10) %lf %lf", &dist, &ang);
    flags[Flag::FLB10].set_distang(time, dist, ang);
}
{FL0}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l 0) %lf %lf", &dist, &ang);
    flags[Flag::FL0].set_distang(time, dist, ang);
}
{FLT30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l t 30) %lf %lf", &dist, &ang);
    flags[Flag::FLT30].set_distang(time, dist, ang);
}
{FLT20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l t 20) %lf %lf", &dist, &ang);
    flags[Flag::FLT20].set_distang(time, dist, ang);
}
{FLT10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f l t 10) %lf %lf", &dist, &ang);
    flags[Flag::FLT10].set_distang(time, dist, ang);
}
{FTR50}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t r 50) %lf %lf", &dist, &ang);
    flags[Flag::FTR50].set_distang(time, dist, ang);
}
{FTR40}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t r 40) %lf %lf", &dist, &ang);
    flags[Flag::FTR40].set_distang(time, dist, ang);
}
{FTR30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t r 30) %lf %lf", &dist, &ang);
    flags[Flag::FTR30].set_distang(time, dist, ang);
}
{FTR20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t r 20) %lf %lf", &dist, &ang);
    flags[Flag::FTR20].set_distang(time, dist, ang);
}
{FTR10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t r 10) %lf %lf", &dist, &ang);
    flags[Flag::FTR10].set_distang(time, dist, ang);
}
{FT0}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t 0) %lf %lf", &dist, &ang);
    flags[Flag::FT0].set_distang(time, dist, ang);
}
{FTL50}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t l 50) %lf %lf", &dist, &ang);
    flags[Flag::FTL50].set_distang(time, dist, ang);
}
{FTL40}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t l 40) %lf %lf", &dist, &ang);
    flags[Flag::FTL40].set_distang(time, dist, ang);
}
{FTL30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t l 30) %lf %lf", &dist, &ang);
    flags[Flag::FTL30].set_distang(time, dist, ang);
}
{FTL20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t l 20) %lf %lf", &dist, &ang);
    flags[Flag::FTL20].set_distang(time, dist, ang);
}
{FTL10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f t l 10) %lf %lf", &dist, &ang);
    flags[Flag::FTL10].set_distang(time, dist, ang);
}
{FBR50}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b r 50) %lf %lf", &dist, &ang);
    flags[Flag::FBR50].set_distang(time, dist, ang);
}
{FBR40}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b r 40) %lf %lf", &dist, &ang);
    flags[Flag::FBR40].set_distang(time, dist, ang);
}
{FBR30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b r 30) %lf %lf", &dist, &ang);
    flags[Flag::FBR30].set_distang(time, dist, ang);
}
{FBR20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b r 20) %lf %lf", &dist, &ang);
    flags[Flag::FBR20].set_distang(time, dist, ang);
}
{FBR10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b r 10) %lf %lf", &dist, &ang);
    flags[Flag::FBR10].set_distang(time, dist, ang);
}
{FB0}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b 0) %lf %lf", &dist, &ang);
    flags[Flag::FB0].set_distang(time, dist, ang);
}
{FBL50}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b l 50) %lf %lf", &dist, &ang);
    flags[Flag::FBL50].set_distang(time, dist, ang);
}
{FBL40}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b l 40) %lf %lf", &dist, &ang);
    flags[Flag::FBL40].set_distang(time, dist, ang);
}
{FBL30}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b l 30) %lf %lf", &dist, &ang);
    flags[Flag::FBL30].set_distang(time, dist, ang);
}
{FBL20}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b l 20) %lf %lf", &dist, &ang);
    flags[Flag::FBL20].set_distang(time, dist, ang);
}
{FBL10}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((f b l 10) %lf %lf", &dist, &ang);
    flags[Flag::FBL10].set_distang(time, dist, ang);
}
{BALL}{WS}{FLOAT}{WS}{FLOAT}			{
    std::sscanf(yytext, "((b) %lf %lf", &dist, &ang);
    ball->set_distang(time, dist, ang, estx, esty, esta);
}
{BIGBALL}{WS}{FLOAT}{WS}{FLOAT}		        {
    std::sscanf(yytext, "((B) %lf %lf", &dist, &ang);
    ball->set_distang(time, dist, ang, estx, esty, esta);
    bigball_time = time;
}
{BIGPLYR}{WS}{FLOAT}{WS}{FLOAT}		        {
    std::sscanf(yytext, "((P) %lf %lf", &dist, &ang);
    bigplayer_time = time;
}
{PLYR}{WS}"\""{WORD}{WS}{INT2}")"{WS}{FLOAT}{WS}{FLOAT}		{
    std::sscanf(yytext, "((p \"%s %d) %lf %lf", teamname_buffer, &squadnumber_buffer, &dist, &ang);
    if(!std::strncmp(teamname_buffer, team.c_str(), strlen(teamname_buffer)-1))
        own_team[squadnumber_buffer-1].set_distang(time, dist, ang, estx, esty, esta);
    else
        other_team[squadnumber_buffer-1].set_distang(time, dist, ang, estx, esty, esta);
}
{PLYR}{WS}"\""{WORD}{WS}{INT2}{WS}"goalie)"{WS}{FLOAT}{WS}{FLOAT}	{
    std::sscanf(yytext, "((p \"%s %d goalie) %lf %lf", teamname_buffer, &squadnumber_buffer, &dist, &ang);
    if(!std::strcmp(teamname_buffer, team.c_str()))
        own_team[squadnumber_buffer-1].set_distang(time, dist, ang, estx, esty, esta);
    else
        other_team[squadnumber_buffer-1].set_distang(time, dist, ang, estx, esty, esta);
}
{HEAR}{WS}{INT4}{WS}{WORD}{WS}{WORD}")"	{
    std::sscanf(yytext, "(hear %d %s %s)", &time, sender_buffer, hear_buffer);
    if (! std::strncmp(hear_buffer, "before_kick_off", 15))
        play_mode = before_kick_off;
    else if (! std::strncmp(hear_buffer, "play_on", 7))
        play_mode = play_on;
    else if (! std::strncmp(hear_buffer, "half_time", 9))
        play_mode = half_time;
    else if (! std::strncmp(hear_buffer, "drop_ball", 9))
        play_mode = drop_ball;
    else if (! std::strncmp(hear_buffer, "kick_off_l", 10))
        play_mode = kick_off_l;
    else if (! std::strncmp(hear_buffer, "kick_off_r", 10))
        play_mode = kick_off_r;
    else if (! std::strncmp(hear_buffer, "corner_kick_l", 13))
        play_mode = corner_kick_l;
    else if (! std::strncmp(hear_buffer, "corner_kick_r", 13))
        play_mode = corner_kick_r;
    else if (! std::strncmp(hear_buffer, "free_kick_r", 11))
        play_mode = free_kick_r;
    else if (! std::strncmp(hear_buffer, "free_kick_l", 11))
        play_mode = free_kick_l;
    else if (! std::strncmp(hear_buffer, "kick_in_l", 9))
        play_mode = kick_in_l;
    else if (! std::strncmp(hear_buffer, "kick_in_r", 9))
        play_mode = kick_in_r;
    else if (! std::strncmp(hear_buffer, "goal_l", 6))
        play_mode = goal_l;
    else if (! std::strncmp(hear_buffer, "goal_r", 6))
        play_mode = goal_r;
    else if (! std::strncmp(hear_buffer, "goal_kick_l", 11))
        play_mode = goal_kick_l;
    else if (! std::strncmp(hear_buffer, "goal_kick_r", 11))
        play_mode = goal_kick_r;
    else if (! std::strncmp(hear_buffer, "back_pass_l", 11))
        play_mode = back_pass_l;
    else if (! std::strncmp(hear_buffer, "back_pass_r", 11))
        play_mode = back_pass_r;
    else if (! std::strncmp(hear_buffer, "offside_l", 9))
        play_mode = offside_l;
    else if (! std::strncmp(hear_buffer, "offside_r", 9))
        play_mode = offside_r;
    else if (! std::strncmp(hear_buffer, "foul_charge_r", 13))
        play_mode = foul_charge_r;
    else if (! std::strncmp(hear_buffer, "foul_charge_l", 13))
        play_mode = foul_charge_l;
    else if (! std::strncmp(hear_buffer, "indirect_free_kick_l", 20))
        play_mode = indirect_free_kick_l;
    else if (! std::strncmp(hear_buffer, "indirect_free_kick_r", 20))
        play_mode = indirect_free_kick_r;
    else if (! std::strncmp(hear_buffer, "free_kick_fault_r", 17))
        play_mode = indirect_free_kick_r;
    else if (! std::strncmp(hear_buffer, "free_kick_fault_l", 17))
        play_mode = indirect_free_kick_l;
    //else std::cout << sender_buffer << " " << hear_buffer << "\n";
}
{HEAR}{WS}{INT4}{WS}{FLOAT}{WS}{WORD}{WS}{INT4}{WS}"\""{PASS}{WS}{INT4}"\""     {
  std::sscanf(yytext, "(hear %d %lf %s %d \"pass %d\"", &time, &ang, sender_buffer, &squadnumber_buffer, &pass_buffer);
  if (! std::strcmp(sender_buffer, "our") && pass != pass_buffer)
  {
    pass = pass_buffer;
    pass_hear_time = time;
  }
}
{HEAR}{WS}{INT4}{WS}{FLOAT}{WS}{WORD}{WS}"\""{PASS}{WS}{INT4}"\""       {
  std::sscanf(yytext, "(hear %d %lf %s \"pass %d\"", &time, &ang, sender_buffer, &pass_buffer);
  if (! std::strcmp(sender_buffer, "our") && pass != pass_buffer)
  {
    pass = pass_buffer;
    pass_hear_time = time;
  }
}
{HEAR}{WS}{INT4}{WS}{FLOAT}{WS}{WORD}{WS}{INT4}{WS}"\""{DASH}{WS}{INT4}"\""     {
  std::sscanf(yytext, "(hear %d %lf %s %d \"dash %d\"", &time, &ang, sender_buffer, &squadnumber_buffer, &dash_buffer);
  if (! std::strcmp(sender_buffer, "our") && dash != dash_buffer)
  {
    dash = dash_buffer;
    dash_hear_time = time;
  }
}
{HEAR}{WS}{INT4}{WS}{FLOAT}{WS}{WORD}{WS}"\""{DASH}{WS}{INT4}"\""       {
  std::sscanf(yytext, "(hear %d %lf %s \"dash %d\"", &time, &ang, sender_buffer, &dash_buffer);
  if (! std::strcmp(sender_buffer, "our") && dash != dash_buffer)
  {
    dash = pass_buffer;
    dash_hear_time = time;
  }
}
.	{
    ;
}
%%

int yyFlexLexer::yylex() {
    return -1;
}