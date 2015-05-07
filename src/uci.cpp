/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2013 Marco Costalba, Joona Kiiski, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "evaluate.h"
#include "notation.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "ucioption.h"

using namespace std;

extern void benchmark(const Position& pos, istream& is);

namespace {

  // FEN string of the initial position, normal chess
	/*
	局面を表す文字列 
	fenStrは局面を文字列で表現したもの
	＜例＞
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	R--rook,N--night,B--bishop,Q--queen,K--king,P--pawn,大文字はwhite（先手）
	r--rook,n--night,b--bishop,q--queen,k--king,p--pawn,小文字はblack（後手）
	数字は空白の数,/は行の終わり 局面を表現する文字列のあと空白を入れてこの局面で次に
	指すカラーをw/bで表現、その次のKQkqは不明 -も不明　0 1も不明
	（追記）
	KQkqはキャスリングに関係するなにか
	*/
  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  // Keep track of position keys along the setup moves (from start position to the
  // position just before to start searching). Needed by repetition draw detection.
	/*
	StateStackPtrはStateInfoクラスを格納しているスタック
	StateStackPtr自体はsearch.hで定義してある

	search.cppのsearch名前空間の中に同名の変数がある
	こっちのSetupStates変数はこのファイル内のpostion関数で
	初期局面を設定し、そのあとの駒の動きがあればdo_move関数で局面を
	更新、同時にSetupStates変数にも情報を登録しておく
	それをstart_thinking関数でsearch.cppで定義してある
	同名の変数SetupStates変数にmoveしている（moveはC++11で追加された機能）
	このためこっちのSetupStates変数は一時的な保持になっている

	positionは局面を保持するクラスで、do_moveによって
	変更を加えられるが（局面更新）もとの局面に戻す時に
	使用される情報を入れておく構造体かな
	*/
  Search::StateStackPtr SetupStates;
	/*

	*/
  void setoption(istringstream& up);
  void position(Position& pos, istringstream& up);
  void go(const Position& pos, istringstream& up);
}


/// Wait for a command from the user, parse this text string as an UCI command,
/// and call the appropriate functions. Also intercepts EOF from stdin to ensure
/// that we exit gracefully if the GUI dies unexpectedly. In addition to the UCI
/// commands, the function also supports a few debug commands.

void UCI::loop(const string& args) 
{

  Position pos(StartFEN, false, Threads.main()); // The root position
  string token, cmd = args;

  do {
      if (args.empty() && !getline(cin, cmd)) // Block here waiting for input
          cmd = "quit";

      istringstream is(cmd);

      is >> skipws >> token;

      if (token == "quit" || token == "stop" || token == "ponderhit")
      {
          // GUI sends 'ponderhit' to tell us to ponder on the same move the
          // opponent has played. In case Signals.stopOnPonderhit is set we are
          // waiting for 'ponderhit' to stop the search (for instance because we
          // already ran out of time), otherwise we should continue searching but
          // switching from pondering to normal search.
          if (token != "ponderhit" || Search::Signals.stopOnPonderhit)
          {
              Search::Signals.stop = true;
              Threads.main()->notify_one(); // Could be sleeping
          }
          else
              Search::Limits.ponder = false;
      }
      else if (token == "perft" && (is >> token)) // Read perft depth
      {
          stringstream ss;

          ss << Options["Hash"]    << " "
             << Options["Threads"] << " " << token << " current perft";

          benchmark(pos, ss);
      }
      else if (token == "key")
          sync_cout << hex << uppercase << setfill('0')
                    << "position key: "   << setw(16) << pos.key()
                    << "\nmaterial key: " << setw(16) << pos.material_key()
                    << "\npawn key:     " << setw(16) << pos.pawn_key()
                    << dec << sync_endl;

      else if (token == "uci")
          sync_cout << "id name " << engine_info(true)
                    << "\n"       << Options
                    << "\nuciok"  << sync_endl;

      else if (token == "eval")
      {
          Search::RootColor = pos.side_to_move(); // Ensure it is set
          sync_cout << Eval::trace(pos) << sync_endl;
      }
      else if (token == "ucinewgame") { /* Avoid returning "Unknown command" */ }
      else if (token == "go")         go(pos, is);
      else if (token == "position")   position(pos, is);
      else if (token == "setoption")  setoption(is);
      else if (token == "flip")       pos.flip();
      else if (token == "bench")      benchmark(pos, is);
      else if (token == "d")          sync_cout << pos.pretty() << sync_endl;
      else if (token == "isready")    sync_cout << "readyok" << sync_endl;
      else
          sync_cout << "Unknown command: " << cmd << sync_endl;

  } while (token != "quit" && args.empty()); // Args have one-shot behaviour

  Threads.wait_for_think_finished(); // Cannot quit while search is running
}


namespace {

  // position() is called when engine receives the "position" UCI command.
  // The function sets up the position described in the given fen string ("fen")
  // or the starting position ("startpos") and then makes the moves given in the
  // following move list ("moves").
	/*
	position startposまたは fen 局面を構成するfen文字列を入力することで
	局面を再設定できる
	*/
	void position(Position& pos, istringstream& is) 
	{

    Move m;
    string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token; // Consume "moves" token if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    pos.set(fen, Options["UCI_Chess960"], Threads.main());
		/*
		StateInfoをテンプレートパラメータにしてstackコンテナを生成している
		それをStateStackPtrに変換している。
		StateStackPtrはスマートポインタ
		*/
    SetupStates = Search::StateStackPtr(new std::stack<StateInfo>());

    // Parse move list (if any)
    while (is >> token && (m = move_from_uci(pos, token)) != MOVE_NONE)
    {
        SetupStates->push(StateInfo());
        pos.do_move(m, SetupStates->top());
    }
  }


  // setoption() is called when engine receives the "setoption" UCI command. The
  // function updates the UCI option ("name") to the given value ("value").
	/*
	オプションを設定する
	オプションはこのような形で呼び出される(UCIプロトコル）

	setoption name <id> [value <x>]
	<例>
	setoption name Hash value 70

	name <id>
		<id> = USI_Hash, type spin
		HashのメモリーをMB単位で設定するオプション,デフォルトでは32MB
		<id> = USI_Ponder, type check
		先読みをするかどうかの設定,デフォルトではtrue
		<id> = USI_OwnBook, type check
		定跡Bookを使用するかどうか、デフォルトではfalse
		<id> = USI_MultiPV, type spin
		MultiPVをサポートするか、何本PVを保持するか、デフォルトでは１
		<id> = USI_ShowCurrLine, type check
		stockfishはこのオプションをサポートしていない？
		<id> = USI_ShowRefutations, type check
		stockfishはこのオプションをサポートしていない？
		<id> = USI_LimitStrength, type check
		stockfishはこのオプションをサポートしていない？
		<id> = USI_Strength, type spin
		stockfishはこのオプションをサポートしていない？
		<id> = USI_AnalyseMode, type check
		stockfishはこのオプションをサポートしていない？
	*/
	void setoption(istringstream& is) 
	{

    string token, name, value;

    is >> token; // Consume "name" token

    // Read option name (can contain spaces)
		/*
		nameとvalueの間にある文字列をspaceを挟みながら連結してname変数に入れておく
		*/
		while (is >> token && token != "value")
        name += string(" ", !name.empty()) + token;

    // Read option value (can contain spaces)
    while (is >> token)
        value += string(" ", !value.empty()) + token;
		/*
		Option-mapに値を更新
		*/
    if (Options.count(name))
        Options[name] = value;
    else
        sync_cout << "No such option: " << name << sync_endl;
  }


  // go() is called when engine receives the "go" UCI command. The function sets
  // the thinking time and other parameters from the input string, and starts
  // the search.
	/*
	User　Interfaceからこのコマンドがきたらオプションを設定の上
	Threads.start_thinking関数を呼んで探索開始
	*/
	void go(const Position& pos, istringstream& is) 
	{

    Search::LimitsType limits;
    vector<Move> searchMoves;
    string token;

    while (is >> token)
    {
			/*
				go のあとにsearchmoves を続けて特定の指し手をa2a3,c2c4などと指定するとその手のみ探索する
				*/
				if (token == "searchmoves")
            while (is >> token)
                searchMoves.push_back(move_from_uci(pos, token));

        else if (token == "wtime")     is >> limits.time[WHITE];
        else if (token == "btime")     is >> limits.time[BLACK];
        else if (token == "winc")      is >> limits.inc[WHITE];
        else if (token == "binc")      is >> limits.inc[BLACK];
        else if (token == "movestogo") is >> limits.movestogo;
        else if (token == "depth")     is >> limits.depth;
        else if (token == "nodes")     is >> limits.nodes;
        else if (token == "movetime")  is >> limits.movetime;
        else if (token == "mate")      is >> limits.mate;
        else if (token == "infinite")  limits.infinite = true;
        else if (token == "ponder")    limits.ponder = true;
    }

    Threads.start_thinking(pos, limits, searchMoves, SetupStates);
  }
}
