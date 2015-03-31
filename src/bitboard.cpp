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

#include <algorithm>
#include <cstring>
#include <iostream>

#include "bitboard.h"
#include "bitcount.h"
#include "misc.h"
#include "rkiss.h"

using namespace std;

CACHE_LINE_ALIGNMENT

Bitboard RMasks[SQUARE_NB];
Bitboard RMagics[SQUARE_NB];
Bitboard* RAttacks[SQUARE_NB];
unsigned RShifts[SQUARE_NB];

Bitboard BMasks[SQUARE_NB];
Bitboard BMagics[SQUARE_NB];
Bitboard* BAttacks[SQUARE_NB];
unsigned BShifts[SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard FileBB[FILE_NB];
Bitboard RankBB[RANK_NB];
Bitboard AdjacentFilesBB[FILE_NB];
Bitboard InFrontBB[COLOR_NB][RANK_NB];
/*
非飛び駒（bishop,rook,queenを除く駒）が駒をとれる位置に
ビットが立ったbitboardの配列
*/
Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard DistanceRingsBB[SQUARE_NB][8];
Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
/*
ROOK,BISHOP,QUEENの座標ごとの利きを入れておく
他に駒がない状態の時の利き
*/
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

int SquareDistance[SQUARE_NB][SQUARE_NB];

namespace {

  // De Bruijn sequences. See chessprogramming.wikispaces.com/BitScan
	/*
	https://chessprogramming.wikispaces.com/BitScan
	2進数だと
	0011 1111 0111 1001 1101 0111 0001 1011 0100 1100 1011 0000 1010 1000 1001
	*/
  const uint64_t DeBruijn_64 = 0x3F79D71B4CB0A89ULL;
	/*
	https://chessprogramming.wikispaces.com/Looking+for+Magics
	*/
  const uint32_t DeBruijn_32 = 0x783A9B23;

  CACHE_LINE_ALIGNMENT
	/*
	MS1BTable配列はmsb（上位ビットからBitScanする）をハートウエア命令ではなくソフトウエアで行うときに
	必要になってくる配列で、添え字のbitboard値から上位側からScanしたときの最初のbitが立っているindexを
	返す（0オリジン）
	*/
  int MS1BTable[256];
	/*
	bsf_index関数の返り値を添え字にして、そのbitが立っている座標を返す
	BSFTable配列はMS1BTable配列と違って下位側からスキャンした座標が返る
	*/
  Square BSFTable[SQUARE_NB];
	/*
	ROOKの全座標位置での全駒配置パターンごとのbitboard配列
	*/
  Bitboard RTable[0x19000]; // Storage space for rook attacks
	/*
	BISHIOPの全座標位置での全駒配置パターンごとのbitboard配列
	*/
	Bitboard BTable[0x1480];  // Storage space for bishop attacks

  typedef unsigned (Fn)(Square, Bitboard);

  void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[],
                   Bitboard masks[], unsigned shifts[], Square deltas[], Fn index);
  /*
  bit scanしているが仕組みがよくわからん
  ここに解説らしきものが書かれているが読めない
  https://chessprogramming.wikispaces.com/De+Bruijn+Sequence+Generator
	わかっているのはBSFTable配列と組み合わせて使い、このbsf_index関数にbitboardを渡したら
	BSFTable配列経由でbitboardのindexを返してくる
	返してくるのは下位bitからスキャンして最初にbitが立っているindex（0オリジン）
	たとえば0x03は２進数で0011なので0を返してくる、0x02は0010なので1を返してくる
	なお0x00はbitが立っていないので63を返してくる
  */
  FORCE_INLINE unsigned bsf_index(Bitboard b) {

    // Matt Taylor's folding for 32 bit systems, extended to 64 bits by Kim Walisch
    b ^= (b - 1);
    return Is64Bit ? (b * DeBruijn_64) >> 58
                   : ((unsigned(b) ^ unsigned(b >> 32)) * DeBruijn_32) >> 26;
  }
}

/// lsb()/msb() finds the least/most significant bit in a nonzero bitboard.
/// pop_lsb() finds and clears the least significant bit in a nonzero bitboard.
/*
BitScanをソフトウエアで行うときはここが有効になる、ハードウエアで行う場合はbitboard.hに
ある_BitScanForward64命令などを使った関数を使う。

*/
#ifndef USE_BSFQ

Square lsb(Bitboard b) { return BSFTable[bsf_index(b)]; }

/*
pop_lsbは下位ビットから数え始めて最初の1bitのインデックスを返す,indexは0から始まる
その数えたbitは抜いてしまい(pop)0にする
*/
Square pop_lsb(Bitboard* b) {

  Bitboard bb = *b;
  *b = bb & (bb - 1);
  return BSFTable[bsf_index(bb)];
}

/*
void printBSFTable()
{
	cout << "BSFTable" << endl;
	for (int i = 0; i < 64; i++){
		cout << " " << BSFTable[i];
	}
}
*/

/*
MS1BTableはこのような数列になる
00112222333333334444444444444444
55555555555555555555555555555555
66666666666666666666666666666666
66666666666666666666666666666666
77777777777777777777777777777777
77777777777777777777777777777777
77777777777777777777777777777777
77777777777777777777777777777777
最初の0-8indexを取って対応する数値と比較してみる
MS1BTable	10進数		16進数		2進数
0					0				0x00		0000 0000
0					1				0x01		0000 0001
1					2				0x02		0000 0010
1					3				0x03		0000 0011
2					4				0x04		0000 0100
2					5				0x05		0000 0100
2					6				0x06		0000 0110
2					7				0x07		0000 0111
3					8				0x08		0000 1000
上の例でわかるようにMS1BTableは上位bitからスキャンして最初のbitのindexを返す
indexは0-オリジン

msbは渡されたbit列(bitboard)の先頭のbit位置を返す、但し、最下位bitの位置は0とする
bitが立っていない時も0を返す
																								16進数			2進数										msbの返す値
cout << "msb :" << endl;
cout << "msb(0x00000)" << msb(0x00000) << endl;->0x00000->0 0000 0000 0000 0000		0
cout << "msb(0x00001)" << msb(0x00001) << endl;->0x00001->0 0000 0000 0000 0001		0
cout << "msb(0x00010)" << msb(0x00010) << endl;->0x00010->0 0000 0000 0001 0000		4
cout << "msb(0x00100)" << msb(0x00100) << endl;->0x00100->0 0000 0001 0000 0000		8
cout << "msb(0x01000)" << msb(0x01000) << endl;->0x01000->0 0000 0000 0000 0000		12
cout << "msb(0x10000)" << msb(0x10000) << endl;->0x10000->1 0000 0000 0000 0000		16
*/
Square msb(Bitboard b) {

  unsigned b32;
  int result = 0;

	/*
	bが32bitより大きいようであれば32右シフトする
	*/
  if (b > 0xFFFFFFFF)
  {
      b >>= 32;
      result = 32;
  }

  b32 = unsigned(b);
	/*
	bが16bitより大きいようであれば16右シフトする
	*/
  if (b32 > 0xFFFF)
  {
      b32 >>= 16;
      result += 16;
  }
	/*
	bが8bitより大きいようであれば8右シフトする
	*/
	if (b32 > 0xFF)
  {
      b32 >>= 8;
      result += 8;
  }

  return (Square)(result + MS1BTable[b32]);
}

#endif // ifndef USE_BSFQ


/// Bitboards::print() prints a bitboard in an easily readable format to the
/// standard output. This is sometimes useful for debugging.

/*
引数のbiboardを表示する
ただしstackfishの内部盤データは行が通常のchess盤と上下逆さまになっている
のでここで元に戻している
＜表示例＞
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
| X | X | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
|   |   | X |   |   |   |   |   |
+---+---+---+---+---+---+---+---+
*/

void Bitboards::print(Bitboard b) {

  sync_cout;
	//for (Rank rank = RANK_8; rank >= RANK_1; --rank) 本来のプログラム、内部構造をそのまま出すように改造
  for (Rank rank = RANK_1; rank <= RANK_8; ++rank)
  {
      std::cout << "+---+---+---+---+---+---+---+---+" << '\n';

      for (File file = FILE_A; file <= FILE_H; ++file)
          std::cout << "| " << (b & (file | rank) ? "X " : "  ");

      std::cout << "|\n";
  }
  std::cout << "+---+---+---+---+---+---+---+---+" << sync_endl;
}


/// Bitboards::init() initializes various bitboard arrays. It is called during
/// program initialization.

/*
各種Bitboardを初期設定している
*/
void Bitboards::init() {
	/*
	MS1BTableは
	このような数列になる
	00112222333333334444444444444444
	55555555555555555555555555555555
	66666666666666666666666666666666
	66666666666666666666666666666666
	77777777777777777777777777777777
	77777777777777777777777777777777
	77777777777777777777777777777777
	77777777777777777777777777777777
	サイズの関係で横は３２列取っている
	行は８行ある32*8＝256
	0 -- 2
	1 -- 2
	2 -- 4
	3 -- 8
	4 -- 16
	5 -- 32
	6 -- 64
	7 -- 128
	最初の０を除外すると前の数の２倍づつになっている
	*/
  for (int k = 0, i = 0; i < 8; ++i)
      while (k < (2 << i))
          MS1BTable[k++] = i;
	/*
	BSFTableはbdfの値と駒座標を結ぶテーブル？
	0 47  1 56 48 27  2 60
	57 49 41 37 28 16  3 61
	54 58 35 52 50 42 21 44
	38 32 29 23 17 11  4 62
	46 55 26 59 40 36 15 53
	34 51 20 43 31 22 10 45
	25 39 14 33 19 30  9 24
	13 18  8 12  7  6  5 63
	こう表示される
	*/
  for (int i = 0; i < 64; ++i)
      BSFTable[bsf_index(1ULL << i)] = Square(i);
	/*
	SquareBBは指定した座標にbitが立っているbitboard
	SquareBB[SQ_A1]の表示、内部データとは上下逆さま

	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	*/
  for (Square s = SQ_A1; s <= SQ_H8; ++s)
      SquareBB[s] = 1ULL << s;

	/*
	FileBB[FILE_A]
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
	| X |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+  */	

  FileBB[FILE_A] = FileABB;
	/*
	RankBB[RANK_1]
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+	*/

  RankBB[RANK_1] = Rank1BB;

  for (int i = 1; i < 8; ++i)
  {
      FileBB[i] = FileBB[i - 1] << 1;
      RankBB[i] = RankBB[i - 1] << 8;
  }

	/*
	隣の列のbitboardを返す
	AdjacentFilesBB[FIle_B]の例
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X |   | X |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+  */
	for (File f = FILE_A; f <= FILE_H; ++f)
      AdjacentFilesBB[f] = (f > FILE_A ? FileBB[f - 1] : 0) | (f < FILE_H ? FileBB[f + 1] : 0);
	/*
	InFrontBBは指定した行より敵サイド領域のbitを立てる
	InFrontBB[WHITE][RANK_2]の例

	WHITE
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	BLACK

	InFrontBB[BLACK][RANK_3]の例
	WHITE
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	BLACK
	*/
  for (Rank r = RANK_1; r < RANK_8; ++r)
      InFrontBB[WHITE][r] = ~(InFrontBB[BLACK][r + 1] = InFrontBB[BLACK][r] | RankBB[r]);
	/*
	ForwardBB
	自分の位置から前方（Forward）のbitboardを返す
	＜例＞
	ForwardBB[color=WHITE][s=SQ_A1]
	+---+---+---+---+---+---+---+---+
	| S |   |   |   |   |   |   |   |
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
	| X |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+

	ForwardBB[color=BLACK][s=SQ_F7]
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | S |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	PawnAttackSpanの確認
	PAWNが駒を取れる位置のbitboardを返す
	＜例＞
	PawnAttackSpan[WHITE][s=SQ_C1]
	WHITE
	+---+---+---+---+---+---+---+---+
	|   |   | s |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	BLACK

	PawnAttackSpan[BLACK][SQ_F8]
	WHITE
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | X |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   | s |   |   |
	+---+---+---+---+---+---+---+---+
	BLACK

	PassedPawnMaskの確認
	PassedPawnMaskはForwardBBビットボードとPawnAttackSpanビットボードのOR演算なので
	PAWN駒の３列にbitboardが立つことになる。PAWNの移動可能場所にbitが立ったもの
	＜例＞
	PassedPawnMask[WHITE][s=SQ_E1]
	WHITE
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | s |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	BLACK

	PassedPawnMask[BLACK][SQ_D7]
	WHITE
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | s |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	BLACK
	*/

  for (Color c = WHITE; c <= BLACK; ++c)
      for (Square s = SQ_A1; s <= SQ_H8; ++s)
      {
          ForwardBB[c][s]      = InFrontBB[c][rank_of(s)] & FileBB[file_of(s)];
          PawnAttackSpan[c][s] = InFrontBB[c][rank_of(s)] & AdjacentFilesBB[file_of(s)];
          PassedPawnMask[c][s] = ForwardBB[c][s] | PawnAttackSpan[c][s];
      }

	/*
	SquareDistance変数の初期化
	SquareDistance変数には座標s1とs2の距離を入れている
	但し列と行の距離の大きい方をいれる。
	DistanceRingsBB変数の初期化
	DistanceRingsBBはチェビシェフ距離を表している
	*/
  for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
      for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
      {
          SquareDistance[s1][s2] = std::max(file_distance(s1, s2), rank_distance(s1, s2));
          if (s1 != s2)
             DistanceRingsBB[s1][SquareDistance[s1][s2] - 1] |= s2;
      }

	/*
	StepAttacksBB変数の初期設定
	宣言 Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
	非飛び駒（bishop,rook,queenを除く駒）が駒をとれる位置に
	ビットが立ったbitboardの配列
	<例>
	piece=W_KING,sq=SQ_D3
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | s | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	*/
  int steps[][9] = { {}, { 7, 9 }, { 17, 15, 10, 6, -6, -10, -15, -17 },
                     {}, {}, {}, { 9, 7, -7, -9, 8, 1, -1, -8 } };

  for (Color c = WHITE; c <= BLACK; ++c)
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          for (Square s = SQ_A1; s <= SQ_H8; ++s)
              for (int k = 0; steps[pt][k]; ++k)
              {
                  Square to = s + Square(c == WHITE ? steps[pt][k] : -steps[pt][k]);

                  if (is_ok(to) && square_distance(s, to) < 3)
                      StepAttacksBB[make_piece(c, pt)][s] |= to;
              }
	/*
	RDeltas=ROOKの方向子
	BDeltas=BISHOPの方向子
	*/
  Square RDeltas[] = { DELTA_N,  DELTA_E,  DELTA_S,  DELTA_W  };
  Square BDeltas[] = { DELTA_NE, DELTA_SE, DELTA_SW, DELTA_NW };
	/*
	マジックナンバーの初期化
	*/
  init_magics(RTable, RAttacks, RMagics, RMasks, RShifts, RDeltas, magic_index<ROOK>);
  init_magics(BTable, BAttacks, BMagics, BMasks, BShifts, BDeltas, magic_index<BISHOP>);
	/*
	PseudoAttacks[ROOK], PseudoAttacks[BISHOP], PseudoAttacks[QUEEN]に他の駒が
	ない状態の利きbitboardをいれる。

	PseudoAttacks[BISHOP][s1] & s2　->bitboardとsquareのAND演算は演算子オーバーロードに
	定義してある。（bitboard.hに定義してある　PseudoAttacks[BISHOP][s1] & s2は
	PseudoAttacks[BISHOP][s1] & SquareBB[s2]となる）

	LineBB[s1][s2]はs1, s2のあいだにRook, bishopの利きがあるのbitboardを返す
	分かりにくいので例を示す
	<例>
	s1 = SQ_A1, s2 = SQ_B2のLineBBは
	駒の配置
	+---+---+---+---+---+---+---+---+
	|s1 |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |s2 |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	返すbitboard
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   |   |   |
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
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+

	駒の配置
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |s1 |   |   |   |s2 |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	返すbitboard
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+

	駒の配置
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |s1 |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |S2 |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	返すbitboard,s1とs2の間に利きが（Rook,bishop)が
	存在しない場合は空のbitboardを返す
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+


	BetweenBB[s1][s2]はs1,s2のあいだにRook,bishopの利きがあるのbitboardを返す
	分かりにくいので例を示す
	<例>
	s1=SQ_A1,s2=SQ_B2のBetweenBBは
	駒の配置
	+---+---+---+---+---+---+---+---+
	|s1 |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |s2 |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	返すbitboards1とs2の間には遠方利きがないでの
	駒同士には利きが利いているが空きの升には
	利きがないのでこのようなbitboardをかえす
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+

	駒の配置
	+---+---+---+---+---+---+---+---+
	|s1 |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |s2 |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	s1とs2の間にある利きを返す
	Betweenは何何のあいだという意味がある
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   | x |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+

	駒の配置
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |s1 |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |s2 |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	このような駒配置の場合rook,bishopの利きが
	存在しないのでbitboardもbitがたたない（表示省略）
	*/
		for (Square s = SQ_A1; s <= SQ_H8; ++s)
  {
      PseudoAttacks[QUEEN][s]  = PseudoAttacks[BISHOP][s] = attacks_bb<BISHOP>(s, 0);
      PseudoAttacks[QUEEN][s] |= PseudoAttacks[  ROOK][s] = attacks_bb<  ROOK>(s, 0);
  }

  for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
      for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
          if (PseudoAttacks[QUEEN][s1] & s2)
          {
              Square delta = (s2 - s1) / square_distance(s1, s2);

              for (Square s = s1 + delta; s != s2; s += delta)
                  BetweenBB[s1][s2] |= s;

              PieceType pt = (PseudoAttacks[BISHOP][s1] & s2) ? BISHOP : ROOK;
              LineBB[s1][s2] = (PseudoAttacks[pt][s1] & PseudoAttacks[pt][s2]) | s1 | s2;
          }
	/*test start*/
	/*
	BSFTable配列
	*/
	Bitboard bb;
	printf("BSFTable\n");

	bb = 0x00;
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:63
	bb = 0x01;		//２進数で 0001
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:0
	bb = 0x02;		//２進数で 0010
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:1
	bb = 0x03;		//２進数で 0011
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:0
	bb = 0x04;		//２進数で 0100
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:2
	bb = 0x05;		//２進数で 0101
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:0
	bb = 0x06;		//２進数で 0110
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//返してきた数は:1

	int j;
	/*test end*/
}


namespace {
/*
与えられた座標から、与えられた方向子によって四方向に利きを伸ばし
盤外でないことをチエックしつつ、その軌跡をbitboard attack変数に残していく。
他の駒に当たったらそこでその方向はやめる
つまり利きをスライドしている
*/
  Bitboard sliding_attack(Square deltas[], Square sq, Bitboard occupied) {

    Bitboard attack = 0;

    for (int i = 0; i < 4; ++i)
        for (Square s = sq + deltas[i];is_ok(s) && square_distance(s, s - deltas[i]) == 1;s += deltas[i])
						{
							attack |= s;
							if (occupied & s)
								break;
						}

    return attack;
  }


  Bitboard pick_random(RKISS& rk, int booster) {

    // Values s1 and s2 are used to rotate the candidate magic of a
    // quantity known to be the optimal to quickly find the magics.
		/*
		boosterの数値を使って（乱数のシード？）乱数を作る
		*/
    int s1 = booster & 63, s2 = (booster >> 6) & 63;

    Bitboard m = rk.rand<Bitboard>();
    m = (m >> s1) | (m << (64 - s1));
    m &= rk.rand<Bitboard>();
    m = (m >> s2) | (m << (64 - s2));
    return m & rk.rand<Bitboard>();
  }


  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // chessprogramming.wikispaces.com/Magic+Bitboards. In particular, here we
  // use the so called "fancy" approach.
	/*
	table=>RTable[0x19000]配列はなぜこんなに大きな配列なのか
	magic tableなので sにRookがいる場合 X に駒がある/ないのパターンは
	6bit＝64とおり（最後の升目は不要）なので
	横×縦＝64*64=4096とおり、これが４隅あるので
	4096*4=16,384
	+---+---+---+---+---+---+---+---+
	| S | X | X | X | X | X | X | ○ |
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
	| ○ |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	辺に駒がいた場合
	横方向は5bitなので32とおり縦は6bitのなので
	64とおり,32*64=2,048
	辺に駒をおける場所（隅を除く）２４か所なので
	2048*24=49,152とおり
	+---+---+---+---+---+---+---+---+
	| ○ | X | X | S | X | X | X | ○ |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | ○ |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	隅、辺を除く升目は36
	縦、横とも5bitなので32とおり
	32*32*36=36,864
	+---+---+---+---+---+---+---+---+
	|   |   |   | ○ |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| ○ | X | X | s | X | X | X | ○ |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | ○ |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	16,384+49,152+36,864=102,400
	これを16進数に変換すると0x19000となる

	bishopの配列はBitboard BTable[0x1480]となって
	Rook程ではないが大きい配列
	bishopは座標によって利き数が大きく異なる
	升目に直接入れた数字がその升目での利き数
	+---+---+---+---+---+---+---+---+
	|128|///| 32|///| 32|///| 32|///|
	+---+---+---+---+---+---+---+---+
	|///| 32|///| 32|///| 32|///| 32|
	+---+---+---+---+---+---+---+---+
	| 32|///|128|///|128|///| 32|///|
	+---+---+---+---+---+---+---+---+
	|///| 32|///|512|///|128|///| 32|
	+---+---+---+---+---+---+---+---+
	| 32|///|128|///|512|///| 32|///|
	+---+---+---+---+---+---+---+---+
	|///| 32|///|128|///|128|///| 32|
	+---+---+---+---+---+---+---+---+
	| 32|///| 32|///| 32|///| 32|///|
	+---+---+---+---+---+---+---+---+
	|///| 32|///| 32|///| 32|///|128|
	+---+---+---+---+---+---+---+---+
	32が22＝704
	64が2＝128
	128が6＝768
	512が2＝1024
	2624となる
	色違いがあるので
	2624*2=5248=>１６進法に変換すると0x1480
	*/

  void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[],
                   Bitboard masks[], unsigned shifts[], Square deltas[], Fn index) {
		/*
		[0][8]が32bit[1][8]が64bit機
		*/
    int MagicBoosters[][8] = { { 3191, 2184, 1310, 3618, 2091, 1308, 2452, 3996 },
                               { 1059, 3608,  605, 3234, 3326,   38, 2029, 3043 } };
    RKISS rk;
    Bitboard occupancy[4096], reference[4096], edges, b;
    int i, size, booster;

    // attacks[s] is a pointer to the beginning of the attacks table for square 's'
    attacks[SQ_A1] = table;

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        // Board edges are not considered in the relevant occupancies
				/*
				edgesは指定の座標の上下の両端と左右の両端bitboardを作る
				たとえば座標sがSQ_A1なら次のように返す.このedgesはmask[sq]を作るためのヘルパー
				+---+---+---+---+---+---+---+---+
				| S |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   | X |
				+---+---+---+---+---+---+---+---+
				| X | X | X | X | X | X | X | X |
				+---+---+---+---+---+---+---+---+
				*/
        edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) | ((FileABB | FileHBB) & ~file_bb(s));
				/*
				printf("edges:%d\n", s);
				Bitboards::print(edges);
				*/
        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask. Hence we deduce the size of the shift to
        // apply to the 64 or 32 bits word to get the index.
				/*
				sliding_attack関数は渡したbitboardにおいて
				指定した座標から届く限りの利きのbitboardを作って返す。渡すbitboardは0なので盤内一杯の
				bitboardを返す。これをedgesと組み合わせることでmask bitboardができる
				shift配列をつくる,以下にshift配列の作り方を書くがこれは64bit機であることを前提にした
				説明でIs64Bit=trueであることを前提としている。
				CPW(chess programming wiki)からの説明を転記している
				
				bishopがb1にいて5bitの利きbitがあったする（左図）利き座標を便宜上C,D,E,F,Gとする
				これに適切なMagic Number（中図）を掛けて上位bitに単写像する（右図）
				現在利きbitは上位bitにあるのでbitCountするために右シフトして下位bitに下ろす
				これがshift配列の役目でどれだけ右シフトさせるのかはshift配列にはいっている
				計算式は一番右側で64-5となる
				プログラムでは(Is64Bit ? 64 : 32)=>64,popcount<Max15>(masks[s])=>5となる
				. . . . . . . .     . . . . . . . .     . . .[C D E F G]
				. . . . . . . .     . 1 . . . . . .     . . . . . . . .
				. . . . . . G .     . 1 . . . . . .     . . . . . . . .
				. . . . . F . .     . 1 . . . . . .     . . . . . . . .
				. . . . E . . .  *  . 1 . . . . . .  =  . . . . . . . .    >> (64- 5)
				. . . D . . . .     . 1 . . . . . .     . . . . . . . .
				. . C . . . . .     . . . . . . . .     . . . . . . . .
				. . . . . . . .     . . . . . . . .     . . . . . . . .
				*/
        masks[s]  = sliding_attack(deltas, s, 0) & ~edges;
        shifts[s] = (Is64Bit ? 64 : 32) - popcount<Max15>(masks[s]);

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding sliding attack bitboard in reference[].
				/*
				ここで使われているテクニックを解説しているページ
				https://chessprogramming.wikispaces.com/Traversing+Subsets+of+a+Set
				ここではいろんな駒配置に応じたreference[]を作る
				たとえば座標SQ_A1にROOKがいる場合、横方向に6bit(利きが利くのはB1,C1,D1,E1,F1,G1の座標）
				2^6とおりのパターンがある=64,縦方向に6bit(利きが利くのがA2,A3,A4,A5,A6,A7の座標）
				なんで2^64とおり全部で64 * 64 = 4096とおり
				この時の駒の配置を記録したのがoccupancy[],その時の利きのbitboardを記録したのが
				reference[]になる.
				6bit+6bitが最大bitになる。BISHOPでもこれを超えることはない。
				おそらくQUEENはROOKとBISHOPの組み合わせで処理しているので問題ないと思われる
				*/
        b = size = 0;
        do {
            occupancy[size] = b;
            reference[size++] = sliding_attack(deltas, s, b);
            b = (b - masks[s]) & masks[s];
						/*
						printf("Carry-Rippler:%d\n", s);
						Bitboards::print(b);
						Bitboards::print(occupancy[size-1]);
						Bitboards::print(reference[size-1]);
						*/
				} while (b);

        // Set the offset for the table of the next square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
				/*
				上のoccupancy[],reference[]はいつも64*64=4096必要なわけではなく４隅は4096必要だが
				端は2048とうり、行端、列端合わせて４か所なので2048*6*4＝49,152
				隅と端を除く中央部は36升で動くことのできるのは縦、横とも5bitなので2^5*2^5=1024
				全部合わせて4096*4+49152+1024*36=102400(0x19000)これがRTable配列の大きさになる
				つまりRTableはROOKの全ての座標での、全ての駒配置に対応した利きのbitboardを持っている
				*/
        if (s < SQ_H8)
            attacks[s + 1] = attacks[s] + size;
				/*
				32bit,64bitでは乱数シードは異なる？
				*/
        booster = MagicBoosters[Is64Bit][rank_of(s)];

        // Find a magic for square 's' picking up an (almost) random number
        // until we find the one that passes the verification test.
				/*
				おそらくmagic numberを乱数で生成している
				で、生成した乱数をmaskにかけて上位bitに写像している
				popcount<Max15>((magics[s] * masks[s]) >> 56) < 6のところは
				乱数生成したmagic　numberがチャンと写像していればそのbit数は6bitになるはずなので
				６より小さい場合はNGなので再度乱数を発生させ、チャンとしたmagic numberができるまで
				ループさせる。
				*/
        do {
            do magics[s] = pick_random(rk, booster);
            while (popcount<Max15>((magics[s] * masks[s]) >> 56) < 6);

            std::memset(attacks[s], 0, size * sizeof(Bitboard));

            // A good magic must map every possible occupancy to an index that
            // looks up the correct sliding attack in the attacks[s] database.
            // Note that we build up the database for square 's' as a side
            // effect of verifying the magic.
						/*
						attacks[s][index(s,occupancy[i])のindexはmagic_index関数ポインタです。
						magic_index関数は最大で12bitのbit列を最上位bitから最下位bitに写像する（単射）ので
						4096(0-4095)のインデックスを返す関数ともいえる
						それを座標ごとのアドレスにreference[](座標ごとのbitboard)を記録させている
						RTableのアドレスは座標と駒の配置によって決まるindexでそのindexにそのbitboard（利きbotboard)を記録させる
						*/
            for (i = 0; i < size; ++i)
            {
                Bitboard& attack = attacks[s][index(s, occupancy[i])];

                if (attack && attack != reference[i])
                    break;

                assert(reference[i] != 0);

                attack = reference[i];
            }
        } while (i != size);
    }
  }
}
