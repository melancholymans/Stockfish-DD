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

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <cassert>
#include <cstddef>

#include "bitboard.h"
#include "types.h"


/// The checkInfo struct is initialized at c'tor time and keeps info used
/// to detect if a move gives check.
class Position;
struct Thread;
/*
checkに役立つbitboardを返す
pinnedはpinつけされた駒bitboardを返す
dcCandidatesは敵KINGへの利きを邪魔している自陣駒bitboardを返す
checkSq[]は敵KINGへ利きを利かしている自陣駒bitboardを返す
*/
struct CheckInfo {

  explicit CheckInfo(const Position&);

  Bitboard dcCandidates;						//敵KINGにpin付けされた自陣駒bitboard つまり敵KINGへの利きを邪魔している自陣駒のこと
  Bitboard pinned;									//自陣KINGにpinつけされた自陣駒bitboard
  Bitboard checkSq[PIECE_TYPE_NB];
  Square ksq;												//敵KINGの座標
};


/// The StateInfo struct stores information needed to restore a Position
/// object to its previous state when we retract a move. Whenever a move
/// is made on the board (by calling Position::do_move), a StateInfo
/// object must be passed as a parameter.
/*
positionは局面を保持するクラスで、do_moveによって
変更を加えられるが（局面更新）もとの局面に戻す時に
使用される情報を入れておく構造体
*/
struct StateInfo {
	/*
	PAWNだけの局面情報に基づいたハッシュ値を保管する変数
	*/
	Key pawnKey;
	/*
	カラー、駒種ごとの駒数できますユニークな数（ハッシュ値として使う）
	*/
	Key	materialKey;
	/*
	npMaterial=non-pawn-material　PAWNを入れない他の駒の評価値の集計
	*/
  Value npMaterial[COLOR_NB];
	/*
	castleRights：キャスリング
	rule50：50手ルールお互い駒をとらないなど条件が成立したら引き分けにするルール
					途中で駒を取ったらキャンセルするなどdo_move関数ないで処理している
	http://ja.wikipedia.org/wiki/50%E6%89%8B%E3%83%AB%E3%83%BC%E3%83%AB
	pliesFromNull：do_move関数でカウントアップ、do_null_moveでゼロクリア
	is_draw関数でドロー判定するときrule50とpliesFromNullを比較して小さいほうで判定している
	*/
  int castleRights, rule50, pliesFromNull;
	/*
	位置評価値の集計値
	*/
  Score psq;
	/*
	アンパッサン
	*/
  Square epSquare;
	/*
	この局面のハッシュ値
	*/
  Key key;
	/*
	手番側のKINGへ利きを利かしている敵の駒のbitboard
	*/
  Bitboard checkersBB;
	/*
	この局面で取った駒種（do_move関数で更新）
	*/
  PieceType capturedType;
	/*
	ひとつ上のレベルへのリンク
	*/
  StateInfo* previous;
};


/// When making a move the current StateInfo up to 'key' excluded is copied to
/// the new one. Here we calculate the quad words (64bits) needed to be copied.
/*
offsetofの機能
構造体のメンバのバイト位置を示す整数を返す
用途不明
*/
const size_t StateCopySize64 = offsetof(StateInfo, key) / sizeof(uint64_t) + 1;


/// The Position class stores the information regarding the board representation
/// like pieces, side to move, hash keys, castling info, etc. The most important
/// methods are do_move() and undo_move(), used by the search to update node info
/// when traversing the search tree.
/*
局面を表現するクラス
*/
class Position {
public:
  Position() {}
	/*
	他の局面をコピーして生成するコンストラクタ
	*/
  Position(const Position& p, Thread* t) { *this = p; thisThread = t; }
	/*
	fen文字から生成コンストラクタ,c960は変形ルールを適用するどうかのflag
	*/
  Position(const std::string& f, bool c960, Thread* t) { set(f, c960, t); }
	/*代入演算子オーバーロード*/
  Position& operator=(const Position&);
	/*
	初期化、Position全体の初期化で、盤面の初期化はあとのset関数で行う
	main関数から一度だけ呼ばれる
	*/
  static void init();

  // Text input/output
	/*
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
	/*
	set関数はPositionクラスのコンストラクタから呼ばれ
	fenStrを解析して内部データを更新している。
	更新される内部データは
	board[s]
	byTypeBB[ALL_PIECES]
	byColorBB[c]
	index[s]
	pieceList[c][pt][index[s]]
	他にもいろいろ局面保持、局面更新に必要なものを初期化しているが
	詳細不明
	*/
	void set(const std::string& fen, bool isChess960, Thread* th);
	/*
	現在の局面のfenStr文字列に変換する
	*/
	const std::string fen() const;
	/*
	指した手と現在の局面を文字列にして返す
	*/
	const std::string pretty(Move m = MOVE_NONE) const;
  // Position representation
	/*
	全ての駒のビットがたったbitboardを返す
	関数の宣言のあとについているconstはこの
	positionクラスのメンバー変数は変更できない
	ことを示している。（bitboardを読みだして返す
	関数なので変更することがない）
	*/
	Bitboard pieces() const;
	/*
	指定した駒種が立ったbitboardを返す
	*/
	Bitboard pieces(PieceType pt) const;
	/*
	pt1,pt2で指定した駒種が立ったbitboardを返す
	*/
	Bitboard pieces(PieceType pt1, PieceType pt2) const;
	/*
	指定したカラーの駒が立っているbitboardを返す
	*/
	Bitboard pieces(Color c) const;
	/*
	指定した駒種、指定したカラーの駒が立っているbitboardを返す
	*/
	Bitboard pieces(Color c, PieceType pt) const;
	/*
	指定した駒種pt1,pt2、指定したカラーの駒が立っているbitboardを返す
	*/
	Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
	/*
	メンバー変数board[64]の座標を指定して駒コードを返す
	*/
	Piece piece_on(Square s) const;
	/*
	カラーを指定してpieceList[COLOR][PIECE_TYPE][16]変数からkingの座標を返す
	*/
	Square king_square(Color c) const;
	/*
	アンパッサンの座標を返す
	*/
	Square ep_square() const;
	/*
	指定した座標に駒がなかったらtrueを返す
	*/
	bool empty(Square s) const;
	/*
	カラーと駒種を指定すればpieceCount[COLOR][PIECE_TYPE]を
	返す。pieceCount配列はカラーごと駒種ことの駒数を記録している配列
	ところでtemplate<PieceType Pt>とはなんだろう
	おそらく駒種ごとにcount関数をテンプレート化しているのでは
	それだけ高速に動作する？
	*/
	template<PieceType Pt> int count(Color c) const;
	/*
	カラーと駒種を指定すればpieceList[COLOR][PIECE_TYPE]の
	座標リスト（配列）を返してくる
	この関数も駒種ごとにテンプレート化されている？
	*/
	template<PieceType Pt> const Square* list(Color c) const;

  // Castling
	/*
	用途不明
	多分、キャステイングのことだと思う
	*/
	int can_castle(CastleRight f) const;
	/*
	用途不明
	多分、キャステイングのことだと思う
	*/
	int can_castle(Color c) const;
	/*
	用途不明
	多分、キャステイングのことだと思う
	*/
	bool castle_impeded(Color c, CastlingSide s) const;
	/*
	用途不明
	多分、キャステイングのことだと思う
	*/
	Square castle_rook_square(Color c, CastlingSide s) const;

  // Checking
	/*
	StateInfoのメンバーcheckersBBを返してくる
	手番側のKINGに王手をかけている敵側のbitboardを返す
	*/
	Bitboard checkers() const;
	/*
	Colorに自陣カラー kingColorに敵陣カラーを指定すれば
	敵KINGに釘付けされた駒（自陣側の駒、敵陣の駒ではない）を返す
	但しpinされた駒は１つだけで２つ挟まっているとpinとは判断されない
	つまり敵のKINGへの利きを邪魔している自陣側の駒を返す	
	*/
	Bitboard discovered_check_candidates() const;
	/*
	用途不明
	多分、kingへのチエックに関するなにか
	*/
	Bitboard pinned_pieces(Color toMove) const;

  // Attacks to/from a given square
	/*
	利き計算だとおもう
	*/
	Bitboard attackers_to(Square s) const;
  Bitboard attackers_to(Square s, Bitboard occ) const;
  Bitboard attacks_from(Piece p, Square s) const;
  static Bitboard attacks_from(Piece p, Square s, Bitboard occ);
  template<PieceType> Bitboard attacks_from(Square s) const;
  template<PieceType> Bitboard attacks_from(Square s, Color c) const;

  // Properties of moves
	/*
	渡された手が合法手かチエックする
	*/
	bool legal(Move m, Bitboard pinned) const;
	/*
	用途不明
	*/
	bool pseudo_legal(const Move m) const;
	/*
	着手が駒を取る手ならtrueを返す
	*/
	bool capture(Move m) const;
	/*
	用途不明
	着手の種別をチエックしているが
	関数名から判断するに駒をとる手
	もしくは成る手を判定しているようだが
	？
	*/
	bool capture_or_promotion(Move m) const;
	/*
	用途不明
	合法手の判定？
	*/
	bool gives_check(Move m, const CheckInfo& ci) const;
	/*
	着手データの駒種がPAWNでかつ行がRank_4以上だったらtrueを返す
	用途不明
	*/
	bool passed_pawn_push(Move m) const;
	/*
	着手データから駒コードを取得する
	*/
	Piece moved_piece(Move m) const;
	/*
	取った駒種を入れておく
	*/
	PieceType captured_piece_type() const;

  // Piece specific
	/*
	用途不明
	*/
	bool pawn_passed(Color c, Square s) const;
	/*
	PAWNが相手陣地の最下段にいるかチエック、いたら
	trueを返す、PAWNが成るためのチエックに使用しているのかも？
	*/
	bool pawn_on_7th(Color c) const;
	/*
	用途不明
	同じカラーのbishopが２個以上あって（つまりとられていない）
	boardのカラー（市松模様の色）が異なっていればtrue
	ただ味方同士のbishopは互いに異なるboardカラーに配置されるので
	trueが返ってくるのは当たり前のような気がするが、chess960では
	違うのかも？
	*/
	bool bishop_pair(Color c) const;
	/*
	WHITE、BLACK側のbishopが互い違いのboardカラーに位置すれば
	trueを返す
	つまり違うboardカラーの場合お互いに取り合うことはないと
	判断できる
	*/
	bool opposite_bishops() const;

  // Doing and undoing moves
	/*
	局面更新
	ここからいろいろ引数を追加して下の
	do_moveを呼んでいるラッパー
	*/
	void do_move(Move m, StateInfo& st);
	/*
	局面更新
	*/
	void do_move(Move m, StateInfo& st, const CheckInfo& ci, bool moveIsCheck);
	/*
	局面復元
	*/
	void undo_move(Move m);
	/*
	null moveを動かす
	*/
	void do_null_move(StateInfo& st);
	/*
	do_null_moveの復元
	*/
	void undo_null_move();

  // Static exchange evaluation
	/*
	SEEの名前からして静止探索？
	*/
	int see(Move m, int asymmThreshold = 0) const;
	/*
	用途不明
	*/
	int see_sign(Move m) const;

  // Accessing hash keys
	/*
	用途不明
	StateInfoのメンバーkeyを返すだけ
	*/
	Key key() const;
	/*
	用途不明
	*/
	Key exclusion_key() const;
	/*
	用途不明
	StateInfoのメンバーpawnKeyを返すだけ
	*/
	Key pawn_key() const;
	/*
	用途不明
	StateInfoのメンバーmaterialKeyを返すだけ
	*/
	Key material_key() const;

  // Incremental piece-square evaluation
	/*
	用途不明
	StateInfoのメンバーpsqを返す
	*/
	Score psq_score() const;
	/*
	用途不明
	StateInfoのメンバーnpMaterial
	*/
	Value non_pawn_material(Color c) const;

  // Other properties of the position
	/*
	メンバー変数sideToMoveを返す
	*/
	Color side_to_move() const;
	/*
	ゲームの何手目かを返す
	*/
	int game_ply() const;
	/*
	チェス960（Chess 960）は、変則チェスの一種
	chess960かどうかを返す
	*/
	bool is_chess960() const;
	/*
	用途不明
	メンバー変数thisThreadを返す
	*/
	Thread* this_thread() const;
	/*
	探索木のノード数を返す
	*/
	int64_t nodes_searched() const;
	/*
	search関数で探索木を分割して探索した場合それぞれの部分木でのノード数を
	合計するときに呼ばれる
	*/
	void set_nodes_searched(int64_t n);
	/*
	用途不明
	引き分けの判定をしている？
	*/
	bool is_draw() const;

  // Position consistency check, for debugging
	/*
	局面の不整合などをチエックしている
	詳細不明なところもある
	*/
	bool pos_is_ok(int* failedStep = nullptr) const;
	/*
	カラーを逆転したfen文字列を作ってpositionクラスを初期化する
	*/
	void flip();

private:
  // Initialization helpers (used while setting up a position)
	/*
	positionクラスをクリアにする
	一部用途不明あり
	*/
	void clear();
	/*
	用途不明
	*/
	void set_castle_right(Color c, Square rfrom);

  // Helper functions
  void do_castle(Square kfrom, Square kto, Square rfrom, Square rto);
  Bitboard hidden_checkers(Square ksq, Color c, Color toMove) const;
  void put_piece(Square s, Color c, PieceType pt);
  void remove_piece(Square s, Color c, PieceType pt);
  void move_piece(Square from, Square to, Color c, PieceType pt);

  // Computing hash keys from scratch (for initialization and debugging)
  Key compute_key() const;
  Key compute_pawn_key() const;
  Key compute_material_key() const;

  // Computing incremental evaluation scores and material counts
  Score compute_psq_score() const;
  Value compute_non_pawn_material(Color c) const;

  // Board and pieces
	/*
	board関係の主要変数
	*/
	//Piece型の配列、６４個ある
	Piece board[SQUARE_NB];
	//駒種ごとのbitboard、駒種は６種類しかないが８個まで要素がある
	Bitboard byTypeBB[PIECE_TYPE_NB];
	//カラーごとのbitboard
	Bitboard byColorBB[COLOR_NB];
	//カラーごと、駒種ごとの駒数を記憶しておく配列
	int pieceCount[COLOR_NB][PIECE_TYPE_NB];
	//カラーごと、駒種ごと、どの座標にいるのか記憶しておく配列
	Square pieceList[COLOR_NB][PIECE_TYPE_NB][16];
	//用途不明
	int index[SQUARE_NB];

  // Other info
	/*
	キャスリング関係の変数かな
	用途不明
	*/
	int castleRightsMask[SQUARE_NB];
  Square castleRookSquare[COLOR_NB][CASTLING_SIDE_NB];
  Bitboard castlePath[COLOR_NB][CASTLING_SIDE_NB];
	/*
	開始局面のためのStateInfo変数、Position::clear関数内で
	StateInfo* m_stからポインターされる
	*/
  StateInfo startState;
	/*
	do_moveを呼び出した回数
	つまりsearchでのノード数
	*/
	int64_t nodes;
	/*
	do_move関数で++されundo_moveで--されるので探索ではなくゲームの手数
	*/
	int gamePly;
	/*
	手番
	*/
	Color sideToMove;
	/*
	まだ、Thread関係には手がでないので保留
	*/
	Thread* thisThread;
	/*
	局面復元のために必要な情報(StateInfo）を保持するためのポインター
	StateInfoは各レベルの自動変数 StateInfo st;をdo_move関数内で情報を設定している
	m_stは常に最深部のStateInfoを指している
	StateInfoのpreviousを辿ることで１つ手間のレベルの指し手情報にアクセスできる
	*/
	StateInfo* m_st;	//あまりにも同じ名前がついていて紛らしいので変更2015/3 st->m_st
	/*
	変則chess960かどうかのフラグ
	*/
	int chess960;
};
/*
searchのノード数を返す
*/
inline int64_t Position::nodes_searched() const {
  return nodes;
}
/*
search関数で探索木を分割して探索した場合それぞれの部分木でのノード数を
合計するときに呼ばれる
*/
inline void Position::set_nodes_searched(int64_t n) {
  nodes = n;
}
/*
board配列の指定座標にある駒コードを返す
*/
inline Piece Position::piece_on(Square s) const {
  return board[s];
}
/*
着手データから駒コードを取得する
駒種ではない
*/
inline Piece Position::moved_piece(Move m) const {
  return board[from_sq(m)];
}
/*
board[]配列の指定した座標に駒がなければtrueを返す
*/
inline bool Position::empty(Square s) const {
  return board[s] == NO_PIECE;
}
/*
手番を返す
*/
inline Color Position::side_to_move() const {
  return sideToMove;
}
/*
全ての駒種のbitboardを返す
*/
inline Bitboard Position::pieces() const {
  return byTypeBB[ALL_PIECES];
}
/*
駒種ごとのbitboardを返す
全ての駒種のbitboardを返すにはALL_PIECES
*/
inline Bitboard Position::pieces(PieceType pt) const {
  return byTypeBB[pt];
}
/*
pt1,pt2駒種のbitboardを返す
*/
inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
  return byTypeBB[pt1] | byTypeBB[pt2];
}
/*
カラーごとのbitboardを返す
*/
inline Bitboard Position::pieces(Color c) const {
  return byColorBB[c];
}
/*
カラーと駒種を指定したbitboardを返す
*/
inline Bitboard Position::pieces(Color c, PieceType pt) const {
  return byColorBB[c] & byTypeBB[pt];
}
/*
指定したカラー、駒種pt1,pt2のbitboardを返す
*/
inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
  return byColorBB[c] & (byTypeBB[pt1] | byTypeBB[pt2]);
}
/*
指定したカラー、駒種の数を返す
*/
template<PieceType Pt> inline int Position::count(Color c) const {
  return pieceCount[c][Pt];
}
/*
指定したカラー、駒種の座標リストを返す
*/
template<PieceType Pt> inline const Square* Position::list(Color c) const {
  return pieceList[c][Pt];
}
/*
用途不明
*/
inline Square Position::ep_square() const {
  return m_st->epSquare;
}
/*
指定したカラーのKINGの座標を返す
*/
inline Square Position::king_square(Color c) const {
  return pieceList[c][KING][0];
}
/*
用途不明
*/
inline int Position::can_castle(CastleRight f) const {
  return m_st->castleRights & f;
}
/*
用途不明
*/
inline int Position::can_castle(Color c) const {
  return m_st->castleRights & ((WHITE_OO | WHITE_OOO) << (2 * c));
}
/*
用途不明
*/
inline bool Position::castle_impeded(Color c, CastlingSide s) const {
  return byTypeBB[ALL_PIECES] & castlePath[c][s];
}
/*
用途不明
*/
inline Square Position::castle_rook_square(Color c, CastlingSide s) const {
  return castleRookSquare[c][s];
}
/*
指定した座標に指定した駒種（テンプレート引数で指定）の利きbitboard
利いているbitboardを返す（駒種を指定できる、指定していなかったら
非飛び駒の利きを返す）
*/
template<PieceType Pt>
inline Bitboard Position::attacks_from(Square s) const {

  return  Pt == BISHOP || Pt == ROOK ? attacks_bb<Pt>(s, pieces())
        : Pt == QUEEN  ? attacks_from<ROOK>(s) | attacks_from<BISHOP>(s)
        : StepAttacksBB[Pt][s];
}
/*
指定した座標に利いている駒のbitboardを返す
テンプレートでPAWN専用にしている
template<>になっているのはPAWNを指定しているから
*/
template<>
inline Bitboard Position::attacks_from<PAWN>(Square s, Color c) const {
  return StepAttacksBB[make_piece(c, PAWN)][s];
}
/*
指定した駒が指定した座標にいる場合の利きのbitboardを返す
つまりfromは指定した座標から(from）利いている（attacks)という意味
反対にattackers_toは指定した座標toに来ている(to)利きを出している
*/
inline Bitboard Position::attacks_from(Piece p, Square s) const {
  return attacks_from(p, s, byTypeBB[ALL_PIECES]);
}
/*
指定した座標に利いている駒のbitboardを返す、カラーは無関係
*/
inline Bitboard Position::attackers_to(Square s) const {
  return attackers_to(s, byTypeBB[ALL_PIECES]);
}
/*
checkersBBは自陣のKINGに王手Checkを掛けている駒のbitboard
そのbitboardを返す
checkersBBは局面クラスが生成された時、set_state関数で最初の初期化をする
その後はdo_move関数で更新する
*/
inline Bitboard Position::checkers() const {
  return m_st->checkersBB;
}
/*
Colorに自陣カラー kingColorに敵陣カラーを指定すれば
敵KINGに釘付けされた駒（自陣側の駒、敵陣の駒ではない）を返す
但しpinされた駒は１つだけで２つ挟まっているとpinとは判断されない
つまり敵のKINGへの利きを邪魔している自陣側の駒を返す
*/
inline Bitboard Position::discovered_check_candidates() const {
  return hidden_checkers(king_square(~sideToMove), sideToMove, sideToMove);
}
/*
自陣サイド（toMove）のpinつけされている駒のbitboardを返す
*/
inline Bitboard Position::pinned_pieces(Color toMove) const {
  return hidden_checkers(king_square(toMove), ~toMove, toMove);
}
/*
passed_pawn_mask関数は？
指定した座標にいるPAWNが移動可能な範囲の中に敵側のPAWNとのAND
なのでこれから取ることが可能なPAWNのbitboardを返す
*/
inline bool Position::pawn_passed(Color c, Square s) const {
  return !(pieces(~c, PAWN) & passed_pawn_mask(c, s));
}
/*
着手データの駒種がPAWNでかつ行がRank_4以上だったらtrueを返す
用途不明
*/
inline bool Position::passed_pawn_push(Move m) const {

  return   type_of(moved_piece(m)) == PAWN
        && pawn_passed(sideToMove, to_sq(m));
}
/*
現局面のハッシュ値
*/
inline Key Position::key() const {
  return m_st->key;
}
/*
局面上のPAWNだけを使って得られたハッシュ値
*/
inline Key Position::pawn_key() const {
  return m_st->pawnKey;
}
/*
駒だけでの情報で得られたハッシュ値
key()と違って手番、キャスリング、アンパッサンの情報は入っていない
*/
inline Key Position::material_key() const {
  return m_st->materialKey;
}
/*
現局面での位置評価値の集計値
*/
inline Score Position::psq_score() const {
  return m_st->psq;
}
/*
PAWNを除いた駒評価値
*/
inline Value Position::non_pawn_material(Color c) const {
  return m_st->npMaterial[c];
}
/*
ゲームの何手目かを返す
*/
inline int Position::game_ply() const {
  return gamePly;
}
/*
WHITE、BLACK側のbishopが互い違いのboardカラーに位置すれば
trueを返す
つまり違うboardカラーの場合お互いに取り合うことはないと
判断できる
*/
inline bool Position::opposite_bishops() const {

  return   pieceCount[WHITE][BISHOP] == 1
        && pieceCount[BLACK][BISHOP] == 1
        && opposite_colors(pieceList[WHITE][BISHOP][0], pieceList[BLACK][BISHOP][0]);
}
/*
同じカラーのbishopが２個以上あって（つまりとられていない）
boardのカラー（市松模様の色）が異なっていればtrue
ただ味方同士のbishopは互いに異なるboardカラーに配置されるので
trueが返ってくるのは当たり前のような気がするが、chess960では
違うのかも？
*/
inline bool Position::bishop_pair(Color c) const {

  return   pieceCount[c][BISHOP] >= 2
        && opposite_colors(pieceList[c][BISHOP][0], pieceList[c][BISHOP][1]);
}
/*
PAWNがRANK_7にいるbitboard(BLACKにとってはRank_2）
つまり次の１手でQUEENになれる候補ということ
*/
inline bool Position::pawn_on_7th(Color c) const {
  return pieces(c, PAWN) & rank_bb(relative_rank(c, RANK_7));
}
/*
変形chess960かどうかを返す
*/
inline bool Position::is_chess960() const {
  return chess960;
}
/*
通常の動き(NORMAL=0)＋駒をとる動作ならtrue
promoto　｜｜　アンパッサンならtrue（ノーマル、キャスリング以外）
*/
inline bool Position::capture_or_promotion(Move m) const {

  assert(is_ok(m));
  return type_of(m) ? type_of(m) != CASTLE : !empty(to_sq(m));
}
/*
単純に駒を取る手＋アンパッサンならtrue　
*/
inline bool Position::capture(Move m) const {

  // Note that castle is coded as "king captures the rook"
  assert(is_ok(m));
  return (!empty(to_sq(m)) && type_of(m) != CASTLE) || type_of(m) == ENPASSANT;
}
/*
取った駒の駒種を入れておく、do_move関数で更新される
*/
inline PieceType Position::captured_piece_type() const {
  return m_st->capturedType;
}
/*
この探索用のスレッドを返す
*/
inline Thread* Position::this_thread() const {
  return thisThread;
}
/*
駒を置くことによって生じるbitboardの更新
駒の移動ではない、初期の駒配置に使用する
*/
inline void Position::put_piece(Square s, Color c, PieceType pt) {

  board[s] = make_piece(c, pt);
  byTypeBB[ALL_PIECES] |= s;
  byTypeBB[pt] |= s;
  byColorBB[c] |= s;
  pieceCount[c][ALL_PIECES]++;
  index[s] = pieceCount[c][pt]++;
  pieceList[c][pt][index[s]] = s;
}
/*
駒の移動による局面(bitboardなど）の更新
do_move,undo_move関数から呼ばれる
*/
inline void Position::move_piece(Square from, Square to, Color c, PieceType pt) {

  // index[from] is not updated and becomes stale. This works as long
  // as index[] is accessed just by known occupied squares.
  Bitboard from_to_bb = SquareBB[from] ^ SquareBB[to];
  byTypeBB[ALL_PIECES] ^= from_to_bb;
  byTypeBB[pt] ^= from_to_bb;
  byColorBB[c] ^= from_to_bb;
  board[from] = NO_PIECE;
  board[to] = make_piece(c, pt);
  index[to] = index[from];
  pieceList[c][pt][index[to]] = to;
}
/*
駒を取られた時の局面の更新
byTypeBB,byTypeBB,byColorBBを更新
index[],pieceList[][][],pieceCount[][]配列を更新する関数
do_move,undo_move,do_castle関数から呼ばれる
*/
inline void Position::remove_piece(Square s, Color c, PieceType pt) {

  // WARNING: This is not a reversible operation. If we remove a piece in
  // do_move() and then replace it in undo_move() we will put it at the end of
  // the list and not in its original place, it means index[] and pieceList[]
  // are not guaranteed to be invariant to a do_move() + undo_move() sequence.
  byTypeBB[ALL_PIECES] ^= s;
  byTypeBB[pt] ^= s;
  byColorBB[c] ^= s;
  /* board[s] = NO_PIECE; */ // Not needed, will be overwritten by capturing
  pieceCount[c][ALL_PIECES]--;
  Square lastSquare = pieceList[c][pt][--pieceCount[c][pt]];
  index[lastSquare] = index[s];
  pieceList[c][pt][index[lastSquare]] = lastSquare;
  pieceList[c][pt][pieceCount[c][pt]] = SQ_NONE;
}

#endif // #ifndef POSITION_H_INCLUDED
