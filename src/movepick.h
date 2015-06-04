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

#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include <algorithm> // For std::max
#include <cstring>   // For std::memset

#include "movegen.h"
#include "position.h"
#include "search.h"
#include "types.h"


/// The Stats struct stores moves statistics. According to the template parameter
/// the class can store History, Gains and Countermoves. History records how often
/// different moves have been successful or unsuccessful during the current search
/// and is used for reduction and move ordering decisions. Gains records the move's
/// best evaluation gain from one ply to the next and is used for pruning decisions.
/// Countermoves store the move that refute a previous one. Entries are stored
/// according only to moving piece and destination square, hence two moves with
/// different origin but same destination and piece will be considered identical.
/*
テンプレートによりGainsStats,HistoryStats,Countermovesと３つの
記録するものを作っている

GainsStatsは
search関数の冒頭でGainsStats Gainsと宣言している
id_loop関数内で自メソッドclear関数でtable[][]配列を0クリアしている
内部のtable[][]配列には
table[Piece][Value]を保存
駒の移動によってどれくらい評価値に変化があったかを記録している

HistoryStatsも
search関数の冒頭でHistoryStats Historyと宣言している
id_loop関数内で自メソッドclear関数でtable[][]配列を0クリアしている
内部のtable[][]配列には
table[Piece][Value]を保存
最善手の駒種と移動先座標によって評価値を累計していく
良くとおる升ほど点数が高い、移動履歴累計評価のようなもの？

Countermovesは
CountermovesStats Countermovesと宣言している
id_loop関数内で自メソッドclear関数でtable[][]配列を0クリアしている
内部のtable[][]配列には
table[Piece][Value]を保存
敵駒の種別ごとに特定の座標に移動してきた場合の最善応手を記録しておく（探索した結果を記録）
table[PIece][pair<Move, Move>]に保存しておく
この手の組み合わせをカウンター手と呼ぶ
*/
template<bool Gain, typename T>
struct Stats {

  static const Value Max = Value(2000);
	/*
	[]演算子のオーバライド
	Pieceを指定してtable配列を返す
	*/
  const T* operator[](Piece p) const { return table[p]; }
	/*このクラスで保持しているtableを0クリアにする*/
  void clear() { std::memset(table, 0, sizeof(table)); }
	/*
	templateがCountermovesの時に使用するupadte関数
	search関数から呼ばれる
	Piece pは直前に動いた敵駒の駒種
	Square to　その敵駒の移動先座標
	Move m　全ての探索をした結果一番良かった手
	つまり相手が特定の駒種で特定の座標に移動してきた場合の
	自分側の最善手を覚えておくためのテーブル
	*/
	void update(Piece p, Square to, Move m) {

    if (m == table[p][to].first)
        return;

    table[p][to].second = table[p][to].first;
    table[p][to].first = m;
  }
	/*
	templateがGainsStats,HistoryStatsが使用する
	update関数
	*/
	void update(Piece p, Square to, Value v) {
		/*
		if文以降が実行するのはGainsStats関数がつかう
		*/
		if (Gain)
        table[p][to] = std::max(v, table[p][to] - 1);
		/*
		else if文以降条件が成立すればHistoryStats関数が更新する
		search関数からupdate関数がよばれ、最善手の駒種とその移動先
		に移動した探索深さの２乗が評価値として使われる
		つまり良く通る升目ほど評価値が高いただし2000点どまり
		*/
		else if (abs(table[p][to] + v) < Max)
        table[p][to] +=  v;
  }

private:
  T table[PIECE_NB][SQUARE_NB];
};

typedef Stats< true, Value> GainsStats;
typedef Stats<false, Value> HistoryStats;
typedef Stats<false, std::pair<Move, Move> > CountermovesStats;


/// MovePicker class is used to pick one pseudo legal move at a time from the
/// current position. The most important method is next_move(), which returns a
/// new pseudo legal move each time it is called, until there are no moves left,
/// when MOVE_NONE is returned. In order to improve the efficiency of the alpha
/// beta algorithm, MovePicker attempts to return the moves which are most likely
/// to get a cut-off first.
/*
着手リストを作るのはmovegen.cppの仕事だが,その着手リストをExtMove moves[MAX_MOVES]に保持して
search関数のリクエストに応じて指し手を渡すのがMovePickerのお仕事
search関数からは３回呼ばれているが、３回ともコンストラクタがことなる
メインの探索に使用されるのは３番目のコンストラクタ
*/
class MovePicker {

  MovePicker& operator=(const MovePicker&); // Silence a warning under MSVC

public:
  MovePicker(const Position&, Move, Depth, const HistoryStats&, Square);
  MovePicker(const Position&, Move, const HistoryStats&, PieceType);
  MovePicker(const Position&, Move, Depth, const HistoryStats&, Move*, Search::Stack*);
	/*
	次の手を返す　テンプレートパラメータSpNodeは探索分岐からの呼び出しの時にtrueを渡す
	*/
  template<bool SpNode> Move next_move();

private:
  template<GenType> void score();
  void generate_next();
	/*
	現在の局面
	*/
  const Position& pos;
	/*
	移動履歴累計評価？のようなものでオーダリングの点つけに利用する
	*/
  const HistoryStats& history;
	/*
	ここはパス
	*/
  Search::Stack* ss;
	/*
	カウンター手を保存
	*/
  Move* countermoves;
	/*
	探索深さ
	*/
  Depth depth;
	/*
	置換表の指し手
	*/
  Move ttMove;
	/*
	キラー手最初の２つがキラー手
	のこりつはカウンター手を登録する
	*/
  ExtMove killers[4];
	/*
	直前の敵駒の移動先座標が入っている
	取り返し手を生成するときに利用する
	*/
  Square recaptureSquare;
	/*
	用途不明
	stageは着手リストの生成場合分けのための変数
	*/
  int captureThreshold, stage;
	/*
	moves配列のポインタ
	curが現在の返すポインタ
	endが終端を指している
	endQuiets
	endBadCapturesは不明
	*/
  ExtMove *cur, *end, *endQuiets, *endBadCaptures;
	/*
	movegen.hにMoveListというstruct型のクラスがありそのなかにもmlistという
	Move型の配列がある？
	*/
	ExtMove moves[MAX_MOVES];
};

#endif // #ifndef MOVEPICK_H_INCLUDED
