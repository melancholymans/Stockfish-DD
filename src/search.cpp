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
/*
�p��W
http://misakirara.s296.xrea.com/misaki/words.html
*/
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>

#include <stdio.h>

#include "book.h"
#include "evaluate.h"
#include "movegen.h"
#include "movepick.h"
#include "notation.h"
#include "search.h"
#include "timeman.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"

namespace Search {
	/*
	ponder�Ƃ�
	��ǂ݂̂���
	���Ƃ���computer�������ŁA�J�n�ǖʂ���T�����J�n����76���Ƃ��Ɍ��߂�USI�v���g�R���� bestmove 76fu
	�Ƒ��鎞�A��ǂ݂������������肪�w���ł��낤���\�z����best move 76fu ponder 34fu�Ƒ���
	�G���W���̑���i�l�Ԃł���R���s���[�^�ł���USI�C���^�[�t�F�C�X������Ă�����́j�̓G���W�����\�z�����ǖʂ�
	�������ăG���W�����ɑ���Ԃ��B�G���W�����͂����ɗ\�z��Ő������ꂽ�ǖʂ�T������B
	���葤������w�����ꍇ�w�����肪�\�z��ƈꏏ�������ꍇ���葤��ponderhit��Ԃ��Ă���G���W�������łɒT�����I�����Ă����
	bestmove �Ŏ��Ԃ��΂悢���A�T���r���Ȃ炻�̂܂ܒT�����p������΂悢ponderhit��Ԃ������_�Ŏ�Ԃ��ς���Ă���
	�����\�z��ƈႦ�Α��葤��stop���G���W���ɂ�����A�ēx�\�z��ł͂Ȃ��^�̎w����i����̎�j�ōX�V���ꂽ�ǖʂ𑗂��Ă���
	go�R�}���h�𑗂��Ă���̂ŃG���W�����͒ʏ�T���������OK


	ponderhit�R�}���h���󂯂Ƃ������Ƃ̋���
	- ���łɒT�����I���Ă���ꍇ
	�@���łɒT���������Ă���ꍇ��think�֐��̏I���̂ق��Ŏ~�܂��Ă���B������UCI��ponderhit�R�}���h�𑗂��Ă����
	  Search::Signals.stopOnPonderhit��true�ɂȂ��Ă���̂�Signals.stop�t���O��true�ɂȂ�wait�֐��𔲂���UCI��
	  �w�����Ԃ����Ƃ��ł���B���̎���bestmove XX ponder XX�ƕԂ��̂ł��[����ponder�T�����J��Ԃ����Ƃ��ł���B

	 - �T���r���ł������ꍇ
	 �@UCI����ponderhit�R�}���h�𑗂��Ă����Search::Limits.ponder = false�Ƃ���B�܂�ponderhit�R�}���h��
		�󂯎�����u�Ԃ���G���W�����Ɏ�Ԃ��ړ����Ă��肻�̂܂ܒʏ�T���Ɉڍs����̂ł���B
		��Limits.ponder=true��false�ł͒T���ɂǂ��������Ⴄ�̂���
			+ ponder��true�ɂȂ��Ă���ꍇ�F�X�ȒT�������̏������������Ă�Signals.stop��true�ɂȂ炸
			Signals.stopOnPonderhit��true�ɂȂ邾���ōςށBSignals.stopOnPonderhit�ϐ��͒T���̋����ɂ�
			�֌W�Ȃ�ponder��true�ɂȂ�Ɓi�����Ԃ̎��Ԃ𗘗p���Ă���̂Łj�������Ȃ����R�ɒT�����Ă���Ƃ�����
			������ponderhiti���󂯎������T���ɐ��񂪂����Ƃ����Ӗ��Œʏ�̒T���ɂȂ�

		��ǂݎ肪�قȂ����ꍇ��UCI����stop�R�}���h�𑗂��Ă���B�R�}���h��UCI::loop�Ŏ󂯎���Search::Signals.stop = true�Ƃ���B
		��Search::Signals.stop = true�ƂȂ����ꍇ�̒T���̋�����
		id_loop�֐��Ő������̕ϐ����`�G�b�N����Ă���Atrue�ɂȂ����瑦�T�����[�v�𔲂���BRootMoves[0]�ɂƂ肠�����o�^���Ă�����Ԃ���
		�����ponder�肪���������Ƃ�O��Ƃ�����Ȃ̂�UCI���Ŕj�������B
		UCI����ponder�肪�Ԉ���Ă��邱�Ƃ�m���Ă���̂�ponder��ł͂Ȃ���ōX�V���ꂽ�ǖʂ�position�R�}���h�ő����Ă�������
		go ponder�R�}���h�𑗂邱�ƂŃG���W���ɍēx�T����v������B

	��������HP��聄
	������g���Ƃ��́A�K��go ponder�Ƃ����悤�ɁAgo�̂������Ƃ�ponder���������ƂɂȂ�܂��B
	ponder�Ƃ������t�́A�����ł́u�n�l�v�Ɩ󂳂�Ă��܂����A�v�l�Q�[���ɂ����ẮA����̎�Ԓ��Ɏ��̎���l����u��ǂ݁v���Ӗ����܂��B
	go ponder�́A��ǂ݂��J�n���鍇�}�ƂȂ�܂��B�i��ǂ݂��J�n���ׂ��ǖʂ́A���̑O��position�R�}���h�ɂ���đ����Ă��Ă��܂��B�j
	�G���W���́Ago ponder�ɂ���Ďv�l���J�n����ꍇ�AGUI�����玟�̃R�}���h�istop�܂���ponderhit�j�������Ă���O��bestmove��
	�w�����Ԃ��Ă͂����܂���B�i���Ƃ��A�v�l�J�n�̎��_�ŋl��ł���悤�ȏꍇ�ł������Ƃ��Ă��ł��B�j���肪����w���ƁA����ɂ����
	stop�܂���ponderhit�������ė���̂ŁA�����҂��Ă���bestmove�Ŏw�����Ԃ����ƂɂȂ�܂��B�i���̕ӂ̗�����ẮA
	��q����u�΋ǂɂ�����ʐM�̋�̗�v��ǂ�ŉ������B�j

	���u�΋ǂɂ�����ʐM�̋�̗�v��
	���ɁA��ǂ݋@�\��������܂��B
	�G���W�������ŁA��肪���菉���ǖʂ���P�Z���Ǝw�����ǖʂł����
	>position startpos moves 1g1f
	>go
	�G���W����bestmove�R�}���h�Ŏw�����Ԃ��܂����A���̎��ɐ�ǂݗv�����o�����Ƃ��ł��܂��B�G���W���̎w���肪4a3b�ŁA
	����ɑ΂��鑊��̎w�����6i7h�Ɨ\�z�����̂ł����
	<bestmove 4a3b ponder 6i7h
	GUI�͂������M����ƁA������position�R�}���h�Ŏv�l�J�n�ǖʂ𑗂�܂��B���̋ǖʂ́A���݋ǖʂɁA�G���W�����\�z��������̎w����
	�i���̏ꍇ��6i7h�j��ǉ��������̂ɂȂ�܂��B����ɑ�����go ponder�R�}���h�𑗂�܂��B
	>position startpos moves 1g1f 4a3b 6i7h
	>go ponder
	�G���W���͂������M����Ɛ�ǂ݂��J�n���܂��Bgo�R�}���h�̉���ɂ������܂������Ago ponder�ɂ���Đ�ǂ݂��J�n�����ꍇ�A
	����GUI����stop�܂���ponderhit�������Ă���܂ŁA�G���W����bestmove��Ԃ��Ă͂����܂���B���肪���̎���w���O�Ɏv�l��
	�I������Ƃ��Ă��AGUI����stop�܂���ponderhit�������Ă���܂ő҂��ƂɂȂ�܂��B
	�₪�āA���肪����w���܂��B���̎肪�G���W���̗\�z��ƈ�v�����ꍇ�ƁA�����łȂ��ꍇ�œ��삪�قȂ�܂��B
	�G���W���̗\�z�肪�O�ꂽ�ꍇ
	���̏ꍇ�AGUI�̓G���W����stop�𑗂�܂��B
	>stop
	�G���W���͂���ɑ΂��A�v�l���Ȃ炷���Ɏv�l��ł��؂��āA�����_�ōőP�ƍl���Ă�����bestmove�ŕԂ��܂��B���Ɏv�l���I����Ă����Ȃ�A
	�T���ς݂̎w�����bestmove�ŕԂ��܂��B�ibestmove�̂��Ƃ�ponder�ő���̗\�z���ǉ����Ă��\���܂��񂪁A������ɂ��떳������܂��B�j
	<bestmove 6a5b ponder 4i5h
	���́Astop�ɑ΂���bestmove�ŕԂ��ꂽ�w����́A�O�ꂽ�\�z��i���̏ꍇ��6i7h�j�ɑ΂���w����Ȃ̂ŁAGUI�͂��̓��e�𖳎����āA
	����������̎w����i���݋ǖʁj�𑗂�܂��B������go�R�}���h������܂��B���肪7g7f�Ǝw�����̂ł����
	>position startpos moves 1g1f 4a3b 7g7f
	>go
	�G���W���͂���ɂ���Ēʏ�̎v�l���J�n���܂��B
	�G���W���̗\�z�肪���������ꍇ
	���̏ꍇ�AGUI�̓G���W����ponderhit�𑗂�܂��B
	>ponderhit
	�\�z�肪���������̂ŁA�G���W���͈��������v�l���p�����č\���܂���B���Ɏv�l���I����Ă�����A�����ɂ��̎w�����Ԃ����Ƃ��ł��܂��B
	bestmove�Ŏw�����Ԃ��Ƃ��A�O��Ɠ��l��ponder��ǉ����Đ�ǂݗv�����o�����Ƃ��ł��܂��B
	<bestmove 6a5b ponder 4i5h
	�ȉ��A���l�ɂ��đ΋ǂ��p������܂��B

	Q1 �ŏ���ponder���g�p����ƌ��߂�I�v�V�����͂ǂ��ɂ���B
	ucioption.cpp����ponder�I�v�V������true���^�����Ă���
	���̃I�v�V������true�Ȃ�optimumSearchTime�ɏ�����悹�����
	*/

	/*
	SignalsType�͍\����
		bool stopOnPonderhit
		bool firstRootMove
		bool stop
		bool failedLowAtRoot;

	stopOnPonderhitponder UCI����ponderhit�R�}���h�𑗂��Ă�����true�ɂȂ�
	firstRootMove	�T���̍ŏ��̎菇
	Signals.stop�͒T�����~�߂�t���O
	failedLowAtRoot��Winodw�T����Low���s�ɂȂ��true�ɂȂ�.�����ݒ��start_thinking�֐�����false�ɐݒ�
	*/
	volatile SignalsType Signals;
	/*
	�T���̐������ځiuci�I�v�V�����Ŏ󂯎��j
	*/
  LimitsType Limits;
	/*
	���[�g�ł̒��胊�X�g
	*/
	std::vector<RootMove> RootMoves;
	/*
	�T���J�n�ǖ�
	*/
	Position RootPos;
	/*
	���[�g�ł̎��
	*/
	Color RootColor;
	/*
	�T���ɗv�������Ԃ��~���Z�R���h�Ōv��
	start_thinking�֐����Ō��ݎ��Ԃ����Ă����A�T���I����̎��Ԃƍ����������邱�ƂŌo�ߎ��Ԃ��v������
	*/
	Time::point SearchTime;
	/*
	����菇������A�ŏ��̋ǖʂ��t�@�C���Ȃǂ���ǂݍ��񂾏ꍇposition�N���X�ɂ�����SetupState������
	����SetupStates����R�s�[���󂯎��
	*/
	StateStackPtr SetupStates;
}	//namespace Search�I��

using std::string;
using Eval::evaluate;
using namespace Search;

namespace {

  // Set to true to force running with one thread. Used for debugging
	/*
	�ʏ��false�Ƃ��}���`�X���b�h�ŒT���Atrue�̓f�o�b�N�p�r�łP�X���b�h�ŒT��
	*/
  const bool FakeSplit = false;

  // Different node types, used as template parameter
	/*
	search�֐��̃e���v���[�g�����Ńm�[�h�̎�ʂ�\��
	Root:���[�g�ǖʂ̒T��
	PV:�œK����菇�p�̒T���m�[�h
	NonPV:�����_�ŕs��
	SplitPointRoot:���[�g�ǖʂł̒T������ɂȂ����m�[�h
	SplitPointPV:�����_�ŕs��
	SplitPointNonPV:�����_�ŕs��
	*/
	enum NodeType { Root, PV, NonPV, SplitPointRoot, SplitPointPV, SplitPointNonPV };

  // Dynamic razoring margin based on depth
	/*
	https://chessprogramming.wikispaces.com/Razoring
	Razoring�}����̎��̃}�[�W�����v�Z����A�[�x���[���Ȃ�ƃ}�[�W���͏������Ȃ�
	sockfish��search�֐����ł�depth�͌����Ă���
	*/
	inline Value razor_margin(Depth d) { return Value(512 + 16 * int(d)); }

  // Futility lookup tables (initialized at startup) and their access functions
	/*
	search::init()�ŏ����������
	*/
	int FutilityMoveCounts[2][32]; // [improving][depth]
	/*
	�}����iFutility Pruning�j�̂Ƃ��̃}�[�W�������߂�@100*depth
	*/
	inline Value futility_margin(Depth d) {
    return Value(100 * int(d));
  }

  // Reduction lookup tables (initialized at startup) and their access function
	/*
	Reduction�i�k���H�j
	�p�r�s��
	*/
	int8_t Reductions[2][2][64][64]; // [pv][improving][depth][moveNumber]
	/*
	Search�֐��̎}���蕔����Ă΂��
	*/
	template <bool PvNode> inline Depth reduction(bool i, Depth d, int mn) {

    return (Depth) Reductions[PvNode][i][std::min(int(d) / ONE_PLY, 63)][std::min(mn, 63)];
  }

	/*
	PVSize=�őP����菇��������iMultiPV),PVIdx=��������菇�̃C���f�b�N�X
	*/
	size_t PVSize, PVIdx;
	/*
	���Ԑ���H
	*/
	TimeManager TimeMgr;
	/*
	root�ǖʂōőP���ς�����
	�T�����Ԑ���Ŏg�p�[�񐔂������Ǝ��ԉ�������
	*/
	double BestMoveChanges;
	/*
	�p�r�s��
	*/
	Value DrawValue[COLOR_NB];
	/*
	History�̓N���X�Ńv���C�x�[�g�ϐ���Value table[pieceType][SQ]�������Ă���
	�ŏ���id_loop�֐�����0�N���A��update�֐��ōX�V����
	��ړ�������̍��W�ɓ��_���^�������̈ʒu�]���ő����̋�ړ�����قǍ����_
	��̈ړ������̂悤�Ȃ���
	*/
	HistoryStats History;
	/*
	�p�r�s��
	*/
	GainsStats Gains;
	/*
	�p�r�s��
	*/
	CountermovesStats Countermoves;
	/*
	��ʒT���֐�
	*/
	template <NodeType NT>
  Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);
	/*
	���Ԃ񖖒[�p�T���֐�
	*/
	template <NodeType NT, bool InCheck>
  Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth);
	/*
	�������[�v�������Ă��肱��id_loop�֐�����search�֐����Ă�
	idle_loop�֐�->think�֐�->id_loop�֐�->search�֐��ƌĂ΂��悤�ɂȂ��Ă���
	main�֐���Threads.init()���Ă��new_thread�֐�->thread_create�֐�->start_routine�֐�->idle_loop�֐��ň�U
	sleep��ԂɑJ�ڂ���
	UCI����̃R�}���hgo�ɂ��start_thking�֐�����sleep��Ԃ�������idle_loop�֐�����T�����J�n�����
	*/
	void id_loop(Position& pos);
	/*
	�p�r�s��
	*/
	Value value_to_tt(Value v, int ply);
	/*
	�p�r�s��
	*/
	Value value_from_tt(Value v, int ply);
  bool allows(const Position& pos, Move first, Move second);
  bool refutes(const Position& pos, Move first, Move second);
	/*
	�p�r�s��
	*/
	string uci_pv(const Position& pos, int depth, Value alpha, Value beta);
	/*
	�X�L�����x���̊Ǘ��i���[�U�[��chess�X�L���j
	�ō��X�L����20�Œ�X�L����0�ŃX�L�����x���ɍ��킹�Ď�𒲐�����
	�ǂ�����Ē������邩�ƌ����Ɣ����[���[�x�ƃX�L�����x��������������
	pick_move�֐����Ăт��̃N���X��Move best�ɗǂ���o�^���Ă���
	�����Ŕ����[���͂�߂��A���̂܂ܒT���͑��������Ă�����skill�ϐ���
	id_loop�֐��𔲂��鎞�f�X�g���N�^���Ă΂�[���[�x�œ���ꂽ�őP����iRootMoves[0]�j
	��best��ƌ������邱�Ƃɂ���Ď኱�ア����̗p����
	*/
	struct Skill {
    Skill(int l) : level(l), best(MOVE_NONE) {}
   ~Skill() {
      if (enabled()) // Swap best PV line with the sub-optimal one
          std::swap(RootMoves[0], *std::find(RootMoves.begin(),
                    RootMoves.end(), best ? best : pick_move()));
    }

    bool enabled() const { return level < 20; }
    bool time_to_pick(int depth) const { return depth == 1 + level; }
    Move pick_move();

    int level;
    Move best;
  };

} // namespace


/// Search::init() is called during startup to initialize various lookup tables
/*
main�֐�����Ă΂�Ă���
search�n�̏�����
*/
void Search::init() 
{

  int d;  // depth (ONE_PLY == 2)
  int hd; // half depth (ONE_PLY == 1)
  int mc; // moveCount

  // Init reductions array

	for (hd = 1; hd < 64; ++hd) for (mc = 1; mc < 64; ++mc)
  {
      double    pvRed = log(double(hd)) * log(double(mc)) / 3.0;
      double nonPVRed = 0.33 + log(double(hd)) * log(double(mc)) / 2.25;
      Reductions[1][1][hd][mc] = (int8_t) (   pvRed >= 1.0 ? floor(   pvRed * int(ONE_PLY)) : 0);
      Reductions[0][1][hd][mc] = (int8_t) (nonPVRed >= 1.0 ? floor(nonPVRed * int(ONE_PLY)) : 0);

      Reductions[1][0][hd][mc] = Reductions[1][1][hd][mc];
      Reductions[0][0][hd][mc] = Reductions[0][1][hd][mc];

      if (Reductions[0][0][hd][mc] > 2 * ONE_PLY)
          Reductions[0][0][hd][mc] += ONE_PLY;

      else if (Reductions[0][0][hd][mc] > 1 * ONE_PLY)
          Reductions[0][0][hd][mc] += ONE_PLY / 2;
  }
	/*
	Reductions�z��̒��g���t�@�C���ɏ����o����Excell�ŃO���t��
	FILE *fp;
	fopen_s(&fp, "Reductions.csv", "w");
	for (int pv = 0; pv < 2; pv++){
		for (int imp = 0; imp < 2; imp++){
			for (int depth = 0; depth < 64; depth++){
				for (int mc = 0; mc < 64; mc++){
					fprintf(fp, "%d", Reductions[pv][imp][depth][mc]);
					if (mc != 63) fprintf(fp, ",");
				}
				fprintf(fp, "\n");
			}
		}
	}
	fclose(fp);
	*/
  // Init futility move count array
	/*
	Excell�Ōv�Z�����Ă݂�����
	FutilityMoveCounts[0]={2,3,3,4,5,6,8,10,12,14,16,19,22,25,28,31,35,39,43,47,51,56,60,65,70,75,81,86,92,98,104,110}
	FutilityMoveCounts[1]={3,4,5,7,8,11,13,16,19,22,25,29,33,38,42,47,52,57,63,69,75,81,88,94,101,109,116,124,131,140,148,156}
	�Q���Ȑ��ɂȂ�
	*/
  for (d = 0; d < 32; ++d)
  {
      FutilityMoveCounts[0][d] = int(2.4 + 0.222 * pow(d +  0.0, 1.8));
      FutilityMoveCounts[1][d] = int(3.0 +   0.3 * pow(d + 0.98, 1.8));
  }
}


/// Search::perft() is our utility to verify move generation. All the leaf nodes
/// up to the given depth are generated and counted and the sum returned.
/*
�����̊֐�Search::perft����Ă΂��
�W�J�ł���m�[�h�̐���Ԃ��B
benchmark����g�p�����
*/
static size_t perft(Position& pos, Depth depth) 
{

  StateInfo st;
  size_t cnt = 0;
  CheckInfo ci(pos);
  const bool leaf = depth == 2 * ONE_PLY;

  for (const ExtMove& ms : MoveList<LEGAL>(pos))
  {
      pos.do_move(ms.move, st, ci, pos.gives_check(ms.move, ci));
      cnt += leaf ? MoveList<LEGAL>(pos).size() : ::perft(pos, depth - ONE_PLY);
      pos.undo_move(ms.move);
  }
  return cnt;
}

size_t Search::perft(Position& pos, Depth depth) 
{
  return depth > ONE_PLY ? ::perft(pos, depth) : MoveList<LEGAL>(pos).size();
}

/// Search::think() is the external interface to Stockfish's search, and is
/// called by the main thread when the program receives the UCI 'go' command. It
/// searches from RootPos and at the end prints the "bestmove" to output.
/*
idle_loop�֐�->think�֐�->id_loop�֐�->search�֐��ƌĂ΂��悤�ɂȂ��Ă���
�O���[�o���ϐ���RootPos�ϐ���wait_for_think_finished�֐��Ō��݂̋ǖʂ��R�s�[���Ă�����Ă���
*/
void Search::think() 
{

  static PolyglotBook book; // Defined static to initialize the PRNG only once

  RootColor = RootPos.side_to_move();
	/*
	���Ԑ���H
	*/
	TimeMgr.init(Limits, RootPos.game_ply(), RootColor);
	/*
	���[�g�ł̍��@��̎肪�Ȃ����
	UCI��info�R�}���h�ŒʒB����
	finalize:�ɔ��
	UCI�v���g�R���ɂ̓G���W�������畉����ʒm����R�}���h���Ȃ��悤�ł�
	http://www.geocities.jp/shogidokoro/usi.html
	*/
	if (RootMoves.empty())
  {
      RootMoves.push_back(MOVE_NONE);
      sync_cout << "info depth 0 score "
                << score_to_uci(RootPos.checkers() ? -VALUE_MATE : VALUE_DRAW)
                << sync_endl;

      goto finalize;
  }
	/*
	Limits.infinite��go ponder�R�}���h�̂��Ƃ�infinite�I�v�V�����������
	stop���|������܂Ŗ������T���𑱂���B
	Limits.mate��go ponder�̃I�v�V�����A�����T�������� x move�Ŏ萔�𐧌��ɂ���
	Limits.mate�ɂ��̎萔�������Ă���

	���Book���g�p����Ȃ�i�f�t�H���g��false�j���T���A
	���̎肪RootMoves�z��ɂ���΂��̎��RootMoves�̐擪�ɍs����finalize���x����
	�ړ����邱�ƁB�܂�T��������Վ��D��̂���
	*/
	if (Options["OwnBook"] && !Limits.infinite && !Limits.mate)
  {
      Move bookMove = book.probe(RootPos, Options["Book File"], Options["Best Book Move"]);

      if (bookMove && std::count(RootMoves.begin(), RootMoves.end(), bookMove))
      {
          std::swap(RootMoves[0], *std::find(RootMoves.begin(), RootMoves.end(), bookMove));
          goto finalize;
      }
  }
	/*
	https://chessprogramming.wikispaces.com/Contempt+Factor
	Contempt Factor=�y�̗v��?�@�����f�t�H���g�ł�0 ����l��-50 -> 50
	UCI_AnalyseMode=UCI ��̓��[�h�H�@�����f�t�H���g�ł�false
	���������Ɣ��f����]���l�i臒l�j�����߂�B

	�Γ��̑���Ȃ�臒l��VALUE_DRAW=0
	���肪���������炵��
	PHASE_MIDGAME=128
	game_phase�֐��͋ǖʂ̕]���l�𐳋K���i�]���l��0-128�ɂ���j�����l�ɂ��ĕԂ�
	�����Ō��߂��]���l��VALUE_DRAW(=0�j�ɉ��Z����
	����DrawValue[]�͋ǖʂ̕]���l�����Ĉ���������Ԃ����肷����̂ł͂Ȃ�
	���������Ɣ��f���ꂽ���ɕԂ����̕]���l
	*/
  if (Options["Contempt Factor"] && !Options["UCI_AnalyseMode"])
  {
      int cf = Options["Contempt Factor"] * PawnValueMg / 100; // From centipawns
      cf = cf * Material::game_phase(RootPos) / PHASE_MIDGAME; // Scale down with phase
      DrawValue[ RootColor] = VALUE_DRAW - Value(cf);
      DrawValue[~RootColor] = VALUE_DRAW + Value(cf);
  }
  else
      DrawValue[WHITE] = DrawValue[BLACK] = VALUE_DRAW;
	/*
	Options["Write Search Log"]�̓f�t�H���g��false
	Limits.time White,Black���ꂼ��̎�������
	Limits.inc�@winc,binc(�P��m sec)�ڍוs��
	Limits.movestogo �T���ɐ�����݂�����̂̂悤�����ڍוs��
	*/
	if (Options["Write Search Log"])
  {
      Log log(Options["Search Log Filename"]);
      log << "\nSearching: "  << RootPos.fen()
          << "\ninfinite: "   << Limits.infinite
          << " ponder: "      << Limits.ponder
          << " time: "        << Limits.time[RootColor]
          << " increment: "   << Limits.inc[RootColor]
          << " moves to go: " << Limits.movestogo
          << std::endl;
  }
	// Reset the threads, still sleeping: will be wake up at split time
	/*
	thread�̓��C���X���b�h�ƒT���X���b�h(start_routine�X���b�h)��timer�X���b�h������͗l
	���ƒT�����ɕ����̃X���b�h�ŒT���؂�T�������@����������Ă���
	*/
	for (Thread* th : Threads)
      th->maxPly = 0;

  Threads.sleepWhileIdle = Options["Idle Threads Sleep"];
  Threads.timer->run = true;
  Threads.timer->notify_one(); // Wake up the recurring timer
	/*
	�T�����J�n
	*/
	id_loop(RootPos); // Let's start searching !

  Threads.timer->run = false; // Stop the timer
  Threads.sleepWhileIdle = true; // Send idle threads to sleep
	/*
	search�̃��O���L�^����I�v�V������true�ł���΃f�t�H���g�ł�false
	�t�@�C������SearchLog.txt�ɂȂ�
	*/
	if (Options["Write Search Log"])
  {
      Time::point elapsed = Time::now() - SearchTime + 1;

      Log log(Options["Search Log Filename"]);
      log << "Nodes: "          << RootPos.nodes_searched()
          << "\nNodes/second: " << RootPos.nodes_searched() * 1000 / elapsed
          << "\nBest move: "    << move_to_san(RootPos, RootMoves[0].pv[0]);

      StateInfo st;
      RootPos.do_move(RootMoves[0].pv[0], st);
      log << "\nPonder move: " << move_to_san(RootPos, RootMoves[0].pv[1]) << std::endl;
      RootPos.undo_move(RootMoves[0].pv[0]);
  }

finalize:

  // When search is stopped this info is not printed
  sync_cout << "info nodes " << RootPos.nodes_searched()
            << " time " << Time::now() - SearchTime + 1 << sync_endl;

  // When we reach max depth we arrive here even without Signals.stop is raised,
  // but if we are pondering or in infinite search, according to UCI protocol,
  // we shouldn't print the best move before the GUI sends a "stop" or "ponderhit"
  // command. We simply wait here until GUI sends one of those commands (that
  // raise Signals.stop).
	/*
	ponder�T�����Ȃ�stopOnPonderhit��true�ɁA�ʏ�T���Ȃ�stop��true��
	UCI����ponderhit�R�}���h�͗��Ȃ��̂�->����UCI::loop��ponderhit�R�}���h���󂯎������
	Signals.stop��true�ɂ��ĒT�����~�߂鏈���ɓ���B
	������ponder�T�����S�Ă̒T�����I��������UCI�����v�l���I���Ă��Ȃ��̂ɃG���W��������Ɏw�����Ԃ��̂�h�����߂̏���
	UCI��stop�R�}���h�iUCI���v�l�I���̃t���O�j��ponderhit�R�}���h���o���̂�҂��Ă��鏈���iwait_for�֐��őҋ@������j
	*/
  if (!Signals.stop && (Limits.ponder || Limits.infinite))
  {
      Signals.stopOnPonderhit = true;
      RootPos.this_thread()->wait_for(Signals.stop);
  }

  // Best move could be MOVE_NONE when searching on a stalemate position
	/*
	UCI�ɒT�����ʂ�Ԃ��Ă���,ponder�������Ă���Ƃ��Ƃ����łȂ����̏ꍇ���������Ă���Ǝv������
	���̂悤�Ȃ��Ƃ͂��Ă��Ȃ��B���best move ��ponder��i�őP����菇�̑���̎�pv[1]��Ԃ��Ă���j
	���ʂ��󂯎����UCI�������f����ponder����������Ă���̂���
	*/
	sync_cout << "bestmove " << move_to_uci(RootMoves[0].pv[0], RootPos.is_chess960())
            << " ponder "  << move_to_uci(RootMoves[0].pv[1], RootPos.is_chess960())
            << sync_endl;
}


namespace {

  // id_loop() is the main iterative deepening loop. It calls search() repeatedly
  // with increasing depth until the allocated thinking time has been consumed,
  // user stops the search, or the maximum search depth is reached.
	/*
	think�֐�����Ăяo����Ă���
	��������search�֐���NodeType(Root, PV, NonPV)��ݒ肵�ČĂяo��
	*/
	void id_loop(Position& pos) 
	{

		Stack stack[MAX_PLY_PLUS_6], *ss = stack+2; // To allow referencing (ss-2)
		int depth;
		Value bestValue, alpha, beta, delta;

		std::memset(ss-2, 0, 5 * sizeof(Stack));
		(ss-1)->currentMove = MOVE_NULL; // Hack to skip update gains

		depth = 0;
		BestMoveChanges = 0;
		bestValue = delta = alpha = -VALUE_INFINITE;
		beta = VALUE_INFINITE;

		TT.new_search();
		History.clear();
		Gains.clear();
		Countermoves.clear();
		/*
		�f�t�H���g�Ȃ�MultiPV�ɂP��Ԃ��iMultiPV���Q�ȏ�ɂ���ƁA�����̉���菇�������j
		�f�t�H���g�Ȃ�skill.level�ɂQ�O��Ԃ�
		*/
		PVSize = Options["MultiPV"];
		Skill skill(Options["Skill Level"]);

		// Do we have to play with skill handicap? In this case enable MultiPV search
		// that we will use behind the scenes to retrieve a set of possible moves.
		/*
		�f�t�H���g�Ȃ�skill.level��20�ɂȂ�̂�skill.enabled()��false���������̂�MultiPV��1�̂܂�
		�A��skill.level��20�����ł���΁APVSize�Œ�ł��S�ȏ�ɂȂ�
		*/
		if (skill.enabled() && PVSize < 4)
			PVSize = 4;
		/*
		����菇��Root�ǖʂ̉\�w���萔���傫���͂ł��Ȃ�
		*/
		PVSize = std::min(PVSize, RootMoves.size());

		// Iterative deepening loop until requested to stop or target depth reached
		/*
		�����[���@�iIterative deepening loop�j
		�w��̐[�x�iMAX_PLY�j�܂ŒB���邩�Astop���|��܂Ŕ����T�����s��
		Limits.depth��UCI�v���g�R������T���[�x���w�肵�Ă���΂�����ɏ]����������MAX_PLY���傫�Ȑ[�x�͈Ӗ��Ȃ�
		depth=1����J�n�����MAX_PLY��120
		*/
		printf("Signals.stop = %d\n", Signals.stop);
		printf("Limits.depth = %d\n", Limits.depth);

		while (++depth <= MAX_PLY && /*!Signals.stop && �ꎞ�I�ɃR�����g�A�E�g2015/5/9*/ (!Limits.depth || depth <= Limits.depth))
		{
			// Age out PV variability metric
			/*
			�ŏ���0.0�ɏ������i����id_loop�֐��̖`���Łj
			*/
			BestMoveChanges *= 0.8;

			// Save last iteration's scores before first PV line is searched and all
			// the move scores but the (new) PV are set to -VALUE_INFINITE.
			/*
			RootMoves�̓R���X�g���N�^�̂Ƃ�prevScore�ϐ�,score�ϐ��Ƃ�
			-VALUE_INFINITE(32001)�ɏ����ݒ肳��Ă���
			prevScore�͂��̂��ƂŕύX�����̂ł����ōď����������̂���

			RootMoves���̂�start_thinking�֐��ŏ���������Ă���
			����for���L�@��C++11����
			*/
			for (RootMove& rm : RootMoves)
				rm.prevScore = rm.score;

			// MultiPV loop. We perform a full root search for each PV line
			//�����̉���菇���iMultiPV���L���Ȃ�S�ȏ�MultiPV��false�Ȃ�1�j�����J��Ԃ�
			for (PVIdx = 0; PVIdx < PVSize /*&& !Signals.stop�ꎞ�I�ɃR�����g�A�E�g2015/5/9*/; ++PVIdx)
			{
				// Reset aspiration window starting size
				/*
				prevScore��-32001�Ȃ̂�
				alpha=max(-32001-16,-32001)= -32001
				beta=min(-32001+16,+32001)=  -31985
				*/
				if (depth >= 5)
				{
					delta = Value(16);
					alpha = std::max(RootMoves[PVIdx].prevScore - delta,-VALUE_INFINITE);
					beta  = std::min(RootMoves[PVIdx].prevScore + delta, VALUE_INFINITE);
				}

				// Start with a small aspiration window and, in case of fail high/low,
				// research with bigger window until not failing high/low anymore.
				//https://chessprogramming.wikispaces.com/Aspiration+Windows
				/*
				aspiration window�Ƃ�����@�炵���A�T�������������ĒT�����x���グ�邪
				�T�����s�i�^�̕]���l�����̊O�ɂ������ꍇfail high/low)�̎��A�ĒT���R�X�g���|��̂��f�����b�g
				*/
				while (true)
				{
					bestValue = search<Root>(pos, ss, alpha, beta, depth * ONE_PLY, false);

					// Bring to front the best move. It is critical that sorting is
					// done with a stable algorithm because all the values but the first
					// and eventually the new best one are set to -VALUE_INFINITE and
					// we want to keep the same order for all the moves but the new
					// PV that goes to the front. Note that in case of MultiPV search
					// the already searched PV lines are preserved.
					/*
					RootMoves�z�������\�[�g���g���Ă���
					�\�[�g���ɂ��C���T�[�g�\�[�g�ƃ}�[�W�\�[�g���g�������Ă��邪
					��r�֐����w�肵�Ă��Ȃ��̂ŕW����less�֐��Ŕ�r���Ă��邪
					�W����less�֐�����RootMoves��bool operator<(const RootMove& m) const { return score > m.score; }
					���g����score���m���r���Ă���
					*/
					std::stable_sort(RootMoves.begin() + PVIdx, RootMoves.end());

					// Write PV back to transposition table in case the relevant
					// entries have been overwritten during the search.
					/*
					����ꂽ�����i�őP��菇�j��TT�ɓo�^���Ă���
					*/
					for (size_t i = 0; i <= PVIdx; ++i)
						RootMoves[i].insert_pv_in_tt(pos);

					// If search has been stopped break immediately. Sorting and
					// writing PV back to TT is safe becuase RootMoves is still
					// valid, although refers to previous iteration.
					/*
					stop��������΂��̉i�v���[�v����ł�
					*/
					if (Signals.stop)
						break;

					// When failing high/low give some update (without cluttering
					// the UI) before to research.
					/*
					high/low���s�����Ƃ�
					uci_pv�֐��̓��e��W���o�͂ɏo��
					*/
					if ((bestValue <= alpha || bestValue >= beta) && Time::now() - SearchTime > 3000)
						sync_cout << uci_pv(pos, depth, alpha, beta) << sync_endl;

					// In case of failing low/high increase aspiration window and
					// research, otherwise exit the loop.
					/*
					Low���s�����ꍇ�̍ĒT���̂��߂̕]���l�ݒ�
					*/
					if (bestValue <= alpha)
					{
						/*
						alpha�l��Ԃ��Ă����l���炳���delta(16)������A�܂�Window���L���čĒT������A������VALUE_INFINITE���͉����Ȃ�
						*/
						alpha = std::max(bestValue - delta, -VALUE_INFINITE);
						/*
						failedLowAtRoot��Low���s�̃t���O
						check_time�֐����Ŏg�p����Ă���
						*/
						Signals.failedLowAtRoot = true;
						Signals.stopOnPonderhit = false;
					}
					/*
					High���s�����ꍇ�̍ĒT���̂��߂̕]���l�ݒ�
					alpha�̔��΂�beta�l��delta(16)�����グ��A�܂�Window���L���čĒT������A������VALUE_INFINITE���͏グ�Ȃ�
					*/
					else if (bestValue >= beta)
						beta = std::min(bestValue + delta, VALUE_INFINITE);
					/*
					Low,High���s���Ȃ��̂Ő^�̕]���l��Window���ŕԂ��Ă����̂Ŏ��̔����[���Ɉڂ�
					*/
					else
						break;
					/*
					Low,High���s�����ꍇ�͂P�U����
					16->24.0->36.0->54.0->81.0->121.5
					�Ə��X��delta���L���Ď��s���Ȃ��T�����s��
					*/
					delta += delta / 2;

					assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
				}//while(true)�I��

				// Sort the PV lines searched so far and update the GUI
				/*
				RootMove��RootMove�N���X��score�l�ň���\�[�g����
				*/
				std::stable_sort(RootMoves.begin(), RootMoves.begin() + PVIdx + 1);

				if (PVIdx + 1 == PVSize || Time::now() - SearchTime > 3000)
					sync_cout << uci_pv(pos, depth, alpha, beta) << sync_endl;
			}//MultiPV�I��

			// Do we need to pick now the sub-optimal best move ?
			/*
			�X�L�����x����20���� ���@time_to_pick �֐���depth�i�����[���[�x�j���X�L�����x���Ɠ����Ȃ�true����ȊO��
			false��Ԃ��B�X�L�����x���͍ō��łQ�O�ŒႪ�O�Ȃ̂ŃX�L�����x�����������pick_move�֐����ĂԐ[�x�͐󂭂Ȃ�
			skill.pick_move�֐���best���Ԃ��悤�ɂȂ��Ă��邪����͎g���Ă��Ȃ�
			���̃o�[�W�����ł�pick_move�֐��͎c���Ă��邪�����̃o�[�W������stockfish�ł͏�����Ă���
			*/
			if (skill.enabled() && skill.time_to_pick(depth))
				skill.pick_move();
			/*
			search Log���ݒ肵�Ă����(�f�t�H���g�ł�false�jSearchLog.txt�Ƀ��O���c��
			*/
			if (Options["Write Search Log"])
			{
				RootMove& rm = RootMoves[0];
				if (skill.best != MOVE_NONE)
					rm = *std::find(RootMoves.begin(), RootMoves.end(), skill.best);

				Log log(Options["Search Log Filename"]);
				log << pretty_pv(pos, depth, rm.score, Time::now() - SearchTime, &rm.pv[0]) << std::endl;
			}

			// Do we have found a "mate in x"?
			/*
			Limits.mate�͉����T����𐧌�����I�v�V������
			�����ɂ����Ă������������������T�����~�ł��邪
			���̏����̈Ӗ����悭�킩���
			*/
			if (Limits.mate && bestValue >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - bestValue <= 2 * Limits.mate)
				Signals.stop = true;

			// Do we have time for the next iteration? Can we stop searching now?
			/*
			�T����Limits�ɂ�鐧���A�T�����~����stop�t���O�Ȃǂ��|���Ă��Ȃ����
			*/
			if (Limits.use_time_management() && !Signals.stop && !Signals.stopOnPonderhit)
			{
				bool stop = false; // Local variable, not the volatile Signals.stop

				// Take in account some extra time if the best move has changed
				/*
				BestMoveChanges��root�ǖʂōőP�肪�ύX�ɂȂ�����
				���̉񐔂�������Ί댯�ȋǖʂƔ��f���Ď��Ԑ����L�΂�
				*/
				if (depth > 4 && depth < 50 && PVSize == 1)
					TimeMgr.pv_instability(BestMoveChanges);

				// Stop search if most of available time is already consumed. We
				// probably don't have enough time to search the first move at the
				// next iteration anyway.
				if (Time::now() - SearchTime > (TimeMgr.available_time() * 62) / 100)
					stop = true;

				// Stop search early if one move seems to be much better than others
				if (    depth >= 12
				&&  BestMoveChanges <= DBL_EPSILON
				&& !stop
				&&  PVSize == 1
				&&  bestValue > VALUE_MATED_IN_MAX_PLY
				&& (   RootMoves.size() == 1
				|| Time::now() - SearchTime > (TimeMgr.available_time() * 20) / 100))
				{
					Value rBeta = bestValue - 2 * PawnValueMg;
					ss->excludedMove = RootMoves[0].pv[0];
					ss->skipNullMove = true;
					Value v = search<NonPV>(pos, ss, rBeta - 1, rBeta, (depth - 3) * ONE_PLY, true);
					ss->skipNullMove = false;
					ss->excludedMove = MOVE_NONE;

					if (v < rBeta)
						stop = true;
				}

				if (stop)
				{
					// If we are allowed to ponder do not stop the search now but
					// keep pondering until GUI sends "ponderhit" or "stop".
					/*
					stop�V�O�i�������Ă���ponder���Ȃ�stopOnPonderhit��true�ɂ��ĒT����~����
					ponder�łȂ���Βʏ�̒�~�t���O���Z�b�g
					*/
					if (Limits.ponder)
						Signals.stopOnPonderhit = true;
					else
						Signals.stop = true;
				}
			}
		}//�����[���I��
	}


  // search<>() is the main search function for both PV and non-PV nodes and for
  // normal and SplitPoint nodes. When called just after a split point the search
  // is simpler because we have already probed the hash table, done a null move
  // search, and searched the first move before splitting, we don't have to repeat
  // all this work again. We also don't need to store anything to the hash table
  // here: This is taken care of after we return from the split point.
	/*
	NodeType�Ƃ́H�@
	SpNode�Ƃ́@split��Sp�����i�T������j
	id_loop����search�֐����ĂԎ���NodeType=Root,SpNode=false�ŌĂ΂��
	Depth depth�͔����[���̂��т�2->4->6�Ƒ����Ă���

	�T����search,qsearch�֐����ł�depth��ONE_PLY(=2)�Â����Ă���
	*/
	template <NodeType NT>
  Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) 
	{

    const bool PvNode   = (NT == PV || NT == Root || NT == SplitPointPV || NT == SplitPointRoot);
    const bool SpNode   = (NT == SplitPointPV || NT == SplitPointNonPV || NT == SplitPointRoot);
    const bool RootNode = (NT == Root || NT == SplitPointRoot);

    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(depth > DEPTH_ZERO);

    Move quietsSearched[64];
		StateInfo st;
    const TTEntry *tte;
    SplitPoint* splitPoint;
    Key posKey;
    Move ttMove, move, excludedMove, bestMove, threatMove;
    Depth ext, newDepth, predictedDepth;
    Value bestValue, value, ttValue, eval, nullValue, futilityValue;
    bool inCheck, givesCheck, pvMove, singularExtensionNode, improving;
    bool captureOrPromotion, dangerous, doFullDepthSearch;
    int moveCount, quietCount;

    // Step 1. Initialize node
		/*
		�����̓m�[�h�̏������H
		*/
		Thread* thisThread = pos.this_thread();
		/*
		pos.checkers()�͎�ԑ���KING�ɉ���������Ă�����bitboard��Ԃ�
		�܂�inCheck��true�Ȃ��ԑ��ɉ��肪�������Ă���
		*/
    inCheck = pos.checkers();
		/*
		Root�APvNode�̎��͂����͂Ƃ���Ȃ�
		�T�����򂷂�Ƃ��ɂƂ���Ǝv����
		*/
		if (SpNode)
    {
        splitPoint = ss->splitPoint;
        bestMove   = splitPoint->bestMove;
        threatMove = splitPoint->threatMove;
        bestValue  = splitPoint->bestValue;
        tte = nullptr;
        ttMove = excludedMove = MOVE_NONE;
        ttValue = VALUE_NONE;

        assert(splitPoint->bestValue > -VALUE_INFINITE && splitPoint->moveCount > 0);

        goto moves_loop;
    }
		/*
		struct Stack {
		SplitPoint* splitPoint;
		int ply;
		Move currentMove;
		Move ttMove;
		Move excludedMove;
		Move killers[2];
		Depth reduction;
		Value staticEval;
		int skipNullMove;
		};
		ss�͍ŏ��̂Q�̓J�b�g����index�Q����search�֐��ɓn�����
		������struct Stack�̎c��̏������i�T������Ɋ֌W�̂Ȃ������j
		ss->currentMove�͌��ݒ��ڂ���Ă���w����������w����ł��邪�ŏ���MOVE_NONE
		(ss+1)->excludedMove���̎w����͑��̌Z�����ُ�ɓ_���������n�������ʂ��^����悤�Ȋ댯�Ȏ�
		ss->ply�͈�O�̐[�x��+�P����B
		(ss+1)->skipNullMove = false��null_move�̎��Q��A����null_move�ɂȂ�Ȃ��悤�ɂ���t���O�A�Ȃ̂łP���̐[�x�ɂȂ��Ă���
		(ss+1)->reduction = DEPTH_ZERO;�͕s���Areduction�i�팸�j
		moveCount�͕s��
		quietCount�͕s��
		(ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE�L���[������������Ă��邪�Ȃ��Q���̃L���[�������������
		*/
		moveCount = quietCount = 0;
    bestValue = -VALUE_INFINITE;
    ss->currentMove = threatMove = (ss+1)->excludedMove = bestMove = MOVE_NONE;
    ss->ply = (ss-1)->ply + 1;
    (ss+1)->skipNullMove = false; (ss+1)->reduction = DEPTH_ZERO;
    (ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE;

    // Used to send selDepth info to GUI
		/*
		P��Node�ł����̃X���b�h�������Ă���maxPly���ss->ply���[���ꍇss->ply�ɍ��킹��
		*/
		if (PvNode && thisThread->maxPly < ss->ply)
        thisThread->maxPly = ss->ply;

		/*
		RootNode�ȊO�̂�
		���ƂŃR�����g����
		*/
		if (!RootNode)
    {
        // Step 2. Check for aborted search and immediate draw
        if (Signals.stop || pos.is_draw() || ss->ply > MAX_PLY)
            return DrawValue[pos.side_to_move()];

        // Step 3. Mate distance pruning. Even if we mate at the next move our score
        // would be at best mate_in(ss->ply+1), but if alpha is already bigger because
        // a shorter mate was found upward in the tree then there is no need to search
        // further, we will never beat current alpha. Same logic but with reversed signs
        // applies also in the opposite condition of being mated instead of giving mate,
        // in this case return a fail-high score.
        alpha = std::max(mated_in(ss->ply), alpha);
        beta = std::min(mate_in(ss->ply+1), beta);
        if (alpha >= beta)
            return alpha;
    }

    // Step 4. Transposition table lookup
    // We don't want the score of a partial search to overwrite a previous full search
    // TT value, so we use a different position key in case of an excluded move.
		/*
		ttMove�Ƃ̓g�����X�|�W�V�����e�[�u��������o�����w����
		�A�����[�g�m�[�h�ł�RootMoves[PVIdx].pv[0]������o�����w����
		����̓��[�g�m�[�h�ł͂܂��g�����X�|�W�V�����e�[�u���Ɏ肪�o�^����Ă��Ȃ����炩��
		Root�m�[�h�łȂ��g�����X�|�W�V�����e�[�u���Ɏ肪�Ȃ������ꍇ��MOVE_NONE
		�g�����X�|�W�V�����e�[�u���̎�̕]���l��value_from_tt�֐��Őݒ�A�莩�̂��Ȃ������ꍇ��VALUE_NONE
		*/
		excludedMove = ss->excludedMove;
    posKey = excludedMove ? pos.exclusion_key() : pos.key();
    tte = TT.probe(posKey);
    ttMove = RootNode ? RootMoves[PVIdx].pv[0] : tte ? tte->move() : MOVE_NONE;
    ttValue = tte ? value_from_tt(tte->value(), ss->ply) : VALUE_NONE;

    // At PV nodes we check for exact scores, while at non-PV nodes we check for
    // a fail high/low. Biggest advantage at probing at PV nodes is to have a
    // smooth experience in analysis mode. We don't probe at Root nodes otherwise
    // we should also update RootMoveList to avoid bogus output.
		/*
		�p�r�s��
		*/
		if (!RootNode
        && tte
        && tte->depth() >= depth
        && ttValue != VALUE_NONE // Only in case of TT access race
        && (           PvNode ?  tte->bound() == BOUND_EXACT
            : ttValue >= beta ? (tte->bound() &  BOUND_LOWER)
                              : (tte->bound() &  BOUND_UPPER)))
    {
        TT.refresh(tte);
        ss->currentMove = ttMove; // Can be MOVE_NONE

        if (    ttValue >= beta
            &&  ttMove
            && !pos.capture_or_promotion(ttMove)
            &&  ttMove != ss->killers[0])
        {
            ss->killers[1] = ss->killers[0];
            ss->killers[0] = ttMove;
        }
        return ttValue;
    }

    // Step 5. Evaluate the position statically and update parent's gain statistics
		//Step5�͂��̋ǖʂ̐Î~�]���l���g�����X�|�W�V�����e�[�u���̃G���g���[�Ɣ�r���Č��߂�
		//����ɂ��̋ǖʂ̐e��Gain���X�V����iGain�͂Ȃ�Ȃ̂��킩���Ă��Ȃ��j
		/*
		���肪�����Ă���Ȃ�moves_lopp���x���ɂƂ�ŒT�����n�߂�
		��������moves_lopp���x���܂ł͎}����̏����Ȃ̂ŉ��肪�������Ă���ꍇ�͖��Ӗ�
		*/
		if (inCheck)
    {
        ss->staticEval = eval = VALUE_NONE;
        goto moves_loop;
    }
		/*
		���肪�������Ă��Ȃ��Ē�Վ肪����A���̕]���l��VALUE_NONE�Ȃ猻�ǖʂ̕]���l��
		eval��ss->staticEval�ɗ^����B
		*/
		else if (tte)
    {
        // Never assume anything on values stored in TT
        if ((ss->staticEval = eval = tte->eval_value()) == VALUE_NONE)
            eval = ss->staticEval = evaluate(pos);

        // Can ttValue be used as a better position evaluation?
				/*
				�g�����X�|�W�V�����e�[�u���ɓo�^���Ă������G���g���[�̕]���l�����ǖʂ̕]���l���傫���ăg�����X�|�W�V�����e�[�u����
				�o�^���ꂽ����bound�l��BOUND_LOWER(�����l�j�Ȃ�g�����X�|�W�V�����e�[�u���̕]���l��M�p����

				�g�����X�|�W�V�����e�[�u���ɓo�^���Ă������G���g���[�̕]���l�����ǖʂ̕]���l��菬�����g�����X�|�W�V�����e�[�u����
				�o�^���ꂽ����bound�l��BOUND_UPPER(����l�j�Ȃ�g�����X�|�W�V�����e�[�u���̕]���l���̗p����
				*/
        if (ttValue != VALUE_NONE)
            if (tte->bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER))
                eval = ttValue;
    }
    else
    {
				/*
				���ǖʂ��g�����X�|�W�V�����e�[�u���̃G���g���[�ɂȂ������ꍇ�]���l�͕]���֐����Ă�Őݒ肷��
				�Ɠ����Ƀg�����X�|�W�V�����e�[�u���Ɍ��ǖʂ�o�^���Ă����A���̎��]���l��VALUE_NONE�ɐݒ肵�Ă���̂�
				���̋ǖʂ̒T�����܂��ς�ł��Ȃ��i�Î~�]���l��ss->staticEval�ŗ^���Ă��邱�̐Î~�]���l�����o���֐���tte->eval_value()�j
				*/
        eval = ss->staticEval = evaluate(pos);
        TT.store(posKey, VALUE_NONE, BOUND_NONE, DEPTH_NONE, MOVE_NONE, ss->staticEval);
    }
		/*
		���O�̎肪�ߊl��łȂ��i��̎w����p�^�[�����m�[�}���ł��邱�Ƃ��`�G�b�N���Ă���̂Ȃ炱�̃`�G�b�N�͂���Ȃ��̂ł́j
		���ǖʂ̐Î~�]����VALUE_NONE�łȂ�
		�ЂƂO�̎�̐Î~�]���l��VALUE_NONE�łȂ�
		��O�̎��move�ɑ�����Ă�������ɂ��ꂪMOVE_NULL�łȂ����Ɓinull_move�łȂ��j
		���O�̎肪�ߊl�ł��Ȃ����̈ړ��ł������Ƃ�

		Gains�̃A�b�v�f�[�g���s���A����̋�킪to���W�Ɉړ����邱�Ƃɂ���Đ������]���l���ȑO�̕]���l��荂����΂��̕]���l��update����
		*/
    if (   !pos.captured_piece_type()
        &&  ss->staticEval != VALUE_NONE
        && (ss-1)->staticEval != VALUE_NONE
        && (move = (ss-1)->currentMove) != MOVE_NULL
        &&  type_of(move) == NORMAL)
    {
        Square to = to_sq(move);
        Gains.update(pos.piece_on(to), to, -(ss-1)->staticEval - ss->staticEval);
    }

    // Step 6. Razoring (skipped when in check)
		/*
		Razoring�}�������
		PvNode�łȂ�����
		�c��[����4*ONE_PLY(=2)�������Ă��邱�Ɓi�����������[�ɋ߂��m�[�h���v���t�����e�B�A�m�[�h�ƌĂ�ł���H�j
		���݂̕]���l�Ƀ}�[�W���irazor_margin=512+depth*16 �c��[�������Ȃ��Ȃ��Ă����܂薖�[�ɂȂ��Ă����قǎ}����̏����͊ɂ��Ȃ�j
		�g�����X�|�W�V�����e�[�u���̎肪�Ȃ�
		�x�[�^�l��VALUE_MATE_IN_MAX_PLY(=29900)��菬����
		��ԑ���PAWN�����ƈ����QUEEN�ɂȂ�Ȃ�

		�x�[�^�l���}�[�W���l�irazor_margin�j����������qsearch�֐����Ăяo���Ă���i�����������āj
		�܂�{���ł���΂��Ɛ���T���؂�W�J����qsearch�֐����Ăяo���Ȃ��Ă͂Ȃ�Ȃ���
		���݂̕]���l���x�[�^�l��藣��Ă���Ώȗ����ėǂ��Ƃ����}�؂�H
		https://chessprogramming.wikispaces.com/Razoring
		*/
		if (!PvNode
        &&  depth < 4 * ONE_PLY
        &&  eval + razor_margin(depth) < beta
        &&  ttMove == MOVE_NONE
        &&  abs(beta) < VALUE_MATE_IN_MAX_PLY
        && !pos.pawn_on_7th(pos.side_to_move()))
    {
        Value rbeta = beta - razor_margin(depth);
        Value v = qsearch<NonPV, false>(pos, ss, rbeta-1, rbeta, DEPTH_ZERO);
        if (v < rbeta)
            // Logically we should return (v + razor_margin(depth)), but
            // surprisingly this did slightly weaker in tests.
            return v;
    }

    // Step 7. Futility pruning: child node (skipped when in check)
		/*
		Futility Pruning �́C�`�F�X�ōL���p�����Ă���
		�}�����@�ł���D�{�����[�ɂ����Ĕ��肳��郿��
		�@�̎}��������̔�������̐e�m�[�h (frontier node)
		�ŉ��̒l��p���čs�����Ƃɂ��C�s�v�ȐÎ~�T��
		�m�[�h�̓W�J�ƐÓI�]���֐��̌Ăяo�����팸����D

		���Ȃ킿�C�e�m�[�h P �ɂ�����]���l�ɁC�w���� m
		�ɑ΂��ĉςȃ}�[�W���l Vdiff(m) �������C�Ȃ���
		�̍ŏ��l�ɖ����Ȃ��ꍇ�� p �ȉ����}���肷�邱�Ƃ�
		�\�ƂȂ�D�œK�� Vdiff(m) �̒l�͕]���֐��ɂ��
		�ĈقȂ�C�������قǎ}���肪�L���ɓ����D
		http://www-als.ics.nitech.ac.jp/paper/H18-B/kanai.pdf

		eval�͌��ǖʂ̕]���l������futility_margin�֐����Ԃ��}�[�W�����������l
		���\�z�]���l�ł��̗\�z�]���l��beta�l�𒴂���̂ŃJ�b�g����

		PvNode�łȂ�����
		���̎}����͎c��[����2*7 = 14 ��菬�������Ɓi���[�ɋ߂����Ɓj�������̂ЂƂ�
		���݂̕]���l�i�����܂ŐÎ~�]���l�A�T����̕]���l�ł͂Ȃ��j��futility_margin�l�������Ă��x�[�^�l��荂���[���Ȃ�x�[�^�J�b�g���Ă�OK
		*/
		if (!PvNode
        && !ss->skipNullMove
        &&  depth < 7 * ONE_PLY
        &&  eval - futility_margin(depth) >= beta
        &&  abs(beta) < VALUE_MATE_IN_MAX_PLY
        &&  abs(eval) < VALUE_KNOWN_WIN
        &&  pos.non_pawn_material(pos.side_to_move()))
        return eval - futility_margin(depth);

    // Step 8. Null move search with verification search (is omitted in PV nodes)
		/*
		�k�����[�u�i�}����j
		�k�����[�u�̏����F
		PvNode�łȂ�����
		�O�̎肪null_Move�łȂ����Ɓi�Q��null_move�ł͈Ӗ����Ȃ��j
		�̂���T���[����2*ONE_PLY��葽�����ƁA�܂薖�[�ǖʁi�t�����g�m�[�h�j�ȊO�Ńk�����[�u�n�j
		non_pawn_material��PAWN�ȊO�̋�]���l�̍��v��Ԃ��[���܂��ɒ[�ɂ����Ȃ����ǖʂł͂Ȃ����Ɓ[������Ȃ��ǖʂ�null_Move����Ƌɒ[�ȕ]���ɂȂ�
		*/
		if (!PvNode
        && !ss->skipNullMove
        &&  depth >= 2 * ONE_PLY
        &&  eval >= beta
        &&  abs(beta) < VALUE_MATE_IN_MAX_PLY
        &&  pos.non_pawn_material(pos.side_to_move()))
    {
        ss->currentMove = MOVE_NULL;

        // Null move dynamic reduction based on depth
				/*
				Null�@Move�̒T���[�x�����߂�
				*/
				Depth R = 3 * ONE_PLY + depth / 4;

        // Null move dynamic reduction based on value
				/*
				���݂̐Î~�]���l����Pawn�P���̋�]���l�������Ă��x�[�^�[�l���\���傫���Ȃ�T���[����ONE_PLY���₷
				�󂢒T������null_move���e������betacut���Ă��܂��̂ŐT�d�ɂ�����i�[������H
				*/
        if (eval - PawnValueMg > beta)
            R += ONE_PLY;

				/*
				�p�X�̎�����s�i�ǖʂ͍X�V���Ȃ��j
				*/
				pos.do_null_move(st);
				/*
				�p�X�̎�̎��̓p�X���Ȃ�
				*/
				(ss + 1)->skipNullMove = true;
				/*
				�k�����[�u�̐[�x�����݂̎c��̐[�x���[���Ȃ�qsearch�֐��i���[�T���j�����łȂ����search�֐��i��ʒT���j��
				�T������
				���ݎ��(null move)->������(�ʏ�move)->���ݎ��(null move)

				*/
				nullValue = depth - R < ONE_PLY ? -qsearch<NonPV, false>(pos, ss + 1, -beta, -alpha, DEPTH_ZERO)
                                      : - search<NonPV>(pos, ss+1, -beta, -alpha, depth-R, !cutNode);
				/*
				skipNullMove�����Ƃɖ߂�
				null move�����ɖ߂�
				*/
				(ss + 1)->skipNullMove = false;
        pos.undo_null_move();
				/*
				����p�X���Ă����̕]���l��beta���傫���Ȃ�ʏ�Ɏ���w�����
				beta Cut���N�����Ɛ��������
				*/
				if (nullValue >= beta)
        {
            // Do not return unproven mate scores
            if (nullValue >= VALUE_MATE_IN_MAX_PLY)
                nullValue = beta;
						/*
						�c��[����12*ONE_PLAY��菬�����Ȃ牓���Ȃ��}�؂�i���[�ǖʂɋ߂��Ȃ�j
						*/
						if (depth < 12 * ONE_PLY)
                return nullValue;

            // Do verification search at high depths
						/*
						�c��[����12*ONE_PLY��荂���ʒu��null_move�ɐ��������ꍇ
						�ēx�T�������Ă���
						*/
						ss->skipNullMove = true;
            Value v = search<NonPV>(pos, ss, alpha, beta, depth-R, false);
            ss->skipNullMove = false;
						/*
						������ς��čĒT������,����ł�beta�l�𒴂���悤�ł���Ή����Ȃ�Null Move Cut
						*/
						if (v >= beta)
                return nullValue;
        }
        else
        {
            // The null move failed low, which means that we may be faced with
            // some kind of threat. If the previous move was reduced, check if
            // the move that refuted the null move was somehow connected to the
            // move which was reduced. If a connection is found, return a fail
            // low score (which will cause the reduced move to fail high in the
            // parent node, which will trigger a re-search with full depth).
						/*
						null_move�Ɏ��s�����ꍇ
						(ss+1)->current�ɂ͂�����̗D�ʂ��Ђ�����Ԃ���i�G���́j�������Ă���͂��Ȃ̂�
						���̎��threatMove�i���Ђ̎�j�ɓ���Ă���
						*/
            threatMove = (ss+1)->currentMove;

            if (   depth < 5 * ONE_PLY
                && (ss-1)->reduction
                && threatMove != MOVE_NONE
                && allows(pos, (ss-1)->currentMove, threatMove))
                return alpha;
        }
    }

    // Step 9. ProbCut (skipped when in check)
    // If we have a very good capture (i.e. SEE > seeValues[captured_piece_type])
    // and a reduced search returns a value much above beta, we can (almost) safely
    // prune the previous move.
		/*
		ProbCut �Ƃ̓I�Z���v���O����Logistello �̊J����M.Buro ���Ă̑O�����}����@�ŁA���W�ł�Multi-ProbCut �Ƃ�����@������B

		��{�A�C�f�A�́A�󂢒T���͐[���T���̋ߎ��ɂȂ�Ƃ������ƂŁA�[���T��������O�ɐ󂢒T�����s���A���ʂ��T��������
		�O�ꂽ��J�b�g���Ă��ǂ��񂶂�Ȃ��H�Ƃ�����@�B

		�c��[���������d �ɂȂ�����A�[��d' �̒T�����s��
		�]���l�Ƀ}�[�W��m �����A�T��������O��Ă��Ȃ����`�F�b�N
		�O�ꂽ=>�J�b�g
		�O��Ȃ�=>�[��d �̒T�����s��
		�Ȃ��A������O�ꂽ�O��Ȃ��̃`�F�b�N�́Anull-window search �����������B

		�ׂ������Ƃ��l�����ɁA���������ȃR�[�h������(�Ȃ��AD_probcut ��ProbCut
		���s���Ƃ��炩���ߌ��߂Ă������[���ŁA��ł���d, D_shallow �͐󂢒T���̐[���ŁA��ł���d')�B
		http://d.hatena.ne.jp/tawake/20060710/1152520755 ���p�ӏ�
		�J�b�g�Ƃ����̂�
		if (value >= rbeta)
		return value;
		�̕������Ɣ��f�����
		*/
		if (!PvNode
        &&  depth >= 5 * ONE_PLY
        && !ss->skipNullMove
        &&  abs(beta) < VALUE_MATE_IN_MAX_PLY)
    {
        Value rbeta = beta + 200;
        Depth rdepth = depth - ONE_PLY - 3 * ONE_PLY;

        assert(rdepth >= ONE_PLY);
        assert((ss-1)->currentMove != MOVE_NONE);
        assert((ss-1)->currentMove != MOVE_NULL);
				/*
				pos.captured_piece_type()�͂Ƃ������ido_move�֐���st->capturedType�ɓo�^�����)
				*/
				MovePicker mp(pos, ttMove, History, pos.captured_piece_type());
        CheckInfo ci(pos);

        while ((move = mp.next_move<false>()) != MOVE_NONE)
            if (pos.legal(move, ci.pinned))
            {
                ss->currentMove = move;
                pos.do_move(move, st, ci, pos.gives_check(move, ci));
                value = -search<NonPV>(pos, ss+1, -rbeta, -rbeta+1, rdepth, !cutNode);
                pos.undo_move(move);
                if (value >= rbeta)
                    return value;
            }
    }

    // Step 10. Internal iterative deepening (skipped when in check)
		/*
		�p�r�s��
		*/
		if (depth >= (PvNode ? 5 * ONE_PLY : 8 * ONE_PLY)
        && ttMove == MOVE_NONE
        && (PvNode || ss->staticEval + Value(256) >= beta))
    {
        Depth d = depth - 2 * ONE_PLY - (PvNode ? DEPTH_ZERO : depth / 4);

        ss->skipNullMove = true;
        search<PvNode ? PV : NonPV>(pos, ss, alpha, beta, d, true);
        ss->skipNullMove = false;

        tte = TT.probe(posKey);
        ttMove = tte ? tte->move() : MOVE_NONE;
    }

moves_loop: // When in check and at SpNode search starts from here
		/*
		countermoves�͂����ŏ���������Ă���
		*/
		Square prevMoveSq = to_sq((ss - 1)->currentMove);
    Move countermoves[] = { Countermoves[pos.piece_on(prevMoveSq)][prevMoveSq].first,
                            Countermoves[pos.piece_on(prevMoveSq)][prevMoveSq].second };
		/*
		���胊�X�g�����A��ʒT���Ŏg�p�����w����I�[�_�����O�B
		*/
		MovePicker mp(pos, ttMove, depth, History, countermoves, ss);
    CheckInfo ci(pos);
    value = bestValue; // Workaround a bogus 'uninitialized' warning under gcc
		/*
		�ǖʂ�2��O���D�ʂɂȂ��Ă��邩���f�i�Î~�]���l�Ŕ��f�j���ėǂ��Ȃ��Ă���Ȃ�true
		*/
    improving =   ss->staticEval >= (ss-2)->staticEval
               || ss->staticEval == VALUE_NONE
               ||(ss-2)->staticEval == VALUE_NONE;
		/*
		Singular Extension�Ƃ͂��̃m�[�h�̕]���l���Z��m�[�h�̕]���l���傫���ꍇ�n����
		���ʂȂǂ��^����̂ł��̃m�[�h�̒T�������������@
		�T���������L���ɂȂ������RootNode�ł͂Ȃ����Ɓ@�����@
		SpNode�ł͂Ȃ����ƁiSpNode�͒T������̂��ƁH�j�@����
		depth��8*ONE_PLY���傫�����Ɓi�܂�RootNode�ɋ߂��A���[�߂��ł͂Ȃ����Ɓ@����
		!excludedMove�͑��̌Z���ɔ�׈ُ�ɕ]���l�������n�������ʂ��^����悤�Ȋ댯�Ȏ�łȂ����Ɓ@����
		�g�����X�|�W�V�����e�[�u���̕]���l�������l�@����
		�g�����X�|�W�V�����e�[�u���̎w����̐[�x�����݂̐[�x���3*ONE_PLAY���������̂�肨����������

		�Z��̃m�[�h�̕]���l�Ƃ��S�R�o�Ă��Ȃ��͉̂���
		*/
		singularExtensionNode = !RootNode
                           && !SpNode
                           &&  depth >= 8 * ONE_PLY
                           &&  ttMove != MOVE_NONE
                           && !excludedMove  // Recursive(�ċA�I) singular search is not allowed(�����ꂽ)->�ċA�I�ȃV���M���[�g���͋�����Ȃ��H
                           && (tte->bound() & BOUND_LOWER)
                           &&  tte->depth() >= depth - 3 * ONE_PLY;

    // Step 11. Loop through moves
    // Loop through all pseudo-legal moves until no moves remain or a beta cutoff occurs
		/*
		�������炪���C���̒T��
		*/
		while ((move = mp.next_move<SpNode>()) != MOVE_NONE)
    {
      assert(is_ok(move));

			/*
			�p�r�s��
			excludedMove��excluded�͎Ւf����A���ۂ���Ƃ����Ӗ�
			excludedMove�͑��̌Z�����]���l�������n�������ʂ��^�����Ȃ̂ł��̂悤�Ȏ�̓p�X���Ď��̌Z����
			*/
			if (move == excludedMove)
          continue;

      // At root obey the "searchmoves" option and skip moves not listed in Root
      // Move List, as a consequence any illegal move is also skipped. In MultiPV
      // mode we also skip PV moves which have been already searched.
			/*
			RootNode���[�h��next_move�֐���RootMoves�ɂȂ���������Ă����炻��̓p�X����
			RootMoves�ɂ���肵���ǂ܂Ȃ�
			*/
			if (RootNode && !std::count(RootMoves.begin() + PVIdx, RootMoves.end(), move))
          continue;
			/*
			RootNode�̂Ƃ���SpNode��false
			SpNode�͒T������̂��ƁH
			*/
			if (SpNode)
      {
          // Shared counter cannot be decremented later if move turns out to be illegal
          if (!pos.legal(move, ci.pinned))
              continue;

          moveCount = ++splitPoint->moveCount;
          splitPoint->mutex.unlock();
      }
      else
          ++moveCount;
			/*
			RootNode���[�h��p
			RootNode�ő�1��ڂ̂Ƃ�Signals.firstRootMove��true�ɂ���
			�p�r�s��
			*/
			if (RootNode)
      {
          Signals.firstRootMove = (moveCount == 1);

          if (thisThread == Threads.main() && Time::now() - SearchTime > 3000)
              sync_cout << "info depth " << depth / ONE_PLY
                        << " currmove " << move_to_uci(move, pos.is_chess960())
                        << " currmovenumber " << moveCount + PVIdx << sync_endl;
      }

      ext = DEPTH_ZERO;
      captureOrPromotion = pos.capture_or_promotion(move);
			/*
			�w����p�^�[�����m�[�}���œGKING�ւ̗������ז����Ă����Ȃ��ꍇ
			�ړ���ɓGKING�ɗ��������������bitboard��givesCheck�ɗ^����givesCheck��bool�^�Ȃ̂�bitboard�������true�ɂȂ�A�������Ȃ��Ƃ������Ƃ�����
			�����łȂ���� gives_check�֐����Ăщ��肪�Ȃ������d�Ƀ`�G�b�N����
			*/
			givesCheck = pos.gives_check(move, ci);
			/*
			dangerous���̂́u�댯�ȁv�ƌ����Ӗ�
			���肪�\�Ȏ�@OR�@�w����p�^�[����NORMAL�ȊO(PROMOTION,ENPASSANT,CASTLING)
			PAWN��RANK4�ȏ�̈ʒu�ɂ���.(RANK4��WHITE������݂��ʒu�ABLACK����݂��RANK5�ȉ��j
			*/
			dangerous = givesCheck
                 || pos.passed_pawn_push(move)
                 || type_of(move) == CASTLE;

      // Step 12. Extend checks
			/*
			���肪�\�ȏ�Ԃɂ���(givesCheck=true),�Î~�T�����Ă��]���l��0�ȏ�ł���
			���Ƃ�������ONE_PLY�����T����������
			*/
			if (givesCheck && pos.see_sign(move) >= 0)
          ext = ONE_PLY;

      // Singular extension search. If all moves but one fail low on a search of
      // (alpha-s, beta-s), and just one fails high on (alpha, beta), then that move
      // is singular and should be extended. To verify this we do a reduced search
      // on all the other moves but the ttMove, if result is lower than ttValue minus
      // a margin then we extend ttMove.
			/*
			Singular extension search�Ƃ�
			���̃m�[�h�̕]���l���Z��m�[�h�̕]���l���傫���ꍇ�n����
			���ʂȂǂ��^����̂ł��̃m�[�h�̒T�������������@

			������if���ŕʒT�������[�x�̔����ōs���Ă���
			���̕����ł̕]���l�ɂ����ext�ϐ��i�[�x�����ϐ��j
			��ONE_PLY�����������Ă��镔���ł͂Ȃ���
			*/
			if (singularExtensionNode
          &&  move == ttMove
          && !ext
          &&  pos.legal(move, ci.pinned)
          &&  abs(ttValue) < VALUE_KNOWN_WIN)
      {
          assert(ttValue != VALUE_NONE);

          Value rBeta = ttValue - int(depth);
					/*
					�n�������ʂ��^������excludedMove�Ƃ��ēo�^���Ă����A���̎肪�����������̓p�X�ł���B
					*/
          ss->excludedMove = move;
          ss->skipNullMove = true;
          value = search<NonPV>(pos, ss, rBeta - 1, rBeta, depth / 2, cutNode);
          ss->skipNullMove = false;
          ss->excludedMove = MOVE_NONE;

          if (value < rBeta)
              ext = ONE_PLY;
      }

      // Update current move (this must be done after singular extension search)
			/*
			�ŏI������Singular extension�̒T�������������Ă���Ƃ���
			�����I��ONE_PLY�����Ă���͉̂���
			*/
			newDepth = depth - ONE_PLY + ext;

      // Step 13. Pruning at shallow depth (exclude PV nodes)
			/*
			�}����
			P��Node�ł͂Ȃ��@����
			���肪�������Ă��Ȃ��@�����i���肪�������Ă���悤�ȃm�[�h���}���肵�Ă͊댯�j
			���肪�\�Ȏ蓙�d�v�Ȏ�ł͂Ȃ��@����
			bestValue > VALUE_MATED_IN_MAX_PLY(= �}�C�i�X31880)�ɒ[�ɕ]���l�������킯�ł͂Ȃ����悭���Ȃ��H
			*/
			if (!PvNode
          && !captureOrPromotion
          && !inCheck
          && !dangerous
          &&  bestValue > VALUE_MATED_IN_MAX_PLY)
      {
          // Move count based pruning
					/*
					�c��[����16��菬�����āA�萔��FutilityMoveCounts[improving][depth]��葽��
					FutilityMoveCounts[improving][depth]�z���depth���[���Ȃ��Ă����Ɨݏ�֐��̂悤�ɑ����Ă����܂�depth�������Ă�����
					�}�����臒l���オ��Ƃ�������
					���Ў肪�Ȃ��i��ԑ��̗ǂ��]����C�ɂЂ�����Ԃ��悤�Ȏ肪���݂���Ƃ��o�^����� ���Ў��move���w���ꂽ��Ɏw������j

					�܂�A�萔���[�x���[���ǂ񂾂��債����ł͂Ȃ������Ȃ̂Ŏ}���肷��
					*/
					if (depth < 16 * ONE_PLY
              && moveCount >= FutilityMoveCounts[improving][depth]
              && (!threatMove || !refutes(pos, move, threatMove)))
          {
              if (SpNode)
                  splitPoint->mutex.lock();
							/*
							���̎���p�X���Ď��̌Z��ɍs��
							*/
              continue;
          }
					/*
					reduction��moveCount(1�`63�܂ł̐������j���傫���Ȃ�قǑ傫�Ȑ���Ԃ�(0�`18���x�j
					�܂�depth�i�P�`�U�R�܂ł̐����Ƃ�j���傫���قǑ傫�Ȑ���Ԃ�
					�܂肽������ǂ߂Ύc��[�������炵�Ă����Adepth���傫���Ƃ��i�܂��󂢓ǂ݂̎��j�Ƃ���
					�������񌸂炵�Ă����B
					Move count based pruning�͉������̎}����A����reduction�͏c�����̎}��������Ă����
					*/
					predictedDepth = newDepth - reduction<PvNode>(improving, depth, moveCount);

          // Futility pruning: parent node
					/*
					https://chessprogramming.wikispaces.com/Futility+Pruning
					 Futility�}����Ƃ͂��������ςȕ]���l�Ƀ}�[�W�������������̃}�[�W����alpha-beta�͈̔͂ɓ����Ă��Ȃ�������
					 �}���肷���@
					 ���������ςȕ]���l��ss->staticEval���̋ǖʂ̋�]���l�����̕]���l
					 futility_margin�֐����}�[�W�� futility_margin�֐�=�n���ꂽdepth��100�������Ă��邾��
					 �������ē���ꂽfutilityValue��alpha�l�������Ȃ炱�̎�̓p�X���Ď��̌Z��ɂ���
					 Gains�N���X�͔z��ł͂Ȃ��悤�Ɍ����邪������table[][]�z���ێ����Ă���[]���Z�q�I�[�o�[���[�h��
					 ����table�z��̒l��Ԃ�
					 Gains�͋�킲�Ƃ́A�ړ��������W���Ƃ̒��O�]���l�Ƃ̍����]���l�ł�
					 Futility�}����̃}�[�W���ɒǉ����Ă���,Gains�̗��p�́A��������
					 �}�[�W����傫������Ύ}���肵�₷���A����������Ύ}���肵�ɂ���
					*/
					if (predictedDepth < 7 * ONE_PLY)
          {
              futilityValue = ss->staticEval + futility_margin(predictedDepth)
                            + Value(128) + Gains[pos.moved_piece(move)][to_sq(move)];

              if (futilityValue <= alpha)
              {
                  bestValue = std::max(bestValue, futilityValue);

                  if (SpNode)
                  {
                      splitPoint->mutex.lock();
                      if (bestValue > splitPoint->bestValue)
                          splitPoint->bestValue = bestValue;
                  }
                  continue;
              }
          }

          // Prune moves with negative SEE at low depths
					/*
					�c��[�����S�Ɩ��[�ǖʂŁA�Î~�T���������ʎ�荇���������Ă���Ȃ炱�̎��
					������߂Ď��̌Z��ɂ����}����
					*/
					if (predictedDepth < 4 * ONE_PLY && pos.see_sign(move) < 0)
          {
              if (SpNode)
                  splitPoint->mutex.lock();

              continue;
          }

      }
			/*
			�����܂Ŏ}����
			*/

      // Check for legality only before to do the move

			/*
			���@��ł��邩�̃`�G�b�N�A���@��łȂ���΂��̃m�[�h�̓p�X
			���̂����Ń`�G�b�N�Ȃ̂������Ƒ����ł��Ȃ��̂���
			*/
			if (!RootNode && !SpNode && !pos.legal(move, ci.pinned))
      {
          moveCount--;
          continue;
      }
			/*
			PvNode�m�[�h�ł���P��ڂȂ�pvMove��true�ɂ��Ă���
			��P��ڂȂ̂ŕK���ʂ��Ăق������[�g�ɐݒ肵�Ă���
			*/
      pvMove = PvNode && moveCount == 1;
			/*
			ss��1���̋ǖʂ���O�̎�̎Q�Ƃɂ悭������
			*/
      ss->currentMove = move;
			/*
			captureOrPromotion�@=�@�ߊl�������͐���ł͂Ȃ��@
			quietCount��search�֐��̖`����0�Z�b�g����Ă��Ă��̕��������ŃC���N�������g����Ă���
			�܂�ߊl���Ȃ��A����Ȃ������₩�Ȏ���U�S��܂�quietsSearched�z��ɓo�^���Ă�����
			quietsSearched�z���search�֐��̋Ǐ��ϐ��A�l���X�V���Ă���̂͂�������
			���History�z��i�ړ�����]���j�̓��_��������̂Ɏg�p�����
			*/
      if (!SpNode && !captureOrPromotion && quietCount < 64)
          quietsSearched[quietCount++] = move;

      // Step 14. Make the move
			/*
			�����ŋǖʍX�V
			*/
			pos.do_move(move, st, ci, givesCheck);

      // Step 15. Reduced depth search (LMR). If the move fails high will be
      // re-searched at full depth.
			/*
			https://chessprogramming.wikispaces.com/Late+Move+Reductions
			LRM�͒T���[����Z�k���邱�ƂŎ}������s���B
			LRM�͑S�Ẵm�[�h�Ŏ��{����̂ł͂Ȃ�
				- �c��[�����R�ȏ�@
				- pvMove�m�[�h�Ȃ�����
				- ���̋ǖʂł̎w���肪�u���\���瓾����ł͂Ȃ�����
				- �L���[��ł͂Ȃ�����
			�܂肻��قǏd�v�����Ȏ�ł͂Ȃ�����
			����LRM�}����łȂ��Ƃ���doFullDepthSearch�i�t��Depth������j=true�ƂȂ�B�A��PvNode�ő���߂̂Ƃ���
			false�ɂȂ�B
			*/
			if (depth >= 3 * ONE_PLY
          && !pvMove
          && !captureOrPromotion
          &&  move != ttMove
          &&  move != ss->killers[0]
          &&  move != ss->killers[1])
      {
					/*
					improving,depth,moveCount�p�����[�^���w�肵��Reductions�z��̒l������Ă���
					Reductions�z��̌`��Reductions.xlsx�ɃO���t�����Ă���
					depth,moveCount��������ƒi�K�I��ss->reduction�ɕԂ����l���傫���Ȃ�
					�܂�󂢒T���i�K�ł͂���قǂ���Ȃ��AmoveCount�����Ȃ��ꍇ������Ȃɂ������񊠂�Ȃ�
					*/
          ss->reduction = reduction<PvNode>(improving, depth, moveCount);
					/*
						- PvNode�m�[�h�̎��ł͂Ȃ�
						- cutNode=id_loop�֐����珉���lfalse�œn�����

						cutNode�ϐ����g�p���Ă���̂�search�֐��̒��łP����������
						LRM�}�؂�̂Ƃ����PvNode�łȂ����Ƃ�cutNode��true�ł���ΒT���[����ONE_PLY�ǉ��ō팸�ł���B
						�܂�cutNode�Ƃ�LRM����苭�����邽�߂̃t���O
					*/
          if (!PvNode && cutNode)
              ss->reduction += ONE_PLY;

          else if (History[pos.piece_on(to_sq(move))][to_sq(move)] < 0)
              ss->reduction += ONE_PLY / 2;

          if (move == countermoves[0] || move == countermoves[1])
              ss->reduction = std::max(DEPTH_ZERO, ss->reduction - ONE_PLY);

          Depth d = std::max(newDepth - ss->reduction, ONE_PLY);
          if (SpNode)
              alpha = splitPoint->alpha;

          value = -search<NonPV>(pos, ss+1, -(alpha+1), -alpha, d, true);

          doFullDepthSearch = (value > alpha && ss->reduction != DEPTH_ZERO);
          ss->reduction = DEPTH_ZERO;
      }
      else
          doFullDepthSearch = !pvMove;

      // Step 16. Full depth search, when LMR is skipped or fails high
			/*
			���������̊K�w�ɍ~��čs���Ƃ���
			qsearch�֐���search�֐�����I�����Ă���
			SpNode���T������AdoFullDepthSearch���T���[���̍팸�������ʏ�̐[���T�������鎞�̃t���O
			doFullDepthSearch��seach�֐��̎����ϐ���bool�^��step15�Őݒ肳��Ă���
			*/
			if (doFullDepthSearch)
      {
          if (SpNode)
              alpha = splitPoint->alpha;
					/*
					newDepth < ONE_PLY�����������
					givesCheck ? -qsearch<NonPV,  true>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO) : -qsearch<NonPV, false>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO)
					�����s����
					newDepth < ONE_PLY���������Ȃ����
					-search<NonPV, false>(pos, ss+1, -(alpha+1), -alpha, newDepth, !cutNode)
					�����s����B
					�܂�V�����ݒ肳�ꂽ�T���[����ONE_PLY(2)��菬�����ꍇ��qsearch�֐���
					ONE_PLY���傫���ꍇ��search�֐����Ă�
					*/
					value = newDepth < ONE_PLY ?
                          givesCheck ? -qsearch<NonPV,  true>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO)
                                     : -qsearch<NonPV, false>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO)
                                     : - search<NonPV>(pos, ss+1, -(alpha+1), -alpha, newDepth, !cutNode);
      }

      // Only for PV nodes do a full PV search on the first move or after a fail
      // high, in the latter case search only if value < beta, otherwise let the
      // parent node to fail low with value <= alpha and to try another move.
			/*
			newDepth < ONE_PLY����������΁i�܂肠�ƂP��Ŗ��[�ǖʂȂ�qsearch�֐��j
			givesCheck ? -qsearch<PV,  true>(pos, ss+1, -beta, -alpha, DEPTH_ZERO) : -qsearch<PV, false>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
			�����s��
			newDepth < ONE_PLY���������Ȃ���΁i�܂�܂����[�ǖʂł͂Ȃ��Ȃ�j
			-search<PV, false>(pos, ss+1, -beta, -alpha, newDepth, false)
			�����s����
			�܂�V�����ݒ肳�ꂽ�T���[����ONE_PLY(2)��菬�����ꍇ��qsearch�֐���
			ONE_PLY���傫���ꍇ��search�֐����Ă�
			��Ɠ����悤�ȍ\���ł��邪�A�Ⴄ�̂�NonPV��PV���̈Ⴂ���Ǝv��
			*/
			if (PvNode && (pvMove || (value > alpha && (RootNode || value < beta))))
          value = newDepth < ONE_PLY ?
                          givesCheck ? -qsearch<PV,  true>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
                                     : -qsearch<PV, false>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
                                     : - search<PV>(pos, ss+1, -beta, -alpha, newDepth, false);
      // Step 17. Undo move
			/*
			�����ŋǖʕ���
			*/
			pos.undo_move(move);

      assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

      // Step 18. Check for new best move
			/*
			�T�����򂵂Ă���Thread�Ȃ炱���ŋ��L�f�[�^����bestValue��alpha�l��������Ă���
			�܂��ڍוs��
			*/
			if (SpNode)
      {
          splitPoint->mutex.lock();
          bestValue = splitPoint->bestValue;
          alpha = splitPoint->alpha;
      }

      // Finished searching the move. If Signals.stop is true, the search
      // was aborted because the user interrupted the search or because we
      // ran out of time. In this case, the return value of the search cannot
      // be trusted, and we don't update the best move and/or PV.
			/*
			�T�����~�Ȃ�]���l�������ĕԂ�܂��@��������cutoff_occurred�֐����Ԃ��l��true�Ȃ�Ԃ�܂�
			cutoff_occurred�֐��̋@�\�͕s��
			�T���̒��~��UCI�C���^�[�t�F�C�X����̒T�����~�R�}���h��check_time�֐��Ŏ��Ԑ����Ɉ����|��������Ȃ�
			*/
			if (/*Signals.stop || �ꎞ�I�ɃR�����g�A�E�g*/thisThread->cutoff_occurred())
          return value; // To avoid returning VALUE_INFINITE
			/*
			�m�[�h��RootNode�ł������猻�݂̎w���肪root�ǖʂ̒��胊�X�g�ɂ���΂����rm�ɕۑ�����
			*/
      if (RootNode)
      {
          RootMove& rm = *std::find(RootMoves.begin(), RootMoves.end(), move);

          // PV move or new best move ?
					//pvMove�܂���alpha�l���X�V������
					if (pvMove || value > alpha)
          {
              rm.score = value;
              rm.extract_pv_from_tt(pos);

              // We record how often the best move has been changed in each
              // iteration. This information is used for time management: When
              // the best move changes frequently, we allocate some more time.
							/*
							�őP���肪�ύX�ɂȂ����񐔂��J�E���g���Ă������Ԑ���Ɏg�p����
							�i���܂�ɂ��ώG�ɍőP�肪�ς��悤�Ȃ�댯�ȋǖʂƂ��ĐT�d�ɒT������K�v������j
							BestMoveChanges��id_loop��0�ɏ���������Ă���
							*/
              if (!pvMove)
                  ++BestMoveChanges;
          }
          else
              // All other moves but the PV are set to the lowest value, this
              // is not a problem when sorting becuase sort is stable and move
              // position in the list is preserved, just the PV is pushed up.
							/*
							pvMove�łȂ�alpha�l�ł��Ȃ���͍Œ�̕]���l��^���Ă���
							*/
              rm.score = -VALUE_INFINITE;
      }

      if (value > bestValue)
      {
					/*
					SpNode�Ȃ�bestValue�𒴂����]���l�͋��L�f�[�^��bestValue���X�V���Ă���
					bestValue��alpha,beta,value�̊֌W��
					*/
          bestValue = SpNode ? splitPoint->bestValue = value : value;

          if (value > alpha)
          {
							/*
							value��alpha�l���X�V�����炱�̎w��������L�f�[�^�ɓo�^����Ɠ�����bestMove�ɂ��o�^
							*/
              bestMove = SpNode ? splitPoint->bestMove = move : move;
							/*
							- PvMove�ł���
							- beta�l�𒴂��Ȃ�
							- alpha-beta���̂Ȃ���alpha�l���X�V�����炱�̎w����̕]���l��alpha�l�ɂ���
							  �����ɋ��L�f�[�^��alhpa�l���X�V����
							*/
              if (PvNode && value < beta) // Update alpha! Always alpha < beta
                  alpha = SpNode ? splitPoint->alpha = value : value;
              else
              {
									/*
									�����ł͂Ȃ��Aalpha-beta���𒴂����ꍇ��beta cut���N��������
									while���[�v���o�Č�n�������Ă��̃m�[�h���łĐe�̋ǖʂɋA���Ă���
									*/
                  assert(value >= beta); // Fail high

                  if (SpNode)
                      splitPoint->cutoff = true;

                  break;
              }
          }
      }

      // Step 19. Check for splitting the search
			/*
			�܂��ŏ���MainThread�������ɂ���
			MainThread��PvNode�Ȃ̂�SpNode�ł͂Ȃ��@���@�����[���̐[����Threads.minimumSplitDepth(=8)���[�����Ɓi�󂢒T�����ƒT������̌��ʂ����Ȃ��H�j

			*/
			if (!SpNode
          &&  depth >= Threads.minimumSplitDepth
          &&  Threads.available_slave(thisThread)
          &&  thisThread->splitPointsSize < MAX_SPLITPOINTS_PER_THREAD)
      {
          assert(bestValue < beta);

          thisThread->split<FakeSplit>(pos, ss, alpha, beta, &bestValue, &bestMove,
                                       depth, threatMove, moveCount, &mp, NT, cutNode);
          if (bestValue >= beta)
              break;
      }
    }//���������C���T���̏I��
		/*
		1�̐e�̑S���̎q���̒T�����i�T��������܂߂āj�I�������炱���ɂ���
		SpNode�̃X���b�h�Ȃ炱���ɓ��B������bestValue��Ԃ���Thread::idle_loop�֐��ɕԂ�
		*/
		if (SpNode)
        return bestValue;

    // Step 20. Check for mate and stalemate
    // All legal moves have been searched and if there are no legal moves, it
    // must be mate or stalemate. Note that we can have a false positive in
    // case of Signals.stop or thread.cutoff_occurred() are set, but this is
    // harmless because return value is discarded anyhow in the parent nodes.
    // If we are in a singular extension search then return a fail low score.
    // A split node has at least one move, the one tried before to be splitted.
		/*
		moveCount��0�Ƃ������Ƃ͍��@�肪�Ȃ��������`�G�b�N���C�g�@���@�X�e�C�����C�g
		excludedMove��͒n�������ʂ��^�����ł��邪���Ɏ肪�Ȃ��Ȃ炻�̎��Ԃ��iexcludedMove��͑��̌Z�����]���������̂ŕ]���l��alpha�l���ȁj
		����Ȏ���Ȃ����肪�������Ă�����l�݂̕]���l��Ԃ��A������|���Ă��Ȃ��{���@�肪�Ȃ����X�e�����C�g�Ȃ̂ň�������
		*/
		if (!moveCount)
        return  excludedMove ? alpha
              : inCheck ? mated_in(ss->ply) : DrawValue[pos.side_to_move()];

    // If we have pruned all the moves without searching return a fail-low score
    if (bestValue == -VALUE_INFINITE)
        bestValue = alpha;
		/*
		�g�����X�|�W�V�����e�[�u���ɓo�^
		*/
		TT.store(posKey, value_to_tt(bestValue, ss->ply),
             bestValue >= beta  ? BOUND_LOWER :
             PvNode && bestMove ? BOUND_EXACT : BOUND_UPPER,
             depth, bestMove, ss->staticEval);

    // Quiet best move: update killers, history and countermoves
		if (bestValue >= beta
        && !pos.capture_or_promotion(bestMove)
        && !inCheck)
    {
        if (ss->killers[0] != bestMove)
        {
            ss->killers[1] = ss->killers[0];
            ss->killers[0] = bestMove;
        }

        // Increase history value of the cut-off move and decrease all the other
        // played non-capture moves.
				/*
				bestmove�i��ԗǂ���j��History�z��i�ړ�����]���j�̕]��������
				�����ł͂Ȃ����₩�Ȏ�i�ߊl���Ȃ��A����Ȃ����}�Ȏ�j��quietsSearched�z���
				�o�^���Ă���̂ł��̈ړ�����]���������Ă���
				*/
        Value bonus = Value(int(depth) * int(depth));
        History.update(pos.moved_piece(bestMove), to_sq(bestMove), bonus);
        for (int i = 0; i < quietCount - 1; ++i)
        {
            Move m = quietsSearched[i];
            History.update(pos.moved_piece(m), to_sq(m), -bonus);
        }
				/*
				prevMoveSq�͒��O�̓G��̈ړ���̍��W�������Ă���
				*/
        if (is_ok((ss-1)->currentMove))
            Countermoves.update(pos.piece_on(prevMoveSq), prevMoveSq, bestMove);
    }

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);
		/*
		�Ԃ�
		*/
		return bestValue;
  }//������search�֐��̏I��


  // qsearch() is the quiescence search function, which is called by the main
  // search function when the remaining depth is zero (or, to be more precise,
  // less than ONE_PLY).
	/*
	���[�ǖʐ�p�T���֐��̂͂�
	search�֐��ɔ�ׂ�Ƃ����ԍs�������Ȃ�
	*/
	template <NodeType NT, bool InCheck>
  Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth) 
	{

    const bool PvNode = (NT == PV);

    assert(NT == PV || NT == NonPV);
    assert(InCheck == !!pos.checkers());
    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(depth <= DEPTH_ZERO);

    StateInfo st;
    const TTEntry* tte;
    Key posKey;
    Move ttMove, move, bestMove;
    Value bestValue, value, ttValue, futilityValue, futilityBase, oldAlpha;
    bool givesCheck, evasionPrunable;
    Depth ttDepth;

    // To flag BOUND_EXACT a node with eval above alpha and no available moves
		/*
		�T������Ȃ�alpha��oldalpha�ɑޔ��H
		*/
		if (PvNode)
        oldAlpha = alpha;
		/*
		bestMove,ss->currentMove��������
		*/
    ss->currentMove = bestMove = MOVE_NONE;
    ss->ply = (ss-1)->ply + 1;

    // Check for an instant draw or maximum ply reached
		/*
		�����������肵�Ĉ��������@OR �ő�[�x�܂ł����Ȃ�@�]���l��������ċA��
		*/
		if (pos.is_draw() || ss->ply > MAX_PLY)
        return DrawValue[pos.side_to_move()];

    // Decide whether or not to include checks, this fixes also the type of
    // TT entry depth that we are going to use. Note that in qsearch we use
    // only two types of depth in TT: DEPTH_QS_CHECKS or DEPTH_QS_NO_CHECKS.
		/*
		�p�r�s��
		*/
		ttDepth = InCheck || depth >= DEPTH_QS_CHECKS ? DEPTH_QS_CHECKS
                                                  : DEPTH_QS_NO_CHECKS;

    // Transposition table lookup
		/*
		�g�����X�|�W�V�����e�[�u���Ɏ肪���邩���ׂ�A�����ttMove�Ɏ�����Ă���
		*/
		posKey = pos.key();
    tte = TT.probe(posKey);
    ttMove = tte ? tte->move() : MOVE_NONE;
    ttValue = tte ? value_from_tt(tte->value(),ss->ply) : VALUE_NONE;
		/*
		�g�����X�|�W�V�����e�[�u���̎�̕]���l���^�l�Ȃ�g�����X�|�W�V�����e�[�u���̕]���l��Ԃ�
		*/
		if (tte
        && tte->depth() >= ttDepth
        && ttValue != VALUE_NONE // Only in case of TT access race
        && (           PvNode ?  tte->bound() == BOUND_EXACT
            : ttValue >= beta ? (tte->bound() &  BOUND_LOWER)
                              : (tte->bound() &  BOUND_UPPER)))
    {
        ss->currentMove = ttMove; // Can be MOVE_NONE
        return ttValue;
    }

    // Evaluate the position statically
		/*
		���̏�Ԃŉ��肪�������Ă���ꍇ�̓}�C�i�X32001��Ԃ�
		*/
		if (InCheck)
    {
        ss->staticEval = VALUE_NONE;
        bestValue = futilityBase = -VALUE_INFINITE;
    }
    else
    {
			/*
			���肪�������Ă��Ȃ��ꍇ
			*/
			if (tte)
        {
            // Never assume anything on values stored in TT
						/*
						�p�r�s��
						*/
						if ((ss->staticEval = bestValue = tte->eval_value()) == VALUE_NONE)
                ss->staticEval = bestValue = evaluate(pos);

            // Can ttValue be used as a better position evaluation?
            if (ttValue != VALUE_NONE)
                if (tte->bound() & (ttValue > bestValue ? BOUND_LOWER : BOUND_UPPER))
                    bestValue = ttValue;
        }
        else
            ss->staticEval = bestValue = evaluate(pos);

        // Stand pat. Return immediately if static value is at least beta
        if (bestValue >= beta)
        {
            if (!tte)
                TT.store(pos.key(), value_to_tt(bestValue, ss->ply), BOUND_LOWER,
                         DEPTH_NONE, MOVE_NONE, ss->staticEval);

            return bestValue;
        }

        if (PvNode && bestValue > alpha)
            alpha = bestValue;

        futilityBase = bestValue + Value(128);
    }

    // Initialize a MovePicker object for the current position, and prepare
    // to search the moves. Because the depth is <= 0 here, only captures,
    // queen promotions and checks (only if depth >= DEPTH_QS_CHECKS) will
    // be generated.
		/*
		�����Œ��胊�X�g��MovePickrer�ɂ��点��
		���̃R���X�g���N�^��qsearch��p�ł��B
		�Ō�̈�����sq�ɂ͂P��O�̓G�̋�ړ��������̍��W
		*/
    MovePicker mp(pos, ttMove, depth, History, to_sq((ss-1)->currentMove));
    CheckInfo ci(pos);

    // Loop through the moves until no moves remain or a beta cutoff occurs
    while ((move = mp.next_move<false>()) != MOVE_NONE)
    {
      assert(is_ok(move));

      givesCheck = pos.gives_check(move, ci);

      // Futility pruning
      if (   !PvNode
          && !InCheck
          && !givesCheck
          &&  move != ttMove
          &&  type_of(move) != PROMOTION
          &&  futilityBase > -VALUE_KNOWN_WIN
          && !pos.passed_pawn_push(move))
      {
          futilityValue =  futilityBase
                         + PieceValue[EG][pos.piece_on(to_sq(move))]
                         + (type_of(move) == ENPASSANT ? PawnValueEg : VALUE_ZERO);

          if (futilityValue < beta)
          {
              bestValue = std::max(bestValue, futilityValue);
              continue;
          }

          // Prune moves with negative or equal SEE and also moves with positive
          // SEE where capturing piece loses a tempo and SEE < beta - futilityBase.
          if (   futilityBase < beta
              && pos.see(move, beta - futilityBase) <= 0)
          {
              bestValue = std::max(bestValue, futilityBase);
              continue;
          }
      }

      // Detect non-capture evasions that are candidate to be pruned
      evasionPrunable =    InCheck
                       &&  bestValue > VALUE_MATED_IN_MAX_PLY
                       && !pos.capture(move)
                       && !pos.can_castle(pos.side_to_move());

      // Don't search moves with negative SEE values
      if (   !PvNode
          && (!InCheck || evasionPrunable)
          &&  move != ttMove
          &&  type_of(move) != PROMOTION
          &&  pos.see_sign(move) < 0)
          continue;

      // Check for legality only before to do the move
      if (!pos.legal(move, ci.pinned))
          continue;

      ss->currentMove = move;

      // Make and search the move
      pos.do_move(move, st, ci, givesCheck);
      value = givesCheck ? -qsearch<NT,  true>(pos, ss+1, -beta, -alpha, depth - ONE_PLY)
                         : -qsearch<NT, false>(pos, ss+1, -beta, -alpha, depth - ONE_PLY);
      pos.undo_move(move);

      assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

      // Check for new best move
      if (value > bestValue)
      {
          bestValue = value;

          if (value > alpha)
          {
              if (PvNode && value < beta) // Update alpha here! Always alpha < beta
              {
                  alpha = value;
                  bestMove = move;
              }
              else // Fail high
              {
                  TT.store(posKey, value_to_tt(value, ss->ply), BOUND_LOWER,
                           ttDepth, move, ss->staticEval);

                  return value;
              }
          }
       }
    }

    // All legal moves have been searched. A special case: If we're in check
    // and no legal moves were found, it is checkmate.
    if (InCheck && bestValue == -VALUE_INFINITE)
        return mated_in(ss->ply); // Plies to mate from the root

    TT.store(posKey, value_to_tt(bestValue, ss->ply),
             PvNode && bestValue > oldAlpha ? BOUND_EXACT : BOUND_UPPER,
             ttDepth, bestMove, ss->staticEval);

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
  }


  // value_to_tt() adjusts a mate score from "plies to mate from the root" to
  // "plies to mate from the current position". Non-mate scores are unchanged.
  // The function is called before storing a value to the transposition table.
	/*
	�p�r�s��
	*/
	Value value_to_tt(Value v, int ply) 
	{

    assert(v != VALUE_NONE);

    return  v >= VALUE_MATE_IN_MAX_PLY  ? v + ply
          : v <= VALUE_MATED_IN_MAX_PLY ? v - ply : v;
  }


  // value_from_tt() is the inverse of value_to_tt(): It adjusts a mate score
  // from the transposition table (where refers to the plies to mate/be mated
  // from current position) to "plies to mate/be mated from the root".
	/*
	�g�����X�|�W�V�����e�[�u��������o�����w����ɂ��Ă����]���l�ƁA�g�����X�|�W�V�����e�[�u�����Ăяo�����Ƃ���
	�T���[���������Ƃ��āA�ĕ]�������]���l��Ԃ�
	�]���l��VALUE_NONE�Ȃ�VALUE_NONE�̂܂܁AVALUE_MATE_IN_MAX_PLY�i29900�j���傫�����
	�]���l��茻�ݐ[�x�������AVALUE_MATED_IN_MAX_PLY(-29900)��菬������Ό��ݐ[�x��]���l�ɉ��Z����
	29900>v>-29900�Ȃ炻�̂܂ܕ]���l��Ԃ�
	������29900�̂悤�ȑ傫�Ȑ��l�Ɍ��ݐ[�x��\���悤�ȏ����Ȑ��������Z�A���Z���ĂȂɂ����ʂ�����̂��s��
	*/
	Value value_from_tt(Value v, int ply) 
	{

    return  v == VALUE_NONE             ? VALUE_NONE
          : v >= VALUE_MATE_IN_MAX_PLY  ? v - ply
          : v <= VALUE_MATED_IN_MAX_PLY ? v + ply : v;
  }


  // allows() tests whether the 'first' move at previous ply somehow makes the
  // 'second' move possible, for instance if the moving piece is the same in
  // both moves. Normally the second move is the threat (the best move returned
  // from a null search that fails low).

  bool allows(const Position& pos, Move first, Move second) 
	{

    assert(is_ok(first));
    assert(is_ok(second));
    assert(color_of(pos.piece_on(from_sq(second))) == ~pos.side_to_move());
    assert(type_of(first) == CASTLE || color_of(pos.piece_on(to_sq(first))) == ~pos.side_to_move());

    Square m1from = from_sq(first);
    Square m2from = from_sq(second);
    Square m1to = to_sq(first);
    Square m2to = to_sq(second);

    // The piece is the same or second's destination was vacated by the first move
    // We exclude the trivial case where a sliding piece does in two moves what
    // it could do in one move: eg. Ra1a2, Ra2a3.
    if (    m2to == m1from
        || (m1to == m2from && !aligned(m1from, m2from, m2to)))
        return true;

    // Second one moves through the square vacated by first one
    if (between_bb(m2from, m2to) & m1from)
      return true;

    // Second's destination is defended by the first move's piece
    Bitboard m1att = pos.attacks_from(pos.piece_on(m1to), m1to, pos.pieces() ^ m2from);
    if (m1att & m2to)
        return true;

    // Second move gives a discovered check through the first's checking piece
    if (m1att & pos.king_square(pos.side_to_move()))
    {
        assert(between_bb(m1to, pos.king_square(pos.side_to_move())) & m2from);
        return true;
    }

    return false;
  }


  // refutes() tests whether a 'first' move is able to defend against a 'second'
  // opponent's move. In this case will not be pruned. Normally the second move
  // is the threat (the best move returned from a null search that fails low).
	/*
	first�Ŏ������w�����second�Ŏ��������h��ł��邩���肷��
	����������Əڂ������ׂ�
	first�͍ŏ��Ɏw������ŁAsecond�͂��̌�Ɏw������ł��̊֐��̋@�\��seconnd�肪first���ɂƂ��ċ��Ђ�
	�Ȃ�ꍇfirst�肪���̋��Ў�̑Ή���ƂȂ邩���肷�����
	*/
  bool refutes(const Position& pos, Move first, Move second) 
	{

    assert(is_ok(first));
    assert(is_ok(second));

    Square m1from = from_sq(first);
    Square m2from = from_sq(second);
    Square m1to = to_sq(first);
    Square m2to = to_sq(second);

    // Don't prune moves of the threatened piece
		/*
		first�����������炩���ߖh���ł���Ȃ�true
		*/
    if (m1from == m2to)
        return true;

    // If the threatened piece has value less than or equal to the value of the
    // threat piece, don't prune moves which defend it.
		/*
		���Ў肪�ߊl��ł��@�ߊl�����̕]���l���ߊl������̉��l���傫���i���ŏ��������Ă���)�@�b�b�@�ߊl�����킪KING
		*/
    if (    pos.capture(second)
        && (   PieceValue[MG][pos.piece_on(m2from)] >= PieceValue[MG][pos.piece_on(m2to)]
            || type_of(pos.piece_on(m2from)) == KING))
    {
        // Update occupancy as if the piece and the threat are moving
        Bitboard occ = pos.pieces() ^ m1from ^ m1to ^ m2from;
        Piece pc = pos.piece_on(m1from);

        // The moved piece attacks the square 'tto' ?
				/*
				first��̈ړ��セ�̗�����second���ߊl���邱�Ƃ��\�Ȃ�true
				*/
        if (pos.attacks_from(pc, m1to, occ) & m2to)
            return true;

        // Scan for possible X-ray attackers behind the moved piece
				/*
				���Ў�̈ړ�����W����ROOK��BISHOP�̗������first����QUEEN,ROOK,BISHOP��������xray�ɓ����
				*/
        Bitboard xray =  (attacks_bb<  ROOK>(m2to, occ) & pos.pieces(color_of(pc), QUEEN, ROOK))
                       | (attacks_bb<BISHOP>(m2to, occ) & pos.pieces(color_of(pc), QUEEN, BISHOP));

        // Verify attackers are triggered by our move and not already existing
				/*
				�悭�킩���A���Ђ̋����ԑ���ROOK��BISHOP�ŕߊl�\�Ȃ�true?
				*/
        if (unlikely(xray) && (xray & ~pos.attacks_from<QUEEN>(m2to)))
            return true;
    }

    // Don't prune safe moves which block the threat path
		/*
		first��̈ړ��ɂ����second��̈ړ���j�~�ł����Î~�T���̌��ʂ�0�ȏ�ł����true
		*/
    if ((between_bb(m2from, m2to) & m1to) && pos.see_sign(first) >= 0)
        return true;
		/*
		���Ў��h��ł��Ȃ����false��Ԃ�
		*/
    return false;
  }


  // When playing with strength handicap choose best move among the MultiPV set
  // using a statistical rule dependent on 'level'. Idea by Heinz van Saanen.
	/*
	�N���XSkill�̃f�X�g���N�^�ŌĂ΂��Aid_loop�֐�����Ă΂��
	���x��(�X�L�����x��)�ɉ����Ďw�����ς���i�őP��ł͂Ȃ��j
	*/
	Move Skill::pick_move() 
	{

    static RKISS rk;

    // PRNG sequence should be not deterministic
		/*
		�����������_���ȉ񐔁i�����Ō��߂�@�T�O�����j
		*/
    for (int i = Time::now() % 50; i > 0; --i)
        rk.rand<unsigned>();

    // RootMoves are already sorted by score in descending order
    int variance = std::min(RootMoves[0].score - RootMoves[PVSize - 1].score, PawnValueMg);
		/*
		�f�t�H���g��level��20�Ȃ̂�weakness=120-2*20=80
		*/
    int weakness = 120 - 2 * level;
    int max_s = -VALUE_INFINITE;
    best = MOVE_NONE;

    // Choose best move. For each move score we add two terms both dependent on
    // weakness, one deterministic and bigger for weaker moves, and one random,
    // then we choose the move with the resulting highest score.
		/*
		�}���`�őP���萔����RootMoves�z����`�G�b�N���čł��]���l���������best��ɓo�^����
		*/
    for (size_t i = 0; i < PVSize; ++i)
    {
        int s = RootMoves[i].score;

        // Don't allow crazy blunders even at very low skills
				/*
				RootMoves�z��ɕ���ł���w���胊�X�g�̃X�R�A���P�O�̃X�R�A���Pawn��]���l�Q���ȏ㗣��Ă���ꍇ
				���̃��[�v���I��������
				*/
        if (i > 0 && RootMoves[i-1].score > s + 2 * PawnValueMg)
            break;

        // This is our magic formula
				/*
				�����_���ȗv�f���������s�ϐ��ɃX�R�A������
				*/
        s += (  weakness * int(RootMoves[0].score - s)
              + variance * (rk.rand<unsigned>() % weakness)) / 128;
				/*
				�����\���傫��������best��RootMoves[i]�Ԗڂ̎������
				*/
        if (s > max_s)
        {
            max_s = s;
            best = RootMoves[i].pv[0];
        }
    }
    return best;
  }


  // uci_pv() formats PV information according to UCI protocol. UCI requires
  // to send all the PV lines also if are still to be searched and so refer to
  // the previous search score.
	/*
	�ڍוs���ł��邪,UCI�����Ɍ��i�K�̋ǖʏ��iscore,nodes,nps�Ȃǁj���o�͂���
	id_loop�֐�����̂݌Ăяo�����
	*/
	string uci_pv(const Position& pos, int depth, Value alpha, Value beta) 
	{
    std::stringstream s;
    Time::point elapsed = Time::now() - SearchTime + 1;
    size_t uciPVSize = std::min((size_t)Options["MultiPV"], RootMoves.size());
    int selDepth = 0;

    for (Thread* th : Threads)
        if (th->maxPly > selDepth)
            selDepth = th->maxPly;

    for (size_t i = 0; i < uciPVSize; ++i)
    {
				/*
				PVIdx�͂��̃t�@�C�����̃O���[�o���ϐ��Ȃ̂Ŋ֐����ł��A�N�Z�X�ł���
				PVIdx�̓}���`PV�̃C���f�b�N�X�Aupdate�t���O�͍���PVIdx��菬����index�͕\��
				�ς݂Ȃ̂Ńp�X����t���O�A���̃t���O��true�Ȃ�\���Afalse�Ȃ��\��
				*/
        bool updated = (i <= PVIdx);

        if (depth == 1 && !updated)
            continue;

        int d   = updated ? depth : depth - 1;
        Value v = updated ? RootMoves[i].score : RootMoves[i].prevScore;
				/*
				return count of buffered input characters
				in_avail�֐��͕�������Ԃ��H
				*/
        if (s.rdbuf()->in_avail()) // Not at first line
            s << "\n";

        s << "info depth " << d
          << " seldepth "  << selDepth
          << " score "     << (i == PVIdx ? score_to_uci(v, alpha, beta) : score_to_uci(v))
          << " nodes "     << pos.nodes_searched()
          << " nps "       << pos.nodes_searched() * 1000 / elapsed
          << " time "      << elapsed
          << " multipv "   << i + 1
          << " pv";
				/*
				move_to_uci�֐���Move�`���̎w����f�[�^�������`���̕�����ɂ���
				RootMoves�z��̒��ɓ����Ă������}���`PV�������o�͂���
				*/
        for (size_t j = 0; RootMoves[i].pv[j] != MOVE_NONE; ++j)
            s <<  " " << move_to_uci(RootMoves[i].pv[j], pos.is_chess960());
    }
    return s.str();
  }

} // namespace


/// RootMove::extract_pv_from_tt() builds a PV by adding moves from the TT table.
/// We consider also failing high nodes and not only BOUND_EXACT nodes so to
/// allow to always have a ponder move even when we fail high at root, and a
/// long PV to print that is important for position analysis.
/*
search�֐�����̂݌Ă΂��
PV�o�^�A�]���l�X�V���ɌĂ΂��
pv[0]�̎�����o���Ă��̎�����s���ǖʂ��X�V����
�T���̌��ʂ��u���\�ɂ���̂Łi����͂��j���̎�����o����
���@��`�G�b�N�����Ȃ���pv�i�ŉ���菇�j���č\�z���Ă���
*/
void RootMove::extract_pv_from_tt(Position& pos) 
{
  StateInfo state[MAX_PLY_PLUS_6], *st = state;
  const TTEntry* tte;
  int ply = 0;
  Move m = pv[0];

  pv.clear();

  do {
      pv.push_back(m);

      assert(MoveList<LEGAL>(pos).contains(pv[ply]));

      pos.do_move(pv[ply++], *st++);
      tte = TT.probe(pos.key());

  } while (   tte
           && pos.pseudo_legal(m = tte->move()) // Local copy, TT could change
           && pos.legal(m, pos.pinned_pieces(pos.side_to_move()))
           && ply < MAX_PLY
           && (!pos.is_draw() || ply < 2));

  pv.push_back(MOVE_NONE); // Must be zero-terminating

  while (ply) pos.undo_move(pv[--ply]);
}


/// RootMove::insert_pv_in_tt() is called at the end of a search iteration, and
/// inserts the PV back into the TT. This makes sure the old PV moves are searched
/// first, even if the old TT entries have been overwritten.
/*
�����ɂ���^�C�~���O�͔����[����search�֐�����Ԃ��Ă����Ƃ���Ȃ̂�
pv�ɂ͂P�Â�RootMove�ɑ΂��čőP��菇���o�^����Ă���
�����ł͂����ɓ����Ă�����TT�i�g�����X�|�W�V�����e�[�u���j�ɓo�^���Ă���
���肾���ł͂Ȃ�PV�S��TT�ɓo�^���Ă���
*/
void RootMove::insert_pv_in_tt(Position& pos) 
{

  StateInfo state[MAX_PLY_PLUS_6], *st = state;
  const TTEntry* tte;
  int ply = 0;
	/*
	TT�̓g�����X�|�W�V�����e�[�u����\���O���[�o���ϐ�
	pv:�i�őP��菇�j������ׂ̂P�����̉ϒ��z��łP��RootMove�N���X�Ɋi�[����Ă���
	*/
	do {
      tte = TT.probe(pos.key());
			/*
			�n���ꂽ�ǖʂŁATT�ɓo�^���ꂽ�肪�Ȃ��܂��͂����Ă������̎�Ƃ͈قȂ�Ƃ�
			�������TT�ɓo�^����
			*/
			if (!tte || tte->move() != pv[ply]) // Don't overwrite correct entries
          TT.store(pos.key(), VALUE_NONE, BOUND_NONE, DEPTH_NONE, pv[ply], VALUE_NONE);

      assert(MoveList<LEGAL>(pos).contains(pv[ply]));

      pos.do_move(pv[ply++], *st++);

  } while (pv[ply] != MOVE_NONE);

  while (ply) pos.undo_move(pv[--ply]);
}


/// Thread::idle_loop() is where the thread is parked when it has no work to do
/*
Thread::split�֐�����Ă΂��
option��o["Threads"]�𕡐��ɂ��Ȃ��ƌĂ΂�Ȃ��A�f�t�H���g��1
�܂�ʏ�ł͌Ă΂�Ȃ�
*/
void Thread::idle_loop() 
{

  // Pointer 'this_sp' is not null only if we are called from split(), and not
  // at the thread creation. So it means we are the split point's master.
  SplitPoint* this_sp = splitPointsSize ? activeSplitPoint : nullptr;

  assert(!this_sp || (this_sp->masterThread == this && searching));

  while (true)
  {
      // If we are not searching, wait for a condition to be signaled instead of
      // wasting CPU time polling for work.
      while ((!searching && Threads.sleepWhileIdle) || exit)
      {
          if (exit)
          {
              assert(!this_sp);
              return;
          }

          // Grab the lock to avoid races with Thread::notify_one()
          std::unique_lock<std::mutex> lk(mutex);

          // If we are master and all slaves have finished then exit idle_loop
          if (this_sp && !this_sp->slavesMask)
              break;

          // Do sleep after retesting sleep conditions under lock protection, in
          // particular we need to avoid a deadlock in case a master thread has,
          // in the meanwhile, allocated us and sent the notify_one() call before
          // we had the chance to grab the lock.
          if (!searching && !exit)
              sleepCondition.wait(lk);
      }

      // If this thread has been assigned work, launch a search
      if (searching)
      {
          assert(!exit);
					/*
					split�֐��Ŗڊo�߂�����ꂽ�X���[�u�X���b�h�͑S��mutex�����b�N���đҋ@�A
					������MainThread��split�֐���Threads.mutex��unlock����̂�҂�
					*/
          Threads.mutex.lock();

          assert(searching);
          assert(activeSplitPoint);
          SplitPoint* sp = activeSplitPoint;

          Threads.mutex.unlock();

          Stack stack[MAX_PLY_PLUS_6], *ss = stack+2; // To allow referencing (ss-2)
					/*
					�����œn���ꂽposition�N���X���R�s�[���ăX���b�h�ɓn��(activePosition�ɃA�h���X��n���j
					�X���b�h���ƂɃR�s�[��n��
					*/
          Position pos(*sp->pos, this);

          std::memcpy(ss-2, sp->ss-2, 5 * sizeof(Stack));
          ss->splitPoint = sp;

          sp->mutex.lock();

          assert(activePosition == nullptr);

          activePosition = &pos;
					/*
					nodeType�ɉ����ĕ��򂷂�
					*/
					switch (sp->nodeType) {
          case Root:
              search<SplitPointRoot>(pos, ss, sp->alpha, sp->beta, sp->depth, sp->cutNode);
              break;
          case PV:
              search<SplitPointPV>(pos, ss, sp->alpha, sp->beta, sp->depth, sp->cutNode);
              break;
          case NonPV:
              search<SplitPointNonPV>(pos, ss, sp->alpha, sp->beta, sp->depth, sp->cutNode);
              break;
          default:
              assert(false);
          }
					/*
					�T�����򂵂��X���b�h�͂����ɋA���Ă���Bserach�֐����Ԃ��]���l��S�R���p���Ă��Ȃ��̂�
					�T�����ɓ����]���l�͂��ׂċ��L�f�[�^�[�ɍX�V��������Ǝv����
					*/
          assert(searching);

          searching = false;
          activePosition = nullptr;
					/*
					�����̃X���b�hID�������Ă���
					*/
          sp->slavesMask &= ~(1ULL << idx);
					/*
					�T�����򂪒T�������m�[�h�������Z���Ă���
					*/
          sp->nodes += pos.nodes_searched();

          // Wake up master thread so to allow it to return from the idle loop
          // in case we are the last slave of the split point.
					/*
					MainThread���N�����Ă���
					- Threads.sleepWhileIdle��Think�֐��ŒT���O��false�Ƃ��Ă��܂��̂ł��������s����邱�Ƃ͂Ȃ�
					- ���̃X���b�h��MainThread�ł͂Ȃ��i�T������X���b�h�j
					- ���̃X���b�h���Ō�̃X���b�h�Ȃ�
					*/
          if (Threads.sleepWhileIdle &&  this != sp->masterThread && !sp->slavesMask)
          {
              assert(!sp->masterThread->searching);
              sp->masterThread->notify_one();
          }

          // After releasing the lock we cannot access anymore any SplitPoint
          // related data in a safe way becuase it could have been released under
          // our feet by the sp master. Also accessing other Thread objects is
          // unsafe because if we are exiting there is a chance are already freed.
          sp->mutex.unlock();
      }

      // If this thread is the master of a split point and all slaves have finished
      // their work at this split point, return from the idle loop.
			/*
			while�i�v���[�v�̏o���̈��
			�S�ẴX���b�h���Ȃ��Ȃ����炱��idle_loop�֐�����E�o�ł���
			*/
      if (this_sp && !this_sp->slavesMask)
      {
          this_sp->mutex.lock();
          bool finished = !this_sp->slavesMask; // Retest under lock protection
          this_sp->mutex.unlock();
          if (finished)
              return;
      }
  }//while(true)�I��
}


/// check_time() is called by the timer thread when the timer triggers. It is
/// used to print debug info and, more important, to detect when we are out of
/// available time and so stop the search.
/*
TimerThread::idle_loop()����̂݌Ă΂��
�T���J�n���珊��̎��Ԃ𒴂�����i���ɂ������͂��邪�ڍוs���j�T����~�̃t���O�𗧂Ă�
*/
void check_time() 
{

  static Time::point lastInfoTime = Time::now();
  int64_t nodes = 0; // Workaround silly 'uninitialized' gcc warning

  if (Time::now() - lastInfoTime >= 1000)
  {
      lastInfoTime = Time::now();
      dbg_print();
  }

  if (Limits.ponder)
      return;

  if (Limits.nodes)
  {
      Threads.mutex.lock();

      nodes = RootPos.nodes_searched();

      // Loop across all split points and sum accumulated SplitPoint nodes plus
      // all the currently active positions nodes.
      for (Thread* th : Threads)
          for (int i = 0; i < th->splitPointsSize; ++i)
          {
              SplitPoint& sp = th->splitPoints[i];

              sp.mutex.lock();

              nodes += sp.nodes;
              Bitboard sm = sp.slavesMask;
              while (sm)
              {
                  Position* pos = Threads[pop_lsb(&sm)]->activePosition;
                  if (pos)
                      nodes += pos->nodes_searched();
              }

              sp.mutex.unlock();
          }

      Threads.mutex.unlock();
  }
	/*
	SearchTime��start_thking�֐��Ŏ������Z�b�g���āA�����Ōo�ߎ���elapsed�𑪂�
	���̌o�ߎ��Ԃ�TimeMgr.available_time()�ɐݒ肵�Ă��鎞�Ԃ𒴂�����Ȃǂ̏�����
	�T����~�̃t���O�𗧂Ă�
	��~�̏ڍׂ͕s��
	*/
  Time::point elapsed = Time::now() - SearchTime;
  bool stillAtFirstMove =    Signals.firstRootMove
                         && !Signals.failedLowAtRoot
                         &&  elapsed > TimeMgr.available_time();

  bool noMoreTime =   elapsed > TimeMgr.maximum_time() - 2 * TimerThread::Resolution
                   || stillAtFirstMove;

  if (   (Limits.use_time_management() && noMoreTime)
      || (Limits.movetime && elapsed >= Limits.movetime)
      || (Limits.nodes && nodes >= Limits.nodes))
      Signals.stop = true;
}
