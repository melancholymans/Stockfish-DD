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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include "types.h"

namespace Bitboards {
/*
bitboard関係の初期化,main関数から呼ばれている
*/
void init();
/*
bitboardを表形式で表示
*/
void print(Bitboard b);

}
namespace Bitbases {
	/*
	bitbase.cppのための宣言、bitbase.hはなくこのbitboard.hが兼任している
	init_kpkは終盤データベースのための初期化
	*/
	void init_kpk();
	/*
	終盤データベースの検査
	*/
	bool probe_kpk(Square wksq, Square wpsq, Square bksq, Color us);
}	//namespace Bitbasesの終わり


/*
bitboard変数(64bit変数)とbit位置と座標との対応

H8 G8 F8 E8 D8 C8 B8 A8 H7 G7 F7 E7 D7 C7 B7 A7 H6 G6 F6 E6 D6 C6 B6 A6 H5 G5 F5 E5 D5 C5 B5 A5 H4 G4 F4 E4 D4 C4 B4 A4 H3 G3 F3 E3 D3 C3 B3 A3 H2 G2 F2 E2 D2 C2 B2 A2 H1 G1 F1 E1 D1 C1 B1 A1    
63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
*/

/*
列を特定するためのbitboard
FileABB=
1000 0000
1000 0000
1000 0000
1000 0000
1000 0000
1000 0000
1000 0000
1000 0000

FileBBB=
0100 0000
0100 0000
0100 0000
0100 0000
0100 0000
0100 0000
0100 0000
0100 0000
以下同じ
*/
const Bitboard FileABB = 0x0101010101010101ULL;
const Bitboard FileBBB = FileABB << 1;
const Bitboard FileCBB = FileABB << 2;
const Bitboard FileDBB = FileABB << 3;
const Bitboard FileEBB = FileABB << 4;
const Bitboard FileFBB = FileABB << 5;
const Bitboard FileGBB = FileABB << 6;
const Bitboard FileHBB = FileABB << 7;
/*
行を特定するためのbitboard
Stockfishは通常のchessの行表記と逆さまになっている
通常のchess表記			Stockfishでの表記
BLACK SIDE					WHITE SIDE
8										1
7										2
6										3
5										4
4										5
3										6
2										7
1										8
WHITE SIDE					BLACK

Rank1BB=
1111 1111	下位bit	
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000	上位bit
*/
const Bitboard Rank1BB = 0xFF;
const Bitboard Rank2BB = Rank1BB << (8 * 1);
const Bitboard Rank3BB = Rank1BB << (8 * 2);
const Bitboard Rank4BB = Rank1BB << (8 * 3);
const Bitboard Rank5BB = Rank1BB << (8 * 4);
const Bitboard Rank6BB = Rank1BB << (8 * 5);
const Bitboard Rank7BB = Rank1BB << (8 * 6);
const Bitboard Rank8BB = Rank1BB << (8 * 7);
/*
アライメントを64bitに揃える
*/
CACHE_LINE_ALIGNMENT
/*
RMasks本体はbitboard.cppに宣言されている
初期化はBoards::init関数でされる
RMasks[SQ_A1]で出力されるbitboard
これからSQ_A1にいるROOKの利き用のマスクであることが分かる
最後の座標（SQ_H1,SQ_A8）までマスキングされていないのは
そこに利きが利いていようがいまいが関係ないから
(コンピュータ将棋における Magic Bitboard の提案と実装--参照)
このRMasksはMagic bitboardの計算の時に使用する
+---+---+---+---+---+---+---+---+
|   | X | X | X | X | X | X |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
RMasks[SQ_B2]で出力されるbitboard
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X | X | X | X | X |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
RMasks[SQ_C3]で出力されるbitboard
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X | X | X | X |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
*/
extern Bitboard RMasks[SQUARE_NB];
/*
ROOK用のmagic bitboard
https://chessprogramming.wikispaces.com/Magic+Bitboards
*/
extern Bitboard RMagics[SQUARE_NB];
/*
ROOK用の利きbitboard
*/
extern Bitboard* RAttacks[SQUARE_NB];
/*
RShiftsはbitboardをその座標に応じてbitシフトしているが
用途不明
シフトする回数を座標ごとに整理してみたがよくわからない
規則性はあるようだが詳細不明
   A  B  C  D  E  F  G  H
1 20 21 21 21 21 21 21 20
2 21 22 22 22 22 22 22 21
3 21 22 22 22 22 22 22 21
4 21 22 22 22 22 22 22 21
5 21 22 22 22 22 22 22 21
6 21 22 22 22 22 22 22 21
7 21 22 22 22 22 22 22 21
8 20 21 21 21 21 21 21 20
*/
extern unsigned RShifts[SQUARE_NB];
/*
BMasks本体はbitboard.cppに宣言されている
初期化はBoards::init関数でされる
BMasks[SQ_A1]で出力されるbitboard
これからSQ_A1にいるBISHOPの利き用のマスクであることが分かる
最後の座標（SQ_H8）までマスキングされていないのは
そこに利きが利いていようがいまいが関係ないから
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   | X |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   | X |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
BMasks[SQ_B2]で出力されるbitboard
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   | X |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   | X |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
BMasks[SQ_C3]で出力されるbitboard
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   | X |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   | X |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
*/
extern Bitboard BMasks[SQUARE_NB];
/*
BISHOP用のmagic bitboard
*/
extern Bitboard BMagics[SQUARE_NB];
/*
BISHOP用の利きbitboard
*/
extern Bitboard* BAttacks[SQUARE_NB];
/*
BShiftsはbitboardをその座標に応じてbitシフトしているが
用途不明
シフトする回数を座標ごとに整理してみたがよくわからない
規則性はあるようだが詳細不明
   A  B  C  D  E  F  G  H
1 26 27 27 27 27 27 27 26
2 27 27 27 27 27 27 27 27
3 27 27 25 25 25 25 27 27 
4 27 27 25 23 23 25 27 27 
5 27 27 25 23 23 25 27 27 
6 27 27 25 25 25 25 27 27
7 27 27 27 27 27 27 27 27
8 26 27 27 27 27 27 27 26 
*/
extern unsigned BShifts[SQUARE_NB];
/*
座標の位置だけがbitが立っているbitboard
*/
extern Bitboard SquareBB[SQUARE_NB];
/*
FileABB,FileBBB,FileCBB,FileDBB,FileEBB,FileFBB,FileGBB,FileHBBを
配列にしたもの
*/
extern Bitboard FileBB[FILE_NB];
/*
Rank1BB,Rank2BB,Rank3BB,Rank4BB,Rank5BB,Rank6BB,Rank7BB,Rank8BBを
配列にしたもの
*/
extern Bitboard RankBB[RANK_NB];
/*
指定した列の両脇の
隣の列のbitboard
*/
extern Bitboard AdjacentFilesBB[FILE_NB];
/*
InFrontBBは指定した行より敵サイド領域(Front)のbitを立てる
*/
extern Bitboard InFrontBB[COLOR_NB][RANK_NB];
/*
非飛び駒（bishop,rook,queenを除く駒）が駒をとれる位置に
ビットが立ったbitboardの配列
*/
extern Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
/*
BetweenBB[s1][s2]はs1,s2のあいだにRook,bishopの利きがあるのbitboardを返す
*/
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
/*
LineBB[s1][s2]はs1, s2のあいだにRook, bishopの利きがあるのbitboardを返す
BetweenBBとよく似ているような印象を受けるがBetweenBBは指定した座標の間
LineBBはs1,s2をとおる直線
*/
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
/*
DistanceRingsBB変数には座標s1とs2の距離を入れている
但し列と行の距離の大きい方をいれる。
チェビシェフ距離を表している
*/
extern Bitboard DistanceRingsBB[SQUARE_NB][8];
/*
自分の位置から前方１列（Forward）のbitboardを返す
*/
extern Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
/*
PAWN駒の前３列にbitboardが立つことになる。PAWNの移動可能場所にbitが立ったもの
*/
extern Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
/*
PAWNが駒を取れる位置のbitboardを返す
*/
extern Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
/*
ROOK,BISHOP,QUEENの座標ごとの利きを入れておく
他に駒がない状態の時の利き
*/
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
/*
SquareDistance[SQ_A1][to]で出力させた
基準となる座標（A1)からの距離でrow,col大きい方を返す
01234567
11234567
22234567
33334567
44444567
55555567
66666667
77777777
SquareDistance[SQ_D2][to]で出力させた
基準となる座標（D2)からの距離でrow,col大きい方を返す
32111234
32101234
32111234
32222234
33333334
44444444
55555555
66666666
*/
extern int SquareDistance[SQUARE_NB][SQUARE_NB];
/*
盤の市松模様をbitboardにしたもの
Color of a Square
https://chessprogramming.wikispaces.com/Color+of+a+Square
+---+---+---+---+---+---+---+---+
| X |   | X |   | X |   | X |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   | X |   | X |
+---+---+---+---+---+---+---+---+
| X |   | X |   | X |   | X |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   | X |   | X |
+---+---+---+---+---+---+---+---+
| X |   | X |   | X |   | X |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   | X |   | X |
+---+---+---+---+---+---+---+---+
| X |   | X |   | X |   | X |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   | X |   | X |
+---+---+---+---+---+---+---+---+
*/
const Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL;

/// Overloads of bitwise operators between a Bitboard and a Square for testing
/// whether a given bit is set in a bitboard, and for setting and clearing bits.
/*
bitboardとsqの&演算子を定義してある
引数のbbと座標が重なっているbitboardを返す
*/
inline Bitboard operator&(Bitboard b, Square s) {
  return b & SquareBB[s];
}
/*
bitboardとsqの|=演算子を定義してある
引数のbbに座標のbitを追加したbitboardを返す
*/
inline Bitboard& operator|=(Bitboard& b, Square s) {
  return b |= SquareBB[s];
}
/*
bitboardとsqの^=演算子を定義してある
引数のbbに座標のbitとの排他演算した結果のbitboardを返す
*/
inline Bitboard& operator^=(Bitboard& b, Square s) {
  return b ^= SquareBB[s];
}
/*
bitboardとsqの｜演算子を定義してある
引数のbbに座標のbitとのOR演算した結果のbitboardを返す
*/
inline Bitboard operator|(Bitboard b, Square s) {
  return b | SquareBB[s];
}
/*
bitboardとsqの^演算子を定義してある
引数のbbに座標のbitとの排他演算した結果のbitboardを返す
*/
inline Bitboard operator^(Bitboard b, Square s) {
  return b ^ SquareBB[s];
}
/*
引数のbitboardが１bitしか立っていなかったら
0を返す、複数立っていたら非0を返す
*/
inline bool more_than_one(Bitboard b) {
  return b & (b - 1);
}
/*
座標s1,s2の距離を返す、行と列の距離の内大きいほうを返す
*/
inline int square_distance(Square s1, Square s2) {
  return SquareDistance[s1][s2];
}
/*
列の距離を返す
*/
inline int file_distance(Square s1, Square s2) {
  return abs(file_of(s1) - file_of(s2));
}
/*
行間の距離を返す
*/
inline int rank_distance(Square s1, Square s2) {
  return abs(rank_of(s1) - rank_of(s2));
}


/// shift_bb() moves bitboard one step along direction Delta. Mainly for pawns.
/*
C++テンプレートテクニックpage28にあるテンプレート引数に整数値を渡すである
使い方は
shift_bb<Up>(b)のように呼び出すとUpはDELTA_N（手番がWHITEのとき）を渡すことになる

DELTA_Nは８bit右シフトするので座標でいうと大きくなる
　A   B   C   D   E   F   G   H
 +---+---+---+---+---+---+---+---+
 1| × | × |   | × | × |   |   |   |
 +---+---+---+---+---+---+---+---+
 2|   |   |   |   |   |   |   |   |
 +---+---+---+---+---+---+---+---+
 3|   |   |   |   |   |   |   |   |
 +---+---+---+---+---+---+---+---+
 が、こうなる
 　A   B   C   D   E   F   G   H
	+---+---+---+---+---+---+---+---+
	1|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	2| × | × |   | × | × |   |   |   |
	+---+---+---+---+---+---+---+---+
	3|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	DELTA_Sはその反対
	DELTA_NEは
	+---+---+---+---+---+---+---+---+
	1| × | × |   | × | × | × | × | × |
	+---+---+---+---+---+---+---+---+
	2|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	3|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	が、こうなる
	　A   B   C   D   E   F   G   H
	 +---+---+---+---+---+---+---+---+
	 1|   |   |   |   |   |   |   |   |
	 +---+---+---+---+---+---+---+---+
	 2|   | × | × |   | × | × | × | × |
	 +---+---+---+---+---+---+---+---+
	 3|   |   |   |   |   |   |   |   |
	 +---+---+---+---+---+---+---+---+
	 */
template<Square Delta>
inline Bitboard shift_bb(Bitboard b) {

  return  Delta == DELTA_N  ?  b             << 8 : Delta == DELTA_S  ?  b             >> 8
        : Delta == DELTA_NE ? (b & ~FileHBB) << 9 : Delta == DELTA_SE ? (b & ~FileHBB) >> 7
        : Delta == DELTA_NW ? (b & ~FileABB) << 7 : Delta == DELTA_SW ? (b & ~FileABB) >> 9
        : 0;
}


/// rank_bb() and file_bb() take a file or a square as input and return
/// a bitboard representing all squares on the given file or rank.
/*
Rank1(=0)からRank8(=7)で指定したRankBBを返す
*/
inline Bitboard rank_bb(Rank r) {
  return RankBB[r];
}
/*
座標値を指定してその座標が所属する行のRankBBを返す
*/
inline Bitboard rank_bb(Square s) {
  return RankBB[rank_of(s)];
}
/*
列番号で指定したbitBoardを返す
*/
inline Bitboard file_bb(File f) {
  return FileBB[f];
}
/*
座標値を指定してその座標が所属する列のFileBBを返す
*/
inline Bitboard file_bb(Square s) {
  return FileBB[file_of(s)];
}


/// adjacent_files_bb() takes a file as input and returns a bitboard representing
/// all squares on the adjacent files.
/*
指定した列の隣の列が立っているbitboardを返す
*/
inline Bitboard adjacent_files_bb(File f) {
  return AdjacentFilesBB[f];
}


/// in_front_bb() takes a color and a rank as input, and returns a bitboard
/// representing all the squares on all ranks in front of the rank, from the
/// given color's point of view. For instance, in_front_bb(BLACK, RANK_3) will
/// give all squares on ranks 1 and 2.
/*
指定した行から敵側（どっちが敵側かは指定したカラーで判断）が
立っているbitboard
*/
inline Bitboard in_front_bb(Color c, Rank r) {
  return InFrontBB[c][r];
}


/// between_bb() returns a bitboard representing all squares between two squares.
/// For instance, between_bb(SQ_C4, SQ_F7) returns a bitboard with the bits for
/// square d5 and e6 set.  If s1 and s2 are not on the same line, file or diagonal,
/// 0 is returned.
/*
指定した座標間でそこにRook,Bishopの利きが存在すればその利きbitboardを返す
指定した座標はbitboardには含まれない
*/
inline Bitboard between_bb(Square s1, Square s2) {
  return BetweenBB[s1][s2];
}


/// forward_bb() takes a color and a square as input, and returns a bitboard
/// representing all squares along the line in front of the square, from the
/// point of view of the given color. Definition of the table is:
/// ForwardBB[c][s] = in_front_bb(c, s) & file_bb(s)
/*
指定した座標より前方のbitboardを返す
カラーによって前方の方向が変わる
*/
inline Bitboard forward_bb(Color c, Square s) {
  return ForwardBB[c][s];
}


/// pawn_attack_span() takes a color and a square as input, and returns a bitboard
/// representing all squares that can be attacked by a pawn of the given color
/// when it moves along its file starting from the given square. Definition is:
/// PawnAttackSpan[c][s] = in_front_bb(c, s) & adjacent_files_bb(s);
/*
指定した座標より前方のbitboardを返す
カラーによって前方の方向が変わる
*/
inline Bitboard pawn_attack_span(Color c, Square s) {
  return PawnAttackSpan[c][s];
}


/// passed_pawn_mask() takes a color and a square as input, and returns a
/// bitboard mask which can be used to test if a pawn of the given color on
/// the given square is a passed pawn. Definition of the table is:
/// PassedPawnMask[c][s] = pawn_attack_span(c, s) | forward_bb(c, s)
/*
Pawnが前進または駒をとるbitboardを返す
PassedPawnMask配列はPAWNの前進方向に３列にbitが立ったもので
指定したカラー、座標を指定するとその座標にいるPAWNの移動可能範囲
をbitboardで返す（可能なだけで実際に行けるかはその局面とのbitboardとの
ANDが必要）
*/
inline Bitboard passed_pawn_mask(Color c, Square s) {
  return PassedPawnMask[c][s];
}


/// squares_of_color() returns a bitboard representing all squares with the same
/// color of the given square.
/*
用途不明
*/
inline Bitboard squares_of_color(Square s) {
  return DarkSquares & s ? DarkSquares : ~DarkSquares;
}


/// aligned() returns true if the squares s1, s2 and s3 are aligned
/// either on a straight or on a diagonal line.
/*
座標s1,s2の間のRook またはBishopの利きbitboardと　座標s3の
bitboardとのANDなのでs3がs1とs2の間の利きを遮断しているかの判断ができる
*/
inline bool aligned(Square s1, Square s2, Square s3) {
  return LineBB[s1][s2] & s3;
}


/// Functions for computing sliding attack bitboards. Function attacks_bb() takes
/// a square and a bitboard of occupied squares as input, and returns a bitboard
/// representing all squares attacked by Pt (bishop or rook) on the given square.
/*
テンプレート関数でpiece typeを指定できる
インスタンス化は
magic_index<ROOK>
magic_index<BISHOP>
の２つのみ
*/
template<PieceType Pt>
FORCE_INLINE unsigned magic_index(Square s, Bitboard occ) {

  Bitboard* const Masks  = Pt == ROOK ? RMasks  : BMasks;
  Bitboard* const Magics = Pt == ROOK ? RMagics : BMagics;
  unsigned* const Shifts = Pt == ROOK ? RShifts : BShifts;

  if (Is64Bit)
      return unsigned(((occ & Masks[s]) * Magics[s]) >> Shifts[s]);

  unsigned lo = unsigned(occ) & unsigned(Masks[s]);
  unsigned hi = unsigned(occ >> 32) & unsigned(Masks[s] >> 32);
  return (lo * unsigned(Magics[s]) ^ hi * unsigned(Magics[s] >> 32)) >> Shifts[s];
}
/*
飛び駒用の利きbitboard
<例>
sにrookがいる時のRAttacksが返すbitboardの
RAttacksはbitboard配列,初期化はinit_magics関数で
行う
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
| X | X | X | X | s | X | X | X |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
＜例＞
sにbishopがいるときのBAttacksが返すbitboard

+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   | X |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   | X |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   | X |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | s |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   | X |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X |   |   |   | X |   |   |   |
+---+---+---+---+---+---+---+---+
*/
template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occ) {
  return (Pt == ROOK ? RAttacks : BAttacks)[s][magic_index<Pt>(s, occ)];
}


/// lsb()/msb() finds the least/most significant bit in a nonzero bitboard.
/// pop_lsb() finds and clears the least significant bit in a nonzero bitboard.

#ifdef USE_BSFQ

#  if defined(_MSC_VER) && !defined(__INTEL_COMPILER)

FORCE_INLINE Square lsb(Bitboard b) {
  unsigned long index;
  _BitScanForward64(&index, b);
  return (Square) index;
}

FORCE_INLINE Square msb(Bitboard b) {
  unsigned long index;
  _BitScanReverse64(&index, b);
  return (Square) index;
}

#  elif defined(__arm__)

FORCE_INLINE int lsb32(uint32_t v) {
  __asm__("rbit %0, %1" : "=r"(v) : "r"(v));
  return __builtin_clz(v);
}

FORCE_INLINE Square msb(Bitboard b) {
  return (Square) (63 - __builtin_clzll(b));
}

FORCE_INLINE Square lsb(Bitboard b) {
  return (Square) (uint32_t(b) ? lsb32(uint32_t(b)) : 32 + lsb32(uint32_t(b >> 32)));
}

#  else

FORCE_INLINE Square lsb(Bitboard b) { // Assembly code by Heinz van Saanen
  Bitboard index;
  __asm__("bsfq %1, %0": "=r"(index): "rm"(b) );
  return (Square) index;
}

FORCE_INLINE Square msb(Bitboard b) {
  Bitboard index;
  __asm__("bsrq %1, %0": "=r"(index): "rm"(b) );
  return (Square) index;
}

#  endif

FORCE_INLINE Square pop_lsb(Bitboard* b) {
  const Square s = lsb(*b);
  *b &= *b - 1;
  return s;
}

#else // if defined(USE_BSFQ)
/*
上位bitからスキャンしてbitが立っているindexを返す、0オリジン
*/
extern Square msb(Bitboard b);
/*
下位bitからスキャンbitが立っているindexを返す、0オリジン
*/
extern Square lsb(Bitboard b);
/*
下位bitからスキャンbitが立っているindexを返す、0オリジン
同時にそのbitを消す
*/
extern Square pop_lsb(Bitboard* b);

#endif

/// frontmost_sq() and backmost_sq() find the square corresponding to the
/// most/least advanced bit relative to the given color.
/*
ｍｓｂは最上位bit,LSBは最下位bit
WHITE側のbitのスキャンを上位bitから開始する
BLACK側のbitのスキャンを下位bitから開始することです
*/
inline Square frontmost_sq(Color c, Bitboard b) { return c == WHITE ? msb(b) : lsb(b); }
inline Square  backmost_sq(Color c, Bitboard b) { return c == WHITE ? lsb(b) : msb(b); }

//I am append
//void printBSFTable();

#endif // #ifndef BITBOARD_H_INCLUDED
