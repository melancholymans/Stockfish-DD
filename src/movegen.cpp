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

#include <cassert>

#include "movegen.h"
#include "position.h"

/// Simple macro to wrap a very common while loop, no facny, no flexibility,
/// hardcoded names 'mlist' and 'from'.
/*
b�̓^�[�Q�b�g�ƂȂ�bitboard��from���W�i�}�N���Ȃ̂ł���from�͈����ł͂Ȃ����̃}�N�����}�����ꂽ�ꏊ��
�����ϐ��𗘗p���Ă���,mlist�ϐ��ɂ��Ă����l�j�ɂ����^�[�Q�b�g�Ɉړ����钅�胊�X�g�����

generate_moves�֐�
generate_all�֐�
generate<QUIET_CHECKS>�֐�
generate<EVASIONS>
�Ŏg�p����
*/
#define SERIALIZE(b) while (b) (mlist++)->move = make_move(from, pop_lsb(&b))

/// Version used for pawns, where the 'from' square is given as a delta from the 'to' square
/*
PAWN�̒��胊�X�g����邽�߂̃}�N��
��������b��PAWN�̃^�[�Q�b�g�i�ړ���j�A���������͕����q

generate_promotions�֐�
*/
#define SERIALIZE_PAWNS(b, d) while (b) { Square to = pop_lsb(&b); \
                                         (mlist++)->move = make_move(to - (d), to); }
namespace {
	/*
	generate_all����̂݌Ăяo�����
	�L���X�����O�֌W�̐����֐��Ȃ̂�PASS
	*/
	template<CastlingSide Side, bool Checks, bool Chess960>
  ExtMove* generate_castle(const Position& pos, ExtMove* mlist, Color us) {

    if (pos.castle_impeded(us, Side) || !pos.can_castle(make_castle_right(us, Side)))
        return mlist;

    // After castling, the rook and king final positions are the same in Chess960
    // as they would be in standard chess.
    Square kfrom = pos.king_square(us);
    Square rfrom = pos.castle_rook_square(us, Side);
    Square kto = relative_square(us, Side == KING_SIDE ? SQ_G1 : SQ_C1);
    Bitboard enemies = pos.pieces(~us);

    assert(!pos.checkers());

    const int K = Chess960 ? kto > kfrom ? -1 : 1
                           : Side == KING_SIDE ? -1 : 1;

    for (Square s = kto; s != kfrom; s += (Square)K)
        if (pos.attackers_to(s) & enemies)
            return mlist;

    // Because we generate only legal castling moves we need to verify that
    // when moving the castling rook we do not discover some hidden checker.
    // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
    if (Chess960 && (attacks_bb<ROOK>(kto, pos.pieces() ^ rfrom) & pos.pieces(~us, ROOK, QUEEN)))
        return mlist;

    (mlist++)->move = make<CASTLE>(kfrom, rfrom);

    if (Checks && !pos.gives_check((mlist - 1)->move, CheckInfo(pos)))
        --mlist;

    return mlist;
  }

	/*
	generate_pawn_moves����Ăяo����
	�w�肳�ꂽDelta�����q�ňړ���bitboard������
	���W�����o��������ɏ��i�����𐶐�����
	*/
	template<GenType Type, Square Delta>
  inline ExtMove* generate_promotions(ExtMove* mlist, Bitboard pawnsOn7,
                                      Bitboard target, const CheckInfo* ci) {

    Bitboard b = shift_bb<Delta>(pawnsOn7) & target;

    while (b)
    {
        Square to = pop_lsb(&b);

        if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
            (mlist++)->move = make<PROMOTION>(to - Delta, to, QUEEN);

        if (Type == QUIETS || Type == EVASIONS || Type == NON_EVASIONS)
        {
            (mlist++)->move = make<PROMOTION>(to - Delta, to, ROOK);
            (mlist++)->move = make<PROMOTION>(to - Delta, to, BISHOP);
            (mlist++)->move = make<PROMOTION>(to - Delta, to, KNIGHT);
        }

        // Knight-promotion is the only one that can give a direct check not
        // already included in the queen-promotion.
				/*
				�����p�^�[���������炸�ɉ����������@���@KNIGHT�ɂȂ�������KING�ɉ���������邱�Ƃ�
				�ł���Ȃ�΂��̎�𒅎胊�X�g�ɒǉ�����
				(void)ci; // Silence a warning under MSVC�@��MSVC�Ń��[�j���O�̗}���H
				*/
				if (Type == QUIET_CHECKS && (StepAttacksBB[W_KNIGHT][to] & ci->ksq))
            (mlist++)->move = make<PROMOTION>(to - Delta, to, KNIGHT);
        else
            (void)ci; // Silence a warning under MSVC
    }

    return mlist;
  }

	/*
	PAWN�̎萶����p
	PAWN�ȊO��KNIGHT,ROOK,BISHOP,QUEEN�̓����͑Ώ́i������WHITE�ł�BLACK�ł������j
	���̓_�����͓�������Ώ̂Ȃ��̂������A�Ώ̂Ȃ��͉̂��A��ԁA�p�s�̂�
	*/
	template<Color Us, GenType Type>
  ExtMove* generate_pawn_moves(const Position& pos, ExtMove* mlist,
                               Bitboard target, const CheckInfo* ci) {

    // Compute our parametrized parameters at compile time, named according to
    // the point of view of white side.
		/*
		�����q�𑊑ΓI�Ɍ��߂邱�Ƃ�WHITE,BLACK���p�̃��[�`��������Ă���
		���Ƃ���Up�Ƃ�PAWN���O�ɐi�ޕ����q�ł��邪WHITE���ɂƂ���DELTA_N(=8)�ł���
		BLACK����DELTA_S(= -8)�ł���
		*/
		const Color    Them = (Us == WHITE ? BLACK : WHITE);
    const Bitboard TRank8BB = (Us == WHITE ? Rank8BB  : Rank1BB);
    const Bitboard TRank7BB = (Us == WHITE ? Rank7BB  : Rank2BB);
    const Bitboard TRank3BB = (Us == WHITE ? Rank3BB  : Rank6BB);
    const Square   Up       = (Us == WHITE ? DELTA_N  : DELTA_S);
    const Square   Right    = (Us == WHITE ? DELTA_NE : DELTA_SW);
    const Square   Left     = (Us == WHITE ? DELTA_NW : DELTA_SE);

    Bitboard b1, b2, dc1, dc2, emptySquares;
		/*
		pawnsOn7�͂��ƂP���QUEEN�ɂȂ��PAWN��bitboard
		*/
		Bitboard pawnsOn7 = pos.pieces(Us, PAWN) &  TRank7BB;
		/*
		�܂��܂�QUEEN�ɐ���Ȃ�PAWN
		*/
		Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;
		/*
		����̐����p�^�[���Ȃ�Gbitboard�͓G�̋�bitboard & target
		��鐶���p�^�[���Ȃ炻�̂܂܁A���̑��̃p�^�[���͓G�̋�S��
		*/
		Bitboard enemies = (Type == EVASIONS ? pos.pieces(Them) & target :
                        Type == CAPTURES ? target : pos.pieces(Them));

    // Single and double pawn pushes, no promotions
		/*
		���p�^�[���ł͂Ȃ��ꍇ�̎萶��
		*/
		if (Type != CAPTURES)
    {
			//���Ȃ���Ȃ̂�target�͋󔒂ƂȂ�
			emptySquares = (Type == QUIETS || Type == QUIET_CHECKS ? target : ~pos.pieces());
				//b1��QUEEN�ɂȂ�Ȃ�PAWN������Ƃ炸�ɐi�߂�ꏊ��bitboard
				b1 = shift_bb<Up>(pawnsNotOn7)   & emptySquares;
				/*b2��b1���P�蓮����PAWN�̓�3�����N�ڂɂ���PAWN�������P�蓮����������
				�܂�b2��PAWN������݂̂QRANK������PAWN�̂���
				b1�͂P�����N��������b2�͂Q�����N������Ȃ̂ŁAPAWN�ɂƂ��ċ�����Ȃ����ׂĂ�
				������\���Ă���B
				*/
				b2 = shift_bb<Up>(b1 & TRank3BB) & emptySquares;
				//���p�^�[���ł͂Ȃ���̓��A������������iPAWN�̓����Ȃ̂ŁA����������Ă������Ƃ�A��������������肩�ȁj
				if (Type == EVASIONS) // Consider only blocking squares
        {
            b1 &= target;
            b2 &= target;
        }
				//���p�^�[���ł͂Ȃ��̂����ړ����邱�Ƃɂ���ĉ����������
				//pos.attacks_from<PAWN>(ci->ksq, Them);�͓G�̉�����PAWN�̗����Ƃ������Z����̂�b1,b2�ɂ�
				//����̎肪�����Ă���͂�
				if (Type == QUIET_CHECKS)
        {
            b1 &= pos.attacks_from<PAWN>(ci->ksq, Them);
            b2 &= pos.attacks_from<PAWN>(ci->ksq, Them);

            // Add pawn pushes which give discovered check. This is possible only
            // if the pawn is not on the same file as the enemy king, because we
            // don't generate captures. Note that a possible discovery check
            // promotion has been already generated among captures.
						/*ci->dcCandidates�͓GKING�ւ̗������ז����Ă����̂��ƂȂ̂�
						����ȋ��������A�O���ɋ󔒂�����A���GKING�̂���FILE�ɂ��Ȃ�PAWN��dc1��
						����dc1���RRank�ɂ��đO�����󔒂�PAWN��dc2�ɂ���Ă���b1,b2�ɒǉ�����
						*/
						if (pawnsNotOn7 & ci->dcCandidates)
            {
                dc1 = shift_bb<Up>(pawnsNotOn7 & ci->dcCandidates) & emptySquares & ~file_bb(ci->ksq);
                dc2 = shift_bb<Up>(dc1 & TRank3BB) & emptySquares;

                b1 |= dc1;
                b2 |= dc2;
            }
        }

        SERIALIZE_PAWNS(b1, Up);
        SERIALIZE_PAWNS(b2, Up + Up);
    }

    // Promotions and underpromotions
		/*
		���̏�̏����͎��p�^�[���ł͂Ȃ��A��������͎��A�������p�^�[���𐶐�����
		�����N�V�ɂ���PAWN�����ā@���@�i�����p�^�[��������p�^�[���łȂ��@�������́@�ړ��悪�󔒁j
		�Ȃ�
		*/
		if (pawnsOn7 && (Type != EVASIONS || (target & TRank8BB)))
    {
        if (Type == CAPTURES)
            emptySquares = ~pos.pieces();

        if (Type == EVASIONS)
            emptySquares &= target;

        mlist = generate_promotions<Type, Right>(mlist, pawnsOn7, enemies, ci);
        mlist = generate_promotions<Type, Left >(mlist, pawnsOn7, enemies, ci);
        mlist = generate_promotions<Type, Up>(mlist, pawnsOn7, emptySquares, ci);
    }

    // Standard and en-passant captures
		/*
		�����͎��p�^�[���@�������p�^�[���@�������p�^�[���ȊO�S��
		�����N�V�ɂ���PAWN�ňړ����邱�Ƃɂ���ēG����Ƃ��𐶐����A���胊�X�g�ɓo�^����
		*/
		if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        b1 = shift_bb<Right>(pawnsNotOn7) & enemies;
        b2 = shift_bb<Left >(pawnsNotOn7) & enemies;

        SERIALIZE_PAWNS(b1, Right);
        SERIALIZE_PAWNS(b2, Left);

        if (pos.ep_square() != SQ_NONE)
        {
            assert(rank_of(pos.ep_square()) == relative_rank(Us, RANK_6));

            // An en passant capture can be an evasion only if the checking piece
            // is the double pushed pawn and so is in the target. Otherwise this
            // is a discovery check and we are forced to do otherwise.
            if (Type == EVASIONS && !(target & (pos.ep_square() - Up)))
                return mlist;

            b1 = pawnsNotOn7 & pos.attacks_from<PAWN>(pos.ep_square(), Them);

            assert(b1);

            while (b1)
                (mlist++)->move = make<ENPASSANT>(pop_lsb(&b1), pos.ep_square());
        }
    }

    return mlist;
  }

	/*
	PAWN,KING�ȊO�̋�̓����͂��̊֐������Ő�������
	*/
	template<PieceType Pt, bool Checks> FORCE_INLINE
  ExtMove* generate_moves(const Position& pos, ExtMove* mlist, Color us,
                          Bitboard target, const CheckInfo* ci) {

    assert(Pt != KING && Pt != PAWN);
		/*
		�w�肳�ꂽ�J���[�A�w�肳�ꂽ���̌��݂̍��W���X�g
		Checks�̈Ӗ��FQUIET_CHECKS�ł����true
		*/
		const Square* pl = pos.list<Pt>(us);

    for (Square from = *pl; from != SQ_NONE; from = *++pl)
    {
        if (Checks)
        {
						//Checks��QUIET_CHECKS�̂Ƃ�true�ɂȂ邪PISHOP,ROOK,QUEEN�̈ړ���
						//�ړ��ɂ���ĉ��肪�|����悤�Ȏ�łȂ���ΐ������Ȃ�
						//�i�����肪����������̂�}����̂��ړI�ł́j
						if ((Pt == BISHOP || Pt == ROOK || Pt == QUEEN)
                && !(PseudoAttacks[Pt][from] & target & ci->checkSq[Pt]))
                continue;
						/*
						�G�w��KING�ւ̗����̎ז��ɂȂ��Ă����ړ������͂��łɐ������Ă���̂�
						�����ł͐������Ȃ�
						*/
						if (unlikely(ci->dcCandidates) && (ci->dcCandidates & from))
                continue;
        }
				/*
				from���W�ɂ�����ړ��\�ȏꏊ��bitboard��b�ɓ���Ă���
				SERIALIZE(b)�}�N����
				*/
        Bitboard b = pos.attacks_from<Pt>(from) & target;
				//QUIET_CHECKS�͈ړ����邱�Ƃŉ������������Ɉړ�������ǉ�����
				if (Checks)
            b &= ci->checkSq[Pt];
				/*
				bitboard b�i�^�[�Q�b�gbitboard�j��n���Ē��胊�X�g(mlist�j�𐶐����Ă���
				*/
        SERIALIZE(b);
    }

    return mlist;
  }

	/*
	��̓����𐶐�����
	�����̓�target�͈ړ����bitboard��\�����Ă���
	pos�͍��̋ǖʃN���X�A�e���v���[�g������GenType Type��
	�����p�^�[�����w�肵�Ă���A�p�^�[����CAPTURES,QUIET,NON_EVASIONS��������悤
	CAPTURES�Ƃɂ�������
	QUIET�Ƃ炸�ړ������
	NON_EVASIONS�������������ȊO���ׂ�
	*/
	template<Color Us, GenType Type> FORCE_INLINE
  ExtMove* generate_all(const Position& pos, ExtMove* mlist, Bitboard target,
                        const CheckInfo* ci = nullptr) {

    const bool Checks = Type == QUIET_CHECKS;

    mlist = generate_pawn_moves<Us, Type>(pos, mlist, target, ci);
    mlist = generate_moves<KNIGHT, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves<BISHOP, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves<  ROOK, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves< QUEEN, Checks>(pos, mlist, Us, target, ci);

    if (Type != QUIET_CHECKS && Type != EVASIONS)
    {
        Square from = pos.king_square(Us);
        Bitboard b = pos.attacks_from<KING>(from) & target;
        SERIALIZE(b);
    }

    if (Type != CAPTURES && Type != EVASIONS && pos.can_castle(Us))
    {
        if (pos.is_chess960())
        {
            mlist = generate_castle< KING_SIDE, Checks, true>(pos, mlist, Us);
            mlist = generate_castle<QUEEN_SIDE, Checks, true>(pos, mlist, Us);
        }
        else
        {
            mlist = generate_castle< KING_SIDE, Checks, false>(pos, mlist, Us);
            mlist = generate_castle<QUEEN_SIDE, Checks, false>(pos, mlist, Us);
        }
    }

    return mlist;
  }


} // namespace


/// generate<CAPTURES> generates all pseudo-legal captures and queen
/// promotions. Returns a pointer to the end of the move list.
///
/// generate<QUIETS> generates all pseudo-legal non-captures and
/// underpromotions. Returns a pointer to the end of the move list.
///
/// generate<NON_EVASIONS> generates all pseudo-legal captures and
/// non-captures. Returns a pointer to the end of the move list.
/*
��𐶐�����e���v���[�g�֐�
����������A�����p�^�[���ɉ����ĕ��򂷂�
*/
template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* mlist) {

  assert(Type == CAPTURES || Type == QUIETS || Type == NON_EVASIONS);
  assert(!pos.checkers());

  Color us = pos.side_to_move();
	/*
	CAPTURES=�G���̋�^�[�Q�b�g
	QUIETS=������̂ł͂Ȃ��󔒂Ɉړ������iQUIETS�̖{�̂̈Ӗ������₩�ȁj
	NON_EVASIONS=��������ł͂Ȃ����G��{�󔒂��^�[�Q�b�g
	*/
	Bitboard target = Type == CAPTURES ? pos.pieces(~us)
                  : Type == QUIETS       ? ~pos.pieces()
                  : Type == NON_EVASIONS ? ~pos.pieces(us) : 0;

  return us == WHITE ? generate_all<WHITE, Type>(pos, mlist, target)
                     : generate_all<BLACK, Type>(pos, mlist, target);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);


/// generate<QUIET_CHECKS> generates all pseudo-legal non-captures and knight
/// underpromotions that give check. Returns a pointer to the end of the move list.
/*
�܂��A�GKING�Ƃ̗����̎ז��ɂȂ��Ă��鎩�w����̋����炸�Ɉړ����钅�胊�X�g�����
���̂���generate_all���ĂԁA�A��target�͋󔒂Ƃ���i������Ȃ����߁j

QUIET_CHECKS�͋���Ƃ炸�ɉ�����������̂��ƁH
*/
template<>
ExtMove* generate<QUIET_CHECKS>(const Position& pos, ExtMove* mlist) {

  assert(!pos.checkers());

  Color us = pos.side_to_move();
  CheckInfo ci(pos);
  Bitboard dc = ci.dcCandidates;

  while (dc)
  {
     Square from = pop_lsb(&dc);
     PieceType pt = type_of(pos.piece_on(from));

     if (pt == PAWN)
         continue; // Will be generated togheter with direct checks

     Bitboard b = pos.attacks_from(Piece(pt), from) & ~pos.pieces();

     if (pt == KING)
         b &= ~PseudoAttacks[QUEEN][ci.ksq];

     SERIALIZE(b);
  }

  return us == WHITE ? generate_all<WHITE, QUIET_CHECKS>(pos, mlist, ~pos.pieces(), &ci)
                     : generate_all<BLACK, QUIET_CHECKS>(pos, mlist, ~pos.pieces(), &ci);
}


/// generate<EVASIONS> generates all pseudo-legal check evasions when the side
/// to move is in check. Returns a pointer to the end of the move list.
//EVASIONS�����
/*
�������������𐶐�����
*/
template<>
ExtMove* generate<EVASIONS>(const Position& pos, ExtMove* mlist) {

  assert(pos.checkers());

  int checkersCnt = 0;
  Color us = pos.side_to_move();
  Square ksq = pos.king_square(us), from = ksq /* For SERIALIZE */, checksq;
  Bitboard sliderAttacks = 0;
  Bitboard b = pos.checkers();

  assert(pos.checkers());

  // Find squares attacked by slider checkers, we will remove them from the king
  // evasions so to skip known illegal moves avoiding useless legality check later.
  do
  {
      ++checkersCnt;
      checksq = pop_lsb(&b);

      assert(color_of(pos.piece_on(checksq)) == ~us);

      if (type_of(pos.piece_on(checksq)) > KNIGHT) // A slider
          sliderAttacks |= LineBB[checksq][ksq] ^ checksq;

  } while (b);

  // Generate evasions for king, capture and non capture moves
	/*
	KING�̗����@���@pos.pieces(us) = KING�̈ړ��\���󔒂܂��͓G��
	�iKING�̈ړ��\���󔒂܂��͓G��j���@�G�̔�ы�̗����ȊO�̏ꏊ
	�܂�KING��������ꏊ��mlist�ɓo�^���Ă���
	*/
	b = pos.attacks_from<KING>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
  SERIALIZE(b);

  if (checkersCnt > 1)
      return mlist; // Double check, only a king move can save the day

  // Generate blocking evasions or captures of the checking piece
  Bitboard target = between_bb(checksq, ksq) | checksq;

  return us == WHITE ? generate_all<WHITE, EVASIONS>(pos, mlist, target)
                     : generate_all<BLACK, EVASIONS>(pos, mlist, target);
}


/// generate<LEGAL> generates all the legal moves in the given position
/*
���@��𐶐�����
��KING�ɉ��肪�������Ă�Ή�������𐶐�
�����łȂ���Ή������肶��Ȃ���𐶐�����
*/
template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* mlist) {

  ExtMove *end, *cur = mlist;
  Bitboard pinned = pos.pinned_pieces(pos.side_to_move());
  Square ksq = pos.king_square(pos.side_to_move());

  end = pos.checkers() ? generate<EVASIONS>(pos, mlist)
                       : generate<NON_EVASIONS>(pos, mlist);
  while (cur != end)
		//���@��łȂ�������������
		if ((pinned || from_sq(cur->move) == ksq || type_of(cur->move) == ENPASSANT)
          && !pos.legal(cur->move, pinned))
          cur->move = (--end)->move;
      else
          ++cur;

  return end;
}
