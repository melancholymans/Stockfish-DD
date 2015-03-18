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
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "bitcount.h"
#include "movegen.h"
#include "notation.h"
#include "position.h"
#include "psqtab.h"
#include "rkiss.h"
#include "thread.h"
#include "tt.h"

using std::string;
using std::cout;
using std::endl;
/*
notation.cpp�ɂ��������O�Œ�`����Ă��邪�����瑤��string�^�Anotation.cpp��char�^
���𕶎��P�����ŕ\�����邽�߂̕�����
*/
static const string PieceToChar(" PNBRQK  pnbrqk");
/*
�A�������g��64bit�ɑ����Ă���
*/
CACHE_LINE_ALIGNMENT
/*
psq�Ƃ������O�̕ϐ��͂������������ɂ���APiece-Square�̂悤�ɋ��ʂ̍��W�ɂ���Ă���l������
�悤�ȈӖ��Ɏg���邱�Ƃ��������悤���B
�Ȃ̂Ŗ��O��piece_sq_score�ɕύX����
��������Position::init�ł���
do_move�֐��ł��̒l���W�v����StateInfo->psq�ϐ��Ɋi�[���Ă���
����compute_psq_score�֐��Ŏg�p����Ă���B
*/
Score piece_sq_score[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
/*
��g�̉��l�A���Ղł̉��l��**Mg,�I�Ղł̉��l��**Eg�ɂȂ��Ă���
white���݂̂�Position::init��BLACK���ɃR�s�[����
PieceValue[Mg=0][0-5]��VALUE_ZERO����Queen�܂ł̒��Ղ̋�]���l������
PieceValue[Eg=1][0-5]��VALUE_ZERO����Queen�܂ł̒��Ղ̋�]���l������
init�֐��Ŏc���
PieceValue[Mg=0][8-13]��VALUE_ZERO����Queen�܂ł̒��Ղ̋�]���l������
PieceValue[Eg=1][8-13]��VALUE_ZERO����Queen�܂ł̒��Ղ̋�]���l������
�z��̑S�Ăɒl�������Ă���킯�ł͂Ȃ�
*/
Value PieceValue[PHASE_NB][PIECE_NB] = {
{ VALUE_ZERO, PawnValueMg, KnightValueMg, BishopValueMg, RookValueMg, QueenValueMg },
{ VALUE_ZERO, PawnValueEg, KnightValueEg, BishopValueEg, RookValueEg, QueenValueEg } };
/*
chess�Ȃǂ̋ǖʂ̏�Ԃ��P�̃n�b�V���l�ő�\��������@
�Q�lHP:http://hackemdown.blogspot.jp/2014/06/zobrist-hashing.html
���������Ă���̂�Position::init()�֐���
*/
namespace Zobrist {

  Key psq[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
  Key enpassant[FILE_NB];
  Key castle[CASTLE_RIGHT_NB];
  Key side;
  Key exclusion;
}
/*
�p�r�s��
*/
Key Position::exclusion_key() const { return m_st->key ^ Zobrist::exclusion; }

namespace {

// min_attacker() is an helper function used by see() to locate the least
// valuable attacker for the side to move, remove the attacker we just found
// from the bitboards and scan for new X-ray attacks behind it.
/*
min_attacker�֐���see�֐��̃w���p�[�֐�
��see�֐��͂����炭�Î~�T�����Ǝv��
�ڍוs��
*/
template<int Pt> FORCE_INLINE
PieceType min_attacker(const Bitboard* bb, const Square& to, const Bitboard& stmAttackers,
                       Bitboard& occupied, Bitboard& attackers) {

  Bitboard b = stmAttackers & bb[Pt];
  if (!b)
      return min_attacker<Pt+1>(bb, to, stmAttackers, occupied, attackers);

  occupied ^= b & ~(b - 1);
	/*
	�����̏����s��
	*/
	if (Pt == PAWN || Pt == BISHOP || Pt == QUEEN)
      attackers |= attacks_bb<BISHOP>(to, occupied) & (bb[BISHOP] | bb[QUEEN]);

  if (Pt == ROOK || Pt == QUEEN)
      attackers |= attacks_bb<ROOK>(to, occupied) & (bb[ROOK] | bb[QUEEN]);

  attackers &= occupied; // After X-ray that may add already processed pieces
  return (PieceType)Pt;
}
/*
�e���v���[�g�֐��iKING�̖������j
*/
template<> FORCE_INLINE
PieceType min_attacker<KING>(const Bitboard*, const Square&, const Bitboard&, Bitboard&, Bitboard&) {
  return KING; // No need to update bitboards, it is the last cycle
}

} // namespace


/// CheckInfo c'tor
/*
����CheckInfo�N���X�̓R���X�g���N�^�����Ȃ�
�ǖʃN���Xposition���󂯎���Č��ǖʂŉ���������Ă����킲�Ƃ�bitboard�i�������`�G�b�N���������Ă��Ȃ��ꍇ��0�j
�GKING�ɑ΂���pin�t������Ă�����bitboard��Ԃ�
*/
CheckInfo::CheckInfo(const Position& pos) {

  Color them = ~pos.side_to_move();
  ksq = pos.king_square(them);

  pinned = pos.pinned_pieces(pos.side_to_move());
  dcCandidates = pos.discovered_check_candidates();

  checkSq[PAWN]   = pos.attacks_from<PAWN>(ksq, them);
  checkSq[KNIGHT] = pos.attacks_from<KNIGHT>(ksq);
  checkSq[BISHOP] = pos.attacks_from<BISHOP>(ksq);
  checkSq[ROOK]   = pos.attacks_from<ROOK>(ksq);
  checkSq[QUEEN]  = checkSq[BISHOP] | checkSq[ROOK];
  checkSq[KING]   = 0;
}


/// Position::init() initializes at startup the various arrays used to compute
/// hash keys and the piece square tables. The latter is a two-step operation:
/// First, the white halves of the tables are copied from PSQT[] tables. Second,
/// the black halves of the tables are initialized by flipping and changing the
/// sign of the white scores.
/*
Zobrist�����������Ă���
���̂��ƕ]���l�i��]���l�ƈʒu�]���l�j���������Ă���
*/
void Position::init() {

  RKISS rk;
	/*
	���ځA��킲�Ƃɗ��������炩���ߐݒ肵�Ă���
	�ǖʂ̏�Ԃɉ����ĂP�ӂ̃n�b�V���l�i�ًǖʂœ���̃n�b�V���l���ł�\���͂���j
	*/
	for (Color c = WHITE; c <= BLACK; ++c)
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          for (Square s = SQ_A1; s <= SQ_H8; ++s)
              Zobrist::psq[c][pt][s] = rk.rand<Key>();
	/*
	�A���p�b�T���̃n�b�V���l�H
	�p�r�s��
	*/
	for (File f = FILE_A; f <= FILE_H; ++f)
      Zobrist::enpassant[f] = rk.rand<Key>();
	/*
	�L���X�����O�̃n�b�V���l�H
	�p�r�s��
	*/
	for (int cr = CASTLES_NONE; cr <= ALL_CASTLES; ++cr)
  {
      Bitboard b = cr;
      while (b)
      {
          Key k = Zobrist::castle[1ULL << pop_lsb(&b)];
          Zobrist::castle[cr] ^= k ? k : rk.rand<Key>();
      }
  }
	/*
	�p�r�s��
	*/
	Zobrist::side = rk.rand<Key>();
  Zobrist::exclusion  = rk.rand<Key>();
	/*
	WHITE���̋�]���l�͒��ڐݒ肵�Ă���i����position.cpp�̖`�������j
	�����ł�WHITE���̕]���l��BLACK���ɃR�s�[���Ă���
	*/
	for (PieceType pt = PAWN; pt <= KING; ++pt)
  {
      PieceValue[MG][make_piece(BLACK, pt)] = PieceValue[MG][pt];
      PieceValue[EG][make_piece(BLACK, pt)] = PieceValue[EG][pt];

      Score v = make_score(PieceValue[MG][pt], PieceValue[EG][pt]);
			/*
			PSQT��psqtab.h�ɒ�`���Ă���z���Score�ϐ����i32bit�̏��16bit�Ƀ~�h���Q�[����]���l���A����16bit�ɃG���h�Q�[����]���l��ݒ肵�Ă���j
			�Ոʒu�ɉ����Ċi�[����Ă���B�ʒu�]���l�̊�{�ʒu�]���l�ƌ�����
			psq[BLACK][pt][~s]��~s�͉��Z�q�̃I�[�o�[���[�h�ō��W�ϊ����Ă��� (��A1->A8,B2->B7�j
			white���i���j���v���X�ABLACK�����i���j�}�C�i�X������
			��{�ʒu�]���l�ɋ�]���l�����Z����psq�z������������Ă���
			�������z���static Score psq[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];�Ɛ錾����Ă���
			�����炭�������̂��ꂩ���A�l�[�~���O�����]���l�ƈʒu�]���l��g�ݍ��킹������
			*/
			for (Square s = SQ_A1; s <= SQ_H8; ++s)
      {
         piece_sq_score[WHITE][pt][ s] =  (v + PSQT[pt][s]);
				 piece_sq_score[BLACK][pt][~s] = -(v + PSQT[pt][s]);
      }
  }
}


/// Position::operator=() creates a copy of 'pos'. We want the new born Position
/// object do not depend on any external data so we detach state pointer from
/// the source one.
/*
�ǖʃN���Xposition���R�s�[���鉉�Z�q�̃I�[�o�[���[�h
startState�͗p�r�s��
*/
Position& Position::operator=(const Position& pos) {

  std::memcpy(this, &pos, sizeof(Position));
  startState = *m_st;
  m_st = &startState;
  nodes = 0;

  assert(pos_is_ok());

  return *this;
}


/// Position::set() initializes the position object with the given FEN string.
/// This function is not very robust - make sure that input FENs are correct,
/// this is assumed to be the responsibility of the GUI.
/*
FEN string��ǂݍ���ŋǖʂ�ݒ肵�Ă���
*/
void Position::set(const string& fenStr, bool isChess960, Thread* th) {
/*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1; within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") while Black take lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. This is recorded regardless of whether
      there is a pawn in position to make an en passant capture.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

  char col, row, token;
  size_t p;
  Square sq = SQ_A8;
  std::istringstream ss(fenStr);

  clear();
	//�󔒕������X�L�b�v�����Ȃ��ݒ�
	ss >> std::noskipws;

  // 1. Piece placement
	/*
	FEN string�̃X�L������A8->B8->...->H8
	A7->B7..->H7
	A1->B1->..H1�Ɠǂݎ���Ă��s��
	*/
	while ((ss >> token) && !isspace(token))
  {
			//�����͋󔒂�\���̂Ő��l�������W�����Z����
			if (isdigit(token))
          sq += Square(token - '0'); // Advance the given number of files
			/*
			FEN string�̌��{
			"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
			*/
			else if (token == '/')
          sq -= Square(16);
			//������FEN string����R�[�h�ɕϊ�����put_piece���Ă�œ����f�[�^���X�V���Ă���
			else if ((p = PieceToChar.find(token)) != string::npos)
      {
          put_piece(sq, color_of(Piece(p)), type_of(Piece(p)));
          ++sq;
      }
  }

  // 2. Active color
  ss >> token;
  sideToMove = (token == 'w' ? WHITE : BLACK);
  ss >> token;

  // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
  // Shredder-FEN that uses the letters of the columns on which the rooks began
  // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
  // if an inner rook is associated with the castling right, the castling tag is
  // replaced by the file letter of the involved rook, as for the Shredder-FEN.
	//�L���X�����O�Ɋւ��邱�Ƃ̂悤�����ڍוs��
	while ((ss >> token) && !isspace(token))
  {
      Square rsq;
      Color c = islower(token) ? BLACK : WHITE;

      token = char(toupper(token));

      if (token == 'K')
          for (rsq = relative_square(c, SQ_H1); type_of(piece_on(rsq)) != ROOK; --rsq) {}

      else if (token == 'Q')
          for (rsq = relative_square(c, SQ_A1); type_of(piece_on(rsq)) != ROOK; ++rsq) {}

      else if (token >= 'A' && token <= 'H')
          rsq = File(token - 'A') | relative_rank(c, RANK_1);

      else
          continue;

      set_castle_right(c, rsq);
  }

  // 4. En passant square. Ignore if no pawn capture is possible
	//�p�r�s��
	if (((ss >> col) && (col >= 'a' && col <= 'h'))
      && ((ss >> row) && (row == '3' || row == '6')))
  {
      m_st->epSquare = File(col - 'a') | Rank(row - '1');

      if (!(attackers_to(m_st->epSquare) & pieces(sideToMove, PAWN)))
          m_st->epSquare = SQ_NONE;
  }

  // 5-6. Halfmove clock and fullmove number
	//�p�r�s��
	ss >> std::skipws >> m_st->rule50 >> gamePly;

  // Convert from fullmove starting from 1 to ply starting from 0,
  // handle also common incorrect FEN with fullmove = 0.
	//�p�r�s��
	gamePly = std::max(2 * (gamePly - 1), 0) + int(sideToMove == BLACK);
	//�ǖʕ����̂��߂̏��
	m_st->key = compute_key();
  m_st->pawnKey = compute_pawn_key();
  m_st->materialKey = compute_material_key();
  m_st->psq = compute_psq_score();
  m_st->npMaterial[WHITE] = compute_non_pawn_material(WHITE);
  m_st->npMaterial[BLACK] = compute_non_pawn_material(BLACK);
  m_st->checkersBB = attackers_to(king_square(sideToMove)) & pieces(~sideToMove);
  chess960 = isChess960;
  thisThread = th;

  assert(pos_is_ok());
}


/// Position::set_castle_right() is an helper function used to set castling
/// rights given the corresponding color and the rook starting square.
/*
�����L���X�����O�Ɋւ���Ȃɂ�
�p�r�s��
*/
void Position::set_castle_right(Color c, Square rfrom) {

  Square kfrom = king_square(c);
  CastlingSide cs = kfrom < rfrom ? KING_SIDE : QUEEN_SIDE;
  CastleRight cr = make_castle_right(c, cs);

  m_st->castleRights |= cr;
  castleRightsMask[kfrom] |= cr;
  castleRightsMask[rfrom] |= cr;
  castleRookSquare[c][cs] = rfrom;

  Square kto = relative_square(c, cs == KING_SIDE ? SQ_G1 : SQ_C1);
  Square rto = relative_square(c, cs == KING_SIDE ? SQ_F1 : SQ_D1);

  for (Square s = std::min(rfrom, rto); s <= std::max(rfrom, rto); ++s)
      if (s != kfrom && s != rfrom)
          castlePath[c][cs] |= s;

  for (Square s = std::min(kfrom, kto); s <= std::max(kfrom, kto); ++s)
      if (s != kfrom && s != rfrom)
          castlePath[c][cs] |= s;
}


/// Position::fen() returns a FEN representation of the position. In case
/// of Chess960 the Shredder-FEN notation is used. Mainly a debugging function.
/*
Position::fen() returns a FEN representation of the position. In case of
Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.
�����f�[�^���FEN string�����グ��
*/
const string Position::fen() const {

  std::ostringstream ss;

  for (Rank rank = RANK_8; rank >= RANK_1; --rank)
  {
      for (File file = FILE_A; file <= FILE_H; ++file)
      {
          Square sq = file | rank;

          if (empty(sq))
          {
              int emptyCnt = 1;

              for ( ; file < FILE_H && empty(++sq); ++file)
                  ++emptyCnt;

              ss << emptyCnt;
          }
          else
              ss << PieceToChar[piece_on(sq)];
      }

      if (rank > RANK_1)
          ss << '/';
  }

  ss << (sideToMove == WHITE ? " w " : " b ");

  if (can_castle(WHITE_OO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(WHITE,  KING_SIDE)), false) : 'K');

  if (can_castle(WHITE_OOO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(WHITE, QUEEN_SIDE)), false) : 'Q');

  if (can_castle(BLACK_OO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK,  KING_SIDE)),  true) : 'k');

  if (can_castle(BLACK_OOO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK, QUEEN_SIDE)),  true) : 'q');

  if (m_st->castleRights == CASTLES_NONE)
      ss << '-';

  ss << (ep_square() == SQ_NONE ? " - " : " " + square_to_string(ep_square()) + " ")
      << m_st->rule50 << " " << 1 + (gamePly - int(sideToMove == BLACK)) / 2;

  return ss.str();
}


/// Position::pretty() returns an ASCII representation of the position to be
/// printed to the standard output together with the move's san notation.
/*
�����f�[�^�iboard[]�Ȃǁj�Ɠn���ꂽ�w�������\������
*/
const string Position::pretty(Move move) const {

  const string dottedLine =            "\n+---+---+---+---+---+---+---+---+";
  const string twoRows =  dottedLine + "\n|   | . |   | . |   | . |   | . |"
                        + dottedLine + "\n| . |   | . |   | . |   | . |   |";

  string brd = twoRows + twoRows + twoRows + twoRows + dottedLine;

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      brd[513 - 68 * rank_of(s) + 4 * file_of(s)] = PieceToChar[piece_on(s)];
  }

  std::ostringstream ss;

  if (move)
      ss << "\nMove: " << (sideToMove == BLACK ? ".." : "")
         << move_to_san(*const_cast<Position*>(this), move);

  ss << brd << "\nFen: " << fen() << "\nKey: " << std::hex << std::uppercase
     << std::setfill('0') << std::setw(16) << m_st->key << "\nCheckers: ";

  for (Bitboard b = checkers(); b; )
      ss << square_to_string(pop_lsb(&b)) << " ";

  ss << "\nLegal moves: ";
  for (const ExtMove& ms : MoveList<LEGAL>(*this))
      ss << move_to_san(*const_cast<Position*>(this), ms.move) << " ";

  return ss.str();
}


/// Position:hidden_checkers() returns a bitboard of all pinned / discovery check
/// pieces, according to the call parameters. Pinned pieces protect our king,
/// discovery check pieces attack the enemy king.
/*
ksq�͎��w��KING�̍��W
Color�͓G�T�C�h�̃J���[
toMove�͎��w�T�C�h�̃J���[

�G�̑���pin����Ă��鎩���Ԃ�
*/
Bitboard Position::hidden_checkers(Square ksq, Color c, Color toMove) const {

  Bitboard b, pinners, result = 0;

  // Pinners are sliders that give check when pinned piece is removed
	/*
	�G��QUEEN,ROOK,BISHOP��Ŏ��wKING�ɉe�̗������Ƃ����Ă�����pinners(bitboard)�ɓ���Ă���
	*/
  pinners = (  (pieces(  ROOK, QUEEN) & PseudoAttacks[ROOK  ][ksq])
             | (pieces(BISHOP, QUEEN) & PseudoAttacks[BISHOP][ksq])) & pieces(c);
	/*
	���̉e�̗������|���Ă�����KING�̊Ԃɂ�����b(bitboard)�ɓ����
	���̋�P�ł��鎩���Ԃ��imore_than_one�֐������肷��j
	*/
  while (pinners)
  {
      b = between_bb(ksq, pop_lsb(&pinners)) & pieces();

      if (!more_than_one(b))
          result |= b & pieces(toMove);
  }
  return result;
}


/// Position::attackers_to() computes a bitboard of all pieces which attack a
/// given square. Slider attacks use occ bitboard as occupancy.
/*
attacks_from<>�֐��̊T�v
�w�肵����R�[�h�A�Ս��W�ɂ����̗�����bitboard��Ԃ��B������PAWN�͐i�ޕ�����
�����闘�����Ⴄ�Aattacks_from�͂����܂ŋ���Ƃ闘���݂̂�����
attacks_from�֐��͂R�I�[�o�[���[�h���Ă���
attacks_from(Square s)�̓e���v���[�g�����ŋ����w�肵�āA
�w�肵�����W���痘����bitboard��Ԃ��B
��ы���łȂ����ы�̗���bitboard���Ԃ���B
attacks_from<PAWN>(Square s, Color c)
����PAWN�����ō��W�ƃJ���[���w��ł��ė���bitboard��Ԃ�
attacks_from(Piece pc, Square s)
�w�肵�����W�A�w�肵����킩�痘����bitboard��Ԃ��B
���ы�͑Ή����Ă��Ȃ�

attackers_to�֐��̋@�\
�w�肵�����W�ɗ����Ă���S�Ă̋�i�J���[�Ɋ֌W�Ȃ��j�����o���ăr�b�g�𗧂Ă�
bitboard��Ԃ�
*/
Bitboard Position::attackers_to(Square s, Bitboard occ) const {

  return  (attacks_from<PAWN>(s, BLACK) & pieces(WHITE, PAWN))
        | (attacks_from<PAWN>(s, WHITE) & pieces(BLACK, PAWN))
        | (attacks_from<KNIGHT>(s)      & pieces(KNIGHT))
        | (attacks_bb<ROOK>(s, occ)     & pieces(ROOK, QUEEN))
        | (attacks_bb<BISHOP>(s, occ)   & pieces(BISHOP, QUEEN))
        | (attacks_from<KING>(s)        & pieces(KING));
}


/// Position::attacks_from() computes a bitboard of all attacks of a given piece
/// put in a given square. Slider attacks use occ bitboard as occupancy.

Bitboard Position::attacks_from(Piece p, Square s, Bitboard occ) {

  assert(is_ok(s));

  switch (type_of(p))
  {
  case BISHOP: return attacks_bb<BISHOP>(s, occ);
  case ROOK  : return attacks_bb<ROOK>(s, occ);
  case QUEEN : return attacks_bb<BISHOP>(s, occ) | attacks_bb<ROOK>(s, occ);
  default    : return StepAttacksBB[p][s];
  }
}


/// Position::legal() tests whether a pseudo-legal move is legal
/*
����Move m�����@�肩�������鍇�@�肩�ǂ�����
�A���p�b�T����������
�p�r�s��
�������KING��������
�ړ���ɓG�̗����������Ă�����NG�A�L���X�����O��OK
pin���������Ă��Ȃ�����,pin���������Ă��Ă�pin���͂���Ȃ������Ȃ�OK
*/
bool Position::legal(Move m, Bitboard pinned) const {

  assert(is_ok(m));
  assert(pinned == pinned_pieces(sideToMove));

  Color us = sideToMove;
  Square from = from_sq(m);

  assert(color_of(moved_piece(m)) == us);
  assert(piece_on(king_square(us)) == make_piece(us, KING));

  // En passant captures are a tricky special case. Because they are rather
  // uncommon, we do it simply by testing whether the king is attacked after
  // the move is made.
  if (type_of(m) == ENPASSANT)
  {
      Color them = ~us;
      Square to = to_sq(m);
      Square capsq = to + pawn_push(them);
      Square ksq = king_square(us);
      Bitboard b = (pieces() ^ from ^ capsq) | to;

      assert(to == ep_square());
      assert(moved_piece(m) == make_piece(us, PAWN));
      assert(piece_on(capsq) == make_piece(them, PAWN));
      assert(piece_on(to) == NO_PIECE);

      return   !(attacks_bb<  ROOK>(ksq, b) & pieces(them, QUEEN, ROOK))
            && !(attacks_bb<BISHOP>(ksq, b) & pieces(them, QUEEN, BISHOP));
  }

  // If the moving piece is a king, check whether the destination
  // square is attacked by the opponent. Castling moves are checked
  // for legality during move generation.
  if (type_of(piece_on(from)) == KING)
      return type_of(m) == CASTLE || !(attackers_to(to_sq(m)) & pieces(~us));

  // A non-king move is legal if and only if it is not pinned or it
  // is moving along the ray towards or away from the king.
	/*
	BISHOP,ROOK������PIN����Ă��Ă�PIN���O���Ȃ��������Ȃ�OK
	*/
	return   !pinned
        || !(pinned & from)
        ||  aligned(from, to_sq(m), king_square(us));
}


/// Position::pseudo_legal() takes a random move and tests whether the move is
/// pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.
/*
���@��ł��邩�e�X�g����
�u���\�̎�̌����ɂ��g�p����
*/
bool Position::pseudo_legal(const Move m) const {

  Color us = sideToMove;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);

  // Use a slower but simpler function for uncommon cases
  if (type_of(m) != NORMAL)
      return MoveList<LEGAL>(*this).contains(m);

  // Is not a promotion, so promotion piece must be empty
  if (promotion_type(m) - 2 != NO_PIECE_TYPE)
      return false;

  // If the from square is not occupied by a piece belonging to the side to
  // move, the move is obviously not legal.
  if (pc == NO_PIECE || color_of(pc) != us)
      return false;

  // The destination square cannot be occupied by a friendly piece
  if (pieces(us) & to)
      return false;

  // Handle the special case of a pawn move
  if (type_of(pc) == PAWN)
  {
      // Move direction must be compatible with pawn color
      int direction = to - from;
      if ((us == WHITE) != (direction > 0))
          return false;

      // We have already handled promotion moves, so destination
      // cannot be on the 8/1th rank.
      if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1)
          return false;

      // Proceed according to the square delta between the origin and
      // destination squares.
      switch (direction)
      {
      case DELTA_NW:
      case DELTA_NE:
      case DELTA_SW:
      case DELTA_SE:
      // Capture. The destination square must be occupied by an enemy
      // piece (en passant captures was handled earlier).
      if (piece_on(to) == NO_PIECE || color_of(piece_on(to)) != ~us)
          return false;

      // From and to files must be one file apart, avoids a7h5
      if (abs(file_of(from) - file_of(to)) != 1)
          return false;
      break;

      case DELTA_N:
      case DELTA_S:
      // Pawn push. The destination square must be empty.
      if (!empty(to))
          return false;
      break;

      case DELTA_NN:
      // Double white pawn push. The destination square must be on the fourth
      // rank, and both the destination square and the square between the
      // source and destination squares must be empty.
      if (    rank_of(to) != RANK_4
          || !empty(to)
          || !empty(from + DELTA_N))
          return false;
      break;

      case DELTA_SS:
      // Double black pawn push. The destination square must be on the fifth
      // rank, and both the destination square and the square between the
      // source and destination squares must be empty.
      if (    rank_of(to) != RANK_5
          || !empty(to)
          || !empty(from + DELTA_S))
          return false;
      break;

      default:
          return false;
      }
  }
  else if (!(attacks_from(pc, from) & to))
      return false;

  // Evasions generator already takes care to avoid some kind of illegal moves
  // and pl_move_is_legal() relies on this. So we have to take care that the
  // same kind of moves are filtered out here.
  if (checkers())
  {
      if (type_of(pc) != KING)
      {
          // Double check? In this case a king move is required
          if (more_than_one(checkers()))
              return false;

          // Our move must be a blocking evasion or a capture of the checking piece
          if (!((between_bb(lsb(checkers()), king_square(us)) | checkers()) & to))
              return false;
      }
      // In case of king moves under check we have to remove king so to catch
      // as invalid moves like b1a1 when opposite queen is on c1.
      else if (attackers_to(to, pieces() ^ from) & pieces(~us))
          return false;
  }

  return true;
}


/// Position::move_gives_check() tests whether a pseudo-legal move gives a check
/*
�w���肪����ł����true��Ԃ�
*/
bool Position::gives_check(Move m, const CheckInfo& ci) const {

  assert(is_ok(m));
  assert(ci.dcCandidates == discovered_check_candidates());
  assert(color_of(moved_piece(m)) == sideToMove);

  Square from = from_sq(m);
  Square to = to_sq(m);
  PieceType pt = type_of(piece_on(from));

  // Direct check ?
	//��̈ړ���Ɉړ��������ʉ��肪�|�����ʒu�Ȃ�true
	if (ci.checkSq[pt] & to)
      return true;

  // Discovery check ?
	/*
	�ړ����邱�Ƃŉ��肪�|�����Ȃ�true��Ԃ�
	*/
	if (unlikely(ci.dcCandidates) && (ci.dcCandidates & from))
  {
      // For pawn and king moves we need to verify also direction
      if (   (pt != PAWN && pt != KING)
          || !aligned(from, to, king_square(~sideToMove)))
          return true;
  }

  // Can we skip the ugly special cases ?
  if (type_of(m) == NORMAL)
      return false;

  Color us = sideToMove;
  Square ksq = king_square(~us);
	/*
	�w����p�^�[���ɂ���Ĕ��f�ANORMAL�Ȃ瑦false
	�w����p�^�[��������Ő�������ŉ��肪�ł���悤�Ȃ�true
	*/
	switch (type_of(m))
  {
  case PROMOTION:
      return attacks_from(Piece(promotion_type(m)), to, pieces() ^ from) & ksq;

  // En passant capture with check ? We have already handled the case
  // of direct checks and ordinary discovered check, the only case we
  // need to handle is the unusual case of a discovered check through
  // the captured pawn.
	/*
	�A���p�b�T���֌W�̂悤�����ڍוs��
	*/
	case ENPASSANT:
  {
      Square capsq = file_of(to) | rank_of(from);
      Bitboard b = (pieces() ^ from ^ capsq) | to;

      return  (attacks_bb<  ROOK>(ksq, b) & pieces(us, QUEEN, ROOK))
            | (attacks_bb<BISHOP>(ksq, b) & pieces(us, QUEEN, BISHOP));
  }
	/*
	�L���X�����O�̂悤�����ڍוs��
	*/
	case CASTLE:
  {
      Square kfrom = from;
      Square rfrom = to; // 'King captures the rook' notation
      Square kto = relative_square(us, rfrom > kfrom ? SQ_G1 : SQ_C1);
      Square rto = relative_square(us, rfrom > kfrom ? SQ_F1 : SQ_D1);

      return   (PseudoAttacks[ROOK][rto] & ksq)
            && (attacks_bb<ROOK>(rto, (pieces() ^ kfrom ^ rfrom) | rto | kto) & ksq);
  }
  default:
      assert(false);
      return false;
  }
}


/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.
/*
�ǖʂ��X�V����֐��A���̊֐�do_move�̃I�[�o�[���[�h
*/
void Position::do_move(Move m, StateInfo& newSt) {

  CheckInfo ci(*this);
  do_move(m, newSt, ci, gives_check(m, ci));
}
/*
�ǖʂ��X�V����B��̊֐�
*/
void Position::do_move(Move m, StateInfo& newSt, const CheckInfo& ci, bool moveIsCheck) {

  assert(is_ok(m));
  assert(&newSt != m_st);
	/*
	�W�J�����m�[�h�����J�E���g���Ă���
	think�֐��ŒT�����I����������nodes_searched�֐����Ăяo��
	����nodes����\��������
	*/
	++nodes;
	/*
	StateInfo.key�ɋǖʂ̃n�b�V���l���L�^����Ă���
	*/
	Key k = m_st->key;

  // Copy some fields of old state to our new StateInfo object except the ones
  // which are going to be recalculated from scratch anyway, then switch our state
  // pointer to point to the new, ready to be updated, state.
	/*
	StateCopySize64��StateInfo�\���̂̂Ȃ���key�A�C�e���܂ł̃I�t�Z�b�g�ibyte�P�ʁj����Ԃ�
	�܂�StateInfo�\���̂̈ꕔ����newSt�ɃR�s�[����i���̑S���R�s�[���Ȃ��̂��͕s���j
	*/
	std::memcpy(&newSt, m_st, StateCopySize64 * sizeof(uint64_t));
	/*
	StateInfo���Ȃ��ł���
	*/
	newSt.previous = m_st;
  m_st = &newSt;

  // Update side to move
	/*
	�ǖʂ̃n�b�V���l���X�V���Ă���
	*/
	k ^= Zobrist::side;

  // Increment ply counters.In particular rule50 will be later reset it to zero
  // in case of a capture or a pawn move.
	/*
	gamePly�̓Q�[���萔�̃J�E���g�A�b�v
	rule50�̂��߂̃J�E���g�A�b�v
	pliesFromNull�͍��̂Ƃ���s��do_null_move�֐��ł�0�ɏ���������
	*/
	++gamePly;
  ++m_st->rule50;
  ++m_st->pliesFromNull;

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = piece_on(from);
  PieceType pt = type_of(pc);
  PieceType captured = type_of(m) == ENPASSANT ? PAWN : type_of(piece_on(to));

  assert(color_of(pc) == us);
  assert(piece_on(to) == NO_PIECE || color_of(piece_on(to)) == them || type_of(m) == CASTLE);
  assert(captured != KING);

  if (type_of(m) == CASTLE)
  {
      assert(pc == make_piece(us, KING));

      bool kingSide = to > from;
      Square rfrom = to; // Castle is encoded as "king captures friendly rook"
      Square rto = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
      to = relative_square(us, kingSide ? SQ_G1 : SQ_C1);
      captured = NO_PIECE_TYPE;

      do_castle(from, to, rfrom, rto);

			m_st->psq += piece_sq_score[us][ROOK][rto] - piece_sq_score[us][ROOK][rfrom];
      k ^= Zobrist::psq[us][ROOK][rfrom] ^ Zobrist::psq[us][ROOK][rto];
  }
	/*
	captured�͎������̋��A��������ł͂Ȃ����captured��0
	*/
	if (captured)
  {
      Square capsq = to;

      // If the captured piece is a pawn, update pawn hash key, otherwise
      // update non-pawn material.
			/*
			�����Ƃ�����킪PAWN�Ŏ������A���p�b�T���Ȃ�
			�Ƃ�����͈ړ���̐^���ɂȂ�i�A���p�b�T���̃��[���m�F�j�̂�
			capsq��to�ł͂Ȃ�to+pawn_push(them)�ƂȂ�
			*/
			if (captured == PAWN)
      {
          if (type_of(m) == ENPASSANT)
          {
              capsq += pawn_push(them);

              assert(pt == PAWN);
              assert(to == m_st->epSquare);
              assert(relative_rank(us, to) == RANK_6);
              assert(piece_on(to) == NO_PIECE);
              assert(piece_on(capsq) == make_piece(them, PAWN));

              board[capsq] = NO_PIECE;
          }

          m_st->pawnKey ^= Zobrist::psq[them][PAWN][capsq];
      }
      else
          m_st->npMaterial[them] -= PieceValue[MG][captured];

      // Update board and piece lists
			/*
			�����������Ƃɂ���ĕύX�ɂȂ�
			byTypeBB,byTypeBB,byColorBB���X�V
			index[],pieceList[][][],pieceCount[][]�z����X�V����֐�
			*/
			remove_piece(capsq, them, captured);

      // Update material hash key and prefetch access to materialTable
			/*
			�ǖʂ̃n�b�V���l������ꂽ��̃n�b�V���l���������Ă���
			������materialKey���ύX���Ă���
			*/
			k ^= Zobrist::psq[them][captured][capsq];
      m_st->materialKey ^= Zobrist::psq[them][captured][pieceCount[them][captured]];
      prefetch((char*)thisThread->materialTable[m_st->materialKey]);

      // Update incremental scores
			/*
			m_st->psq�͈ʒu�]���l�̏W�v�l�Ȃ̂Ŏ��ꂽ��̍��������Ă���
			*/
			m_st->psq -= piece_sq_score[them][captured][capsq];

      // Reset rule 50 counter
			/*
			���������̂�rule50�͈�U�L�����Z���ƂȂ�
			*/
			m_st->rule50 = 0;
  }
	/*
	����ȍ~�͋������Ă��Ȃ��w����̍X�V
	*/
	// Update hash key
	/*
	�ړ��O�̃n�b�V���l���������A�ړ���̃n�b�V���l���X�V���Ă���
	*/
	k ^= Zobrist::psq[us][pt][from] ^ Zobrist::psq[us][pt][to];

  // Reset en passant square
	/*
	�A���p�b�T���֌W���Ǝv�����ڍוs��
	*/
	if (m_st->epSquare != SQ_NONE)
  {
      k ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      m_st->epSquare = SQ_NONE;
  }

  // Update castle rights if needed
	/*
	�L���X�����O�֌W���ȁA�ڍוs��
	*/
	if (m_st->castleRights && (castleRightsMask[from] | castleRightsMask[to]))
  {
      int cr = castleRightsMask[from] | castleRightsMask[to];
      k ^= Zobrist::castle[m_st->castleRights & cr];
      m_st->castleRights &= ~cr;
  }

  // Prefetch TT access as soon as we know the new hash key
  prefetch((char*)TT.first_entry(k));

  // Move the piece. The tricky Chess960 castle is handled earlier
	/*
	move_piece�֐��̋@�\��
	byTypeBB,byTypeBB,byColorBB�̍X�V
	board,index[],pieceList[][][]�z��̍X�V
	����͂Ȃ��̂�pieceCount[]�z��̍X�V�͂Ȃ�
	*/
	if (type_of(m) != CASTLE)
      move_piece(from, to, us, pt);

  // If the moving piece is a pawn do some special extra work
  if (pt == PAWN)
  {
      // Set en-passant square, only if moved pawn can be captured
		/*
		(int(to) ^ int(from)) == 16�ƂȂ�PAWN�̓����͂Q�i�Ƃт̂�
		�ł��A���p�b�T���ł���������������Ă���ꍇ�̏���
		*/
		if ((int(to) ^ int(from)) == 16
          && (attacks_from<PAWN>(from + pawn_push(us), us) & pieces(them, PAWN)))
      {
          m_st->epSquare = Square((from + to) / 2);
          k ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      }
		/*
		PAWN���Ȃ�ꍇ�̏���
		*/
		if (type_of(m) == PROMOTION)
      {
          PieceType promotion = promotion_type(m);

          assert(relative_rank(us, to) == RANK_8);
          assert(promotion >= KNIGHT && promotion <= QUEEN);
					/*
					��UPAWN���������鏈��
					*/
					remove_piece(to, us, PAWN);
					/*
					�Ȃ�������ړ���ɒu������
					*/
					put_piece(to, us, promotion);

          // Update hash keys
					/*
					�ǖʂ̃n�b�V���l�����X�V�Apawn��p�̃n�b�V���l���X�V
					*/
					k ^= Zobrist::psq[us][PAWN][to] ^ Zobrist::psq[us][promotion][to];
          m_st->pawnKey ^= Zobrist::psq[us][PAWN][to];
          m_st->materialKey ^=  Zobrist::psq[us][promotion][pieceCount[us][promotion]-1]
                            ^ Zobrist::psq[us][PAWN][pieceCount[us][PAWN]];

          // Update incremental score
					//�ʒu�]���l���X�V
					m_st->psq += piece_sq_score[us][promotion][to] - piece_sq_score[us][PAWN][to];

          // Update material
					/*
					��]���l���X�V
					*/
					m_st->npMaterial[us] += PieceValue[MG][promotion];
      }

      // Update pawn hash key and prefetch access to pawnsTable
      m_st->pawnKey ^= Zobrist::psq[us][PAWN][from] ^ Zobrist::psq[us][PAWN][to];
      prefetch((char*)thisThread->pawnsTable[m_st->pawnKey]);

      // Reset rule 50 draw counter
			/*
			���������������N���A
			*/
			m_st->rule50 = 0;
  }
	/*
	����ȍ~�͋�����Ȃ��w�����PAWN�ȊO�̋��̏����A���ʏ�������
	*/
	// Update incremental scores
	/*
	�ʒu�]���l�̍X�V
	*/
	m_st->psq += piece_sq_score[us][pt][to] - piece_sq_score[us][pt][from];

  // Set capture piece
	/*
	�Ƃ������
	*/
	m_st->capturedType = captured;

  // Update the key with the final value
	/*
	�ŏI�n�b�V���l��o�^
	*/
	m_st->key = k;

  // Update checkers bitboard, piece must be already moved
  m_st->checkersBB = 0;
	/*
	moveIsCheck��do_move�֐��̈����̂P��
	����̎肪����Ȃ�true
	*/
	if (moveIsCheck)
  {
      if (type_of(m) != NORMAL)
          m_st->checkersBB = attackers_to(king_square(them)) & pieces(us);
      else
      {
          // Direct checks
					/*
					ci.checkSq[pt]�ɂ͋�킲�ƂɓGKING�ɉ���������邱�Ƃŏo����bitboard���͂����Ă���
					�����Move�ɂ���Ă��̏ꏊ�Ɉړ��ł�������ci.checkSq[pt] & to�Ń`�G�b�N���Ă���
					�����ă`�G�b�N���\�ł����checkerBB�ɒǉ����Ă���
					*/
					if (ci.checkSq[pt] & to)
						m_st->checkersBB |= to;

          // Discovery checks
					/*
					ROOK�܂���BISHOP�ł͂Ȃ�����������Ƃ�ROOK,BISHOP�̗������GKING�ɓ͂����̂ł͂Ȃ����`�G�b�N���Ă���
					*/
					if (ci.dcCandidates && (ci.dcCandidates & from))
          {
              if (pt != ROOK)
                  m_st->checkersBB |= attacks_from<ROOK>(king_square(them)) & pieces(us, QUEEN, ROOK);

              if (pt != BISHOP)
                  m_st->checkersBB |= attacks_from<BISHOP>(king_square(them)) & pieces(us, QUEEN, BISHOP);
          }
      }
  }
	/*
	��Ԃ̕ύX
	*/
	sideToMove = ~sideToMove;

  assert(pos_is_ok());
}


/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.
/*
do_move�֐��ɂ���ׂ������R�[�h�ʂ����Ȃ�
*/
void Position::undo_move(Move m) {

  assert(is_ok(m));

  sideToMove = ~sideToMove;

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  PieceType pt = type_of(piece_on(to));
  PieceType captured = m_st->capturedType;

  assert(empty(from) || type_of(m) == CASTLE);
  assert(captured != KING);

  if (type_of(m) == PROMOTION)
  {
      PieceType promotion = promotion_type(m);

      assert(promotion == pt);
      assert(relative_rank(us, to) == RANK_8);
      assert(promotion >= KNIGHT && promotion <= QUEEN);
			/*
			remove_piece�֐��͋����菜�����̏���
			�ЂƂ܂��A�Ȃ���������Ƃɖ߂��ʏ�̈ړ��̏����Ƌ��ʉ�����
			*/
			remove_piece(to, us, promotion);
      put_piece(to, us, PAWN);
      pt = PAWN;
  }
	/*
	�L���X�����O�֌W�̖߂�����
	*/
	if (type_of(m) == CASTLE)
  {
      bool kingSide = to > from;
      Square rfrom = to; // Castle is encoded as "king captures friendly rook"
      Square rto = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
      to = relative_square(us, kingSide ? SQ_G1 : SQ_C1);
      captured = NO_PIECE_TYPE;
      pt = KING;
      do_castle(to, from, rto, rfrom);
  }
  else
		/*
		�ړ��̖߂��ifrom,to���e���R�ɂ��Ă���j
		*/
		move_piece(to, from, us, pt); // Put the piece back at the source square

	if (captured)
  {
      Square capsq = to;

      if (type_of(m) == ENPASSANT)
      {
          capsq -= pawn_push(us);

          assert(pt == PAWN);
          assert(to == m_st->previous->epSquare);
          assert(relative_rank(us, to) == RANK_6);
          assert(piece_on(capsq) == NO_PIECE);
      }

      put_piece(capsq, them, captured); // Restore the captured piece
  }

  // Finally point our state pointer back to the previous state
  m_st = m_st->previous;
  --gamePly;

  assert(pos_is_ok());
}


/// Position::do_castle() is a helper used to do/undo a castling move. This
/// is a bit tricky, especially in Chess960.
/*
�L���X�����O�֌W�A�ڍוs��
*/
void Position::do_castle(Square kfrom, Square kto, Square rfrom, Square rto) {

  // Remove both pieces first since squares could overlap in Chess960
  remove_piece(kfrom, sideToMove, KING);
  remove_piece(rfrom, sideToMove, ROOK);
  board[kfrom] = board[rfrom] = NO_PIECE; // Since remove_piece doesn't do it for us
  put_piece(kto, sideToMove, KING);
  put_piece(rto, sideToMove, ROOK);
}


/// Position::do(undo)_null_move() is used to do(undo) a "null move": It flips
/// the side to move without executing any move on the board.
/*
�k�����[�u�p�̋ǖʍX�V�֐�
*/
void Position::do_null_move(StateInfo& newSt) {

  assert(!checkers());

  std::memcpy(&newSt, m_st, sizeof(StateInfo)); // Fully copy here

  newSt.previous = m_st;
  m_st = &newSt;

  if (m_st->epSquare != SQ_NONE)
  {
      m_st->key ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      m_st->epSquare = SQ_NONE;
  }

  m_st->key ^= Zobrist::side;
  prefetch((char*)TT.first_entry(m_st->key));

  ++m_st->rule50;
  m_st->pliesFromNull = 0;

  sideToMove = ~sideToMove;

  assert(pos_is_ok());
}
/*
�k�����[�u�p�̋ǖʕ����֐�
*/
void Position::undo_null_move() {

  assert(!checkers());

  m_st = m_st->previous;
  sideToMove = ~sideToMove;
}


/// Position::see() is a static exchange evaluator: It tries to estimate the
/// material gain or loss resulting from a move. Parameter 'asymmThreshold' takes
/// tempi into account. If the side who initiated the capturing sequence does the
/// last capture, he loses a tempo and if the result is below 'asymmThreshold'
/// the capturing sequence is considered bad.
/*
�Î~�T��
*/
int Position::see_sign(Move m) const {

  assert(is_ok(m));

  // Early return if SEE cannot be negative because captured piece value
  // is not less then capturing one. Note that king moves always return
  // here because king midgame value is set to 0.
  if (PieceValue[MG][moved_piece(m)] <= PieceValue[MG][piece_on(to_sq(m))])
      return 1;

  return see(m);
}

int Position::see(Move m, int asymmThreshold) const {

  Square from, to;
  Bitboard occupied, attackers, stmAttackers;
  int swapList[32], slIndex = 1;
  PieceType captured;
  Color stm;

  assert(is_ok(m));

  from = from_sq(m);
  to = to_sq(m);
  swapList[0] = PieceValue[MG][piece_on(to)];
  stm = color_of(piece_on(from));
  occupied = pieces() ^ from;

  // Castle moves are implemented as king capturing the rook so cannot be
  // handled correctly. Simply return 0 that is always the correct value
  // unless in the rare case the rook ends up under attack.
	/*
	�L���X�����O�֌W�Ȃ牿�l0�ŕԂ�
	*/
	if (type_of(m) == CASTLE)
      return 0;
	/*
	�w����p�^�[�����A���p�b�T���Ȃ�swapList��pawn�����Ă���
	*/
	if (type_of(m) == ENPASSANT)
  {
      occupied ^= to - pawn_push(stm); // Remove the captured pawn
      swapList[0] = PieceValue[MG][PAWN];
  }

  // Find all attackers to the destination square, with the moving piece
  // removed, but possibly an X-ray attacker added behind it.
	/*
	�w������̈ړ���ɗ����Ă�����bitboard��attackers�ɓ����i�J���[�Ɋ֌W�Ȃ��j
	*/
	attackers = attackers_to(to, occupied) & occupied;

  // If the opponent has no attackers we are finished
	/*
	stmAttackers�ɓG�̋��������
	�����G�̋�Ȃ���΁i�܂��荇�����Ȃ���΁j
	�����Œ��f
	*/
	stm = ~stm;
  stmAttackers = attackers & pieces(stm);
  if (!stmAttackers)
      return swapList[0];

  // The destination square is defended, which makes things rather more
  // difficult to compute. We proceed by building up a "swap list" containing
  // the material gain or loss at each stop in a sequence of captures to the
  // destination square, where the sides alternately capture, and always
  // capture with the least valuable piece. After each capture, we look for
  // new X-ray attacks from behind the capturing piece.
  captured = type_of(piece_on(from));
	/*
	slIndex�͂P����n�܂�
	swapList[]�͎�荇����X�g
	*/
	do {
      assert(slIndex < 32);

      // Add the new entry to the swap list
			/*
			��荇���ɂȂ��Ă��������̋�]���l��swapList�ɋL�^����
			*/
			swapList[slIndex] = -swapList[slIndex - 1] + PieceValue[MG][captured];
      ++slIndex;

      // Locate and remove the next least valuable attacker
			/*
			�܂���荇����PAWN���炨���Ȃ��Ato���W�ɂ��Ă�������荇��
			���̋���Ԃ�
			*/
			captured = min_attacker<PAWN>(byTypeBB, to, stmAttackers, occupied, attackers);
      stm = ~stm;
      stmAttackers = attackers & pieces(stm);

      // Stop before processing a king capture
			/*
			��荇���ɂȂ菙�X�ɋ�킪PAWN����オ���Ă�����KNIG�ɂȂ����炻���ł�߂�
			*/
			if (captured == KING && stmAttackers)
      {
          swapList[slIndex++] = QueenValueMg * 16;
          break;
      }

  } while (stmAttackers);

  // If we are doing asymmetric SEE evaluation and the same side does the first
  // and the last capture, he loses a tempo and gain must be at least worth
  // 'asymmThreshold', otherwise we replace the score with a very low value,
  // before negamaxing.
  if (asymmThreshold)
      for (int i = 0; i < slIndex; i += 2)
          if (swapList[i] < asymmThreshold)
              swapList[i] = - QueenValueMg * 16;

  // Having built the swap list, we negamax through it to find the best
  // achievable score from the point of view of the side to move.
	/*
	swapList��k��ŏ��̕]���l�𓾂�H
	*/
	while (--slIndex)
      swapList[slIndex - 1] = std::min(-swapList[slIndex], swapList[slIndex - 1]);

  return swapList[0];
}


/// Position::clear() erases the position object to a pristine state, with an
/// empty board, white to move, and no castling rights.
/*
position�N���X���N���A�ɂ���A
startState�͗p�r�s��
*/
void Position::clear() {

  std::memset(this, 0, sizeof(Position));
  startState.epSquare = SQ_NONE;
  m_st = &startState;

  for (int i = 0; i < PIECE_TYPE_NB; ++i)
      for (int j = 0; j < 16; ++j)
          pieceList[WHITE][i][j] = pieceList[BLACK][i][j] = SQ_NONE;
}


/// Position::compute_key() computes the hash key of the position. The hash
/// key is usually updated incrementally as moves are made and unmade, the
/// compute_key() function is only used when a new position is set up, and
/// to verify the correctness of the hash key when running in debug mode.
/*
�ǖʂ̑S�Ă̋�Ǝ�ԂƃL���X�����O�A�A���p�b�T���Ɍ������ăn�b�V���l�����߂�
*/
Key Position::compute_key() const {

  Key k = Zobrist::castle[m_st->castleRights];

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      k ^= Zobrist::psq[color_of(piece_on(s))][type_of(piece_on(s))][s];
  }

  if (ep_square() != SQ_NONE)
      k ^= Zobrist::enpassant[file_of(ep_square())];

  if (sideToMove == BLACK)
      k ^= Zobrist::side;

  return k;
}

/// Position::compute_pawn_key() computes the hash key of the position. The
/// hash key is usually updated incrementally as moves are made and unmade,
/// the compute_pawn_key() function is only used when a new position is set
/// up, and to verify the correctness of the pawn hash key when running in
/// debug mode.
/*
�ǖʏ��PAWN�����̏��Ɋ�Â��ăn�b�V���l�����߂�
*/
Key Position::compute_pawn_key() const {

  Key k = 0;

  for (Bitboard b = pieces(PAWN); b; )
  {
      Square s = pop_lsb(&b);
      k ^= Zobrist::psq[color_of(piece_on(s))][PAWN][s];
  }

  return k;
}

/// Position::compute_material_key() computes the hash key of the position.
/// The hash key is usually updated incrementally as moves are made and unmade,
/// the compute_material_key() function is only used when a new position is set
/// up, and to verify the correctness of the material hash key when running in
/// debug mode.
/*
Zobrist::psq[2][8][64]�ƍŌ�̏��͖{���A���W��\�����̂���
��ŗ����\�������Ă���
�f�o�b�N�`�G�b�N��pos_is_ok�֐�����ƁAset�֐�����̂݌Ă΂�Ă���
����imaterial�j�̏��Ńn�b�V���l�����߂Ă���
*/
Key Position::compute_material_key() const {

  Key k = 0;

  for (Color c = WHITE; c <= BLACK; ++c)
      for (PieceType pt = PAWN; pt <= QUEEN; ++pt)
          for (int cnt = 0; cnt < pieceCount[c][pt]; ++cnt)
              k ^= Zobrist::psq[c][pt][cnt];

  return k;
}


/// Position::compute_psq_score() computes the incremental scores for the middle
/// game and the endgame. These functions are used to initialize the incremental
/// scores when a new position is set up, and to verify that the scores are correctly
/// updated by do_move and undo_move when the program is running in debug mode.
/*
�ʒu�]���l�̏W�v
*/
Score Position::compute_psq_score() const {

  Score score = SCORE_ZERO;

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      Piece pc = piece_on(s);
			score += piece_sq_score[color_of(pc)][type_of(pc)][s];
  }

  return score;
}


/// Position::compute_non_pawn_material() computes the total non-pawn middle
/// game material value for the given side. Material values are updated
/// incrementally during the search, this function is only used while
/// initializing a new Position object.
/*
PAWN����������]���l�̏W�v�i���Օ]���l�j
*/
Value Position::compute_non_pawn_material(Color c) const {

  Value value = VALUE_ZERO;

  for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt)
      value += pieceCount[c][pt] * PieceValue[MG][pt];

  return value;
}


/// Position::is_draw() tests whether the position is drawn by material,
/// repetition, or the 50 moves rule. It does not detect stalemates, this
/// must be done by the search.
/*
�h���[�i���������j
���̏ꍇ�́A�u�����I�v�Ƀh���[�ƂȂ�B
�X�e�C�����C�g �F �����̎�ԂŁA�����̃L���O�Ƀ`�F�b�N����Ă͂��Ȃ����A���@�肪�Ȃ��󋵂��w���B
�h���[�E�I�t�@�[�F �Е����h���[���Ă��A�����Е�����������������ꍇ�B
�f�b�h�E�|�W�V����[8]�F ��̕��͕s���̂��߁A�o��������̃L���O���`�F�b�N���C�g�ł��Ȃ��Ȃ����󋵂��w���B���̋�̑g�����̎��́A���Ƃ��G�̋�L���O������ł����Ă��`�F�b�N���C�g���邱�Ƃ͂ł��Ȃ��B[9]
�L���O + �r�V���b�v1��
�L���O + �i�C�g1��
�i�L���O + �i�C�g2��

���̏ꍇ�A����̃v���[���[�́u�\���i�N���[���j�v�ɂ��h���[�ƂȂ�
50�胋�[�� �F 50��A�����ė��҂Ƃ��|�[�����������A�܂����݂��ɋ�����Ȃ��ꍇ�B
�X���[�t�H�[���h�E���s�e�B�V�����i���`�O���j�F ����̋ǖʂ�3�񌻂ꂽ�ꍇ�B
*/
bool Position::is_draw() const {

  // Draw by material?
  if (   !pieces(PAWN)
      && (non_pawn_material(WHITE) + non_pawn_material(BLACK) <= BishopValueMg))
      return true;

  // Draw by the 50 moves rule?
  if (m_st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
      return true;

  int i = 4, e = std::min(m_st->rule50, m_st->pliesFromNull);

  if (i <= e)
  {
      StateInfo* stp = m_st->previous->previous;

      do {
          stp = stp->previous->previous;

          if (stp->key == m_st->key)
              return true; // Draw after first repetition

          i += 2;

      } while (i <= e);
  }

  return false;
}


/// Position::flip() flips position with the white and black sides reversed. This
/// is only useful for debugging especially for finding evaluation symmetry bugs.

static char toggle_case(char c) {
  return char(islower(c) ? toupper(c) : tolower(c));
}
/*
getline(is,str,delim);
is:������̒��o���ƂȂ���̓X�g���[���B
str:���̓X�g���[�����璊�o���������̓ǂݍ��ݐ�ƂȂ镶����B
delim:�s�̋�؂�L���B
split�̑���݂����Ȋ֐�
string.insert(index,string)
index�̈ʒu�ɕ������}������
std::transform(start,end,result,func)
start����end�܂ł͈̔͂Ɋ֐�func��K�p����result�Ɍ��ʂ�Ԃ�
�֐�toggle_case�͕����i������ł͂Ȃ��j���󂯎�肻�ꂪ�������Ȃ�
�啶���ɂ��ĕԂ��A�啶���������珬�����ɂ��ĕԂ�
*/
void Position::flip() {

  string f, token;
  std::stringstream ss(fen());
	/*
	fen��������t�ɂ��Ă���
	rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR�������Ă��镶��
	f�ɂ�"RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbqkbnr "
	�Ɣ��΂ɍ\�z����
	*/
	for (Rank rank = RANK_8; rank >= RANK_1; --rank) // Piece placement
  {
      std::getline(ss, token, rank > RANK_1 ? '/' : ' ');
      f.insert(0, token + (f.empty() ? " " : "/"));
  }
	/*
	�J���[��ς��Ă���
	*/
	ss >> token; // Active color
  f += (token == "w" ? "B " : "W "); // Will be lowercased later

  ss >> token; // Castling availability
  f += token + " ";
	/*
	�啶�����������ɁA��������啶���ɕϊ�
	�܂�WHITE��BLACK��BLACK��WHITE�ɂ���
	*/
	std::transform(f.begin(), f.end(), f.begin(), toggle_case);

  ss >> token; // En passant square
  f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

  std::getline(ss, token); // Half and full moves
  f += token;

  set(f, is_chess960(), this_thread());

  assert(pos_is_ok());
}


/// Position::pos_is_ok() performs some consitency checks for the position object.
/// This is meant to be helpful when debugging.
/*
�f�o�b�N�p�̃`�G�b�N������́H
*/
bool Position::pos_is_ok(int* failedStep) const {

  int dummy, *step = failedStep ? failedStep : &dummy;

  // What features of the position should be verified?
  const bool all = false;

  const bool debugBitboards       = all || false;
  const bool debugKingCount       = all || false;
  const bool debugKingCapture     = all || false;
  const bool debugCheckerCount    = all || false;
  const bool debugKey             = all || false;
  const bool debugMaterialKey     = all || false;
  const bool debugPawnKey         = all || false;
  const bool debugIncrementalEval = all || false;
  const bool debugNonPawnMaterial = all || false;
  const bool debugPieceCounts     = all || false;
  const bool debugPieceList       = all || false;
  const bool debugCastleSquares   = all || false;

  *step = 1;
	/*
	sideToMove��WHITE,BLACK�ɂȂ��Ă��邩
	������king_square�z��ɐ�����king�̍��W�������Ă��邩
	*/
	if (sideToMove != WHITE && sideToMove != BLACK)
      return false;

  if ((*step)++, piece_on(king_square(WHITE)) != W_KING)
      return false;

  if ((*step)++, piece_on(king_square(BLACK)) != B_KING)
      return false;

  if ((*step)++, debugKingCount)
  {
      int kingCount[COLOR_NB] = {};

      for (Square s = SQ_A1; s <= SQ_H8; ++s)
          if (type_of(piece_on(s)) == KING)
              ++kingCount[color_of(piece_on(s))];

      if (kingCount[0] != 1 || kingCount[1] != 1)
          return false;
  }

  if ((*step)++, debugKingCapture)
      if (attackers_to(king_square(~sideToMove)) & pieces(sideToMove))
          return false;

  if ((*step)++, debugCheckerCount && popcount<Full>(m_st->checkersBB) > 2)
      return false;

  if ((*step)++, debugBitboards)
  {
      // The intersection of the white and black pieces must be empty
		/*
		���݂��̃J���[��AND���Ƃ�ƕK��0�ɂȂ�͂��A����Ȃ�������
		��������
		*/
		if (pieces(WHITE) & pieces(BLACK))
          return false;

      // The union of the white and black pieces must be equal to all
      // occupied squares
		/*
		WHITE��BLACK��OR�͑S�Ă̋�ƈꏏ�̂͂�
		�����łȂ���΂�������
		*/
		if ((pieces(WHITE) | pieces(BLACK)) != pieces())
          return false;

      // Separate piece type bitboards must have empty intersections
		/*
		��킪�Ⴄ���̓��m�͏d�Ȃ�Ȃ�
		�d�Ȃ����炨������
		*/
		for (PieceType p1 = PAWN; p1 <= KING; ++p1)
          for (PieceType p2 = PAWN; p2 <= KING; ++p2)
              if (p1 != p2 && (pieces(p1) & pieces(p2)))
                  return false;
  }

  if ((*step)++, ep_square() != SQ_NONE && relative_rank(sideToMove, ep_square()) != RANK_6)
      return false;

  if ((*step)++, debugKey && m_st->key != compute_key())
      return false;

  if ((*step)++, debugPawnKey && m_st->pawnKey != compute_pawn_key())
      return false;

  if ((*step)++, debugMaterialKey && m_st->materialKey != compute_material_key())
      return false;

  if ((*step)++, debugIncrementalEval && m_st->psq != compute_psq_score())
      return false;

  if ((*step)++, debugNonPawnMaterial)
      if (   m_st->npMaterial[WHITE] != compute_non_pawn_material(WHITE)
          || m_st->npMaterial[BLACK] != compute_non_pawn_material(BLACK))
          return false;

  if ((*step)++, debugPieceCounts)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (PieceType pt = PAWN; pt <= KING; ++pt)
              if (pieceCount[c][pt] != popcount<Full>(pieces(c, pt)))
                  return false;

  if ((*step)++, debugPieceList)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (PieceType pt = PAWN; pt <= KING; ++pt)
              for (int i = 0; i < pieceCount[c][pt];  ++i)
                  if (   board[pieceList[c][pt][i]] != make_piece(c, pt)
                      || index[pieceList[c][pt][i]] != i)
                      return false;

  if ((*step)++, debugCastleSquares)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (CastlingSide s = KING_SIDE; s <= QUEEN_SIDE; s = CastlingSide(s + 1))
          {
              CastleRight cr = make_castle_right(c, s);

              if (!can_castle(cr))
                  continue;

              if (  (castleRightsMask[king_square(c)] & cr) != cr
                  || piece_on(castleRookSquare[c][s]) != make_piece(c, ROOK)
                  || castleRightsMask[castleRookSquare[c][s]] != cr)
                  return false;
          }

  *step = 0;
  return true;
}
