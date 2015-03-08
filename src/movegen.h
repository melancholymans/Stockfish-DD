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

#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include "types.h"

enum GenType {			//GenType　手を生成するパターン
  CAPTURES,					//とにかく取る手
  QUIETS,						//穏やかな手（とらずに移動するだけ）
  QUIET_CHECKS,			//駒をとらないで王手をかける手（穏やかに王手？）
  EVASIONS,					//王手を回避する手
  NON_EVASIONS,			//王手をを回避する手以外の手
  LEGAL							//合法手を生成　具体的には王手がかかっていなかったらNON_EVASIONSを呼び、王手がかかっていたらEVASIONSを呼ぶ
};

class Position;

template<GenType>
ExtMove* generate(const Position& pos, ExtMove* mlist);

/// The MoveList struct is a simple wrapper around generate(), sometimes comes
/// handy to use this class instead of the low level generate() function.
/*
着手リストを管理するクラス
着手リスト自体はprivate変数 mlist[MAX_MOVES=256]
コンストラクタを呼ばれた時点でcurはmlistの先頭を指しており
テンプレート変数で指定した生成関数で着手リストがmlistに登録されている

このクラス内にbegin,end()関数を持っているのでC++11の範囲ベースfor文が使える
実際book.cppのprobe関数内で使っている(生成パターンはLEGAL)。begin,end関数はクラス内着手リストを
使っている。MovePick.cppで使っているMovePickerクラス内の着手リスト（moves）とはちがう
endgame.cppでも使っている（生成パターンはLEGAL）,notation.cppのなかのmove_from_uci関数でも使っている（生成パターンはLEGAL）
同じくmove_to_san関数内でも使用している（生成パターンはLEGAL）

search関数で次の手を生成してオダーリングするなど大掛かりなことをするのはMovePickクラス
ちょっと合法手を生成したい場合はMoveListクラスと分けて使っているのかな？
*/
template<GenType T>
struct MoveList {

  explicit MoveList(const Position& pos) : last(generate<T>(pos, mlist)) {}
  const ExtMove* begin() const { return mlist; }
  const ExtMove* end() const { return last; }
	//mlistのサイズを返す
	size_t size() const { return last - mlist; }
	//渡された手が着手リスト（mlist）にあったらtrueを返す
	bool contains(Move m) const {
    for (const ExtMove& ms : *this) if (ms.move == m) return true;
    return false;
  }

private:
  ExtMove mlist[MAX_MOVES], *last;
};

#endif // #ifndef MOVEGEN_H_INCLUDED
