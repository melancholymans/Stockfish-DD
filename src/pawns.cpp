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

#include "bitboard.h"
#include "bitcount.h"
#include "pawns.h"
#include "position.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Doubled pawn penalty by file
	/*
	�����PAWN���Q�ȏア��Ɩh���̎�_�Ƃ����i���[���ᔽ�ł͂Ȃ��j
	�����ɂ����PAWN���m�Ŏ�邱�Ƃ��ł��Ȃ�����
	����pawns.cpp�ɒ�`���Ă���evaluate�֐��Ŏg�p����Ă���
	http://en.wikipedia.org/wiki/Doubled_pawns
	*/
  const Score Doubled[FILE_NB] = {
    S(13, 43), S(20, 48), S(23, 48), S(23, 48),
    S(23, 48), S(23, 48), S(20, 48), S(13, 43) };

  // Isolated pawn penalty by opposed flag and file
	/*
	�Ǘ�����pawn�ւ̃y�i���e�B
	����pawns.cpp�ɒ�`���Ă���evaluate�֐��Ŏg�p����Ă���
	http://en.wikipedia.org/wiki/Isolated_pawn
	*/
  const Score Isolated[2][FILE_NB] = {
  { S(37, 45), S(54, 52), S(60, 52), S(60, 52),
    S(60, 52), S(60, 52), S(54, 52), S(37, 45) },
  { S(25, 30), S(36, 35), S(40, 35), S(40, 35),
    S(40, 35), S(40, 35), S(36, 35), S(25, 30) } };

  // Backward pawn penalty by opposed flag and file
	/*
	PAWN�̎΂ߗ����������Ă���PAWN�͋�����
	����Ă������PAWN�iBackward pawn�j���͎̂ア
	����pawns.cpp�ɒ�`���Ă���evaluate�֐��Ŏg�p����Ă���
	http://en.wikipedia.org/wiki/Backward_pawn
	*/
  const Score Backward[2][FILE_NB] = {
  { S(30, 42), S(43, 46), S(49, 46), S(49, 46),
    S(49, 46), S(49, 46), S(43, 46), S(30, 42) },
  { S(20, 28), S(29, 31), S(33, 31), S(33, 31),
    S(33, 31), S(33, 31), S(29, 31), S(20, 28) } };

  // Pawn chain membership bonus by file and rank (initialized by formula)
	/*
	����URL�ɏ����Ă���悤�Ȕz�u�ɂȂ�΃{�[�i�X���ł�
	https://chessprogramming.wikispaces.com/Pawn+chain
	*/
  Score ChainMember[FILE_NB][RANK_NB];

  // Candidate passed pawn bonus by rank
	/*
	�p�r�s�������A���̕]�����ڂ̓v���X����Ă���̂Ńy�i���e�B�ł͂Ȃ��{�[�i�X�̂悤��
	https://chessprogramming.wikispaces.com/Candidate+Passed+Pawn
	*/
  const Score CandidatePassed[RANK_NB] = {
    S( 0, 0), S( 6, 13), S(6,13), S(14,29),
    S(34,68), S(83,166), S(0, 0), S( 0, 0) };

  // Weakness of our pawn shelter in front of the king indexed by [rank]
	/*
	�p�r�s��
	*/
  const Value ShelterWeakness[RANK_NB] =
  { V(100), V(0), V(27), V(73), V(92), V(101), V(101) };

  // Danger of enemy pawns moving toward our king indexed by
  // [no friendly pawn | pawn unblocked | pawn blocked][rank of enemy pawn]
	/*
	king���ӂ̊댯�A�p�r�s��
	*/
  const Value StormDanger[3][RANK_NB] = {
  { V( 0),  V(64), V(128), V(51), V(26) },
  { V(26),  V(32), V( 96), V(38), V(20) },
  { V( 0),  V( 0), V( 64), V(25), V(13) } };

  // Max bonus for king safety. Corresponds to start position with all the pawns
  // in front of the king and no enemy pawn on the horizon.
	/*
	king�̈��S�H�A�p�r�s��
	*/
  const Value MaxSafetyBonus = V(263);

  #undef S
  #undef V
	/*
	PAWN�͔�Ώ̂ȋ�Ȃ̂Ŋ֐������ŕ����q�𑵂��Ă���
	��Ԃ̃J���[��Us,�G��Them
	WHITE�ɂƂ��āFUp�͍��W�l�傫���Ȃ�����A�܂�BLACK���Ɍ������Ă���
	Right��BLACK���Ɍ������Ȃ���E���i���肩�猩�ĉE���H�j
	Left��BLACK���Ɍ������Ȃ��獶���i���肩�猩�č����j

	BLACK�ɂƂ���Up�͍��W�����Ȃ��Ȃ�����A�܂�WHITE���Ɍ������Ă���
	Right��WHITE���Ɍ������Ȃ���E���i���肩��݂ĉE���H�j
	left��WHITE���Ɍ������Ȃ��獶���i���肩��݂č����H�j

	������PAWN���L�̕]�����ڂ����Z���āi���Z�j���Ă���
	�ŏIvalue�l�ɏW�񂵂ĕԂ�
	*/
  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    const Color  Them  = (Us == WHITE ? BLACK    : WHITE);
    const Square Up    = (Us == WHITE ? DELTA_N  : DELTA_S);
    const Square Right = (Us == WHITE ? DELTA_NE : DELTA_SW);
    const Square Left  = (Us == WHITE ? DELTA_NW : DELTA_SE);

    Bitboard b;
    Square s;
    File f;
    bool passed, isolated, doubled, opposed, chain, backward, candidate;
    Score value = SCORE_ZERO;
		/*
		��ԑ���PAWN�̍��W�̃��X�g
		*/
    const Square* pl = pos.list<PAWN>(Us);
		/*
		��ԑ���PAWN��bitboard
		*/
    Bitboard ourPawns = pos.pieces(Us, PAWN);
		/*
		�G����PAWN��bitboard
		*/
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us] = e->candidatePawns[Us] = 0;
    e->kingSquares[Us] = SQ_NONE;
    e->semiopenFiles[Us] = 0xFF;
		/*
		��ԑ���PAWN���������i���΂ߑO�j�ɓ����������Ƃ�bitboard�����Ă���
		pawnAttacks=PAWN�̃A�^�b�J�[��
		*/
    e->pawnAttacks[Us] = shift_bb<Right>(ourPawns) | shift_bb<Left>(ourPawns);
		/*
		DarkSquares�i�_�[�N���F�Ղ̏��̐F���Â��F�ɂȂ��Ă��鏡�̂��Ɓj�ɂ����ԑ���PAWN�̐�
		*/
    e->pawnsOnSquares[Us][BLACK] = popcount<Max15>(ourPawns & DarkSquares);
		/*
		DarkSquares�i�_�[�N���j�ɏ���Ă��Ȃ���ԑ���PAWN�̐�
		*/
    e->pawnsOnSquares[Us][WHITE] = pos.count<PAWN>(Us) - e->pawnsOnSquares[Us][BLACK];

    // Loop through all pawns of the current color and score each pawn
		/*
		��ԑ���PAWN�̍��W��s�ϐ��ɓ����
		*/
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));
				/*��ԑ���PAWN�̗�ԍ�*/
        f = file_of(s);

        // This file cannot be semi-open
				/*
				semiopenFiles[]�z��̏����l��0xFF�A�Wbit���S�ė����Ă����Ԃɐݒ肳���
				�����ł�PAWN��������bit�𔽓]�����r�b�gAND���Ƃ邱�Ƃ�PAWN���������O��
				���Ȃ�����P�Ƃ��ĕ\�����Ă���
				*/
        e->semiopenFiles[Us] &= ~(1 << f);

        // Our rank plus previous one. Used for chain detection
				/*
				PAWN��������W�̍s��bitboard�Ƃ��̂������̍s�iPAWN�̐i�s�����Ƃ͋t�j��bitboard��OR����
				*/
        b = rank_bb(s) | rank_bb(s - pawn_push(Us));

        // Flag the pawn as passed, isolated, doubled or member of a pawn
        // chain (but not the backward one).
				/*
				pawn chain�����o
				chain�Ɏ�ԑ���PAWN��bitboard�ƌ��ݒ��ڂ��Ă���PAWN�������̗��e�̗������adjacent_files_bb��PAWN������s��
				����̍s��bitboard�Ƃ�bit AND�ɂ���āA���ڂ��Ă���PAWN�̍��΂ߌ�납�E�΂ߌ��ɂ���PAWN������
				*/
        chain    =   ourPawns   & adjacent_files_bb(f) & b;
				/*
				isolated���Ǘ������Ƃ����Ӗ�
				���ڂ��Ă���PAWN�̗��e��PAWN�����Ȃ���,isolated�ϐ���true�ɂȂ�A���鎞��false�ƂȂ�
				*/
        isolated = !(ourPawns   & adjacent_files_bb(f));
				/*
				���ڂ��Ă���PAWN�̑O���ɓ��J���[��PAWN��������true�A���Ȃ�������false
				*/
        doubled  =   ourPawns   & forward_bb(Us, s);
				/*
				doubled���Η��ƌ����Ӗ�
				���ڂ��Ă���PAWN�̑O���ɂ���GPAWN�������true,���Ȃ����false
				*/
        opposed  =   theirPawns & forward_bb(Us, s);
				/*
				��ԑ���PAWN���GPAWN���Ƃ��Ȃ�false�A���Ȃ��Ȃ�true
				*/
        passed   = !(theirPawns & passed_pawn_mask(Us, s));

        // Test for backward pawn.
        // If the pawn is passed, isolated, or member of a pawn chain it cannot
        // be backward. If there are friendly pawns behind on adjacent files
        // or if can capture an enemy pawn it cannot be backward either.
				/*
				�GPAWN���Ƃ�Ȃ��@||�@������PAWN����Ǘ����Ă���i�����s�ɂ��Ȃ��j�@||�@�����PAWN�ƃ`�G�C���łȂ����Ă���@||
				���ڂ�PAWN�̌���ɖ�����PAWN������@||�@�G��PAWN����邱�Ƃ��\
				�Ȃ�backward�ϐ���false
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   | P |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   | P |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				P->PAWN
				*/
        if (   (passed | isolated | chain)
            || (ourPawns & pawn_attack_span(Them, s))
            || (pos.attacks_from<PAWN>(s, Us) & theirPawns))
            backward = false;
        else
        {
            // We now know that there are no friendly pawns beside or behind this
            // pawn on adjacent files. We now check whether the pawn is
            // backward by looking in the forward direction on the adjacent
            // files, and picking the closest pawn there.
					/*
					���if���̏������������Ȃ����i�����炭�܂�PAWN��]�蓮�����Ă��Ȃ�PAWN���m���אڂ��Ă���悤�ȏ�Ԃ�z��H�j
					���ڂ��Ă���PAWN�̗����̒��ɂ���S�Ă�PAWN�i�G�A�����S�āj��bitboard�ɂ���A���̎�ԑ�����r�b�g�X�L��������
					����PAWN������sbitboard�ƒ��ڂ�PAWN�̗����̃r�b�g�A���h�����
					�����̏����̖ړI���悭�킩���
					https://chessprogramming.wikispaces.com/Backward+Pawn
					*/
            b = pawn_attack_span(Us, s) & (ourPawns | theirPawns);
            b = pawn_attack_span(Us, s) & rank_bb(backmost_sq(Us, b));

            // If we have an enemy pawn in the same or next rank, the pawn is
            // backward because it cannot advance without being captured.
						/*
						�����̏������悭�킩��񂪁A�ŏI�I��backward�ϐ���true�ɂ���̂�false�ɂ���̂����ړI
						*/
            backward = (b | shift_bb<Up>(b)) & theirPawns;
        }

        assert(opposed | passed | (pawn_attack_span(Us, s) & theirPawns));

        // A not passed pawn is a candidate to become passed, if it is free to
        // advance and if the number of friendly pawns beside or behind this
        // pawn on adjacent files is higher or equal than the number of
        // enemy pawns in the forward direction on the adjacent files.
				/*
				�i�O���ɓGPAWN������@||�@���i�ł���@||�@backward�i�H�j�@|| �Ǘ�����PAWN�ł���j�łȂ����Ɓ@&&
				���ڂ��Ă���PAWN�̒��O�ɂ���GPAWN��������PAWN����邱�Ƃ��ł��� &&
				�O���ɂ���GPAWN�̐�������ɂ��閡��PAWN�̐��������ȏ�
				�Ȃ�candidate��true
				*/
        candidate =   !(opposed | passed | backward | isolated)
                   && (b = pawn_attack_span(Them, s + pawn_push(Us)) & ourPawns) != 0
                   &&  popcount<Max15>(b) >= popcount<Max15>(pawn_attack_span(Us, s) & theirPawns);

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate passed pawns. Only the frontmost passed
        // pawn on each file is considered a true passed pawn.
				/*
				�GPAWN������@&&�@�������m��PAWN���c��ɂȂ��Ă��Ȃ��Ȃ�
				passedPawns bitboard�ɒ��ڂ��Ă���PAWN��ǉ�����
				����bitboard�͂��̊֐����ł͊��p���Ȃ���evaluate.cpp�̊֐��Ŏg�p����Ă���
				*/
        if (passed && !doubled)
            e->passedPawns[Us] |= s;

        // Score this pawn
				/*
				�Ǘ�����PAWN�Ȃ�Isolated�z��ɐݒ肵�Ă���l��]���l��������i�y�i���e�B�j
				��ɂ���ăy�i���e�B�͈قȂ�B�[�͏����������͑傫���Ȃ�B�l�͒��ՂƏI�Ղ̃Z�b�g�ɂȂ��Ă���
				*/
        if (isolated)
            value -= Isolated[opposed][f];
				/*
				������PAWN���m���c��ɂȂ��Ă���ꍇ�̃y�i���e�B
				��ɂ���ăy�i���e�B�l�͈قȂ�B�[��蒆�����y�i���e�B������
				*/
        if (doubled)
            value -= Doubled[f];
				/*
				���ڂ��Ă���PAWN�Ɍ㑱���閡����PAWN�����Ȃ��A���e�ɖ�����PAWN�����Ȃ����̃y�i���e�B
				��ɂ���ăy�i���e�B�l�͈قȂ�B�[��蒆�����y�i���e�B������
				*/
        if (backward)
            value -= Backward[opposed][f];
				/*
				������PAWN���`�G�[����ɂȂ��Ă���΃{�[�i�X
				ChainMember�z���init�֐����Őݒ�
				*/
        if (chain)
            value += ChainMember[f][relative_rank(Us, s)];
				/*
				�GPAWN������ėL���ɂȂ肻����PAWN�̉\��������ꍇ�̓{�[�i�X
				*/
        if (candidate)
        {
            value += CandidatePassed[relative_rank(Us, s)];
						/*
						����bitboard�͂��̊֐����ł͊��p���Ȃ���evaluate.cpp�̊֐��Ŏg�p����Ă���
						*/
            if (!doubled)
                e->candidatePawns[Us] |= s;
        }
    }

    return value;
  }

} // namespace

namespace Pawns {

/// init() initializes some tables by formula instead of hard-code their values
/*
ChainMember�z��̏�����
*/
void init() {

  const int chainByFile[8] = { 1, 3, 3, 4, 4, 3, 3, 1 };
  int bonus;

  for (Rank r = RANK_1; r < RANK_8; ++r)
      for (File f = FILE_A; f <= FILE_H; ++f)
      {
          bonus = r * (r-1) * (r-2) + chainByFile[f] * (r/2 + 1);
          ChainMember[f][r] = make_score(bonus, bonus);
      }
}


/// probe() takes a position object as input, computes a Entry object, and returns
/// a pointer to it. The result is also stored in a hash table, so we don't have
/// to recompute everything when the same pawn structure occurs again.
/*
*/
Entry* probe(const Position& pos, Table& entries) {

  Key key = pos.pawn_key();
  Entry* e = entries[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->value = evaluate<WHITE>(pos, e) - evaluate<BLACK>(pos, e);
  return e;
}


/// Entry::shelter_storm() calculates shelter and storm penalties for the file
/// the king is on, as well as the two adjacent files.

template<Color Us>
Value Entry::shelter_storm(const Position& pos, Square ksq) {

  const Color Them = (Us == WHITE ? BLACK : WHITE);

  Value safety = MaxSafetyBonus;
  Bitboard b = pos.pieces(PAWN) & (in_front_bb(Us, rank_of(ksq)) | rank_bb(ksq));
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);
  Rank rkUs, rkThem;
  File kf = std::max(FILE_B, std::min(FILE_G, file_of(ksq)));

  for (File f = kf - File(1); f <= kf + File(1); ++f)
  {
      b = ourPawns & file_bb(f);
      rkUs = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;
      safety -= ShelterWeakness[rkUs];

      b  = theirPawns & file_bb(f);
      rkThem = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;
      safety -= StormDanger[rkUs == RANK_1 ? 0 : rkThem == rkUs + 1 ? 2 : 1][rkThem];
  }

  return safety;
}


/// Entry::update_safety() calculates and caches a bonus for king safety. It is
/// called only when king square changes, about 20% of total king_safety() calls.

template<Color Us>
Score Entry::update_safety(const Position& pos, Square ksq) {

  kingSquares[Us] = ksq;
  castleRights[Us] = pos.can_castle(Us);
  minKPdistance[Us] = 0;

  Bitboard pawns = pos.pieces(Us, PAWN);
  if (pawns)
      while (!(DistanceRingsBB[ksq][minKPdistance[Us]++] & pawns)) {}

  if (relative_rank(Us, ksq) > RANK_4)
      return kingSafety[Us] = make_score(0, -16 * minKPdistance[Us]);

  Value bonus = shelter_storm<Us>(pos, ksq);

  // If we can castle use the bonus after the castle if it is bigger
  if (pos.can_castle(make_castle_right(Us, KING_SIDE)))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_G1)));

  if (pos.can_castle(make_castle_right(Us, QUEEN_SIDE)))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_C1)));

  return kingSafety[Us] = make_score(bonus, -16 * minKPdistance[Us]);
}

// Explicit template instantiation
template Score Entry::update_safety<WHITE>(const Position& pos, Square ksq);
template Score Entry::update_safety<BLACK>(const Position& pos, Square ksq);

} // namespace Pawns
