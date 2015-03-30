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
bitboard�֌W�̏�����,main�֐�����Ă΂�Ă���
*/
void init();
/*
bitboard��\�`���ŕ\��
*/
void print(Bitboard b);

}
namespace Bitbases {
	/*
	bitbase.cpp�̂��߂̐錾�Abitbase.h�͂Ȃ�����bitboard.h�����C���Ă���
	init_kpk�͏I�Ճf�[�^�x�[�X�̂��߂̏�����
	*/
	void init_kpk();
	/*
	�I�Ճf�[�^�x�[�X�̌���
	*/
	bool probe_kpk(Square wksq, Square wpsq, Square bksq, Color us);
}	//namespace Bitbases�̏I���


/*
bitboard�ϐ�(64bit�ϐ�)��bit�ʒu�ƍ��W�Ƃ̑Ή�

H8 G8 F8 E8 D8 C8 B8 A8 H7 G7 F7 E7 D7 C7 B7 A7 H6 G6 F6 E6 D6 C6 B6 A6 H5 G5 F5 E5 D5 C5 B5 A5 H4 G4 F4 E4 D4 C4 B4 A4 H3 G3 F3 E3 D3 C3 B3 A3 H2 G2 F2 E2 D2 C2 B2 A2 H1 G1 F1 E1 D1 C1 B1 A1    
63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
*/

/*
�����肷�邽�߂�bitboard
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
�ȉ�����
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
�s����肷�邽�߂�bitboard
Stockfish�͒ʏ��chess�̍s�\�L�Ƌt���܂ɂȂ��Ă���
�ʏ��chess�\�L			Stockfish�ł̕\�L
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
1111 1111	����bit	
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000
0000 0000	���bit
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
�A���C�����g��64bit�ɑ�����
*/
CACHE_LINE_ALIGNMENT
/*
RMasks�{�̂�bitboard.cpp�ɐ錾����Ă���
��������Boards::init�֐��ł����
RMasks[SQ_A1]�ŏo�͂����bitboard
���ꂩ��SQ_A1�ɂ���ROOK�̗����p�̃}�X�N�ł��邱�Ƃ�������
�Ō�̍��W�iSQ_H1,SQ_A8�j�܂Ń}�X�L���O����Ă��Ȃ��̂�
�����ɗ����������Ă��悤�����܂����֌W�Ȃ�����
(�R���s���[�^�����ɂ����� Magic Bitboard �̒�ĂƎ���--�Q��)
����RMasks��Magic bitboard�̌v�Z�̎��Ɏg�p����
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
RMasks[SQ_B2]�ŏo�͂����bitboard
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
RMasks[SQ_C3]�ŏo�͂����bitboard
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
ROOK�p��magic bitboard
https://chessprogramming.wikispaces.com/Magic+Bitboards
*/
extern Bitboard RMagics[SQUARE_NB];
/*
ROOK�p�̗���bitboard
*/
extern Bitboard* RAttacks[SQUARE_NB];
/*
RShifts��bitboard�����̍��W�ɉ�����bit�V�t�g���Ă��邪
�p�r�s��
�V�t�g����񐔂����W���Ƃɐ������Ă݂����悭�킩��Ȃ�
�K�����͂���悤�����ڍוs��
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
BMasks�{�̂�bitboard.cpp�ɐ錾����Ă���
��������Boards::init�֐��ł����
BMasks[SQ_A1]�ŏo�͂����bitboard
���ꂩ��SQ_A1�ɂ���BISHOP�̗����p�̃}�X�N�ł��邱�Ƃ�������
�Ō�̍��W�iSQ_H8�j�܂Ń}�X�L���O����Ă��Ȃ��̂�
�����ɗ����������Ă��悤�����܂����֌W�Ȃ�����
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
BMasks[SQ_B2]�ŏo�͂����bitboard
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
BMasks[SQ_C3]�ŏo�͂����bitboard
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
BISHOP�p��magic bitboard
*/
extern Bitboard BMagics[SQUARE_NB];
/*
BISHOP�p�̗���bitboard
*/
extern Bitboard* BAttacks[SQUARE_NB];
/*
BShifts��bitboard�����̍��W�ɉ�����bit�V�t�g���Ă��邪
�p�r�s��
�V�t�g����񐔂����W���Ƃɐ������Ă݂����悭�킩��Ȃ�
�K�����͂���悤�����ڍוs��
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
���W�̈ʒu������bit�������Ă���bitboard
*/
extern Bitboard SquareBB[SQUARE_NB];
/*
FileABB,FileBBB,FileCBB,FileDBB,FileEBB,FileFBB,FileGBB,FileHBB��
�z��ɂ�������
*/
extern Bitboard FileBB[FILE_NB];
/*
Rank1BB,Rank2BB,Rank3BB,Rank4BB,Rank5BB,Rank6BB,Rank7BB,Rank8BB��
�z��ɂ�������
*/
extern Bitboard RankBB[RANK_NB];
/*
�w�肵����̗��e��
�ׂ̗��bitboard
*/
extern Bitboard AdjacentFilesBB[FILE_NB];
/*
InFrontBB�͎w�肵���s���G�T�C�h�̈�(Front)��bit�𗧂Ă�
*/
extern Bitboard InFrontBB[COLOR_NB][RANK_NB];
/*
���ы�ibishop,rook,queen��������j������Ƃ��ʒu��
�r�b�g��������bitboard�̔z��
*/
extern Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
/*
BetweenBB[s1][s2]��s1,s2�̂�������Rook,bishop�̗����������bitboard��Ԃ�
*/
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
/*
LineBB[s1][s2]��s1, s2�̂�������Rook, bishop�̗����������bitboard��Ԃ�
BetweenBB�Ƃ悭���Ă���悤�Ȉ�ۂ��󂯂邪BetweenBB�͎w�肵�����W�̊�
LineBB��s1,s2���Ƃ��钼��
*/
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
/*
DistanceRingsBB�ϐ��ɂ͍��Ws1��s2�̋��������Ă���
�A����ƍs�̋����̑傫�����������B
�`�F�r�V�F�t������\���Ă���
*/
extern Bitboard DistanceRingsBB[SQUARE_NB][8];
/*
�����̈ʒu����O���P��iForward�j��bitboard��Ԃ�
*/
extern Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
/*
PAWN��̑O�R���bitboard�������ƂɂȂ�BPAWN�̈ړ��\�ꏊ��bit������������
*/
extern Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
/*
PAWN���������ʒu��bitboard��Ԃ�
*/
extern Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
/*
ROOK,BISHOP,QUEEN�̍��W���Ƃ̗��������Ă���
���ɋ�Ȃ���Ԃ̎��̗���
*/
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
/*
SquareDistance[SQ_A1][to]�ŏo�͂�����
��ƂȂ���W�iA1)����̋�����row,col�傫������Ԃ�
01234567
11234567
22234567
33334567
44444567
55555567
66666667
77777777
SquareDistance[SQ_D2][to]�ŏo�͂�����
��ƂȂ���W�iD2)����̋�����row,col�傫������Ԃ�
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
�Ղ̎s���͗l��bitboard�ɂ�������
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
bitboard��sq��&���Z�q���`���Ă���
������bb�ƍ��W���d�Ȃ��Ă���bitboard��Ԃ�
*/
inline Bitboard operator&(Bitboard b, Square s) {
  return b & SquareBB[s];
}
/*
bitboard��sq��|=���Z�q���`���Ă���
������bb�ɍ��W��bit��ǉ�����bitboard��Ԃ�
*/
inline Bitboard& operator|=(Bitboard& b, Square s) {
  return b |= SquareBB[s];
}
/*
bitboard��sq��^=���Z�q���`���Ă���
������bb�ɍ��W��bit�Ƃ̔r�����Z�������ʂ�bitboard��Ԃ�
*/
inline Bitboard& operator^=(Bitboard& b, Square s) {
  return b ^= SquareBB[s];
}
/*
bitboard��sq�́b���Z�q���`���Ă���
������bb�ɍ��W��bit�Ƃ�OR���Z�������ʂ�bitboard��Ԃ�
*/
inline Bitboard operator|(Bitboard b, Square s) {
  return b | SquareBB[s];
}
/*
bitboard��sq��^���Z�q���`���Ă���
������bb�ɍ��W��bit�Ƃ̔r�����Z�������ʂ�bitboard��Ԃ�
*/
inline Bitboard operator^(Bitboard b, Square s) {
  return b ^ SquareBB[s];
}
/*
������bitboard���Pbit���������Ă��Ȃ�������
0��Ԃ��A���������Ă������0��Ԃ�
*/
inline bool more_than_one(Bitboard b) {
  return b & (b - 1);
}
/*
���Ws1,s2�̋�����Ԃ��A�s�Ɨ�̋����̓��傫���ق���Ԃ�
*/
inline int square_distance(Square s1, Square s2) {
  return SquareDistance[s1][s2];
}
/*
��̋�����Ԃ�
*/
inline int file_distance(Square s1, Square s2) {
  return abs(file_of(s1) - file_of(s2));
}
/*
�s�Ԃ̋�����Ԃ�
*/
inline int rank_distance(Square s1, Square s2) {
  return abs(rank_of(s1) - rank_of(s2));
}


/// shift_bb() moves bitboard one step along direction Delta. Mainly for pawns.
/*
C++�e���v���[�g�e�N�j�b�Npage28�ɂ���e���v���[�g�����ɐ����l��n���ł���
�g������
shift_bb<Up>(b)�̂悤�ɌĂяo����Up��DELTA_N�i��Ԃ�WHITE�̂Ƃ��j��n�����ƂɂȂ�

DELTA_N�͂Wbit�E�V�t�g����̂ō��W�ł����Ƒ傫���Ȃ�
�@A   B   C   D   E   F   G   H
 +---+---+---+---+---+---+---+---+
 1| �~ | �~ |   | �~ | �~ |   |   |   |
 +---+---+---+---+---+---+---+---+
 2|   |   |   |   |   |   |   |   |
 +---+---+---+---+---+---+---+---+
 3|   |   |   |   |   |   |   |   |
 +---+---+---+---+---+---+---+---+
 ���A�����Ȃ�
 �@A   B   C   D   E   F   G   H
	+---+---+---+---+---+---+---+---+
	1|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	2| �~ | �~ |   | �~ | �~ |   |   |   |
	+---+---+---+---+---+---+---+---+
	3|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	DELTA_S�͂��̔���
	DELTA_NE��
	+---+---+---+---+---+---+---+---+
	1| �~ | �~ |   | �~ | �~ | �~ | �~ | �~ |
	+---+---+---+---+---+---+---+---+
	2|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	3|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	���A�����Ȃ�
	�@A   B   C   D   E   F   G   H
	 +---+---+---+---+---+---+---+---+
	 1|   |   |   |   |   |   |   |   |
	 +---+---+---+---+---+---+---+---+
	 2|   | �~ | �~ |   | �~ | �~ | �~ | �~ |
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
Rank1(=0)����Rank8(=7)�Ŏw�肵��RankBB��Ԃ�
*/
inline Bitboard rank_bb(Rank r) {
  return RankBB[r];
}
/*
���W�l���w�肵�Ă��̍��W����������s��RankBB��Ԃ�
*/
inline Bitboard rank_bb(Square s) {
  return RankBB[rank_of(s)];
}
/*
��ԍ��Ŏw�肵��bitBoard��Ԃ�
*/
inline Bitboard file_bb(File f) {
  return FileBB[f];
}
/*
���W�l���w�肵�Ă��̍��W������������FileBB��Ԃ�
*/
inline Bitboard file_bb(Square s) {
  return FileBB[file_of(s)];
}


/// adjacent_files_bb() takes a file as input and returns a bitboard representing
/// all squares on the adjacent files.
/*
�w�肵����ׂ̗̗񂪗����Ă���bitboard��Ԃ�
*/
inline Bitboard adjacent_files_bb(File f) {
  return AdjacentFilesBB[f];
}


/// in_front_bb() takes a color and a rank as input, and returns a bitboard
/// representing all the squares on all ranks in front of the rank, from the
/// given color's point of view. For instance, in_front_bb(BLACK, RANK_3) will
/// give all squares on ranks 1 and 2.
/*
�w�肵���s����G���i�ǂ������G�����͎w�肵���J���[�Ŕ��f�j��
�����Ă���bitboard
*/
inline Bitboard in_front_bb(Color c, Rank r) {
  return InFrontBB[c][r];
}


/// between_bb() returns a bitboard representing all squares between two squares.
/// For instance, between_bb(SQ_C4, SQ_F7) returns a bitboard with the bits for
/// square d5 and e6 set.  If s1 and s2 are not on the same line, file or diagonal,
/// 0 is returned.
/*
�w�肵�����W�Ԃł�����Rook,Bishop�̗��������݂���΂��̗���bitboard��Ԃ�
�w�肵�����W��bitboard�ɂ͊܂܂�Ȃ�
*/
inline Bitboard between_bb(Square s1, Square s2) {
  return BetweenBB[s1][s2];
}


/// forward_bb() takes a color and a square as input, and returns a bitboard
/// representing all squares along the line in front of the square, from the
/// point of view of the given color. Definition of the table is:
/// ForwardBB[c][s] = in_front_bb(c, s) & file_bb(s)
/*
�w�肵�����W���O����bitboard��Ԃ�
�J���[�ɂ���đO���̕������ς��
*/
inline Bitboard forward_bb(Color c, Square s) {
  return ForwardBB[c][s];
}


/// pawn_attack_span() takes a color and a square as input, and returns a bitboard
/// representing all squares that can be attacked by a pawn of the given color
/// when it moves along its file starting from the given square. Definition is:
/// PawnAttackSpan[c][s] = in_front_bb(c, s) & adjacent_files_bb(s);
/*
�w�肵�����W���O����bitboard��Ԃ�
�J���[�ɂ���đO���̕������ς��
*/
inline Bitboard pawn_attack_span(Color c, Square s) {
  return PawnAttackSpan[c][s];
}


/// passed_pawn_mask() takes a color and a square as input, and returns a
/// bitboard mask which can be used to test if a pawn of the given color on
/// the given square is a passed pawn. Definition of the table is:
/// PassedPawnMask[c][s] = pawn_attack_span(c, s) | forward_bb(c, s)
/*
Pawn���O�i�܂��͋���Ƃ�bitboard��Ԃ�
PassedPawnMask�z���PAWN�̑O�i�����ɂR���bit�����������̂�
�w�肵���J���[�A���W���w�肷��Ƃ��̍��W�ɂ���PAWN�̈ړ��\�͈�
��bitboard�ŕԂ��i�\�Ȃ����Ŏ��ۂɍs���邩�͂��̋ǖʂƂ�bitboard�Ƃ�
AND���K�v�j
*/
inline Bitboard passed_pawn_mask(Color c, Square s) {
  return PassedPawnMask[c][s];
}


/// squares_of_color() returns a bitboard representing all squares with the same
/// color of the given square.
/*
�p�r�s��
*/
inline Bitboard squares_of_color(Square s) {
  return DarkSquares & s ? DarkSquares : ~DarkSquares;
}


/// aligned() returns true if the squares s1, s2 and s3 are aligned
/// either on a straight or on a diagonal line.
/*
���Ws1,s2�̊Ԃ�Rook �܂���Bishop�̗���bitboard�Ɓ@���Ws3��
bitboard�Ƃ�AND�Ȃ̂�s3��s1��s2�̊Ԃ̗������Ւf���Ă��邩�̔��f���ł���
*/
inline bool aligned(Square s1, Square s2, Square s3) {
  return LineBB[s1][s2] & s3;
}


/// Functions for computing sliding attack bitboards. Function attacks_bb() takes
/// a square and a bitboard of occupied squares as input, and returns a bitboard
/// representing all squares attacked by Pt (bishop or rook) on the given square.
/*
�e���v���[�g�֐���piece type���w��ł���
�C���X�^���X����
magic_index<ROOK>
magic_index<BISHOP>
�̂Q�̂�
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
��ы�p�̗���bitboard
<��>
s��rook�����鎞��RAttacks���Ԃ�bitboard��
RAttacks��bitboard�z��,��������init_magics�֐���
�s��
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
���၄
s��bishop������Ƃ���BAttacks���Ԃ�bitboard

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
���bit����X�L��������bit�������Ă���index��Ԃ��A0�I���W��
*/
extern Square msb(Bitboard b);
/*
����bit����X�L����bit�������Ă���index��Ԃ��A0�I���W��
*/
extern Square lsb(Bitboard b);
/*
����bit����X�L����bit�������Ă���index��Ԃ��A0�I���W��
�����ɂ���bit������
*/
extern Square pop_lsb(Bitboard* b);

#endif

/// frontmost_sq() and backmost_sq() find the square corresponding to the
/// most/least advanced bit relative to the given color.
/*
�������͍ŏ��bit,LSB�͍ŉ���bit
WHITE����bit�̃X�L���������bit����J�n����
BLACK����bit�̃X�L����������bit����J�n���邱�Ƃł�
*/
inline Square frontmost_sq(Color c, Bitboard b) { return c == WHITE ? msb(b) : lsb(b); }
inline Square  backmost_sq(Color c, Bitboard b) { return c == WHITE ? lsb(b) : msb(b); }

//I am append
//void printBSFTable();

#endif // #ifndef BITBOARD_H_INCLUDED
