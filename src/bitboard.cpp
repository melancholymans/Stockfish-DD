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
���ы�ibishop,rook,queen��������j������Ƃ��ʒu��
�r�b�g��������bitboard�̔z��
*/
Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard DistanceRingsBB[SQUARE_NB][8];
Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
/*
ROOK,BISHOP,QUEEN�̍��W���Ƃ̗��������Ă���
���ɋ�Ȃ���Ԃ̎��̗���
*/
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

int SquareDistance[SQUARE_NB][SQUARE_NB];

namespace {

  // De Bruijn sequences. See chessprogramming.wikispaces.com/BitScan
	/*
	https://chessprogramming.wikispaces.com/BitScan
	2�i������
	0011 1111 0111 1001 1101 0111 0001 1011 0100 1100 1011 0000 1010 1000 1001
	*/
  const uint64_t DeBruijn_64 = 0x3F79D71B4CB0A89ULL;
	/*
	https://chessprogramming.wikispaces.com/Looking+for+Magics
	*/
  const uint32_t DeBruijn_32 = 0x783A9B23;

  CACHE_LINE_ALIGNMENT
	/*
	MS1BTable�z���msb�i��ʃr�b�g����BitScan����j���n�[�g�E�G�A���߂ł͂Ȃ��\�t�g�E�G�A�ōs���Ƃ���
	�K�v�ɂȂ��Ă���z��ŁA�Y������bitboard�l�����ʑ�����Scan�����Ƃ��̍ŏ���bit�������Ă���index��
	�Ԃ��i0�I���W���j
	*/
  int MS1BTable[256];
	/*
	bsf_index�֐��̕Ԃ�l��Y�����ɂ��āA����bit�������Ă�����W��Ԃ�
	BSFTable�z���MS1BTable�z��ƈ���ĉ��ʑ�����X�L�����������W���Ԃ�
	*/
  Square BSFTable[SQUARE_NB];
	/*
	ROOK�̑S���W�ʒu�ł̑S��z�u�p�^�[�����Ƃ�bitboard�z��
	*/
  Bitboard RTable[0x19000]; // Storage space for rook attacks
	/*
	BISHIOP�̑S���W�ʒu�ł̑S��z�u�p�^�[�����Ƃ�bitboard�z��
	*/
	Bitboard BTable[0x1480];  // Storage space for bishop attacks

  typedef unsigned (Fn)(Square, Bitboard);

  void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[],
                   Bitboard masks[], unsigned shifts[], Square deltas[], Fn index);
  /*
  bit scan���Ă��邪�d�g�݂��悭�킩���
  �����ɉ���炵�����̂�������Ă��邪�ǂ߂Ȃ�
  https://chessprogramming.wikispaces.com/De+Bruijn+Sequence+Generator
	�킩���Ă���̂�BSFTable�z��Ƒg�ݍ��킹�Ďg���A����bsf_index�֐���bitboard��n������
	BSFTable�z��o�R��bitboard��index��Ԃ��Ă���
	�Ԃ��Ă���͉̂���bit����X�L�������čŏ���bit�������Ă���index�i0�I���W���j
	���Ƃ���0x03�͂Q�i����0011�Ȃ̂�0��Ԃ��Ă���A0x02��0010�Ȃ̂�1��Ԃ��Ă���
	�Ȃ�0x00��bit�������Ă��Ȃ��̂�63��Ԃ��Ă���
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
BitScan���\�t�g�E�G�A�ōs���Ƃ��͂������L���ɂȂ�A�n�[�h�E�G�A�ōs���ꍇ��bitboard.h��
����_BitScanForward64���߂Ȃǂ��g�����֐����g���B

*/
#ifndef USE_BSFQ

Square lsb(Bitboard b) { return BSFTable[bsf_index(b)]; }

/*
pop_lsb�͉��ʃr�b�g���琔���n�߂čŏ���1bit�̃C���f�b�N�X��Ԃ�,index��0����n�܂�
���̐�����bit�͔����Ă��܂�(pop)0�ɂ���
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
MS1BTable�͂��̂悤�Ȑ���ɂȂ�
00112222333333334444444444444444
55555555555555555555555555555555
66666666666666666666666666666666
66666666666666666666666666666666
77777777777777777777777777777777
77777777777777777777777777777777
77777777777777777777777777777777
77777777777777777777777777777777
�ŏ���0-8index������đΉ����鐔�l�Ɣ�r���Ă݂�
MS1BTable	10�i��		16�i��		2�i��
0					0				0x00		0000 0000
0					1				0x01		0000 0001
1					2				0x02		0000 0010
1					3				0x03		0000 0011
2					4				0x04		0000 0100
2					5				0x05		0000 0100
2					6				0x06		0000 0110
2					7				0x07		0000 0111
3					8				0x08		0000 1000
��̗�ł킩��悤��MS1BTable�͏��bit����X�L�������čŏ���bit��index��Ԃ�
index��0-�I���W��

msb�͓n���ꂽbit��(bitboard)�̐擪��bit�ʒu��Ԃ��A�A���A�ŉ���bit�̈ʒu��0�Ƃ���
bit�������Ă��Ȃ�����0��Ԃ�
																								16�i��			2�i��										msb�̕Ԃ��l
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
	b��32bit���傫���悤�ł����32�E�V�t�g����
	*/
  if (b > 0xFFFFFFFF)
  {
      b >>= 32;
      result = 32;
  }

  b32 = unsigned(b);
	/*
	b��16bit���傫���悤�ł����16�E�V�t�g����
	*/
  if (b32 > 0xFFFF)
  {
      b32 >>= 16;
      result += 16;
  }
	/*
	b��8bit���傫���悤�ł����8�E�V�t�g����
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
������biboard��\������
������stackfish�̓����Ճf�[�^�͍s���ʏ��chess�ՂƏ㉺�t���܂ɂȂ��Ă���
�̂ł����Ō��ɖ߂��Ă���
���\���၄
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
	//for (Rank rank = RANK_8; rank >= RANK_1; --rank) �{���̃v���O�����A�����\�������̂܂܏o���悤�ɉ���
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
�e��Bitboard�������ݒ肵�Ă���
*/
void Bitboards::init() {
	/*
	MS1BTable��
	���̂悤�Ȑ���ɂȂ�
	00112222333333334444444444444444
	55555555555555555555555555555555
	66666666666666666666666666666666
	66666666666666666666666666666666
	77777777777777777777777777777777
	77777777777777777777777777777777
	77777777777777777777777777777777
	77777777777777777777777777777777
	�T�C�Y�̊֌W�ŉ��͂R�Q�����Ă���
	�s�͂W�s����32*8��256
	0 -- 2
	1 -- 2
	2 -- 4
	3 -- 8
	4 -- 16
	5 -- 32
	6 -- 64
	7 -- 128
	�ŏ��̂O�����O����ƑO�̐��̂Q�{�ÂɂȂ��Ă���
	*/
  for (int k = 0, i = 0; i < 8; ++i)
      while (k < (2 << i))
          MS1BTable[k++] = i;
	/*
	BSFTable��bdf�̒l�Ƌ���W�����ԃe�[�u���H
	0 47  1 56 48 27  2 60
	57 49 41 37 28 16  3 61
	54 58 35 52 50 42 21 44
	38 32 29 23 17 11  4 62
	46 55 26 59 40 36 15 53
	34 51 20 43 31 22 10 45
	25 39 14 33 19 30  9 24
	13 18  8 12  7  6  5 63
	�����\�������
	*/
  for (int i = 0; i < 64; ++i)
      BSFTable[bsf_index(1ULL << i)] = Square(i);
	/*
	SquareBB�͎w�肵�����W��bit�������Ă���bitboard
	SquareBB[SQ_A1]�̕\���A�����f�[�^�Ƃ͏㉺�t����

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
	�ׂ̗��bitboard��Ԃ�
	AdjacentFilesBB[FIle_B]�̗�
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
	InFrontBB�͎w�肵���s���G�T�C�h�̈��bit�𗧂Ă�
	InFrontBB[WHITE][RANK_2]�̗�

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

	InFrontBB[BLACK][RANK_3]�̗�
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
	�����̈ʒu����O���iForward�j��bitboard��Ԃ�
	���၄
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
	PawnAttackSpan�̊m�F
	PAWN���������ʒu��bitboard��Ԃ�
	���၄
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

	PassedPawnMask�̊m�F
	PassedPawnMask��ForwardBB�r�b�g�{�[�h��PawnAttackSpan�r�b�g�{�[�h��OR���Z�Ȃ̂�
	PAWN��̂R���bitboard�������ƂɂȂ�BPAWN�̈ړ��\�ꏊ��bit������������
	���၄
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
	SquareDistance�ϐ��̏�����
	SquareDistance�ϐ��ɂ͍��Ws1��s2�̋��������Ă���
	�A����ƍs�̋����̑傫�����������B
	DistanceRingsBB�ϐ��̏�����
	DistanceRingsBB�̓`�F�r�V�F�t������\���Ă���
	*/
  for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
      for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
      {
          SquareDistance[s1][s2] = std::max(file_distance(s1, s2), rank_distance(s1, s2));
          if (s1 != s2)
             DistanceRingsBB[s1][SquareDistance[s1][s2] - 1] |= s2;
      }

	/*
	StepAttacksBB�ϐ��̏����ݒ�
	�錾 Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
	���ы�ibishop,rook,queen��������j������Ƃ��ʒu��
	�r�b�g��������bitboard�̔z��
	<��>
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
	RDeltas=ROOK�̕����q
	BDeltas=BISHOP�̕����q
	*/
  Square RDeltas[] = { DELTA_N,  DELTA_E,  DELTA_S,  DELTA_W  };
  Square BDeltas[] = { DELTA_NE, DELTA_SE, DELTA_SW, DELTA_NW };
	/*
	�}�W�b�N�i���o�[�̏�����
	*/
  init_magics(RTable, RAttacks, RMagics, RMasks, RShifts, RDeltas, magic_index<ROOK>);
  init_magics(BTable, BAttacks, BMagics, BMasks, BShifts, BDeltas, magic_index<BISHOP>);
	/*
	PseudoAttacks[ROOK], PseudoAttacks[BISHOP], PseudoAttacks[QUEEN]�ɑ��̋
	�Ȃ���Ԃ̗���bitboard�������B

	PseudoAttacks[BISHOP][s1] & s2�@->bitboard��square��AND���Z�͉��Z�q�I�[�o�[���[�h��
	��`���Ă���B�ibitboard.h�ɒ�`���Ă���@PseudoAttacks[BISHOP][s1] & s2��
	PseudoAttacks[BISHOP][s1] & SquareBB[s2]�ƂȂ�j

	LineBB[s1][s2]��s1, s2�̂�������Rook, bishop�̗����������bitboard��Ԃ�
	������ɂ����̂ŗ������
	<��>
	s1 = SQ_A1, s2 = SQ_B2��LineBB��
	��̔z�u
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
	�Ԃ�bitboard
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

	��̔z�u
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
	�Ԃ�bitboard
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

	��̔z�u
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
	�Ԃ�bitboard,s1��s2�̊Ԃɗ������iRook,bishop)��
	���݂��Ȃ��ꍇ�͋��bitboard��Ԃ�
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


	BetweenBB[s1][s2]��s1,s2�̂�������Rook,bishop�̗����������bitboard��Ԃ�
	������ɂ����̂ŗ������
	<��>
	s1=SQ_A1,s2=SQ_B2��BetweenBB��
	��̔z�u
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
	�Ԃ�bitboards1��s2�̊Ԃɂ͉����������Ȃ��ł�
	��m�ɂ͗����������Ă��邪�󂫂̏��ɂ�
	�������Ȃ��̂ł��̂悤��bitboard��������
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

	��̔z�u
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
	s1��s2�̊Ԃɂ��闘����Ԃ�
	Between�͉����̂������Ƃ����Ӗ�������
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

	��̔z�u
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
	���̂悤�ȋ�z�u�̏ꍇrook,bishop�̗�����
	���݂��Ȃ��̂�bitboard��bit�������Ȃ��i�\���ȗ��j
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
	BSFTable�z��
	*/
	Bitboard bb;
	printf("BSFTable\n");

	bb = 0x00;
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:63
	bb = 0x01;		//�Q�i���� 0001
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:0
	bb = 0x02;		//�Q�i���� 0010
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:1
	bb = 0x03;		//�Q�i���� 0011
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:0
	bb = 0x04;		//�Q�i���� 0100
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:2
	bb = 0x05;		//�Q�i���� 0101
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:0
	bb = 0x06;		//�Q�i���� 0110
	printf("%d\n", BSFTable[bsf_index(bb)]);
	//�Ԃ��Ă�������:1

	int j;
	/*test end*/
}


namespace {
/*
�^����ꂽ���W����A�^����ꂽ�����q�ɂ���Ďl�����ɗ�����L�΂�
�ՊO�łȂ����Ƃ��`�G�b�N���A���̋O�Ղ�bitboard attack�ϐ��Ɏc���Ă����B
���̋�ɓ��������炻���ł��̕����͂�߂�
�܂藘�����X���C�h���Ă���
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
		booster�̐��l���g���āi�����̃V�[�h�H�j���������
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
	table=>RTable[0x19000]�z��͂Ȃ�����Ȃɑ傫�Ȕz��Ȃ̂�
	magic table�Ȃ̂� s��Rook������ꍇ X �ɋ����/�Ȃ��̃p�^�[����
	6bit��64�Ƃ���i�Ō�̏��ڂ͕s�v�j�Ȃ̂�
	���~�c��64*64=4096�Ƃ���A���ꂪ�S������̂�
	4096*4=16,384
	+---+---+---+---+---+---+---+---+
	| S | X | X | X | X | X | X | �� |
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
	| �� |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	�ӂɋ�����ꍇ
	��������5bit�Ȃ̂�32�Ƃ���c��6bit�̂Ȃ̂�
	64�Ƃ���,32*64=2,048
	�ӂɋ��������ꏊ�i���������j�Q�S�����Ȃ̂�
	2048*24=49,152�Ƃ���
	+---+---+---+---+---+---+---+---+
	| �� | X | X | S | X | X | X | �� |
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
	|   |   |   | �� |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	���A�ӂ��������ڂ�36
	�c�A���Ƃ�5bit�Ȃ̂�32�Ƃ���
	32*32*36=36,864
	+---+---+---+---+---+---+---+---+
	|   |   |   | �� |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	| �� | X | X | s | X | X | X | �� |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | �� |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	16,384+49,152+36,864=102,400
	�����16�i���ɕϊ������0x19000�ƂȂ�

	bishop�̔z���Bitboard BTable[0x1480]�ƂȂ���
	Rook���ł͂Ȃ����傫���z��
	bishop�͍��W�ɂ���ė��������傫���قȂ�
	���ڂɒ��ړ��ꂽ���������̏��ڂł̗�����
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
	32��22��704
	64��2��128
	128��6��768
	512��2��1024
	2624�ƂȂ�
	�F�Ⴂ������̂�
	2624*2=5248=>�P�U�i�@�ɕϊ������0x1480
	*/

  void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard magics[],
                   Bitboard masks[], unsigned shifts[], Square deltas[], Fn index) {
		/*
		[0][8]��32bit[1][8]��64bit�@
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
				edges�͎w��̍��W�̏㉺�̗��[�ƍ��E�̗��[bitboard�����
				���Ƃ��΍��Ws��SQ_A1�Ȃ玟�̂悤�ɕԂ�.����edges��mask[sq]����邽�߂̃w���p�[
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
				sliding_attack�֐��͓n����bitboard�ɂ�����
				�w�肵�����W����͂�����̗�����bitboard������ĕԂ��B�n��bitboard��0�Ȃ̂ŔՓ���t��
				bitboard��Ԃ��B�����edges�Ƒg�ݍ��킹�邱�Ƃ�mask bitboard���ł���
				shift�z�������,�ȉ���shift�z��̍����������������64bit�@�ł��邱�Ƃ�O��ɂ���
				������Is64Bit=true�ł��邱�Ƃ�O��Ƃ��Ă���B
				CPW(chess programming wiki)����̐�����]�L���Ă���
				
				bishop��b1�ɂ���5bit�̗���bit������������i���}�j�������W��֋X��C,D,E,F,G�Ƃ���
				����ɓK�؂�Magic Number�i���}�j���|���ď��bit�ɒP�ʑ�����i�E�}�j
				���ݗ���bit�͏��bit�ɂ���̂�bitCount���邽�߂ɉE�V�t�g���ĉ���bit�ɉ��낷
				���ꂪshift�z��̖�ڂłǂꂾ���E�V�t�g������̂���shift�z��ɂ͂����Ă���
				�v�Z���͈�ԉE����64-5�ƂȂ�
				�v���O�����ł�(Is64Bit ? 64 : 32)=>64,popcount<Max15>(masks[s])=>5�ƂȂ�
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
				�����Ŏg���Ă���e�N�j�b�N��������Ă���y�[�W
				https://chessprogramming.wikispaces.com/Traversing+Subsets+of+a+Set
				�����ł͂����ȋ�z�u�ɉ�����reference[]�����
				���Ƃ��΍��WSQ_A1��ROOK������ꍇ�A��������6bit(�����������̂�B1,C1,D1,E1,F1,G1�̍��W�j
				2^6�Ƃ���̃p�^�[��������=64,�c������6bit(�����������̂�A2,A3,A4,A5,A6,A7�̍��W�j
				�Ȃ��2^64�Ƃ���S����64 * 64 = 4096�Ƃ���
				���̎��̋�̔z�u���L�^�����̂�occupancy[],���̎��̗�����bitboard���L�^�����̂�
				reference[]�ɂȂ�.
				6bit+6bit���ő�bit�ɂȂ�BBISHOP�ł�����𒴂��邱�Ƃ͂Ȃ��B
				�����炭QUEEN��ROOK��BISHOP�̑g�ݍ��킹�ŏ������Ă���̂Ŗ��Ȃ��Ǝv����
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
				���occupancy[],reference[]�͂���64*64=4096�K�v�Ȃ킯�ł͂Ȃ��S����4096�K�v����
				�[��2048�Ƃ���A�s�[�A��[���킹�ĂS�����Ȃ̂�2048*6*4��49,152
				���ƒ[��������������36���œ������Ƃ̂ł���̂͏c�A���Ƃ�5bit�Ȃ̂�2^5*2^5=1024
				�S�����킹��4096*4+49152+1024*36=102400(0x19000)���ꂪRTable�z��̑傫���ɂȂ�
				�܂�RTable��ROOK�̑S�Ă̍��W�ł́A�S�Ă̋�z�u�ɑΉ�����������bitboard�������Ă���
				*/
        if (s < SQ_H8)
            attacks[s + 1] = attacks[s] + size;
				/*
				32bit,64bit�ł͗����V�[�h�͈قȂ�H
				*/
        booster = MagicBoosters[Is64Bit][rank_of(s)];

        // Find a magic for square 's' picking up an (almost) random number
        // until we find the one that passes the verification test.
				/*
				�����炭magic number�𗐐��Ő������Ă���
				�ŁA��������������mask�ɂ����ď��bit�Ɏʑ����Ă���
				popcount<Max15>((magics[s] * masks[s]) >> 56) < 6�̂Ƃ����
				������������magic�@number���`�����Ǝʑ����Ă���΂���bit����6bit�ɂȂ�͂��Ȃ̂�
				�U��菬�����ꍇ��NG�Ȃ̂ōēx�����𔭐������A�`�����Ƃ���magic number���ł���܂�
				���[�v������B
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
						attacks[s][index(s,occupancy[i])��index��magic_index�֐��|�C���^�ł��B
						magic_index�֐��͍ő��12bit��bit����ŏ��bit����ŉ���bit�Ɏʑ�����i�P�ˁj�̂�
						4096(0-4095)�̃C���f�b�N�X��Ԃ��֐��Ƃ�������
						��������W���Ƃ̃A�h���X��reference[](���W���Ƃ�bitboard)���L�^�����Ă���
						RTable�̃A�h���X�͍��W�Ƌ�̔z�u�ɂ���Č��܂�index�ł���index�ɂ���bitboard�i����botboard)���L�^������
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
