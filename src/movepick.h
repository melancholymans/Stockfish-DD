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

#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include <algorithm> // For std::max
#include <cstring>   // For std::memset

#include "movegen.h"
#include "position.h"
#include "search.h"
#include "types.h"


/// The Stats struct stores moves statistics. According to the template parameter
/// the class can store History, Gains and Countermoves. History records how often
/// different moves have been successful or unsuccessful during the current search
/// and is used for reduction and move ordering decisions. Gains records the move's
/// best evaluation gain from one ply to the next and is used for pruning decisions.
/// Countermoves store the move that refute a previous one. Entries are stored
/// according only to moving piece and destination square, hence two moves with
/// different origin but same destination and piece will be considered identical.
/*
���v�I�Ȑ��l���L�^����H
�e���v���[�g�ɂ��GainsStats,HistoryStats,MovesStats�ƂR��
�L�^������̂�����Ă���

GainsStats��
search�֐��̖`����GainsStats Gains�Ɛ錾���Ă���
id_loop�֐����Ŏ����\�b�hclear�֐���table[][]�z���0�N���A���Ă���
HistoryStats��
search�֐��̖`����HistoryStats History�Ɛ錾���Ă���
id_loop�֐����Ŏ����\�b�hclear�֐���table[][]�z���0�N���A���Ă���
MovesStats��
MovesStats Countermoves, Followupmoves�Ɛ錾���Ă���
id_loop�֐����Ŏ����\�b�hclear�֐���table[][]�z���0�N���A���Ă���

�p�r�s��
*/
template<bool Gain, typename T>
struct Stats {

  static const Value Max = Value(2000);

  const T* operator[](Piece p) const { return table[p]; }
  void clear() { std::memset(table, 0, sizeof(table)); }
	/*
	template��MovesStats���g�p����
	upadte�֐�
	*/
	void update(Piece p, Square to, Move m) {

    if (m == table[p][to].first)
        return;

    table[p][to].second = table[p][to].first;
    table[p][to].first = m;
  }
	/*
	template��GainsStats,HistoryStats���g�p����
	update�֐�
	*/
	void update(Piece p, Square to, Value v) {
		/*
		if���ȍ~�����s����̂�GainsStats�֐�������
		*/
		if (Gain)
        table[p][to] = std::max(v, table[p][to] - 1);
		/*
		else if���ȍ~���������������HistoryStats�֐����X�V����
		search�֐����牽�炩�̏�����update_stats�֐�����΂�
		����update_stats�֐��̒����炱��update�֐����Ă΂�X�V�����
		������R�[�h���������Ɉړ�����قǕ]���l�͍����Ȃ�
		*/
		else if (abs(table[p][to] + v) < Max)
        table[p][to] +=  v;
  }

private:
  T table[PIECE_NB][SQUARE_NB];
};

typedef Stats< true, Value> GainsStats;
typedef Stats<false, Value> HistoryStats;
typedef Stats<false, std::pair<Move, Move> > CountermovesStats;


/// MovePicker class is used to pick one pseudo legal move at a time from the
/// current position. The most important method is next_move(), which returns a
/// new pseudo legal move each time it is called, until there are no moves left,
/// when MOVE_NONE is returned. In order to improve the efficiency of the alpha
/// beta algorithm, MovePicker attempts to return the moves which are most likely
/// to get a cut-off first.
/*
���胊�X�g�����̂�movegen.cpp�̎d������,���̒��胊�X�g��ExtMove moves[MAX_MOVES]�ɕێ�����
search�֐��̃��N�G�X�g�ɉ����Ďw�����n���̂�MovePicker�̂��d��
search�֐�����͂R��Ă΂�Ă��邪�A�R��Ƃ��R���X�g���N�^�����ƂȂ�
���C���̒T���Ɏg�p�����̂͂R�Ԗڂ̃R���X�g���N�^
*/
class MovePicker {

  MovePicker& operator=(const MovePicker&); // Silence a warning under MSVC

public:
  MovePicker(const Position&, Move, Depth, const HistoryStats&, Square);
  MovePicker(const Position&, Move, const HistoryStats&, PieceType);
  MovePicker(const Position&, Move, Depth, const HistoryStats&, Move*, Search::Stack*);

  template<bool SpNode> Move next_move();

private:
  template<GenType> void score();
  void generate_next();

  const Position& pos;
  const HistoryStats& history;
  Search::Stack* ss;
  Move* countermoves;
  Depth depth;
  Move ttMove;
  ExtMove killers[4];
  Square recaptureSquare;
  int captureThreshold, stage;
  ExtMove *cur, *end, *endQuiets, *endBadCaptures;
	/*
	movegen.h��MoveList�Ƃ���struct�^�̃N���X�����肻�̂Ȃ��ɂ�mlist�Ƃ���
	Move�^�̔z�񂪂���H
	*/
	ExtMove moves[MAX_MOVES];
};

#endif // #ifndef MOVEPICK_H_INCLUDED
