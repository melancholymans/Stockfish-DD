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

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "material.h"
#include "movepick.h"
#include "pawns.h"
#include "position.h"
#include "search.h"

const int MAX_THREADS = 64; // Because SplitPoint::slavesMask is a uint64_t
const int MAX_SPLITPOINTS_PER_THREAD = 8;

struct Thread;
/*
�T�������򂵂����ƃf�[�^�[��ێ����邽�߂̃N���X
*/
struct SplitPoint {

  // Const data after split point has been setup
  const Position* pos;
  const Search::Stack* ss;
  Thread* masterThread;
  Depth depth;
  Value beta;
  int nodeType;
  Move threatMove;
  bool cutNode;

  // Const pointers to shared data
  MovePicker* movePicker;
  SplitPoint* parentSplitPoint;

  // Shared data
  std::mutex mutex;
  volatile uint64_t slavesMask;
  volatile int64_t nodes;
  volatile Value alpha;
  volatile Value bestValue;
  volatile Move bestMove;
  volatile int moveCount;
  volatile bool cutoff;
};


/// ThreadBase struct is the base of the hierarchy from where we derive all the
/// specialized thread classes.
/*
�S�ẴX���b�h�N���X�̃x�[�X�N���X
*/
struct ThreadBase {
	/*
	exit��false�ŏ���������Aexit�̓X���b�h��j�󂷂�Ƃ��̃t���O
	*/
  ThreadBase() : exit(false) {}
  virtual ~ThreadBase() {}
	/*
	idle_loop��MainThread,TimerThread�ŃI�[�o�[���[�h����̂ł����ł͉��z�֐�
	*/
  virtual void idle_loop() = 0;
	/*
	���̊֐����Ă񂾃X���b�h���N����
	*/
  void notify_one();
	/*
	�����ϐ�bool b��true�ɂȂ�܂ŃX���b�h��Q������
	*/
  void wait_for(volatile const bool& b);
	/*
	�W�����C�u�����̃X���b�h�N���X�̃|�C���^�𐶐������̕ϐ��ɓ���Ă���
	*/
  std::thread nativeThread;
	/*
	�W�����C�u�����̃~���[�e�b�N�X�N���X
	*/
  std::mutex mutex;
	/*
	�W�����C�u�����̏����ϐ��N���X
	*/
  std::condition_variable sleepCondition;
	/*
	�N���X��������false�Őݒ肳���
	MainThread,timerThread�̃A�C�h�����[�v���甲���o����true�ɂ���
	delete_thread�֐����������̕ϐ���true�ɂł���
	�����Ă���delete_thread�֐���ThreadPool::exit()�֐���������Ă΂�Ă���
	����exit()�֐���main�֐����I������^�C�~���O�ŌĂ΂�邾���Ȃ̂�
	��U�쐬���ꂽ�X���b�h�̓Q�[�����I������܂Ŏ~�܂�Ȃ�
	*/
  volatile bool exit;
};


/// Thread struct keeps together all the thread related stuff like locks, state
/// and especially split points. We also use per-thread pawn and material hash
/// tables so that once we get a pointer to an entry its life time is unlimited
/// and we don't have to care about someone changing the entry under our feet.
/*
ThreadBase�N���X����h������N���X�ł��̃N���X��y���ThreadPool,MainThread,TimerThread���h�����Ă���
ThreadBase�N���X��Thread����ɕK�v�ȋ@�\�����������̔�ׂ��̃N���X�ɂ͒T���ɕK�v�Ȋ֐��A�f�[�^�[���������Ă���
*/
struct Thread : public ThreadBase {

  Thread();
	/*
	�X���b�h�ҋ@�֐�,���z�֐��Ȃ̂�Thread��idle_loop�֐��ɂ��AMainThread��idle_loop�֐��ɂ�,TimerThread��idle_loop�֐��ɃI�[�o���C�h�����
	*/
  virtual void idle_loop();
	/*
	�p�r�s��
	*/
  bool cutoff_occurred() const;
	/*
	�p�r�s��
	*/
  bool available_to(const Thread* master) const;
	/*
	�T������
	*/
  template <bool Fake>
  void split(Position& pos, const Search::Stack* ss, Value alpha, Value beta, Value* bestValue, Move* bestMove,
             Depth depth, Move threatMove, int moveCount, MovePicker* movePicker, int nodeType, bool cutNode);
	/*
	�T������̏��i�X���b�h�ŗL�A���L�j
	*/
  SplitPoint splitPoints[MAX_SPLITPOINTS_PER_THREAD];
  Material::Table materialTable;
  Endgames endgames;
  Pawns::Table pawnsTable;
  Position* activePosition;
	/*
	�X���b�h�ŗLID
	*/
  size_t idx;
  int maxPly;
  SplitPoint* volatile activeSplitPoint;
  volatile int splitPointsSize;
	/*
	�������ł�false���ݒ肳��
	idle_loop�֐��ŃX���b�h��ڊo�߂������Ƃ���true�ƂȂ�(Search::think()���Ăяo���T�������邽�߁j
	�T���I����Ă�false�ƂȂ�idle_loop�̒��ŐQ��
	*/
  volatile bool searching;
};


/// MainThread and TimerThread are derived classes used to characterize the two
/// special threads: the main one and the recurring timer.
/*
Thread�N���X����h������MainThread�ł��ꂪ�T���J�n���ɓ����ŏ��̃X���b�h
*/
struct MainThread : public Thread {
	/*
	thinking��go�֐�����Ă΂ꂽstart_thmking�֐���true�ɂ���MainThread::idle_loop�֐��ŐQ�Ă���
	�X���b�h��T���ɍs�����邽�߂̃t���O
	*/
  MainThread() : thinking(true) {} // Avoid a race with start_thinking()
	/*
	MainThread��pidle_loop�ŒT���ȊO�ł͂����ŐQ�Ă���
	*/
  virtual void idle_loop();

  volatile bool thinking;
};
/*
�^�C�}�[�p�X���b�h
*/
struct TimerThread : public ThreadBase {
  TimerThread() : run(false) {}
  virtual void idle_loop();
  bool run;
  static const int Resolution = 5; // msec between two check_time() calls
};


/// ThreadPool struct handles all the threads related stuff like init, starting,
/// parking and, the most important, launching a slave thread at a split point.
/// All the access to shared thread data is done through this class.
/*
�T�����򂷂�X���b�h�͂����Ƀv�[������Ă���X���b�h���g��
ThreadPool�N���X��Thread.cpp���ŃO���[�o���錾����Ă���
*/
struct ThreadPool : public std::vector<Thread*> {

  void init(); // No c'tor and d'tor, threads rely on globals that should
  void exit(); // be initialized and valid during the whole thread lifetime.
	/*
	ThreadPool����MainThread�N���X���C���X�^���X��Ԃ�
	*/
  MainThread* main() { return static_cast<MainThread*>((*this)[0]); }
	/*
	uci_option����X���b�h�Ɋւ���I�v�V��������������
	*/
  void read_uci_options();
	/*
	�p�r�s��
	*/
  Thread* available_slave(const Thread* master) const;
	/*
	MainThread��Q�����邽�߂̃X���b�h
	�����v�l���J�n�����邽�߂ɔO�̂��߂̃X���b�h�������I�ɐQ������֐��H
	*/
  void wait_for_think_finished();
	/*
	uci�R�}���h����Ăяo����MainThread�ɒT�����J�n������A���̊֐���ǂ񂾃X���b�h��uci�R�}���h���[�v�ɖ߂�
	*/
  void start_thinking(const Position&, const Search::LimitsType&,
                      const std::vector<Move>&, Search::StateStackPtr&);
	/*
	�p�r�s��
	*/
  bool sleepWhileIdle;
	/*
	�p�r�s��
	*/
  Depth minimumSplitDepth;
	/*
	�p�r�s��
	*/
  size_t maxThreadsPerSplitPoint;
	/*
	�X���b�h�̃X���[�v�̐���ɕK�v�ȃ~���[�e�b�N�Ə����ϐ�
	*/
  std::mutex mutex;
  std::condition_variable sleepCondition;
	/*
	�^�C�}�[�p�X���b�h�N���X�̃C���X�^���X
	*/
  TimerThread* timer;
};

extern ThreadPool Threads;

#endif // #ifndef THREAD_H_INCLUDED
