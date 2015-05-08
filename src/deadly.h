/*
 * deadly.h
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

#ifndef DEADLY_H
#define DEADLY_H

#ifndef __FLEX_LEXER_H
#include <FlexLexer.h>
#endif

#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <deadlyformations.h>

const double PI = 3.14159265359;

enum RefereePlayMode
{
  play_on,
  before_kick_off,
  half_time,
  drop_ball,
  kick_off_r,
  kick_off_l,
  corner_kick_r,
  corner_kick_l,
  free_kick_l,
  free_kick_r,
  kick_in_l,
  kick_in_r,
  goal_l,
  goal_r,
  goal_kick_l,
  goal_kick_r,
  back_pass_r,
  back_pass_l,
  offside_l,
  offside_r,
  foul_charge_r,
  foul_charge_l,
  indirect_free_kick_l,
  indirect_free_kick_r,
  free_kick_fault_r,
  free_kick_fault_l
};

class Flag
{
public:
  static int const FCB = 0, FC = 1, FCT = 2, GRB = 3, GR = 4, GRT = 5, GLB = 6, GL = 7, GLT = 8, PRB = 9, PR = 10, PRT = 11,
                   PLB = 12, PL = 13, PLT = 14, RB = 15, RT = 16, LB = 17, LT = 18,
                   FRB30 = 19, FRB20 = 20, FRB10 = 21, FR0 = 22, FRT10 = 23, FRT20 = 24, FRT30 = 25,
                   FLB30 = 26, FLB20 = 27, FLB10 = 28, FL0 = 29, FLT10 = 30, FLT20 = 31, FLT30 = 32,
                   FTR50 = 33, FTR40 = 34, FTR30 = 35, FTR20 = 36, FTR10 = 37, FT0 = 38, FTL10 = 39, FTL20 = 40, FTL30 = 41, FTL40 = 42, FTL50 = 43,
                   FBR50 = 44, FBR40 = 45, FBR30 = 46, FBR20 = 47, FBR10 = 48, FB0 = 49, FBL10 = 50, FBL20 = 51, FBL30 = 52, FBL40 = 53, FBL50 = 54;
};

class SeenObject
{
public:
  SeenObject(double x, double y): timestamp(-10), x(x), y(y)
  {
    oldx = oldy = 0.0;
  };
  SeenObject(): timestamp(-10)
  {};

  void set_distang(int timestamp, double dist, double ang, double estx, double esty, double esta)
  {
    oldx = x;
    oldy = y;

    this->timestamp = timestamp;
    this->dist = dist;
    this->ang = ang;

    double szog = (esta + ang) * PI / 180.0;
    x = estx + dist * std::cos(szog);
    y = esty + dist * std::sin(szog);
    v = std::sqrt(std::pow(x - oldx, 2) + std::pow(y - oldy, 2));

    this->estx = x + (x - oldx)*2.5;
    this->esty = y + (y - oldy)*2.5;
    
    if (this->estx > 52.5)
      this->estx = 52.0;
    if (this->estx < -52.5)
      this->estx = -52.0;
    
    if (this->esty > 34.0)
      this->esty = 33.5;
    if (this->esty < -34.0)
      this->esty = -33.5;
  }

  void set_distang(int timestamp, double dist, double ang)
  {
    this->timestamp = timestamp;
    this->dist = dist;
    this->ang = ang;
  }

  int get_timestamp() const
  {
    return timestamp;
  }
  double get_dist()  const
  {
    return dist;
  }
  double get_ang()  const
  {
    return ang;
  }
  double get_x()  const
  {
    return x;
  }
  double get_y()  const
  {
    return y;
  }
  double get_oldx()  const
  {
    return oldx;
  }
  double get_oldy()  const
  {
    return oldy;
  }
  double get_estx()  const
  {
    if (dist < 10.0)
      return estx;
    else
      return x;
  }
  double get_esty()  const
  {
    if (dist < 10.0)
      return esty;
    else
      return y;
  }
  double get_velocity()  const
  {
    return v;
  }

private:
  int timestamp;
  double x, y, oldx, oldy, estx, esty;
  double dist, ang, v;
};

class DeadlyLexer : public yyFlexLexer
{
public:
  DeadlyLexer(int time = 0, int squad_number = 0, char lr = 'l'): time(time), squad_number(squad_number), lr(lr)
  {
    own_team = new SeenObject[11];
    other_team = new SeenObject[11];
    ball = new SeenObject(0.0, 0.0);
    flags = new SeenObject[55] {
      {0.0, 34.0}, {0.0, 0.0}, {0.0, -34.0}, {52.5, 7.0}, {52.5, 0.0}, {52.5, -7.0}, {-52.5, 7.0}, {-52.5, 0.0}, {-52.5, -7.0},
      {36.0, 20.0}, {36.0, 0.0}, {36.0, -20.0}, {-36.0, 20.0}, {-36.0, 0.0}, {-36.0, -20.0},
      {52.5, 34.0}, {52.5, -34.0}, {-52.5, 34.0}, {-52.5, -34.0},
      {57.5, 30.0}, {57.5, 20.0}, {57.5, 10.0}, {57.5, 0.0}, {57.5, -10.0}, {57.5, -20.0}, {57.5, -30.0},
      {-57.5, 30.0}, {-57.5, 20.0}, {-57.5, 10.0}, {-57.5, 0.0}, {-57.5, -10.0}, {-57.5, -20.0}, {-57.5, -30.0},
      {50.0, -39.0}, {40.0, -39.0}, {30.0, -39.0}, {20.0, -39.0}, {10.0, -39.0}, {0.0, -39.0},
      {-10.0, -39.0}, {-20.0, -39.0}, {-30.0, -39.0}, {-40.0, -39.0}, {-50.0, -39.0},
      {50.0, 39.0}, {40.0, 39.0}, {30.0, 39.0}, {20.0, 39.0}, {10.0, 39.0}, {0.0, 39.0},
      {-10.0, 39.0}, {-20.0, 39.0}, {-30.0, 39.0}, {-40.0, 39.0}, {-50.0, 39.0}
    };

    side = 1;
    pass = dash = 0;
    nowheretogo = false;
    bigball_time = dash_time = pass_hear_time = dash_hear_time = -10;
    bigplayers = 0;
  };

  ~DeadlyLexer()
  {
    delete[] flags;
    delete[] own_team;
    delete[] other_team;
    delete ball;
  }

  SeenObject *flags, *own_team, *other_team, *ball;
  bool nowheretogo;

  virtual int yylex();

  int get_time() const
  {
    return time;
  }
  int get_squad_number() const
  {
    return squad_number;
  }
  int get_stamina() const
  {
    return stamina;
  }
  RefereePlayMode get_play_mode()
  {
    return play_mode;
  }

  void set_team(const std::string & team)
  {
    this->team = team;
  }
  std::string get_team() const
  {
    return team;
  }

  int get_side() const
  {
    return side;
  }
  double get_x() const
  {
    return estx;
  }
  double get_y() const
  {
    return esty;
  }
  double get_ang() const
  {
    return esta;
  }

  int get_pass() const
  {
    if (time - pass_hear_time < 10)
      return pass;
    else
      return 0;
  }

  int get_wing() const
  {
    if (esty > 0)
      return -1;
    else
      return 1;
  }

  int get_see_bigplayers() const
  {
    return bigplayers;
  }
  bool get_see_bigball() const
  {
    if (time == bigball_time)
      return true;
    else
      return false;
  }
  bool get_see_ball() const
  {
    if (time == 0 || time == 3000)
      return true;
    else if (time == ball->get_timestamp())
      return true;
    else
      return false;
  }
  bool get_see_flag(int i) const
  {
    if (time == flags[i].get_timestamp())
      return true;
    else
      return false;
  }

  int get_dash_time() const
  {
    return dash_time;
  }
  void set_dash_time()
  {
    dash_time = time;
  }

  int check_formation(int k)
  {
    double min = 100.0, dist = 0.0;
    int index = 0, z = 0;
    switch (k)
    {
    case 1:
      z = 87;
      break;
    case 2:
      z = 85;
      break;
    case 3:
      z = 54;
      break;
    case 4:
      z = 51;
      break;
    case 5:
      z = 31;
      break;
    case 6:
      z = 25;
      break;
    case 7:
      z = 21;
      break;
    }

    for (int i = 0; i < z; i++)
    {
      switch(k)
      {
      case 1:
        dist = std::sqrt( std::pow(formationAtt[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationAtt[i][0][1] - ball->get_y(), 2) );
        break;
      case 2:
        dist = std::sqrt( std::pow(formationDef[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationDef[i][0][1] - ball->get_y(), 2) );
        break;
      case 3:
        dist = std::sqrt( std::pow(formationSetplayOur[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationSetplayOur[i][0][1] - ball->get_y(), 2) );
        break;
      case 4:
        dist = std::sqrt( std::pow(formationSetplayOpp[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationSetplayOpp[i][0][1] - ball->get_y(), 2) );
        break;
      case 5:
        dist = std::sqrt( std::pow(formationIndirectOur[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationIndirectOur[i][0][1] - ball->get_y(), 2) );
        break;
      case 6:
        dist = std::sqrt( std::pow(formationIndirectOpp[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationIndirectOpp[i][0][1] - ball->get_y(), 2) );
        break;
      case 7:
        dist = std::sqrt( std::pow(formationKickIn[i][0][0] * side - ball->get_x(), 2) +
                          std::pow(formationKickIn[i][0][1] - ball->get_y(), 2) );
        break;
      }

      if (dist < min)
      {
        min = dist;
        index = i;
      }
    }

    return index;
  }
  
  void init()
  {
    bigplayers = 0;
  }

  bool deadlyUtbaesik()
  {
    if (get_see_bigball())
      return true;

    int z = check_formation(1);
    double x, y, dist, min;

    x = formationAtt[z][get_squad_number()][0] * get_side();
    y = formationAtt[z][get_squad_number()][1];
    min = std::sqrt( std::pow(ball->get_estx() - x, 2) + std::pow(ball->get_esty() - y, 2) );

    for (int i = 1; i < 11; i++)
    {
      x = formationAtt[z][i][0] * get_side();
      y = formationAtt[z][i][1];
      dist = std::sqrt( std::pow(ball->get_estx() - x, 2) + std::pow(ball->get_esty() - y, 2) );

      if (dist < min)
        return false;
    }

    return true;
  }

  bool deadlyNearest()
  {
    if (time - dash_hear_time < 10)
    {
      if (dash == squad_number)
        return true;
      else
        return false;
    }

    if (time - dash_time < 10 && ball->get_dist() < 5.0)
      return true;

    double dist, min;
    min = calcDist(ball->get_estx(), ball->get_esty());

    if (squad_number == 1 && min > 20.0)
      return false;

    for (int i = 1; i < 11; i++)
      if (time == own_team[i].get_timestamp())
      {
        dist = std::sqrt( std::pow(own_team[i].get_x() - ball->get_estx(), 2) +
                          std::pow(own_team[i].get_y() - ball->get_esty(), 2) );
        if (dist < min)
          return false;
      }

    return true;
  }

  double calcAng(double x, double y)
  {
    double angle = std::atan2((get_y() - y), (get_x() - x)) * 180.0 / PI - get_ang();

    return angle;
  }

  double calcDist(double x, double y)
  {
    return std::sqrt( std::pow(get_x() - x, 2) + std::pow(get_y() - y, 2) );
  }

private:
  std::string team;
  RefereePlayMode play_mode;
  int time, bigball_time, bigplayers, dash_time, pass_hear_time, dash_hear_time;
  int side, squad_number, stamina;
  char lr;
  int quality, width;
  double estx, esty, esta, estha;
  int pass, dash;

  char teamname_buffer[128];
  int squadnumber_buffer, pass_buffer, dash_buffer;
  char hear_buffer[1024], sender_buffer[128], quality_buffer[5], width_buffer[10];
  double dist, ang;

  // nocopyable
  DeadlyLexer (const DeadlyLexer &);
  DeadlyLexer & operator= (const DeadlyLexer &);
};

#endif
