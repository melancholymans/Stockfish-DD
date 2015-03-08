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

#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include "types.h"

enum GenType {			//GenType�@��𐶐�����p�^�[��
  CAPTURES,					//�Ƃɂ�������
  QUIETS,						//���₩�Ȏ�i�Ƃ炸�Ɉړ����邾���j
  QUIET_CHECKS,			//����Ƃ�Ȃ��ŉ�����������i���₩�ɉ���H�j
  EVASIONS,					//�������������
  NON_EVASIONS,			//���������������ȊO�̎�
  LEGAL							//���@��𐶐��@��̓I�ɂ͉��肪�������Ă��Ȃ�������NON_EVASIONS���ĂсA���肪�������Ă�����EVASIONS���Ă�
};

class Position;

template<GenType>
ExtMove* generate(const Position& pos, ExtMove* mlist);

/// The MoveList struct is a simple wrapper around generate(), sometimes comes
/// handy to use this class instead of the low level generate() function.
/*
���胊�X�g���Ǘ�����N���X
���胊�X�g���̂�private�ϐ� mlist[MAX_MOVES=256]
�R���X�g���N�^���Ă΂ꂽ���_��cur��mlist�̐擪���w���Ă���
�e���v���[�g�ϐ��Ŏw�肵�������֐��Œ��胊�X�g��mlist�ɓo�^����Ă���

���̃N���X����begin,end()�֐��������Ă���̂�C++11�͈̔̓x�[�Xfor�����g����
����book.cpp��probe�֐����Ŏg���Ă���(�����p�^�[����LEGAL)�Bbegin,end�֐��̓N���X�����胊�X�g��
�g���Ă���BMovePick.cpp�Ŏg���Ă���MovePicker�N���X���̒��胊�X�g�imoves�j�Ƃ͂�����
endgame.cpp�ł��g���Ă���i�����p�^�[����LEGAL�j,notation.cpp�̂Ȃ���move_from_uci�֐��ł��g���Ă���i�����p�^�[����LEGAL�j
������move_to_san�֐����ł��g�p���Ă���i�����p�^�[����LEGAL�j

search�֐��Ŏ��̎�𐶐����ăI�_�[�����O����ȂǑ�|����Ȃ��Ƃ�����̂�MovePick�N���X
������ƍ��@��𐶐��������ꍇ��MoveList�N���X�ƕ����Ďg���Ă���̂��ȁH
*/
template<GenType T>
struct MoveList {

  explicit MoveList(const Position& pos) : last(generate<T>(pos, mlist)) {}
  const ExtMove* begin() const { return mlist; }
  const ExtMove* end() const { return last; }
	//mlist�̃T�C�Y��Ԃ�
	size_t size() const { return last - mlist; }
	//�n���ꂽ�肪���胊�X�g�imlist�j�ɂ�������true��Ԃ�
	bool contains(Move m) const {
    for (const ExtMove& ms : *this) if (ms.move == m) return true;
    return false;
  }

private:
  ExtMove mlist[MAX_MOVES], *last;
};

#endif // #ifndef MOVEGEN_H_INCLUDED
