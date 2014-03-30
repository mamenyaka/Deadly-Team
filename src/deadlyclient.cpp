/***************************************************************************
                          client.cpp  -  A basic client that connects to
                          the server
                             -------------------
    begin                : 27-DEC-2001
    copyright            : (C) 2001 by The RoboCup Soccer Server
                           Maintenance Group.
    email                : sserver-admin@lists.sourceforge.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LGPL as published by the Free Software  *
 *   Foundation; either version 3 of the License, or (at your option) any  *
 *   later version.                                                        *
 *                                                                         *
 ***************************************************************************/

/*
 * deadlyclient.cpp
 *
 * It is used in our Programming laboratory exercises at the University
 * of Debrecen. For details, see the books "Dr. Bátfai Norbert: Programozó
 * Páternoszter újratöltve: C, C++, Java, Python és AspectJ esettanulmányok"
 * http://www.inf.unideb.hu/~nbatfai/konyvek/PROP/prop.book.xml.pdf
 * and Bátfai Norbert: Mesterséges intelligencia a gyakorlatban: bevezetés
 * a robotfoci programozásba" http://www.inf.unideb.hu/~nbatfai/konyvek/MIRC/mirc.book.xml.pdf
 *
 * Norbert Bátfai, PhD
 * batfai.norbert@inf.unideb.hu, nbatfai@gmail.com
 * IT Dept, University of Debrecen
 *
 * Mamenyák András, BSc, University of Debrecen
 * 
 * I would like to thank Komzsik János, BSc, University of Debrecen for his assistance and logo.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "compress.h"

#include <rcssbase/net/socketstreambuf.hpp>
#include <rcssbase/net/udpsocket.hpp>
#include <rcssbase/gzip/gzstream.hpp>

#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif
#include <iostream>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __CYGWIN__
// cygwin is not win32
#elif defined(_WIN32) || defined(__WIN32__) || defined (WIN32)
#  define RCSS_WIN
#  include <winsock2.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h> // select()
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h> // select()
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> // select()
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> // select()
#endif

#include <pthread.h>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cmath>
#include "deadly.h"

class Client {
private:
  rcss::net::Addr M_dest;
  rcss::net::UDPSocket M_socket;
  rcss::net::SocketStreamBuf * M_socket_buf;
  rcss::gz::gzstreambuf * M_gz_buf;
  std::ostream * M_transport;
  int M_comp_level;
  bool M_clean_cycle;
  DeadlyLexer dl;
  char buf[64];
  pthread_t pthread;
  int t;
  bool mehet, coach;

#ifdef HAVE_LIBZ
  Decompressor M_decomp;
#endif

  Client();
  Client( const Client & );
  Client & operator=( const Client & );

public:
  Client( const std::string & server,
          const int port)
    : M_dest( port ),
      M_socket(),
      M_socket_buf( NULL ),
      M_gz_buf( NULL ),
      M_transport( NULL ),
      M_comp_level( -1 ),
      M_clean_cycle( true ),
      t( 0 ),
      mehet( true ),
      coach( false )
  {
    M_dest.setHost( server );
    open();
    bind();
    M_socket_buf->setEndPoint( M_dest );
    std::srand(std::time(NULL));

    if (port == 6002)
      coach = true;
  }

  virtual
  ~Client()
  {
    close();
  }

  void run()
  {
    dl.set_team("DeadlyTeam");
    std::snprintf(buf, 64, "(init %s (goalie) (version 15))", dl.get_team().c_str());
    sndCmd(buf);
    std::snprintf(buf, 64, "(init %s (version 15))", dl.get_team().c_str());
    sndCmd(buf);

    if (coach)
      teamLogo();

    pthread_create(&pthread, NULL, timeThread, (void *) this);

    messageLoop();
  }

private:
  int open()
  {
    if ( M_socket.open() )
    {
      if ( M_socket.setNonBlocking() < 0 )
      {
        std::cerr << __FILE__ << ": " << __LINE__
                  << ": Error setting socket non-blocking: "
                  << strerror( errno ) << std::endl;
        M_socket.close();
        return -1;
      }
    }
    else
    {
      std::cerr << __FILE__ << ": " << __LINE__
                << ": Error opening socket: "
                << strerror( errno ) << std::endl;
      M_socket.close();
      return -1;
    }

    M_socket_buf = new rcss::net::SocketStreamBuf( M_socket );
    M_transport = new std::ostream( M_socket_buf );
    return 0;
  }

  bool bind()
  {
    if ( ! M_socket.bind( rcss::net::Addr() ) )
    {
      std::cerr << __FILE__ << ": " << __LINE__
                << ": Error connecting socket" << std::endl;
      M_socket.close();
      return false;
    }
    return true;
  }

  void close()
  {
    M_socket.close();

    if ( M_transport )
    {
      delete M_transport;
      M_transport = NULL;
    }

    if ( M_gz_buf )
    {
      delete M_gz_buf;
      M_gz_buf = NULL;
    }

    if ( M_socket_buf )
    {
      delete M_socket_buf;
      M_socket_buf = NULL;
    }
  }

  int setCompression( int level )
  {
#ifdef HAVE_LIBZ
    if ( level >= 0 )
    {
      if ( ! M_gz_buf )
      {
        M_gz_buf = new rcss::gz::gzstreambuf( *M_socket_buf );
      }
      M_gz_buf->setLevel( level );
      M_transport->rdbuf( M_gz_buf );
    }
    else
    {
      M_transport->rdbuf( M_socket_buf );
    }
    return M_comp_level = level;
#endif
    return M_comp_level = -1;
  }

  void processMsg( const char * msg,
                   const size_t len )
  {
#ifdef HAVE_LIBZ
    if ( M_comp_level >= 0 )
    {
      M_decomp.decompress( msg, len, Z_SYNC_FLUSH );
      char * out;
      int size;
      M_decomp.getOutput( out, size );
      if ( size > 0 )
      {
        parseMsg( out );
      }
    }
    else
#endif
    {
      parseMsg( msg );
    }
  }

  void parseSeeCmd(char buf[])
  {
    //std::cout << buf << "\n";

    std::istringstream  data(buf);
    dl.switch_streams(&data);
    dl.yylex();

    deadlyMain();
  }

  void parseBodyCmd(char buf[])
  {
    //std::cout << buf << "\n";

    std::istringstream  data(buf);
    dl.switch_streams(&data);
    dl.yylex();
  }

  void parseHearCmd(char buf[])
  {
    //std::cout << buf << "\n";

    std::istringstream  data(buf);
    dl.switch_streams(&data);
    dl.yylex();
  }

  void parseCmd(char buf[])
  {
    //std::cout << buf << "\n";

    std::istringstream  data(buf);
    dl.switch_streams(&data);
    dl.yylex();
  }

  void parseMsg( const char * msg )
  {
    if ( ! std::strncmp( msg, "(ok compression", 15 ) )
    {
      int level;
      if ( std::sscanf( msg, " ( ok compression %d )", &level ) == 1 )
      {
        setCompression( level );
      }
    }
    else if ( ! std::strncmp( msg, "(sense_body", 11 )
              || ! std::strncmp( msg, "(see_global", 11 )
              || ! std::strncmp( msg, "(init", 5 ) )
    {
      M_clean_cycle = true;
    }

    char buf[8192];
    std::strncpy(buf, msg, 8192);

    if (!std::strncmp(msg, "(see", 4))
      parseSeeCmd(buf);
    else if (!std::strncmp(msg, "(sense_body", 11))
      parseBodyCmd(buf);
    else if (!std::strncmp(msg, "(hear", 5))
      parseHearCmd(buf);
    else
      parseCmd(buf);
  }

  static void * timeThread(void *client)
  {
    Client *thisclient = (Client *) client;

    int backup = thisclient->t;

    while (1)
    {
      usleep(1000*1000);

      if (thisclient->t == backup || thisclient->dl.get_time() == 6000)
        break;
      else
        backup = thisclient->t;

      if (thisclient->t > 1000)
      {
        thisclient->t = 1;
        backup = 0;
      }
    }

    std::cout << " Server down: " << thisclient->dl.get_team() << " player " << thisclient->dl.get_squad_number() << " exited\n";
    thisclient->mehet = false;

    return 0;
  }

  void messageLoop()
  {
    fd_set read_fds;
    fd_set read_fds_back;
    char buf[8192];
    std::memset( &buf, 0, sizeof( char ) * 8192 );

    int in = fileno( stdin );
    FD_ZERO( &read_fds );
    FD_SET( in, &read_fds );
    FD_SET( M_socket.getFD(), &read_fds );
    read_fds_back = read_fds;

#ifdef RCSS_WIN
    int max_fd = 0;
#else
    int max_fd = M_socket.getFD() + 1;
#endif

    while ( mehet )
    {
      read_fds = read_fds_back;
      int ret = ::select( max_fd, &read_fds, NULL, NULL, NULL );
      if ( ret < 0 )
      {
        perror( "Error selecting input" );
        break;
      }
      else if ( ret != 0 )
      {
        if ( FD_ISSET( in, &read_fds ) )
        {
          if ( std::fgets( buf, sizeof( buf ), stdin ) != NULL )
          {
            size_t len = std::strlen( buf );
            if ( buf[len-1] == '\n' )
            {
              buf[len-1] = '\0';
              --len;
            }

            M_transport->write( buf, len + 1 );
            M_transport->flush();
            if ( ! M_transport->good() )
            {
              if ( errno != ECONNREFUSED )
              {
                std::cerr << __FILE__ << ": " << __LINE__
                          << ": Error sending to socket: "
                          << strerror( errno ) << std::endl
                          << "msg = [" << buf << "]\n";
              }
              M_socket.close();
            }
          }
        }

        if ( FD_ISSET( M_socket.getFD(), &read_fds ) )
        {
          rcss::net::Addr from;
          int len = M_socket.recv( buf, sizeof( buf ) - 1, from );
          if ( len == -1
               && errno != EWOULDBLOCK )
          {
            if ( errno != ECONNREFUSED )
            {
              std::cerr << __FILE__ << ": " << __LINE__
                        << ": Error receiving from socket: "
                        << strerror( errno ) << std::endl;
            }
            M_socket.close();
          }
          else if ( len > 0 )
          {
            M_dest.setPort( from.getPort() );
            M_socket_buf->setEndPoint( M_dest );
            processMsg( buf, len );
            t++;
          }
        }
      }
    }
  }

  void sndCmd(const char cmd[])
  {
    M_transport->write(cmd, std::strlen(cmd) + 1 );
    M_transport->flush();

    if ( ! M_transport->good() )
    {
      if ( errno != ECONNREFUSED )
      {
        std::cerr << __FILE__ << ": " << __LINE__
                  << ": Error sending to socket: "
                  << strerror( errno ) << std::endl
                  << "msg = [" << cmd << "]\n";
      }
      M_socket.close();
    }

    //std::cout << dl.get_time() << " - (" << dl.get_squad_number() << "): " << cmd << "\n";
  }



  void deadlyMain()
  {
    if (!dl.get_see_ball())
      sndCmd("(turn 50)");
    else if (dl.get_side() == 1)
      switch (dl.get_play_mode())
      {
      case before_kick_off:
      case goal_r:
        beforeKickOff();
        break;
      case free_kick_fault_r:
      case foul_charge_r:
      case back_pass_r:
      case offside_r:
        break;
      case corner_kick_l:
      case free_kick_l:
        setpiece(1);
        break;
      case kick_in_l:
        setpiece(2);
        break;
      case indirect_free_kick_l:
        setpiece(3);
        break;
      case goal_kick_l:
        deadlyGoalKick();
        break;
      case kick_off_l:
        kickOff();
        break;

      case goal_l:
        beforeKickOff();
        break;
      case kick_off_r:
        if (dl.get_squad_number() != 11)
          moveToPosition(4);
        break;
      case corner_kick_r:
      case free_kick_r:
      case kick_in_r:
        moveToPosition(4);
        break;
      case free_kick_fault_l:
      case foul_charge_l:
      case back_pass_l:
      case offside_l:
        break;
      case indirect_free_kick_r:
        moveToPosition(6);
        break;
      case goal_kick_r:
        moveToPosition(9);
        break;

      case play_on:
        playOn();
        break;
      default:
        break;
      }
    else switch (dl.get_play_mode())
      {
      case before_kick_off:
      case goal_r:
        beforeKickOff();
        break;
      case kick_off_l:
        if (dl.get_squad_number() != 11)
          moveToPosition(4);
        break;
      case kick_in_l:
      case corner_kick_l:
      case free_kick_l:
        moveToPosition(4);
        break;
      case free_kick_fault_r:
      case foul_charge_r:
      case back_pass_r:
      case offside_r:
        break;
      case indirect_free_kick_l:
        moveToPosition(6);
        break;
      case goal_kick_l:
        moveToPosition(9);
        break;

      case goal_l:
        beforeKickOff();
        break;
      case free_kick_fault_l:
      case foul_charge_l:
      case back_pass_l:
      case offside_l:
        break;
      case corner_kick_r:
      case free_kick_r:
        setpiece(1);
        break;
      case kick_in_r:
        setpiece(2);
        break;
      case indirect_free_kick_r:
        setpiece(3);
        break;
      case goal_kick_r:
        deadlyGoalKick();
        break;
      case kick_off_r:
        kickOff();
        break;

      case play_on:
        playOn();
        break;
      default:
        break;
      }
  }

  void beforeKickOff()
  {
    if (dl.get_lr() != 'l')
      dl.set_side_away();

    std::snprintf(buf, 64, "(move %lf %lf)",
                  formationKickOff[dl.get_squad_number() - 1][0],
                  formationKickOff[dl.get_squad_number() - 1][1] * dl.get_side());
    sndCmd(buf);
  }

  void kickOff()
  {
    if (std::abs(dl.ball->get_ang()) > 30.0 && !dl.get_see_bigball())
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else if (dl.get_squad_number() == 11)
    {
      if (dl.ball->get_dist() < 0.8)
        deadlyTeamMatePass();
      else
        move(dl.ball->get_x(), dl.ball->get_y());
    }
    else
    {
      moveToPosition(1);
    }
  }

  void deadlyGoalKick()
  {
    if (std::abs(dl.ball->get_ang()) > 30.0 && !dl.get_see_bigball())
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else if (dl.get_squad_number() == 1)
    {
      if (dl.ball->get_dist() < 0.8)
        deadlyTeamMatePass();
      else
        move(dl.ball->get_x(), dl.ball->get_y());
    }
    else
    {
      moveToPosition(8);
    }
  }

  void setpiece(int k)
  {
    /* 1 - setplay
     * 2 - kick in
     * 3 - indirect
     */
    if(std::abs(dl.ball->get_ang()) > 30.0 && !dl.get_see_bigball())
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else if (dl.ball->get_dist() < 0.8)
    {
      if (dl.calcDist(52.0 * dl.get_side(), 0.0) < 30.0 && k == 1)
        deadlyGoal();
      else
        deadlyTeamMatePass();
    }
    else if (dl.deadlyNearest())
    {
      move(dl.ball->get_x(), dl.ball->get_y());
    }
    else
    {
      moveToPosition(k*2 + 1);
    }
  }

  void playOn()
  {
    if (dl.get_squad_number() == 1)
      keeper();
    else if (dl.ball->get_x() * dl.get_side() < -20.0)
      play(2);
    else
      play(1);
  }

  void play(int ad)
  {
    if (std::abs(dl.ball->get_ang()) > 30.0 && !dl.get_see_bigball())
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else if (dl.ball->get_dist() < 0.8)
    {
      if (dl.calcDist(52.0 * dl.get_side(), 0.0) < 30.0)
        deadlyGoal();
      else
        deadlyDash(dl.calcAng(52.0 * dl.get_side(), 0.0));
    }
    else
    {
      if (dl.get_pass() != 0)
      {
        if (dl.get_pass() == dl.get_squad_number())
        {
          move(dl.ball->get_estx(), dl.ball->get_esty());
        }
        else
        {
          std::snprintf(buf, 64, "(say pass %d)", dl.get_pass());
          sndCmd(buf);
        }
      }
      else if (dl.deadlyNearest())
      {
        move(dl.ball->get_estx(), dl.ball->get_esty());
      }
      else
      {
        moveToPosition(ad);
      }
    }
  }

  void keeper()
  {
    if (std::abs(dl.ball->get_ang()) > 30.0 && !dl.get_see_bigball())
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else if (dl.ball->get_dist() < 1.8)
    {
      std::snprintf(buf, 64, "(catch %lf)", dl.ball->get_ang());
      sndCmd(buf);
    }
    else
    {
      if (dl.ball->get_x() * dl.get_side() < -36.5 && std::abs(dl.ball->get_y()) < 20.0)
      {
        if (dl.ball->get_estx() * dl.get_side() < -52.0)
          move(dl.get_x(), dl.get_y() + dl.ball->get_path());
        else
          move(dl.ball->get_estx(), dl.ball->get_esty());
      }
      else
      {
        moveToPosition(2);
      }
    }
  }

  void move(double x, double y)
  {
    std::snprintf(buf, 64, "(pos %lf %lf 100.0)", x, y);
    sndCmd(buf);
  }

  void move2(double x, double y)
  {
    int limit;
    if (dl.get_squad_number() == 1)
      limit = 1.0;
    else if (dl.get_stamina() > 1000)
      limit = 2.5;
    else
      limit = 10.0;

    if (dl.calcDist(x, y) > limit)
      move(x, y);
  }

  void moveToPosition(int k)
  {
    /* 1 - attack; 2 - defence;
     * 3 - setplay our; 4 - setplay opp;
     * 5 - indirect our; 6 - indirect opp;
     * 7 - kick in our;
     * 8 - goal kick our; 9 - goal kick opp;
    */
    int z = 0;

    switch (k)
    {
    case 1:
      z = dl.check_formation(k);
      move2(formationAtt[z][dl.get_squad_number()][0] * dl.get_side(), formationAtt[z][dl.get_squad_number()][1]);
      break;
    case 2:
      z = dl.check_formation(k);
      move2(formationDef[z][dl.get_squad_number()][0] * dl.get_side(), formationDef[z][dl.get_squad_number()][1]);
      break;
    case 3:
      z = dl.check_formation(k);
      move2(formationSetplayOur[z][dl.get_squad_number()][0] * dl.get_side(), formationSetplayOur[z][dl.get_squad_number()][1]);
      break;
    case 4:
      z = dl.check_formation(k);
      move2(formationSetplayOpp[z][dl.get_squad_number()][0] * dl.get_side(), formationSetplayOpp[z][dl.get_squad_number()][1]);
      break;
    case 5:
      z = dl.check_formation(k);
      move2(formationIndirectOur[z][dl.get_squad_number()][0] * dl.get_side(), formationIndirectOur[z][dl.get_squad_number()][1]);
      break;
    case 6:
      z = dl.check_formation(k);
      move2(formationIndirectOpp[z][dl.get_squad_number()][0] * dl.get_side(), formationIndirectOpp[z][dl.get_squad_number()][1]);
      break;
    case 7:
      z = dl.check_formation(k);
      move2(formationKickIn[z][dl.get_squad_number()][0] * dl.get_side(), formationKickIn[z][dl.get_squad_number()][1]);
      break;
    case 8:
      move2(formationGoalKickOur[dl.get_squad_number() - 1][0] * dl.get_side(), formationGoalKickOur[dl.get_squad_number() - 1][1]);
      break;
    case 9:
      move2(formationGoalKickOpp[dl.get_squad_number() - 1][0] * dl.get_side(), formationGoalKickOpp[dl.get_squad_number() - 1][1]);
      break;
    }
  }

  void deadlyGoal()
  {
    int szam;

    if (dl.get_side() == 1)
      szam = 4;
    else
      szam = 7;

    if (dl.get_see_flag(szam-1) && dl.get_see_flag(szam+1))
    {
      double A[11], szog, szog1, szog2;
      for (int i = 0; i < 11; i++)
        A[i] = 0.0;

      if (dl.get_side() == 1)
      {
        szog1 = dl.flags[5].get_ang();  //grt
        szog2 = dl.flags[3].get_ang();  //grb
      }
      else
      {
        szog1 = dl.flags[6].get_ang();  //glb
        szog2 = dl.flags[8].get_ang();  //glt
      }

      int db = 0;
      A[db++] = szog1;

      for (int i = 0; i < 11; i++)
        if (dl.get_time() == dl.other_team[i].get_timestamp())
        {
          szog = dl.other_team[i].get_ang();
          if (szog > szog1 && szog < szog2)
            A[db++] = szog;
        }

      A[db] = szog2;

      if (db < 2)
      {
        if (dl.get_wing() * dl.get_side() == -1)
        {
          std::snprintf(buf, 64, "(kick 100 %lf)", szog1);
          sndCmd(buf);
        }
        else
        {
          std::snprintf(buf, 64, "(kick 100 %lf)", szog2);
          sndCmd(buf);
        }
      }
      else
      {
        for (int i = 0; i < db - 1; i++)
          for (int j = i; j < db; j++)
            if (A[i] > A[j])
            {
              szog = A[i];
              A[i] = A[j];
              A[j] = szog;
            }

        szog = 0.0;
        double max = 0.0;
        for (int i = 0; i < db; i++)
          if (std::abs(A[i] - A[i+1]) > max)
          {
            max = std::abs(A[i] - A[i+1]);
            szog = (A[i] + A[i+1]) / 2.0;
          }

        std::snprintf(buf, 64, "(kick 100 %lf)", szog);
        sndCmd(buf);
      }
    }
    else if (dl.get_x() * dl.get_side() > 35.0 && std::abs(dl.get_y()) < 20.0)
    {
      std::snprintf(buf, 64, "(kick 100 %lf)", dl.calcAng(55.0 * dl.get_side(), 5.0 * dl.get_wing()));
      sndCmd(buf);
    }
    else
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.calcAng(52.0 * dl.get_side(), 0.0));
      sndCmd(buf);
    }
  }

  void deadlyDash(double angle)
  {
    double min = 100.0, ang = 100.0;

    for (int i = 0; i < 11; i++)
      if (dl.get_time() == dl.other_team[i].get_timestamp() &&
          std::abs(dl.other_team[i].get_x()) > std::abs(dl.get_x()) &&
          std::abs(dl.other_team[i].get_ang()) < 30.0)
      {
        if (dl.other_team[i].get_dist() < min)
        {
          min = dl.other_team[i].get_dist();
          ang = dl.other_team[i].get_ang();
        }
      }

    if (min > 10.0 && !dl.get_see_bigplayer() && !dl.nowheretogo)
    {
      if (seeGoal())
      {
        if (std::abs(ang - angle) < 20.0)
          std::snprintf(buf, 64, "(kick 20 %lf)", (ang > 0.0) ? ang - 30.0 : ang + 30.0);
        else
          std::snprintf(buf, 64, "(kick 20 %lf)", angle);

        sndCmd(buf);
        dl.set_dash_time();
      }
    }
    else
    {
      dl.nowheretogo = true;
      deadlyTeamMatePass();
    }
  }

  bool seeGoal()
  {
    int szam;
    if (dl.get_side() == 1)
      szam = 4;
    else
      szam = 7;

    if (!dl.get_see_flag(szam))
    {
      std::snprintf(buf, 64, "(turn %lf)", dl.calcAng(52.0 * dl.get_side(), 0.0));
      sndCmd(buf);
      return false;
    }
    else
    {
      return true;
    }
  }

  void deadlyTeamMatePass()
  {
    double jopassz[11];

    for(int i = 1; i < 11; i++)
      jopassz[i] = 100.0;

    double X[11], Y[11];
    double estx = dl.get_x(), esty = dl.get_y();
    double dist;

    for (int i = 0; i < 11; i++)
      if (dl.get_time() == dl.other_team[i].get_timestamp())
      {
        X[i] = dl.other_team[i].get_x();
        Y[i] = dl.other_team[i].get_y();
      }
      else
      {
        X[i] = Y[i] = 100.0;
      }

    for (int i = 1; i < 11; i++)
      if (dl.get_time() == dl.own_team[i].get_timestamp() && dl.own_team[i].get_dist() > 10.0)
      {
        double x = dl.own_team[i].get_x(), y = dl.own_team[i].get_y();
        double Cx = (estx + x)/2.0, Cy = (esty + y)/2.0, r = dl.own_team[i].get_dist()/2.0;

        for (int j = 0; j < 11; j++)
          if (jopassz[i] > 0.0 && X[j] != 100.0)
          {
            dist = std::sqrt( std::pow(X[j] - x, 2) + std::pow(Y[j] - y, 2) );

            if (dist < jopassz[i])
              jopassz[i] = dist;

            if (std::sqrt( std::pow(X[j] - Cx, 2) + std::pow(Y[j] - Cy, 2) ) < r)
            {
              double A = (y - esty)/(x - estx);
              double B = -1.0;
              double C = esty - A*estx;
              dist = std::abs(A*X[j] + B*Y[j] + C)/std::sqrt(A*A + B*B);

              if (dist < r/2.0)
                jopassz[i] = 0.0;
            }
          }

        if (jopassz[i] == 100.0)
          jopassz[i] += r*2.0;
      }
      else
      {
        jopassz[i] = 0.0;
      }

    int index = 0;
    double max = 5.0;

    for (int i = 1; i < 11; i++)
      if (jopassz[i] > max)
      {
        index = i;
        max = jopassz[i];
      }

    if (index > 0)
    {
      std::snprintf(buf, 64, "(say pass %d)", index + 1);
      sndCmd(buf);
      deadlyPassXY(dl.own_team[index].get_x(), dl.own_team[index].get_y());

      if (dl.nowheretogo)
        dl.nowheretogo = false;
    }
    else
    {
      if (dl.get_wing() * dl.get_side() == 1)
        sndCmd("(turn 50)");
      else
        sndCmd("(turn -50)");
    }
  }

  void deadlyPassXY(double x, double y)
  {
    double power = dl.calcDist(x, y)*2.5 + 10.0;

    if (power > 100.0)
      power = 100.0;

    std::snprintf(buf, 64, "(kick %lf %lf)", power, dl.calcAng(x, y));
    sndCmd(buf);
  }

  void teamLogo()
  {
    char buf[512];

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 0, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"++++++++\" \"++++++++\" \"%&&%%%%%\" \" $$     \" \" $$ $%%%\" \" $$ %&&&\" \" $$ %&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$ %&&&\" \" $$     \" \"$%%$$$$$\" \"$$$$$$$$\" \"        \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 1, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"#$&&&&&&\" \". .$&&&&\" \"%%# #&&&\" \" @%$ $&&\" \"$..%@.&&\" \"&% #% $&\" \"&&@.& #&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&$ %++&\" \"&&$ %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" \"&&% %++&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&$ %++&\" \"&&# &.@&\" \"&&.@% $&\" \"%+ %# %&\" \" .$% #&&\" \"%&$.+&&&\" \"#. @&&&&\" \"+#%&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 2, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&++++++\" \"&& +++++\" \"&& $&%%%\" \"&& $$   \" \"&& $$ $%\" \"&& $$ %&\" \"&& $$ %&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ $%\" \"&& $$   \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $%%%%\" \"&& $$   \" \"&& $$ #$\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$   \" \"&& $%$$$\" \"&& @$$$$\" \"&&      \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 3, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"+++++++$\" \"+++++++$\" \"%%%%%%%&\" \"       $\" \"%%%%%%%&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"%%%%%%%&\" \"       $\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%%%%%%%%\" \"       $\" \"$$$$$$$%\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"       $\" \"$$$$$$$%\" \"$$$$$$$%\" \"       $\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 4, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&%\" \"&&&&&&&$\" \"&&&&&&&+\" \"&&&&&&& \" \"&&&&&&% \" \"&&&&&&$ \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&++\" \"&&&&&& #\" \"&&&&&% $\" \"&&&&&$ %\" \"&&&&&@+&\" \"&&&&&.#&\" \"&&&&% $#\" \"&&&&$ %+\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&@+& \" \"&&&&.#% \" \"&&&% $$ \" \"&&&$ %++\" \"&&&@.& #\" \"&&&.@% $\" \"&&% $$ %\" \"&&$ %++&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 5, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&+++$&&\" \"&%   @&&\" \"&$ &@.&&\" \"&++&$ %&\" \"& #&% $&\" \"% %#&.@&\" \"$ %+%@.&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++& $$ %\" \" #% @% $\" \" $# .&.@\" \" %+  %@.\" \"+& + $$ \" \"#% %.@% \" \"$# &@.&.\" \"%++&$ %+\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"& #&% $$\" \"% %&&.@%\" \"# &&&@.&\" \"+.$$$@ %\" \"       $\" \"&&&&&&&&\" \"       .\" \"+$$$$$# \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"#&&&&&% \" \"$&&&&&&.\" \"%&&&&&&@\" \"&&&&&&&$\" \"&&&&&&&%\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 6, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&+\" \"&&&&&&&+\" \"&&&&&&&%\" \"&&&&&&& \" \"&&&&&&& \" \"&&&&&&& \" \"&&&&&&& \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&& \" \"&&&&&&& \" \"&&&&&&& \" \"&&&&&&& \" \"%&&&&&& \" \"$&&&&&& \" \"@&&&&&& \" \".&&&&&& \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" %&&&&& \" \" $&&&&& \" \" @&&&&& \" \"+.&&&&& \" \"$ %&&&& \" \"% $&&&& \" \"& #&&&& \" \"%++&&&& \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$$ %&&& \" \"@% $&&& \" \".& #&&& \" \" %++&&& \" \" $$ %&& \" \".@% $&&$\" \"@.& #&&$\" \"$ %++&& \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 7, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    usleep(100*1000);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"+++++++#\" \"+++++++.\" \"&&%%%%%%\" \"$$      \" \"$$ $%%%$\" \"$$ %&&&&\" \"$$ %&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&&\" \"$$ %&&&%\" \"$$      \" \"%%$$$$$%\" \"$$$$$$$#\" \"       +\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 8, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"$&&&&&&&\" \" .$&&&&&\" \"%# #&&&&\" \"@%$ $&&&\" \"..%@.&&&\" \"% #% $&&\" \"&@.& #&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&$ %++&&\" \"&$ %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" \"&% %++&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&$ %++&&\" \"&# &.@&&\" \"&.@% $&&\" \"+ %# %&&\" \".$% #&&&\" \"&$.+&&&&\" \". @&&&&&\" \"#%&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 9, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&+$$+%&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$ %&&\" \"& $$    \" \"& $%$$$$\" \"& @$$$$$\" \"&       \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 10, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&$\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"       %\" \"$$$$$$$&\" \"$$$$$$$&\" \"       %\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 11, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"+%$+%&&&\" \".@%.@&&&\" \"$ %$ $&&\" \"&++&++&&\" \"&% $% $&\" \"&&#.%@ %\" \"&&% #% @\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&$ %$ \" \"&&&&++&.\" \"&&&&% $$\" \"&&&&&@.&\" \"&&&&&% #\" \"&&&&&&# \" \"&&&&&&&.\" \"&&&&&&&$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 12, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&#\" \"&&&&&&% \" \"&&&&&&@+\" \"&&&&&$ $\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%&&&&.@&\" \"+&&&$ %#\" \" $&% #% \" \"@.&@.&+.\" \"% + $$ $\" \"%# +&.+&\" \"@&.%# %&\" \" %%% @&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++&@ %&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" \"$ & +&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 13, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&@#&@@&%\" \"$ %$ $&$\" \".#% @&&$\" \" %# %&&#\" \"$% #&&&$\" \"&+.&&&&#\" \"$ $&&&&$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \".+&&&&&$\" \" %&&&&&%\" \"#&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 14, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&%\" \"&&&&&&%@\" \"&&&&&%+$\" \"&&&&%+%&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&+$&&\" \"&&&#$&&&\" \"&&% ####\" \"&&..$$%$\" \"&$.&&&&&\" \"&+%&&&&&\" \"$+&&&&&&\" \"@$&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"+&&&&&&&\" \"@&&&&&&&\" \"$&%&&&&&\" \"%&%&&&&&\" \"%&%&&&&&\" \"%&$&&&&&\" \"%&$&&&&%\" \"%&$&&&&$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$&$&&&&#\" \"#&%%&&%$\" \"@&&$&&$$\" \"#%&%&&&%\" \"$$&&&&&&\" \"%#&&%%&&\" \"&#&&%$&&\" \"&$%&%%#&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&%#&@&#+\" \"&%+&+&&$\" \"&%.%.#&&\" \"&% +$&&&\" \"&$%&&%$$\" \"%&&$.   \" \"%%%%#   \" \"&$%&&$  \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&$$%&. \" \"&&&$@&@ \" \"&&&%+&% \" \"&&&&$&&$\" \"&&&%&&&&\" \"&&&%%&&&\" \"&&&%$&&&\" \"&&&&%%%%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&%%#\" \"&&&&&&&%\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 15, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    usleep(100*1000);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&%\" \"&&&&%$##\" \"&&%$@#%&\" \"&$@#&&&&\" \"#@%&&&&&\" \"#&&&&&&%\" \"&&&&&$+ \" \"&&&%+   \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&#     \" \"&@      \" \"@       \" \"        \" \"+       \" \"@       \" \"#       \" \"%       \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&       \" \"&+      \" \"&%      \" \"&$@$$$$$\" \"&@&&&&&&\" \"#%&&&&&&\" \"#&&&&&&&\" \"%&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"#$&&&&&&\" \"#@#$%&&&\" \"&&%#+$&&\" \"&&&&%$%&\" \"$%&&&&%%\" \" .@%&&&&\" \"    +$&&\" \"      +$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"        \" \"        \" \"        \" \"        \" \"%+      \" \"&&%#++++\" \"%&&&&&&&\" \"%&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$$$&&&&&\" \"$% .#&%&\" \"$&#  .%%\" \"%$&.  +%\" \"&#&+ ..%\" \"&$%# @ %\" \"&%$% # &\" \"&&#&+@.#\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 16, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&#&&$$+\" \"&&$%&&&#\" \"&&%#%&&%\" \"&&&%$$&&\" \"&&&&&%$%\" \"&&&&&&&%\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%##++++#\" \"#+.@$%&&\" \"&##&&&&&\" \"&+&&&&&&\" \"$#&&&&&&\" \".#%&&&&&\" \"  .$&&&&\" \"    +%&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"     .$&\" \"       @\" \"        \" \"       @\" \"       %\" \"      +&\" \"      $&\" \"     .&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"     #&&\" \"    .&&&\" \" ...+&&&\" \"%%%%@$&&\" \"&&&&&+%&\" \"&&&&&%+&\" \"&&&&&&##\" \"&&&&&&&#\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&%%%%%\" \"&&&&&$#%\" \"&&&&&%$&\" \"&&&&&$$&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"+%&&&%$&\" \" .@%&$@&\" \"   ...@.\" \"     .# \" \"   .+###\" \".+#$%+ #\" \"&%&&@   \" \"&&&#  . \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&%.  + \" \"&&%+ .@ \" \"%&$%++$.\" \"%%%%&%$%\" \"$%&&&&&&\" \"$%%&&&%&\" \"%$$#%%@%\" \"%%$%$%$%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 17, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%$#%$&&&\" \"%%#%#%$$\" \"$$$%$%$%\" \"&%%$#%$%\" \"&&&%%%$%\" \"$%&&&&&&\" \"&%%&&&&&\" \"&&%$&&%&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"###$%&&&\" \"&&%$#@#%\" \"&&&&&&$@\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&%%%%\" \"########\" \"$$%%%%%%\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"%&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&@\" \"&&&&&&%$\" \"&&&&&%%&\" \"&&&&&&&&\" \"&&&&&&%#\" \"&&&&&#. \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&$+   \" \"&%+     \" \"+       \" \"        \" \"+       \" \"%$@.....\" \"$&%%&%&&\" \" %&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" @&&&&&&\" \" #%&&%&%\" \"+%$&%%$ \" \"&$%%%%  \" \"&%%%%# .\" \"&&%$$@ @\" \"%#%$&#.@\" \"$%%%%.@+\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 18, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$%#$%.%#\" \"$$$%$$&&\" \"$%$$$&&&\" \"$$%%&&&$\" \"%%&&&%$%\" \"&&&%$%&&\" \"&&%%&&&&\" \"&%%&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"@%&&&&&&\" \"@ #&&&&&\" \"&$ +%&&&\" \"&&.  $&&\" \"&%    #&\" \"&#     $\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%.      \" \"@       \" \"%$      \" \"&&$     \" \"&&&#    \" \"&&&&+   \" \"&&&&%.  \" \"&&&&&$  \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&. \" \"&&&&&&+$\" \"&&&&&&@&\" \"&&&&&%$&\" \"&&&&&#%&\" \"&&&&&#&&\" \"&&&&$%&&\" \"&&&&#&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&$%&&&\" \"&&&%&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&%\" \"&&&&&&%$\" \"&&&&&%#%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&$#+%%\" \"%$@@#$&$\" \"@$&&&&&+\" \"&&&&&&&$\" \"&&&%%%%%\" \"%$+.    \" \".      .\" \"       %\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"      @&\" \"      %%\" \"     +&%\" \"     $&%\" \"   +%%&&\" \"+@%&&&&&\" \"&&&&&&&&\" \"&&&%&&&%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%$@%$$%%\" \"+ .&#%&&\" \"  %%$&&&\" \" @&#%&&&\" \" %%#&&&&\" \" &$%&&&&\" \" %#&&&&&\" \"@%$&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 19, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&$%&&&&&\" \"&#&&&&&&\" \"%#&&&&&&\" \"#%&&&&&&\" \"%&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%&&&&&&&\" \".&&&&&&&\" \" @&&&&&&\" \"  %&&&&&\" \"  +&&&&&\" \"   %&&&&\" \"   @&&&&\" \"   .&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"  . %&&$\" \"%%&.$&&$\" \"&&&%#&&&\" \"&&%&#&&$\" \"&&%&@&&&\" \"&&$&@&&&\" \"&&$&@&&&\" \"&%$&#&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&%$%$&&&\" \"&$%%$&&&\" \"&$&%%&&&\" \"%%&$&&&&\" \"%&&$&&&&\" \"&&&$&&&&\" \"%&%%&&&&\" \"%&#%&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"#&#&&&&&\" \"@&@&&&&&\" \"+$+&&&&&\" \"@..&&&&&\" \"&%$%&&&&\" \"+%&%&&&&\" \"#%&%&&&&\" \"&&%%&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%$%&&&&&\" \"#%&&&&&&\" \"@&&&&&&&\" \"#&&&&&&&\" \"$&&&&&&&\" \"$&&&&&&&\" \"$&&&&&&&\" \"%&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 20, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++++++++\" \"++++++++\" \"%%%%%%%%\" \"        \" \"%%%%%%$ \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&% \" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 21, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++++++++\" \"++++++++\" \"&%%%%%%%\" \"%+      \" \"%+ %%%%%\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"%+ &&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 22, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++&&&+++\" \"++&&& ++\" \"%%&&& $&\" \"  &&& $$\" \"%%&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $$\" \"&&&&& $%\" \"&&&&& @$\" \"&&&&&   \" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 23, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    usleep(100*1000);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++++++++\" \"++++++++\" \"%%%%%%%%\" \"        \" \" $%%%%%%\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" $%%%%%%\" \"        \" \"%%%%%%%%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"        \" \" #$$$$$$\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \" %&&&&&&\" \" %&&&&&&\" \" %&&&&&&\" \"        \" \"$$$$$$$$\" \"$$$$$$$$\" \"        \" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 24, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"++$&&&&&\" \"++$&&&&&\" \"%%&&&&&&\" \"  $&&&&&\" \"%%&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"%%&&&&&&\" \"  $&&&&&\" \"%%%&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"  $&&&&&\" \"$$%&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&%\" \"&&&&&&&$\" \"&&&&&&&@\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&.\" \"&&&&&&% \" \"&&&&&&$ \" \"  $&&&@.\" \"$$%&&&.@\" \"$$%&&% $\" \"  $&&$ %\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 25, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&+++\" \"&&&&%   \" \"&&&&$ &@\" \"&&&&++&$\" \"&&&& #&%\" \"&&&% %#&\" \"&&&$ %+%\" \"&&&++& $\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&& #% @\" \"&&% $# .\" \"&&$ %+  \" \"&&++& + \" \"&& #% %.\" \"&% $# &@\" \"&$ %++&$\" \"&++& #&%\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"& #% %&&\" \"% $# &&&\" \"$ %+.$$$\" \"@+&     \" \".#&&&&&&\" \" $#     \" \" %++$$$$\" \"+& #&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"#% $&&&&\" \"$$ %&&&&\" \"%++&&&&&\" \"& #&&&&&\" \"% $&&&&&\" \"$ %&&&&&\" \"++&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 26, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"$&&&&&&&\" \"@&&&&&&&\" \".&&&&&&&\" \" %&&&&&&\" \" $&&&&&&\" \".@&&&&&&\" \"@.&&&&&&\" \"$ %&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"% $&&&&&\" \"&.@&&&&&\" \"%@.&&&&&\" \"$$ %&&&&\" \"@% $&&&&\" \".&.@&&&&\" \" %+.&&&&\" \" $$ %&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \".@% $&&&\" \"@.& @&&&\" \"@ %+.&&&\" \"  $$ %&&\" \"&&&% $&&\" \"  .& #&&\" \"$# %++&&\" \"&% $$ %&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&.@% $&\" \"&&@.& #&\" \"&&$ %++&\" \"&&% $$ %\" \"&&&.@% $\" \"&&&@.& #\" \"&&&$ %++\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 27, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&+++#&&\" \"&& ++ %&\" \"&& $% $&\" \"&& $&.@&\" \"&& $&# &\" \"&& $&$ %\" \"&& $$& #\" \"&& $$%@.\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $$$$ \" \"&& $$+% \" \"&& $$ %+\" \"&& $$ $$\" \"&& $$ @%\" \"&& $$  &\" \"&& $$  %\" \"&& $$  #\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $$ ..\" \"&& $$ @ \" \"&& $$ $ \" \"&& $$ %+\" \"&& $$ %$\" \"&& $$ %%\" \"&& $$ %&\" \"&& $$ %&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&& $$ %&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 28, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"%&&&&&&&\" \"#&&&&&&&\" \"+&&&&&&%\" \" %&&&&&#\" \" $&&&&&+\" \"++&&&&% \" \"# &&&&$ \" \"% $&&&++\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&.@&&& #\" \"%# &&$ %\" \"#$ %&#.&\" \"+& #&.@%\" \" %@+% $#\" \" $$ + &+\" \"++%  +% \" \"# %+ $$ \" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"% $$ %++\" \"&.@%+& #\" \"&# &$$ %\" \"&$ %&@.&\" \"&& #&.#&\" \"&&@.% %&\" \"&&$ + &&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 29, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&%+++$&\" \"&&$ + $&\" \"&&++& $&\" \"&% #& $&\" \"&$ %& $&\" \"&@.&& $&\" \"& #%& $&\" \"% $#& $&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"# &+& $&\" \".@% & $&\" \" $$ & $&\" \" %+ & $&\" \"+%  & $&\" \"$$  & $&\" \"%@  & $&\" \"&   & $&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"% + & $&\" \"# # & $&\" \".@$ & $&\" \" $$ & $&\" \" %$ & $&\" \"+&$ & $&\" \"$&$ & $&\" \"%&$ & $&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&$ & $&\" \"&&$ & $&\" \"&&$ & $&\" \"&&$ & $&\" \"&&$ & $&\" \"&&$ & $&\" \"&&$ & $&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 30, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);

    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 0, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 1, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 2, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 3, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 4, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 5, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 6, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
    std::snprintf(buf, 512, "(team_graphic (%d %d %s))", 31, 7, " \"8 8 8 1\" \"  c #000200\" \". c #272826\" \"+ c #525451\" \"@ c #696B68\" \"# c #868885\" \"$ c #A1A3A0\" \"% c #C9CBC8\" \"& c #FDFFFB\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" \"&&&&&&&&\" ");
    sndCmd(buf);
  }
};

namespace {
Client * client = static_cast< Client * >( 0 );

void
sig_exit_handle( int )
{
  if ( client )
  {
    std::cout << "TOROL\n";
    delete client;
    client =  static_cast< Client * >( 0 );
  }
  std::exit(EXIT_SUCCESS);
}

}


int
main ( int argc, char **argv )
{
  if ( std::signal( SIGINT, &sig_exit_handle) == SIG_ERR
       || std::signal( SIGTERM, &sig_exit_handle ) == SIG_ERR
       || std::signal( SIGHUP, &sig_exit_handle ) == SIG_ERR )
  {
    std::cerr << "Could not set signal handler\n";

    return 0;
  }

  std::string server = "localhost";
  int port = 6000;

  for (int i = 0; i < argc; i++)
  {
    if (std::strcmp(argv[i], "-server") == 0)
    {
      if (i+1 < argc)
      {
        server = argv[i+1];
        i++;
      }
    }
    else if (std::strcmp(argv[i], "-port") == 0)
    {
      if (i+1 < argc)
      {
        port = std::atoi(argv[i+1]);
        i++;
      }
    }
  }

  client = new Client(server, port);
  client->run();

  return 0;
}
