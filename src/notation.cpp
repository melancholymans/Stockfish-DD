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

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stack>

#include "movegen.h"
#include "notation.h"
#include "position.h"

using namespace std;
/*
move_to_uci、move_to_san関数で使用されている
駒種を文字１文字で表現するための文字列
*/
static const char* PieceToChar[COLOR_NB] = { " PNBRQK", " pnbrqk" };


/// score_to_uci() converts a value to a string suitable for use with the UCI
/// protocol specifications:
///
/// cp <x>     The score from the engine's point of view in centipawns.
/// mate <y>   Mate in y moves, not plies. If the engine is getting mated
///            use negative values for y.
/*
主にuci_pv関数から呼び出し。
uci_pv関数はUCIからの呼び出しに応じて現局面の情報を返すのでそのうちの評価についてこの関数を使っている
このscore_to_uci関数は評価値(Value v,alpha,beta)を使って現局面の評価を行っている
CP -> 現在の評価値がPAWNの駒評価値の何倍かをしめす
mate -> 評価値が正数か負数で評価値表示を切り分けている（UCI側のプロトコルに合わせている）
lowerbound | upperbound alpha,beta cutかどうか
UCIプロトコルのEngine to GUIの
info　scoreコマンドの応答
http://wbec-ridderkerk.nl/html/UCIProtocol.html
*/
string score_to_uci(Value v, Value alpha, Value beta) 
{
  stringstream s;

  if (abs(v) < VALUE_MATE_IN_MAX_PLY)
      s << "cp " << v * 100 / int(PawnValueMg);
  else
      s << "mate " << (v > 0 ? VALUE_MATE - v + 1 : -VALUE_MATE - v) / 2;

  s << (v >= beta ? " lowerbound" : v <= alpha ? " upperbound" : "");

  return s.str();
}


/// move_to_uci() converts a move to a string in coordinate notation
/// (g1f3, a7a8q, etc.). The only special case is castling moves, where we print
/// in the e1g1 notation in normal chess mode, and in e1h1 notation in chess960
/// mode. Internally castle moves are always coded as "king captures rook".
/*
Move形式の指し手データを棋譜形式の文字列にする
*/
const string move_to_uci(Move m, bool chess960) 
{

  Square from = from_sq(m);
  Square to = to_sq(m);

  if (m == MOVE_NONE)
      return "(none)";

  if (m == MOVE_NULL)
      return "0000";
	/*
	キャスリングが行われた時の移動先を計算
	*/
	if (type_of(m) == CASTLE && !chess960)
      to = (to > from ? FILE_G : FILE_C) | rank_of(from);
	/*
	移動元、移動先座標をを棋譜形式文字に変換
	*/
	string move = square_to_string(from) + square_to_string(to);

  if (type_of(m) == PROMOTION)
      move += PieceToChar[BLACK][promotion_type(m)]; // Lower case
	/*
	a2a3qのようにPAWNが成った駒種を小文字で追加するが
	成らない時はa2a3だけとなる
	*/
	return move;
}


/// move_from_uci() takes a position and a string representing a move in
/// simple coordinate notation and returns an equivalent legal Move if any.
/*
棋譜形式指し手(a2a3）をMove形式の指し手に変換する
*/
Move move_from_uci(const Position& pos, string& str) 
{
	/*
	PAWNがなると最後に成った駒種を小文字１字が追加されるのでそれが大文字だったら小文字に変換する
	*/
	if (str.length() == 5) // Junior could send promotion piece in uppercase
      str[4] = char(tolower(str[4]));
	/*
	この局面での合法手を生成し棋譜形式に変換し引数の指し手と比較して合法手のなかに引数の指し手が
	あればMove形式の指し手を返す、なければMOVE_NONEを返す
	*/
	for (const ExtMove& ms : MoveList<LEGAL>(pos))
      if (str == move_to_uci(ms.move, pos.is_chess960()))
          return ms.move;

  return MOVE_NONE;
}


/// move_to_san() takes a position and a legal Move as input and returns its
/// short algebraic notation representation.
/*
Move形式の指し手情報を受け取って一般的な棋譜表記に変換する
相手の駒を取ったときは「Ｑｘｃ２」とします。（QueenがC2に移動しそのさい駒をとったという記法でｘがとったということを表している）
チェックだったときは「Ｑｃ２＋」とします。
チェックメイトだったときは「Ｑｃ２＃」とします。
ポーンがプロモーション（昇格）したときは、「＝」と駒のアルファベットを付け足します。

キャスリングをしたときは特別な表現をします。
右にキャスリングしたときは「Ｏ−Ｏ」とします。
左にキャスリングしたときは「Ｏ−Ｏ−Ｏ」とします。
http://chess.plala.jp/p6-1.html
*/
const string move_to_san(Position& pos, Move m) 
{

  if (m == MOVE_NONE)
      return "(none)";

  if (m == MOVE_NULL)
      return "(null)";

  assert(MoveList<LEGAL>(pos).contains(m));

  Bitboard others, b;
  string san;
  Color us = pos.side_to_move();
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = pos.piece_on(from);
  PieceType pt = type_of(pc);

  if (type_of(m) == CASTLE)
      san = to > from ? "O-O" : "O-O-O";
  else
  {
      if (pt != PAWN)
      {
          san = PieceToChar[WHITE][pt]; // Upper case

          // Disambiguation if we have more then one piece of type 'pt' that can
          // reach 'to' with a legal move.
          others = b = (pos.attacks_from(pc, to) & pos.pieces(us, pt)) ^ from;

          while (b)
          {
              Move move = make_move(pop_lsb(&b), to);
              if (!pos.legal(move, pos.pinned_pieces(pos.side_to_move())))
                  others ^= from_sq(move);
          }

          if (others)
          {
              if (!(others & file_bb(from)))
                  san += file_to_char(file_of(from));

              else if (!(others & rank_bb(from)))
                  san += rank_to_char(rank_of(from));

              else
                  san += square_to_string(from);
          }
      }
      else if (pos.capture(m))
          san = file_to_char(file_of(from));

      if (pos.capture(m))
          san += 'x';

      san += square_to_string(to);

      if (type_of(m) == PROMOTION)
          san += string("=") + PieceToChar[WHITE][promotion_type(m)];
  }

  if (pos.gives_check(m, CheckInfo(pos)))
  {
		/*
		実際に手を動かしてみた局面で合法手が0なら"#"(チエックメイト)非0なら"+"回避手があるならチエック
		*/
		StateInfo st;
      pos.do_move(m, st);
      san += MoveList<LEGAL>(pos).size() ? "+" : "#";
      pos.undo_move(m);
  }

  return san;
}


/// pretty_pv() formats human-readable search information, typically to be
/// appended to the search log file. It uses the two helpers below to pretty
/// format time and score respectively.
/*
このファイル内のpretty_pv関数からのみ呼ばれている
msec単位で渡された経過時間を時:分:秒形式に変換して返している
*/
static string time_to_string(int64_t msecs) 
{

  const int MSecMinute = 1000 * 60;
  const int MSecHour   = 1000 * 60 * 60;

  int64_t hours   =   msecs / MSecHour;
  int64_t minutes =  (msecs % MSecHour) / MSecMinute;
  int64_t seconds = ((msecs % MSecHour) % MSecMinute) / 1000;

  stringstream s;

  if (hours)
      s << hours << ':';

  s << setfill('0') << setw(2) << minutes << ':' << setw(2) << seconds;

  return s.str();
}
/*
pretty_pv関数からのみ呼ばれる
static的な関数なのでこのnotation.cpp内部だけの関数
渡された評価値を文字列に変換
*/
static string score_to_string(Value v) 
{

  stringstream s;

  if (v >= VALUE_MATE_IN_MAX_PLY)
      s << "#" << (VALUE_MATE - v + 1) / 2;

  else if (v <= VALUE_MATED_IN_MAX_PLY)
      s << "-#" << (VALUE_MATE + v) / 2;

  else
      s << setprecision(2) << fixed << showpos << double(v) / PawnValueMg;

  return s.str();
}
/*
id_loop関数からのみ呼び出されておりOptions["Write Search Log"]が有効時のみ
SearchLog.txtファイルにログを記録している
id_loopのなかで反復深化が１回終わるたびに呼び出されてその時の局面,depth,value,かかった探索時間
pv(最善応手手順の最初の手＝最善手）を文字列にして返す
*/
string pretty_pv(Position& pos, int depth, Value value, int64_t msecs, Move pv[]) 
{

  const int64_t K = 1000;
  const int64_t M = 1000000;

  std::stack<StateInfo> st;
  Move* m = pv;
  string san, padding;
  size_t length;
  stringstream s;

  s << setw(2) << depth
    << setw(8) << score_to_string(value)
    << setw(8) << time_to_string(msecs);

  if (pos.nodes_searched() < M)
      s << setw(8) << pos.nodes_searched() / 1 << "  ";

  else if (pos.nodes_searched() < K * M)
      s << setw(7) << pos.nodes_searched() / K << "K  ";

  else
      s << setw(7) << pos.nodes_searched() / M << "M  ";

  padding = string(s.str().length(), ' ');
  length = padding.length();

  while (*m != MOVE_NONE)
  {
      san = move_to_san(pos, *m);

      if (length + san.length() > 80)
      {
          s << "\n" + padding;
          length = padding.length();
      }

      s << san << ' ';
      length += san.length() + 1;

      st.push(StateInfo());
      pos.do_move(*m++, st.top());
  }

  while (m != pv)
      pos.undo_move(*--m);

  return s.str();
}
