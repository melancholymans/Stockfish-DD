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

#include <cstring>
#include <iostream>

#include "bitboard.h"
#include "tt.h"

/*
�g�����X�|�W�V�����e�[�u���̃O���[�o���ϐ�
*/
TranspositionTable TT; // Our global transposition table


/// TranspositionTable::set_size() sets the size of the transposition table,
/// measured in megabytes. Transposition table consists of a power of 2 number
/// of clusters and each cluster consists of ClusterSize number of TTEntry.
/*
�g�����X�|�W�V�����e�[�u���̏�������main�֐�����
TT.set_size(Options["Hash"])�ƌĂ΂�ď���������
Options["Hash"]�̓f�t�H���g�ł�32M�̃N���X�^���p�ӂ����
��肦��l��1--16384M�N���X�^

���̃g�����X�|�W�V�����e�[�u���̓��n�b�V���񐔂�ClusterSize=4�̃I�[�v���n�b�V���e�[�u����
��������Ă���
*/
void TranspositionTable::set_size(size_t mbSize) 
{

  assert(msb((mbSize << 20) / sizeof(TTEntry)) < 32);
	/*
	�K�v�ȃ������[���v�Z
	TTEEntry = 128bit = 16byte
	TTEntry[ClusterSize=4] = 16*4=64byte
	mbSize=32�f�t�H���g�̃I�v�V�����l << 20/64byte = 33554432/64 = 524288
	msb(524288) = 19
	4 << 19 = 2,097,152byte

	���̃������[�ŗp�ӂ����u���\��
	TTEntry��16byte�ł��ꂪ�S�Z�b�g�i�P�N���X�^�j�Ȃ̂�16*4=64byte
	2,097,152byte/64=32,768
	����32�Ƃ����̂��I�v�V�����̂R�Q�̂���
	*/
  uint32_t size = ClusterSize << msb((mbSize << 20) / sizeof(TTEntry[ClusterSize]));
	/*
	�v�����ꂽ�������[���O�Ƃ��Ȃ��Ȃ�Ȃɂ������A��
	*/
  if (hashMask == size - ClusterSize)
      return;
	/*
	hashMask�͋ǖʂ���Z�o����bit��iZobrist�N���X���Ԃ�64bit��)��
	TranspositionTable�̃C���f�b�N�X�ɂ���B
	��̓I�Ȑ��l�́[��1 1111 1111 1111 1111 1100b�̂悤�ɂȂ�B
	����2bit�̓N���X�^���S�ÂȂ̂ŋ󂢂Ă���
	Addres
	0		�N���X�^1�� �G���g��1��
	1		�N���X�^1�� �G���g��2��
	2		�N���X�^1�� �G���g��3��
	3		�N���X�^1�� �G���g��4��

	4		�N���X�^2�� �G���g��1��
	5		�N���X�^2�� �G���g��2��
	6		�N���X�^2�� �G���g��3��
	7		�N���X�^2�� �G���g��4��

	8		�N���X�^3�� �G���g��1��
	9		�N���X�^3�� �G���g��2��
	10	�N���X�^3�� �G���g��3��
	11	�N���X�^3�� �G���g��4��

	12		�N���X�^3�� �G���g��1��
	13		�N���X�^3�� �G���g��2��
	14	�N���X�^3�� �G���g��3��
	15	�N���X�^3�� �G���g��4��

	�N���X�^1�Ԗڂ̃G���g���P�ɃA�N�Z�X����ɂ̓C���f�b�N�X��0->0000b
	�N���X�^2�Ԗڂ̃G���g���P�ɃA�N�Z�X����ɂ̓C���f�b�N�X��4->100b
	�N���X�^3�Ԗڂ̃G���g���P�ɃA�N�Z�X����ɂ̓C���f�b�N�X��8->1000b
	�N���X�^4�Ԗڂ̃G���g���P�ɃA�N�Z�X����ɂ̓C���f�b�N�X��12->1100b
	�̂悤�ɂS��т��ƂɃA�N�Z�X���邽�߂ɉ���2bit�͂����Ă���
	*/
  hashMask = size - ClusterSize;
  free(mem);
  mem = calloc(size * sizeof(TTEntry) + CACHE_LINE_SIZE - 1, 1);
	/*
	�G���[���b�Z�[�W���o���ċ����I��
	*/
  if (!mem)
  {
      std::cerr << "Failed to allocate " << mbSize
                << "MB for transposition table." << std::endl;
      exit(EXIT_FAILURE);
  }
	/*
	�X�}�[�g�|�C���^(uintptr_t)���g���Ă���
	*/
  table = (TTEntry*)((uintptr_t(mem) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1));
}


/// TranspositionTable::clear() overwrites the entire transposition table
/// with zeroes. It is called whenever the table is resized, or when the
/// user asks the program to clear the table (from the UCI interface).
/*
TranspositionTable���[���N���A����
benchmark�֐��AUCI����Ă΂�Ă���i�ݒ肳��Ă���悤����
�Ă΂�Ă��Ȃ��悤�ȋC������j
*/
void TranspositionTable::clear() 
{

  std::memset(table, 0, (hashMask + ClusterSize) * sizeof(TTEntry));
}


/// TranspositionTable::probe() looks up the current position in the
/// transposition table. Returns a pointer to the TTEntry or NULL if
/// position is not found.
/*
�w�肳�ꂽkey��TranspositionTable��T�����ĊY������G���g���[���݂����炻�̃|�C���^�[��Ԃ�
�Ȃ�������nullptr��Ԃ�

Zobrist�N���X���Ԃ�bit���64bit�ł��̉���32bit��TranspositionTable�̃C���f�b�N�X�Ɏg��
���32bit���G���g���[��key�Ɏg��

�ŏ���first_entry�֐����Ăяo������32bit�Ńe�[�u���̃C���f�b�N�X�����A�Y������N���X�^�̍ŏ��̃A�h���X�Ԃ��Ă��炤
�N���E�h�̐��������i�i���S�j�����̃G���g����key�����32bit�ƈ�v���Ă����炻�̃G���g���[�̃A�h���X��Ԃ�
*/
const TTEntry* TranspositionTable::probe(const Key key) const 
{

  const TTEntry* tte = first_entry(key);
  uint32_t key32 = key >> 32;

  for (unsigned i = 0; i < ClusterSize; ++i, ++tte)
      if (tte->key() == key32)
          return tte;

  return nullptr;
}


/// TranspositionTable::store() writes a new entry containing position key and
/// valuable information of current position. The lowest order bits of position
/// key are used to decide on which cluster the position will be placed.
/// When a new entry is written and there are no empty entries available in cluster,
/// it replaces the least valuable of entries. A TTEntry t1 is considered to be
/// more valuable than a TTEntry t2 if t1 is from the current search and t2 is from
/// a previous search, or if the depth of t1 is bigger than the depth of t2.
/*
TranspositionTable�ւ̓o�^
�ǖʂ��n�b�V��������key���󂯎���ď��32bit���N���X�^�̒��̃G���g���[�̎��ʎq�Ɏg�p
first_entry�֐����Ă�ŉ���32bit��TranspositionTable�̃C���f�b�N�X��
�����first_entry�֐��ŃN���X�^�̍ŏ��̃G���g���[�A�h���X��tte�ɓ����
���ԂɃN���X�^���������āi�����ƃG���g���[�͋󂢂Ă��邩�A�������͂��łɏ��͓����Ă��邪
��ʂR�Qbit�������j�㏑��OK�Ȃ炻�̃G���g���[���X�V�Ώۂɂ���
*/
void TranspositionTable::store(const Key key, Value v, Bound b, Depth d, Move m, Value statV) 
{

  int c1, c2, c3;
  TTEntry *tte, *replace;
  uint32_t key32 = key >> 32; // Use the high 32 bits as key inside the cluster

  tte = replace = first_entry(key);

  for (unsigned i = 0; i < ClusterSize; ++i, ++tte)
  {
      if (!tte->key() || tte->key() == key32) // Empty or overwrite old
      {
          if (!m)
              m = tte->move(); // Preserve any existing ttMove

          replace = tte;
          break;
      }

      // Implement replace strategy
			/*
			�X�V���ǉ��Ώۂ̃G���g���[�̎�̑I��
			*/
      c1 = (replace->generation() == generation ?  2 : 0);
      c2 = (tte->generation() == generation || tte->bound() == BOUND_EXACT ? -2 : 0);
      c3 = (tte->depth() < replace->depth() ?  1 : 0);

      if (c1 + c2 + c3 > 0)
          replace = tte;
  }
	/*
	�G���g���[�X�V
	*/
  replace->save(key32, v, b, d, m, generation, statV);
}
