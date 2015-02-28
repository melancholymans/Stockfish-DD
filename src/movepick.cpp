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

#include "movepick.h"
#include "thread.h"

namespace {
	/*
	���胊�X�g�̐����ꍇ����
	*/
	enum Stages {
    MAIN_SEARCH, CAPTURES_S1, KILLERS_S1, QUIETS_1_S1, QUIETS_2_S1, BAD_CAPTURES_S1,
    EVASION,     EVASIONS_S2,
    QSEARCH_0,   CAPTURES_S3, QUIET_CHECKS_S3,
    QSEARCH_1,   CAPTURES_S4,
    PROBCUT,     CAPTURES_S5,
    RECAPTURE,   CAPTURES_S6,
    STOP
  };

  // Our insertion sort, guaranteed to be stable, as is needed
	/*
	�}���\�[�g
	*/
	void insertion_sort(ExtMove* begin, ExtMove* end)
  {
    ExtMove tmp, *p, *q;

    for (p = begin + 1; p < end; ++p)
    {
        tmp = *p;
        for (q = p; q != begin && *(q-1) < tmp; --q)
            *q = *(q-1);
        *q = tmp;
    }
  }

  // Unary predicate used by std::partition to split positive scores from remaining
  // ones so to sort separately the two sets, and with the second sort delayed.
	/*
	generate_next�֐��݂̂���Ă΂��
	�w�����value�i�����w����̉��]���l�j��0�ȏ�ł����true��Ԃ�
	*/
	inline bool has_positive_score(const ExtMove& ms) { return ms.score > 0; }

  // Picks and moves to the front the best move in the range [begin, end),
  // it is faster than sorting all the moves in advance when moves are few, as
  // normally are the possible captures.
	/*
	std::swap��max_element�������v�f��*begin����������
	std::max_element��begin����end�̊Ԃ̗v�f�ōő���̂̃C�[�T���[�^��Ԃ�
	�����ŕs���Ȃ̂�ExtMove��Move,value�̂Q�̗v�f�������Ă��邪�i�\���́j
	�ǂ̗v�f�Ŕ�r����̂��s��,���낢�뎎�����Ă݂���value�ōő�����o���Ă���
	�ǋL
	type.h�ɏ�����Ă���
	inline bool operator<(const ExtMove& f, const ExtMove& s) {
	return f.value < s.value;
	}
	���W����less�֐�����Ăяo����Ă���(��r���Z�q�̃I�[�o�[���C�h�j
	max_element�֐�����ďo��
	*/
	inline ExtMove* pick_best(ExtMove* begin, ExtMove* end)
  {
      std::swap(*begin, *std::max_element(begin, end));
      return begin;
  }
}


/// Constructors of the MovePicker class. As arguments we pass information
/// to help it to return the presumably good moves first, to decide which
/// moves to return (in the quiescence search, for instance, we only want to
/// search captures, promotions and some checks) and about how important good
/// move ordering is at the current node.
/*
�R�X�g���N�^
seach.cpp�̌Ăяo��
MovePicker mp(pos, ttMove, depth, History, countermoves, followupmoves, ss);
�̂悤��search�֐�����Ăяo�����

���C���̒T�����[�`���Ɏg�p����
���肪�������Ă���悤�ł����stage��EVASION�i����萶���j�ɂ����łȂ����MAIN_SEARCH�i�ʏ��j�ɐݒ肵�Ă���
�܂��g�����X�|�W�V�����e�[�u���̎肪�L���ł���΂����Ԃ��B�����łȂ����cur��end�𓯂��ɂ���
next_move�֐����Ă΂ꂽ���ɐV�������胊�X�g�𐶐�����
*/
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const HistoryStats& h,
                       Move* cm, Search::Stack* s) : pos(p), history(h), depth(d) {

  assert(d > DEPTH_ZERO);

  cur = end = moves;
  endBadCaptures = moves + MAX_MOVES - 1;
	/*
	countermoves��

	*/
  countermoves = cm;
  ss = s;

  if (p.checkers())
      stage = EVASION;

  else
      stage = MAIN_SEARCH;

  ttMove = (ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE);
  end += (ttMove != MOVE_NONE);
}
/*
�R���X�g���N�^
seach.cpp�̌Ăяo��
MovePicker mp(pos, ttMove, depth, History, to_sq((ss-1)->currentMove))
����Movepicker��search�֐������΂�Ă���̂ɑ΂���qsearch�֐��i���[��p�T���֐��j����Ă΂�Ă���


���肪�������Ă���悤�ł����stage��EVASION�i�����j�ɂ����łȂ���ΒT���[�x�ɉ�����
DEPTH_QS_NO_CHECKS(-2)�Ƃ�depth���傫�����QSEARCH_0(8)�ɐݒ肵�Ă���
DEPTH_QS_RECAPTURES(-10)���depth���傫�����QSEARCH_1(11)
(�T���[�x���}�C�i�X�Ƃ����̂͂ǂ��������Ƃ��낤�j
������ł��Ȃ��ꍇ��RECAPTURE(15)�ɐݒ�Attm�i�g�����X�|�W�V�����e�[�u��)����̎�͖������čŏ����琶��������

�Ō�̈���sq�͂P�O�̓G��ړ�������̏��̍��W

*/
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const HistoryStats& h,
                       Square sq) : pos(p), history(h), cur(moves), end(moves) {

  assert(d <= DEPTH_ZERO);

  if (p.checkers())
      stage = EVASION;

  else if (d > DEPTH_QS_NO_CHECKS)
      stage = QSEARCH_0;

  else if (d > DEPTH_QS_RECAPTURES)
  {
      stage = QSEARCH_1;

      // Skip TT move if is not a capture or a promotion, this avoids qsearch
      // tree explosion due to a possible perpetual check or similar rare cases
      // when TT table is full.
			/*
			�u���\���瓾���肪����������͐����łȂ��Ȃ疳������
			*/
      if (ttm && !pos.capture_or_promotion(ttm))
          ttm = MOVE_NONE;
  }
  else
  {
			/*
			RE-CAPTURE�i���Ԃ��[���O�ɓ������G������Ԃ��j
			recaptureSquare�ɂ͓G��̈ړ���̍��W������
			*/
      stage = RECAPTURE;
      recaptureSquare = sq;
      ttm = MOVE_NONE;
  }

  ttMove = (ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE);
  end += (ttMove != MOVE_NONE);
}
/*
�R���X�g���N�^
search�֐��̂Ȃ���step9 ProbCut�̂Ƃ���Ŏg��ꂽ�B
MovePicker mp(pos, ttMove, History, pos.captured_piece_type())
�̂悤�ɌĂяo����Ă���

stage��PROBCUT�̂P��
*/
MovePicker::MovePicker(const Position& p, Move ttm, const HistoryStats& h, PieceType pt)
                       : pos(p), history(h), cur(moves), end(moves) {

  assert(!pos.checkers());

  stage = PROBCUT;

  // In ProbCut we generate only captures better than parent's captured piece
	/*
	�������̕]���l��captureThreshold�ɓ���Ă���
	�g�����X�|�W�V�����e�[�u������Ƃ��Ă����w���肪���@��ł���΁ipseudo_legal�֐��͍��@�肩�ǂ����𔻒肷��֐����Ǝv���ڍוs���j
	*/
	captureThreshold = PieceValue[MG][pt];
  ttMove = (ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE);
	/*
	ttMove��������Ȃ��� OR ttMove���w������̐Î~�T���]���l�����łɎ���Ă����]���l��菬�����ꍇ
	ttMove�𖳎�����B�T�������ttMove��������悤�ȉ��l�̍�����Ȃ�̗p���邪�����łȂ���Ή��߂�
	���胊�X�g�𐶐�����
	*/
	if (ttMove && (!pos.capture(ttMove) || pos.see(ttMove) <= captureThreshold))
      ttMove = MOVE_NONE;

  end += (ttMove != MOVE_NONE);
}


/// score() assign a numerical move ordering score to each move in a move list.
/// The moves with highest scores will be picked first.
/*
����̒��胊�X�g�̕]���l��ݒ肷��
*/
template<>
void MovePicker::score<CAPTURES>() {
  // Winning and equal captures in the main search are ordered by MVV/LVA.
  // Suprisingly, this appears to perform slightly better than SEE based
  // move ordering. The reason is probably that in a position with a winning
  // capture, capturing a more valuable (but sufficiently defended) piece
  // first usually doesn't hurt. The opponent will have to recapture, and
  // the hanging piece will still be hanging (except in the unusual cases
  // where it is possible to recapture with the hanging piece). Exchanging
  // big pieces before capturing a hanging piece probably helps to reduce
  // the subtree size.
  // In main search we want to push captures with negative SEE values to
  // badCaptures[] array, but instead of doing it now we delay till when
  // the move has been picked up in pick_move_from_list(), this way we save
  // some SEE calls in case we get a cutoff (idea from Pablo Vazquez).
  Move m;

  for (ExtMove* it = moves; it != end; ++it)
  {
		/*
		�]���l�͎���̋�]���l�������Value�ɃL���X�g�������������Ă���
		�����Ă���Ӗ��͕s��
		*/
		m = it->move;
      it->score =  PieceValue[MG][pos.piece_on(to_sq(m))]
                 - type_of(pos.moved_piece(m));
			/*
			�w����p�^�[�������肾������A�Ȃ������̋�]���l����PAWN�̋�]���l�����������́i���i�]���l�j��ǉ����Ă���
			*/
			if (type_of(m) == PROMOTION)
          it->score += PieceValue[MG][promotion_type(m)] - PieceValue[MG][PAWN];
			/*
			�����w����p�^�[�����A���p�b�T����������
			PAWN�̕]���l��ǉ�����
			�i�A���p�b�T���̎���PAWN�̓����ł�to���W�ɓG�̋���Ȃ��̂Łj
			*/
      else if (type_of(m) == ENPASSANT)
          it->score += PieceValue[MG][PAWN];
  }
}

/*
���₩�Ȏ�̒��胊�X�g�̕]���l��history[][]�ϐ��ŏ��������Ă���
history[][]�͋��ʁA���ڕʂ̋�K�◚���̂悤�Ȃ��̂ŋ��������
���̏��ڂɗ��Ă��鏡�͓��_������
*/
template<>
void MovePicker::score<QUIETS>() {

  Move m;

  for (ExtMove* it = moves; it != end; ++it)
  {
      m = it->move;
      it->score = history[pos.moved_piece(m)][to_sq(m)];
  }
}

/*
MVV-LVA�̓I�[�_�����O�̎�@�̂悤��
https://chessprogramming.wikispaces.com/MVV-LVA
�Ӗ��͂킩��Ȃ�
���̊֐��͉������������̕]�����s���Ă���
�Î~�T���̌��ʂ��}�C�i�X�Ȃ�]���l��-2000������
�����ł͂Ȃ��Î~�T���̌��ʂ��v���X�Ȃ�ŏ��̎������̋�]���l�{2000
������Ȃ���Ȃ�history[][]��]���l�ɂ���
*/
template<>
void MovePicker::score<EVASIONS>() {
  // Try good captures ordered by MVV/LVA, then non-captures if destination square
  // is not under attack, ordered by history value, then bad-captures and quiet
  // moves with a negative SEE. This last group is ordered by the SEE score.
  Move m;
  int seeScore;

  for (ExtMove* it = moves; it != end; ++it)
  {
      m = it->move;
			/*
			���胊�X�g�̎�����o���ĐÎ~�T�����s���Ă݂Ă��̕]���l���}�C�i�X�Ȃ炻�̎�̕]���l����2000�_�����_����
			�����łȂ���΁i�v���X���[���j�{�������Ȃ�
			�ړ���ɂ����̕]���l�i�~�h���Q�[���p�̕]���l�j�|�@�Ƃ��̋��������A�����2000�_�����Z����
			����2000�_�͂����グ���邽�߂̂��́H
			�ȏ�Q��ވȊO�Ȃ�history�z��̒l���̗p
			*/
      if ((seeScore = pos.see_sign(m)) < 0)
          it->score = seeScore - HistoryStats::Max; // At the bottom

      else if (pos.capture(m))
          it->score =  PieceValue[MG][pos.piece_on(to_sq(m))]
                     - type_of(pos.moved_piece(m)) + HistoryStats::Max;
      else
          it->score = history[pos.moved_piece(m)][to_sq(m)];
  }
}


/// generate_next() generates, scores and sorts the next bunch of moves, when
/// there are no more moves to try for the current phase.
/*
next_move�֐������΂��estage�ɂ���Đ�������w����̃p�^�[�����قȂ�
*/
void MovePicker::generate_next() {

  cur = moves;

  switch (++stage) {
		/*
		�����generate�֐��Ő������āAscore<CAPTURES>()�֐��Ŏw����̕]�������Ă���
		*/
	case CAPTURES_S1: case CAPTURES_S3: case CAPTURES_S4: case CAPTURES_S5: case CAPTURES_S6:
      end = generate<CAPTURES>(pos, moves);
      score<CAPTURES>();
      return;
			/*
			�L���[��𐶐�����
			*/
	case KILLERS_S1:
			/*
			killers�z��͂S�܂ł��邪�Q��܂ł����g�p���Ȃ�
			���̌�ɃJ�E���^�[���ǉ����邽�߂Q�蕪�z��e�ʂɗ]�T���������Ă���
			*/
      cur = killers;
      end = cur + 2;

      killers[0].move = ss->killers[0];
      killers[1].move = ss->killers[1];
      killers[2].move = killers[3].move = MOVE_NONE;

      // Be sure countermoves are different from killers
			/*
			countermoves�͒��O�̓G��̓����ɑ΂��čőP������L�^���Ă�����
			countermoves[0]���G���Move���,countermoves[1]������ɑ΂��鉞��
			�P���ɓG�̓����ɑ΂��ĒT���̌��ʂ̍őP���������Ă��邾���Ȃ̂�
			����̏󋵂͈قȂ��Ă���̂ŕK�������œK�ȓ����ł͂Ȃ��B�J�E���^�[��ƌĂ�

			�����ł̓L���[��P�C�Q���J�E���^�[��ƈقȂ�Ȃ�J�E���^�[����L���[��z��ɒǉ�����
			*/
      for (int i = 0; i < 2; ++i)
          if (countermoves[i] != cur->move && countermoves[i] != (cur+1)->move)
              (end++)->move = countermoves[i];
			/*
			���������ڍוs��
			*/

      if (countermoves[1] && countermoves[1] == countermoves[0]) // Due to SMP races
          killers[3].move = MOVE_NONE;

      return;

  case QUIETS_1_S1:
			/*
			generate<QUIETS>(pos, moves)�͋��������肵�Ȃ����₩�Ȏw����𐶐�����
			*/
			endQuiets = end = generate<QUIETS>(pos, moves);
			/*
			���generate<QUIETS>�Ő����������胊�X�g�̓v���C�x�[�g�ϐ�moves[]�ɓ����Ă���
			���̒��胊�X�g��value��history[][]�ϐ��ŏ��������Ă���
			history[][]�ϐ��͋�킲�ƁA���ڂ��Ƃŋ�ړ����邽�тɉ��Z�����i�K�◚��]���Ƃ������Ƃ��납�ȁj
			*/
			score<QUIETS>();
			/*
			std::partition�֐���has_positive_value��true��Ԃ��v�f��false��Ԃ��v�f�𕪂���
			true�ƂȂ�v�f���ŏ��ɕ��ׁAfalse�ƂȂ�v�f�����̌��ɕ��ׂ�
			false�ƂȂ�v�f�̐擪�̃C�[�T���[�^��Ԃ�
			�^�U�𔻒肷��֐�has_positive_value�͎w����̉��]���l��0�ȏ�Ȃ�true��Ԃ�
			insertion_sort�֐��̓C���T�[�g�\�[�g
			end�͗L���Ȓ��萔�������ɐL�тĂ���
			*/
			end = std::partition(cur, end, has_positive_score);
      insertion_sort(cur, end);
      return;

  case QUIETS_2_S1:
			/*
			���܂܂Ŏ����Ă�����͑S�Ė������ă��Z�b�g�H
			*/
			cur = end;
      end = endQuiets;
      if (depth >= 3 * ONE_PLY)
          insertion_sort(cur, end);
      return;

  case BAD_CAPTURES_S1:
      // Just pick them in reverse order to get MVV/LVA ordering
      cur = moves + MAX_MOVES - 1;
      end = endBadCaptures;
      return;

  case EVASIONS_S2:
			/*
			���肪�������Ă���ꍇ�A��������钅�胊�X�g
			*/
			end = generate<EVASIONS>(pos, moves);
      if (end > moves + 1)
          score<EVASIONS>();
      return;

  case QUIET_CHECKS_S3:
      end = generate<QUIET_CHECKS>(pos, moves);
      return;

  case EVASION: case QSEARCH_0: case QSEARCH_1: case PROBCUT: case RECAPTURE:
      stage = STOP;
  case STOP:
      end = cur + 1; // Avoid another next_phase() call
      return;

  default:
      assert(false);
  }
}


/// next_move() is the most important method of the MovePicker class. It returns
/// a new pseudo legal move every time is called, until there are no more moves
/// left. It picks the move with the biggest score from a list of generated moves
/// taking care not returning the ttMove if has already been searched previously.
/*
search�֐�����ŏ��ɌĂ΂��B
cur��moves�z��̐擪���w���Ă���Aend��moves�z��̍Ō���w���Ă���
genmove.cpp�Ŏ�𐶐������end�������������������̂т�
��������肪�Ȃ���cur��end�͓����ɂȂ�A���̓s�xstage���オ���Ă���
��������p�^�[�����ω����Ă���

�������鏇�Ԃ�MAIN_SEARCH����X�^�[�g��BAD_CAPTURES_S1->KILLERS_S1 ... BAD_CAPTURES_S1�Ɛ�������p�^�[����ς��Ȃ���
����������Ă����Bnext_move�֐����Ăяo���Ă���search�֐��̏I��������MOVE_NONE�Ȃ̂�next_move�֐����I������̂�next_move�֐���
case STOP�ɂ����Ƃ��ɂȂ�B
BAD_CAPTURES_S1����̎肪�S�ďI�������generate_next_stage�֐���stage��������EVASION�ɂȂ��case EVASION�ɒH���
stage��STOP�ɂȂ肻�̂܂�case STOP�Ɋ��藎����(break�����Ȃ��̂Łjcur��end���������悤�ɂ���
next_move�֐��ɕԂ�cur == end���������Ȃ��̂�switch���Ɉړ���next_move�֐�����case STOP�Ɉړ�����MOVE_NONE��Ԃ�
search�֐�����̌Ăяo�����I������

���̂P�s�Â������p�^�[���̌Ăяo�����ԂɂȂ�
MAIN_SEARCH�̓��x����CAPTURES_S1����BAD_CAPTURES_S1�܂Ő������I������

MAIN_SEARCH, CAPTURES_S1, KILLERS_S1, QUIETS_1_S1, QUIETS_2_S1, BAD_CAPTURES_S1,

EVASION�̐����p�^�[���ŌĂяo���ꂽ��EVASIONS_S2�Ő�������
EVASION,     EVASIONS_S2,
QSEARCH_0�̐����p�^�[���ŌĂяo���ꂽ��CAPTURES_S3, QUIET_CHECKS_S3�Ő�������
QSEARCH_0,   CAPTURES_S3, QUIET_CHECKS_S3,
QSEARCH_1�̐����p�^�[���ŌĂяo���ꂽ��CAPTURES_S4�Ő�������
QSEARCH_1,   CAPTURES_S4,
PROBCUT�̐����p�^�[����ProbCut�̂Ƃ��Ă΂��
PROBCUT,     CAPTURES_S5,
RECAPTURE�̐����p�^�[���ŌĂяo���ꂽ��CAPTURES_S6�Ő�������
RECAPTURE,   CAPTURES_S6,
MOVE_NONE��Ԃ�
STOP
};
*/
template<>
Move MovePicker::next_move<false>() {

  Move move;

  while (true)
  {
		/*
		cur == end����������͎̂w���肪0�Ƃ�������
		*/
		while (cur == end)
          generate_next();

      switch (stage) {
				/*
				�����ɂ���Ƃ������Ƃ͎��V���ɐ������Ă��Ȃ��AttMove�ŏ\���Ƃ������ƂȂ̂�ttMove��������
				*/
			case MAIN_SEARCH: case EVASION: case QSEARCH_0: case QSEARCH_1: case PROBCUT:
          ++cur;
          return ttMove;

      case CAPTURES_S1:
				/*
				CAPTURES_S1�Ŏ肪�����ł����Ƃ��i���̎w����͐������Ă��Ȃ��j�����ɂ���
				pick_best�֐��ł����Ƃ��_���̂悩�������Ԃ��A
				�Î~�T�����ĕ]���l��0�������Ȃ��i��̎�荇���ɏ������������j
				�Î~�T�����ă}�C�i�X�ɂȂ�悤�Ȃ����̒��胊�X�g��endBadCaptures�̎w��
				�ʒu�Ɉړ����A�ŏ����璅�胊�X�g���Ȃ��Ȃ�܂łÂ���icur == end�j����������܂�
				*/
				move = pick_best(cur++, end)->move;
          if (move != ttMove)
          {
              if (pos.see_sign(move) >= 0)
                  return move;

              // Losing capture, move it to the tail of the array
              (endBadCaptures--)->move = move;
          }
          break;

      case KILLERS_S1:
          move = (cur++)->move;
          if (    move != MOVE_NONE
              &&  pos.pseudo_legal(move)
              &&  move != ttMove
              && !pos.capture(move))
              return move;
          break;

      case QUIETS_1_S1: case QUIETS_2_S1:
          move = (cur++)->move;
          if (   move != ttMove
              && move != killers[0].move
              && move != killers[1].move
              && move != killers[2].move
              && move != killers[3].move)
              return move;
          break;

      case BAD_CAPTURES_S1:
          return (cur--)->move;

      case EVASIONS_S2: case CAPTURES_S3: case CAPTURES_S4:
          move = pick_best(cur++, end)->move;
          if (move != ttMove)
              return move;
          break;

      case CAPTURES_S5:
           move = pick_best(cur++, end)->move;
           if (move != ttMove && pos.see(move) > captureThreshold)
               return move;
           break;

      case CAPTURES_S6:
          move = pick_best(cur++, end)->move;
          if (to_sq(move) == recaptureSquare)
              return move;
          break;

      case QUIET_CHECKS_S3:
          move = (cur++)->move;
          if (move != ttMove)
              return move;
          break;

      case STOP:
          return MOVE_NONE;

      default:
          assert(false);
      }
  }
}


/// Version of next_move() to use at split point nodes where the move is grabbed
/// from the split point's shared MovePicker object. This function is not thread
/// safe so must be lock protected by the caller.
template<>
Move MovePicker::next_move<true>() { return ss->splitPoint->movePicker->next_move<false>(); }
