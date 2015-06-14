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

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

/// For Linux and OSX configuration is done automatically using Makefile. To get
/// started type 'make help'.
///
/// For Windows, part of the configuration is detected automatically, but some
/// switches need to be set manually:
///
/// -DNDEBUG      | Disable debugging mode. Use always.
///
/// -DNO_PREFETCH | Disable use of prefetch asm-instruction. A must if you want
///               | the executable to run on some very old machines.
///
/// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
///               | only in 64-bit mode. For compiling requires hardware with
///               | popcnt support.

#include <cassert>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>

#if defined(_MSC_VER)
// Disable some silly and noisy warning from MSVC compiler
#pragma warning(disable: 4127) // Conditional expression is constant
#pragma warning(disable: 4146) // Unary minus operator applied to unsigned type
#pragma warning(disable: 4800) // Forcing value to bool 'true' or 'false'
#endif
/*
�p�r�s��
*/
#define unlikely(x) (x) // For code annotation purposes
/*
bitScan�Ńn�[�h�E�G�A�ōs�����@�ƃ\�t�g�E�G�A�ōs�����@������
https://chessprogramming.wikispaces.com/BitScan
_WIN64 && 
*/
#if defined(_WIN64) && !defined(IS_64BIT)
#  include <intrin.h> // MSVC popcnt and bsfq instrinsics
#  define IS_64BIT
#  define USE_BSFQ
#endif
/*
POPCNT���߂��g��������INTEL COMPILER�ł͂Ȃ�
*/
#if defined(USE_POPCNT) && defined(_MSC_VER) && defined(__INTEL_COMPILER)
#  include <nmmintrin.h> // Intel header for _mm_popcnt_u64() intrinsic
#endif

/*
����stockfish-DD�o�[�W�����ɂ�PEXT���߂��Ȃ��̂Łistockfish5�ɂ͂������j
�ǉ����Ă���

PEXT���߂��g��������CPU���T�|�[�g���Ă��Ȃ�
PEXT���߂��Ƃ͉��LURL�Q�Ƃ̂���
BMI2 (Bit Manipulation Instruction Set 2)
http://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets#BMI2_.28Bit_Manipulation_Instruction_Set_2.29
*/
#if defined(USE_PEXT)
#  include <immintrin.h> // Header for _pext_u64() intrinsic
#else
#  define _pext_u64(b, m) (0)
#endif

/*
��s�ǂ݂��������̂�<xmmintrin.h>��ǂݍ���ł���
*/
#  if !defined(NO_PREFETCH) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#   include <xmmintrin.h> // Intel and Microsoft header for _mm_prefetch()
#  endif

#define CACHE_LINE_SIZE 64
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#  define CACHE_LINE_ALIGNMENT __declspec(align(CACHE_LINE_SIZE))
#else
#  define CACHE_LINE_ALIGNMENT  __attribute__ ((aligned(CACHE_LINE_SIZE)))
#endif

/*
inline�֐��̓W�J�̎d���H
*/
#ifdef _MSC_VER
#  define FORCE_INLINE  __forceinline
#elif defined(__GNUC__)
#  define FORCE_INLINE  inline __attribute__((always_inline))
#else
#  define FORCE_INLINE  inline
#endif

/*
POPCNT���߂��g������������HasPopCnt��true��
*/
#ifdef USE_POPCNT
const bool HasPopCnt = true;
#else
const bool HasPopCnt = false;
#endif

/*
����stockfish-DD�o�[�W�����ɂ�PEXT���߂��Ȃ��̂Łistockfish5�ɂ͂������j
�ǉ����Ă���

PEXT���߂��g�������Ȃ�HasPext��true��
*/
#ifdef USE_PEXT
const bool HasPext = true;
#else
const bool HasPext = false;
#endif
/*
����IS_64BIT�̓}�V���̂��Ƃ��B�R���p�C���̂��Ƃ��H
�i�ǋL�j
OS���U�Sbit�Ȃ�true�ɂȂ�
�t����MakeFile�ɋL�q���Ă���
*/
#ifdef IS_64BIT
const bool Is64Bit = true;
#else
const bool Is64Bit = false;
#endif

typedef uint64_t Key;
typedef uint64_t Bitboard;
/*
���胊�X�g�̍ő咷��
*/
const int MAX_MOVES = 256;
/*
�ő�T���[��
*/
const int MAX_PLY = 100;
const int MAX_PLY_PLUS_6 = MAX_PLY + 6;

/// A move needs 16 bits to be stored
///
/// bit  0- 5: destination square (from 0 to 63)
/// bit  6-11: origin square (from 0 to 63)
/// bit 12-13: promotion piece type - 2 (from KNIGHT-2 to QUEEN-2)
/// bit 14-15: special move flag: promotion (1), en passant (2), castle (3)
///
/// Special cases are MOVE_NONE and MOVE_NULL. We can sneak these in because in
/// any normal move destination square is always different from origin square
/// while MOVE_NONE and MOVE_NULL have the same origin and destination square.
/*
����̃f�[�^�\��
MOVE_NONE�͎肪�Ȃ����Ƃ�\��
MOVE_NULL�͗p�r�s��
*/
enum Move {
  MOVE_NONE,
  MOVE_NULL = 65
};
/*
������
PROMOTION = ����
ENPASSANT = �A���p�b�T��
CASTLING  = �L���X�����O
*/
enum MoveType {
  NORMAL,
  PROMOTION = 1 << 14,
  ENPASSANT = 2 << 14,
  CASTLE    = 3 << 14
};
/*
�p�r�s��
*/
enum CastleRight {  // Defined as in PolyGlot book hash key
  CASTLES_NONE,
  WHITE_OO,
  WHITE_OOO   = WHITE_OO << 1,
  BLACK_OO    = WHITE_OO << 2,
  BLACK_OOO   = WHITE_OO << 3,
  ALL_CASTLES = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO,
  CASTLE_RIGHT_NB = 16
};
/*
�L���X�����O�̎�ʁiKING���ɃL���X�����O����AQueen���ɃL���X�����O����Q��ނ���j
CASTLING_SIDE_NB�͎�ʂ̐�
*/
enum CastlingSide {
  KING_SIDE,
  QUEEN_SIDE,
  CASTLING_SIDE_NB = 2
};
/*
�p�r�s��
�]���l�Ɋւ���Ȃɂ��H
*/
enum Phase {
  PHASE_ENDGAME,
  PHASE_MIDGAME = 128,
  MG = 0, EG = 1, PHASE_NB = 2
};
/*
�p�r�s��
�]���l�Ɋւ���Ȃɂ��H
*/
enum ScaleFactor {
  SCALE_FACTOR_DRAW   = 0,
  SCALE_FACTOR_NORMAL = 64,
  SCALE_FACTOR_MAX    = 128,
  SCALE_FACTOR_NONE   = 255
};
/*
�u���\�̕]���l�̎��
*/
enum Bound {
  BOUND_NONE,
  BOUND_UPPER,
  BOUND_LOWER,
  BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};
/*
�p�r�s��
�]���l�Ɋւ���Ȃɂ��H
���̕]���l�̈Ӗ��𒲍�
VALUE_INFINITE
�@������̖������\���萔
�@id_loop�֐��i�����[���j��bestNalue,alpha,delta��-VALUE_INFINITE�ŏ�����
�@beta��VALUE_INFINITE�ŏ��������Ă���
VALUE_MATE
	check mate���|���ċl�߂̏�ԂɂȂ������̕]���l�萔
VALUE_KNOWN_WIN
�@�����A�ǖʂ̕]���l�̍��v���قڂ��̐�
�@�]���l��check mate���������Ă��Ȃ����Ƃ��m�F����̂Ɏg�p
VALUE_NONE
	�g�����X�|�W�V�����e�[�u����p�̕]���l�ŁA�L���ȕ]���l���Ȃ�����
	��������]���萔
*/
enum Value : int {
  VALUE_ZERO      = 0,
  VALUE_DRAW      = 0,
  VALUE_KNOWN_WIN = 15000,
  VALUE_MATE      = 30000,
  VALUE_INFINITE  = 30001,
  VALUE_NONE      = 30002,

  VALUE_MATE_IN_MAX_PLY  =  VALUE_MATE - MAX_PLY,
  VALUE_MATED_IN_MAX_PLY = -VALUE_MATE + MAX_PLY,

  Mg = 0, Eg = 1,
	/*
	��̕]���l�H
	*/
	PawnValueMg = 198, PawnValueEg = 258,
  KnightValueMg = 817,   KnightValueEg = 846,
  BishopValueMg = 836,   BishopValueEg = 857,
  RookValueMg   = 1270,  RookValueEg   = 1278,
  QueenValueMg  = 2521,  QueenValueEg  = 2558
};
/*
���
*/
enum PieceType {
  NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
  ALL_PIECES = 0,
  PIECE_TYPE_NB = 8
};
/*
�J���[���܂߂���R�[�h
*/
enum Piece {
  NO_PIECE,
  W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
  B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  PIECE_NB = 16
};
/*
�w�c�ANO_COLOR�͎g���Ă��Ȃ�
COLOR_NB�̓J���[�̐�,�z��̐������߂�
*/
enum Color {
  WHITE, BLACK, NO_COLOR, COLOR_NB = 2
};
/*
�T���[��
���̒T���ł͂P���ONE_PLY���Q
*/
enum Depth {

  ONE_PLY = 2,

  DEPTH_ZERO          =  0 * ONE_PLY,
  DEPTH_QS_CHECKS     =  0 * ONE_PLY,
  DEPTH_QS_NO_CHECKS  = -1 * ONE_PLY,
  DEPTH_QS_RECAPTURES = -5 * ONE_PLY,

  DEPTH_NONE = -127 * ONE_PLY
};
/*
���W�ԍ�
0-63
*/
enum Square {
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
  SQ_NONE,

  SQUARE_NB = 64,
	/*
	�����q�ADELTA_NNDELTA_SS,�͎g�p���Ă��Ȃ�
	*/
	DELTA_N = 8,
  DELTA_E =  1,
  DELTA_S = -8,
  DELTA_W = -1,

  DELTA_NN = DELTA_N + DELTA_N,
  DELTA_NE = DELTA_N + DELTA_E,
  DELTA_SE = DELTA_S + DELTA_E,
  DELTA_SS = DELTA_S + DELTA_S,
  DELTA_SW = DELTA_S + DELTA_W,
  DELTA_NW = DELTA_N + DELTA_W
};
/*
��
*/
enum File {
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB
};
/*
�s
*/
enum Rank {
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB
};


/// Score enum keeps a midgame and an endgame value in a single integer, first
/// LSB 16 bits are used to store endgame value, while upper bits are used for
/// midgame value.
/*
stockfish�ł͕]���l�̓~�h���Q�[���img ���ՂƂ����Ӗ����Ǝv���j�ł̋�]���l��
�G���h�Q�[���ieg �I�Ղ��Ǝv���j�ł̋�]���l�ƕʁX�ɂ���B
��������int�ϐ��Ɋi�[���Ă���
32bit�ϐ��̏��16bit�Ƀ~�h���Q�[���]���l������16bit�ɃG���h�Q�[���]���l������
���̂��߂̃f�[�^�\���̂�ScoreView,�����Ă��̃f�[�^�\���̂����̂�make_score�֐�
*/
enum Score : int { SCORE_ZERO };

inline Score make_score(int mg, int eg) { return Score((mg << 16) + eg); }

/// Extracting the signed lower and upper 16 bits it not so trivial because
/// according to the standard a simple cast to short is implementation defined
/// and so is a right shift of a signed integer.
/*
��]���l�̓��~�h���Q�[���]���l�����o��
��ʃr�b�g�Ɏ��߂��Ă���
*/
inline Value mg_value(Score s) { return Value(((s + 0x8000) & ~0xffff) / 0x10000); }

/// On Intel 64 bit we have a small speed regression with the standard conforming
/// version, so use a faster code in this case that, although not 100% standard
/// compliant it seems to work for Intel and MSVC.
#if defined(IS_64BIT) && (!defined(__GNUC__) || defined(__INTEL_COMPILER))
/*
��]���l�̓��G���h�Q�[���]���l�����o��
���ʃr�b�g�Ɏ��߂��Ă���
*/
inline Value eg_value(Score s) { return Value(int16_t(s & 0xffff)); }

#else

inline Value eg_value(Score s) {
  return Value((int)(unsigned(s) & 0x7fffu) - (int)(unsigned(s) & 0x8000u));
}

#endif
/*
stockfish�ł͂��낢��Ȍ^(Value,PieceType,Piece,Color
Depth,Square,Filem,Rank)����`
���Ă���̂ł��̌^���Ƃ̉��Z�i�l�����Z�A�r�b�g���Z�Ȃǁj��������
�Ē�`���Ă���
*/
#define ENABLE_SAFE_OPERATORS_ON(T)                                         \
inline T operator+(const T d1, const T d2) { return T(int(d1) + int(d2)); } \
inline T operator-(const T d1, const T d2) { return T(int(d1) - int(d2)); } \
inline T operator*(int i, const T d) { return T(i * int(d)); }              \
inline T operator*(const T d, int i) { return T(int(d) * i); }              \
inline T operator-(const T d) { return T(-int(d)); }                        \
inline T& operator+=(T& d1, const T d2) { return d1 = d1 + d2; }            \
inline T& operator-=(T& d1, const T d2) { return d1 = d1 - d2; }            \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); }

#define ENABLE_OPERATORS_ON(T) ENABLE_SAFE_OPERATORS_ON(T)                  \
inline T& operator++(T& d) { return d = T(int(d) + 1); }                    \
inline T& operator--(T& d) { return d = T(int(d) - 1); }                    \
inline T operator/(const T d, int i) { return T(int(d) / i); }              \
inline T& operator/=(T& d, int i) { return d = T(int(d) / i); }

ENABLE_OPERATORS_ON(Value)
ENABLE_OPERATORS_ON(PieceType)
ENABLE_OPERATORS_ON(Piece)
ENABLE_OPERATORS_ON(Color)
ENABLE_OPERATORS_ON(Depth)
ENABLE_OPERATORS_ON(Square)
ENABLE_OPERATORS_ON(File)
ENABLE_OPERATORS_ON(Rank)

/// Added operators for adding integers to a Value
inline Value operator+(Value v, int i) { return Value(int(v) + i); }
inline Value operator-(Value v, int i) { return Value(int(v) - i); }

ENABLE_SAFE_OPERATORS_ON(Score)

/// Only declared but not defined. We don't want to multiply two scores due to
/// a very high risk of overflow. So user should explicitly convert to integer.
inline Score operator*(Score s1, Score s2);

/// Division of a Score must be handled separately for each term
inline Score operator/(Score s, int i) {
  return make_score(mg_value(s) / i, eg_value(s) / i);
}

#undef ENABLE_OPERATORS_ON
#undef ENABLE_SAFE_OPERATORS_ON
/*
��̕]���l�A
PieceValue[0][] �~�h���Q�[���p��]���l
PieceValue[1][] �G���h�Q�[���p��]���l
position.cpp�ŏ�����
*/
extern Value PieceValue[PHASE_NB][PIECE_NB];
/*
���胊�X�g�̃f�[�^�\����
���胊�X�g��movepick.h�ɒ�`���Ă���MovePicker�N���X��
���ɓ����Ă���B
*/
struct ExtMove {
  Move move;
  int score;
};
/*
ExtMove���m�̔�r���Z�q�̃I�[�o���C�h
*/
inline bool operator<(const ExtMove& f, const ExtMove& s) {
  return f.score < s.score;
}
/*
COLOR=WHITE=0,BLACK=1�Ȃ̂�
0 ^ 1 = 1
1 ^ 1 = 0
�ƂȂ��ăJ���[��؂�ւ��邱�Ƃ��ł���
*/
inline Color operator~(Color c) {
  return Color(c ^ BLACK);
}
/*
���W�����E�U��Ԃ�
A1->A8
A2->A7
...
A8->A1

B1->B8
B2->B7
...
B8->B1
*/
inline Square operator~(Square s) {
  return Square(s ^ SQ_A8); // Vertical flip SQ_A1 -> SQ_A8
}

inline Square operator|(File f, Rank r) {
  return Square((r << 3) | f);
}
/*
seach.cpp�Ŏg�p����Ă��邪�p�r�s��
*/
inline Value mate_in(int ply) {
  return VALUE_MATE - ply;
}
/*
seach.cpp�Ŏg�p����Ă��邪�p�r�s��
*/
inline Value mated_in(int ply) {
  return -VALUE_MATE + ply;
}
/*
���ƃJ���[�ŋ�R�[�h���Z�o����
*/
inline Piece make_piece(Color c, PieceType pt) {
  return Piece((c << 3) | pt);
}

inline CastleRight make_castle_right(Color c, CastlingSide s) {
  return CastleRight(WHITE_OO << ((s == QUEEN_SIDE) + 2 * c));
}
/*
��R�[�h����������o��
*/
inline PieceType type_of(Piece p)  {
  return PieceType(p & 7);
}
/*
��R�[�h����J���[�����o��
*/
inline Color color_of(Piece p) {
  assert(p != NO_PIECE);
  return Color(p >> 3);
}
/*
���W�l���Փ��ɓ����Ă��邩�`�G�b�N
*/
inline bool is_ok(Square s) {
  return s >= SQ_A1 && s <= SQ_H8;
}
/*
���W�l�̉��ʂRbit�𔲂��o�����Ƃŗ�ԍ������o����
*/
inline File file_of(Square s) {
  return File(s & 7);
}
/*
���W�l����s�ԍ��𔲂��o��
*/
inline Rank rank_of(Square s) {
  return Rank(s >> 3);
}
/*
�s�����ւ���A�J���[��WHITE�̂Ƃ��͂��̂܂�
��͓���ւ��Ȃ�
relative_square(WHITE,sq))�̎��s��
0  1  2  3  4  5  6  7
8  9 10 11 12 13 14 15
16 17 18 19 20 21 22 23
24 25 26 27 28 29 30 31
32 33 34 35 36 37 38 39
40 41 42 43 44 45 46 47
48 49 50 51 52 53 54 55
56 57 58 59 60 61 62 63
relative_square(BLACK,sq)�̎��s��
56 57 58 59 60 61 62 63
48 49 50 51 52 53 54 55
40 41 42 43 44 45 46 47
32 33 34 35 36 37 38 39
24 25 26 27 28 29 30 31
16 17 18 19 20 21 22 23
8  9 10 11 12 13 14 15
0  1  2  3  4  5  6  7
*/
inline Square relative_square(Color c, Square s) {
  return Square(s ^ (c * 56));
}
/*
�s�ԍ������ւ���A�J���[��WHITE�̂Ƃ��͂��̂܂�
relative_rank(WHITE,sq))�̎��s��
0  0  0  0  0  0  0  0
1  1  1  1  1  1  1  1
2  2  2  2  2  2  2  2
3  3  3  3  3  3  3  3
4  4  4  4  4  4  4  4
5  5  5  5  5  5  5  5
6  6  6  6  6  6  6  6
7  7  7  7  7  7  7  7
relative_rank(BLACK,sq)�̎��s��
7  7  7  7  7  7  7  7
6  6  6  6  6  6  6  6
5  5  5  5  5  5  5  5
4  4  4  4  4  4  4  4
3  3  3  3  3  3  3  3
2  2  2  2  2  2  2  2
1  1  1  1  1  1  1  1
0  0  0  0  0  0  0  0
*/
inline Rank relative_rank(Color c, Rank r) {
  return Rank(r ^ (c * 7));
}
/*
��ƈꏏ���������ɍ��W�l���g��
�s�ԍ������ւ���A�J���[��WHITE�̂Ƃ��͂��̂܂�
*/
inline Rank relative_rank(Color c, Square s) {
  return relative_rank(c, rank_of(s));
}
/*
s2�̈ʒu�̃J���[��Ԃ�
�i����̈ʒu�Ɠ����J���[�Ȃ�O
���΂Ȃ�P��Ԃ��j
chess board�̎s���͗l���C���[�W�����
�ǂ��B
opposite_colors(sq,SQ_A1)
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
opposite_colors(sq,SQ_A2)
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
1  0  1  0  1  0  1  0
0  1  0  1  0  1  0  1
*/
inline bool opposite_colors(Square s1, Square s2) {
  int s = int(s1) ^ int(s2);
  return ((s >> 3) ^ s) & 1;
}
/*
��ԍ��̕�����Ԃ�
File_A -> a or A
File_B -> b or B
...
*/
inline char file_to_char(File f, bool tolower = true) {
  return char(f - FILE_A + (tolower ? 'a' : 'A'));
}
/*
�s�ԍ��̕�����Ԃ�
RANK_1 -> 1
RANK_2 -> 2
...
*/
inline char rank_to_char(Rank r) {
  return char(r - RANK_1 + '1');
}
/*
PAWN�̃J���[����ړ�������DELTA_N��DELTA_S�Ɍ��肵�Ă���
*/
inline Square pawn_push(Color c) {
  return c == WHITE ? DELTA_N : DELTA_S;
}
/*
����f�[�^�\������ړ������W�l�����o��
*/
inline Square from_sq(Move m) {
  return Square((m >> 6) & 0x3F);
}
/*
����f�[�^�\������ړ�����W�l�����o��
*/
inline Square to_sq(Move m) {
  return Square(m & 0x3F);
}
/*
����̎�ʂ����o���Ă���
promotion (1), en passant (2), castling (3)
*/
inline MoveType type_of(Move m) {
  return MoveType(m & (3 << 14));
}
/*
��������̋��
promotion piece type - 2 (from KNIGHT-2 to QUEEN-2)
*/
inline PieceType promotion_type(Move m) {
  return PieceType(((m >> 12) & 3) + 2);
}
/*
�ړ���(6bit)�A�ړ���̍��W(6bit)��
Move�^�Ƀp�b�N���Ă���
*/
inline Move make_move(Square from, Square to) {
  return Move(to | (from << 6));
}
/*
�e���v���[�g�֐�
Move�f�[�^�\���ɂ��邽�߂̊֐�
*/
template<MoveType T>
inline Move make(Square from, Square to, PieceType pt = KNIGHT) {
  return Move(to | (from << 6) | T | ((pt - KNIGHT) << 12));
}
/*
�ړ��̃`�G�b�N�֐�
*/
inline bool is_ok(Move m) {
  return from_sq(m) != to_sq(m); // Catches also MOVE_NULL and MOVE_NONE
}

#include <string>
/*
���W�l�����W������ɂ��ĕԂ�
*/
inline const std::string square_to_string(Square s) {
  return { file_to_char(file_of(s)), rank_to_char(rank_of(s)) };
}

#endif // #ifndef TYPES_H_INCLUDED
