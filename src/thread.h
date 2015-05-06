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
探索が分岐したあとデーターを保持するためのクラス
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
全てのスレッドクラスのベースクラス
*/
struct ThreadBase {
	/*
	exitはfalseで初期化する、exitはスレッドを破壊するときのフラグ
	*/
  ThreadBase() : exit(false) {}
  virtual ~ThreadBase() {}
	/*
	idle_loopはMainThread,TimerThreadでオーバーロードするのでここでは仮想関数
	*/
  virtual void idle_loop() = 0;
	/*
	この関数を呼んだスレッドを起こす
	*/
  void notify_one();
	/*
	条件変数bool bがtrueになるまでスレッドを寝かせる
	*/
  void wait_for(volatile const bool& b);
	/*
	標準ライブラリのスレッドクラスのポインタを生成時この変数に入れておく
	*/
  std::thread nativeThread;
	/*
	標準ライブラリのミューテックスクラス
	*/
  std::mutex mutex;
	/*
	標準ライブラリの条件変数クラス
	*/
  std::condition_variable sleepCondition;
	/*
	クラス生成時はfalseで設定される
	MainThread,timerThreadのアイドルループから抜け出す時trueにする
	delete_thread関数だけがこの変数をtrueにできる
	そしてそのdelete_thread関数はThreadPool::exit()関数だけから呼ばれている
	そのexit()関数はmain関数が終了するタイミングで呼ばれるだけなので
	一旦作成されたスレッドはゲームが終了するまで止まらない
	*/
  volatile bool exit;
};


/// Thread struct keeps together all the thread related stuff like locks, state
/// and especially split points. We also use per-thread pawn and material hash
/// tables so that once we get a pointer to an entry its life time is unlimited
/// and we don't have to care about someone changing the entry under our feet.
/*
ThreadBaseクラスから派生するクラスでこのクラスを土台にThreadPool,MainThread,TimerThreadが派生していく
ThreadBaseクラスがThread制御に必要な機能を実装したの比べこのクラスには探索に必要な関数、データーを実装している
*/
struct Thread : public ThreadBase {

  Thread();
	/*
	スレッド待機関数,仮想関数なのでThreadのidle_loop関数にも、MainThreadのidle_loop関数にも,TimerThreadのidle_loop関数にオーバライドされる
	*/
  virtual void idle_loop();
	/*
	用途不明
	*/
  bool cutoff_occurred() const;
	/*
	用途不明
	*/
  bool available_to(const Thread* master) const;
	/*
	探索分岐
	*/
  template <bool Fake>
  void split(Position& pos, const Search::Stack* ss, Value alpha, Value beta, Value* bestValue, Move* bestMove,
             Depth depth, Move threatMove, int moveCount, MovePicker* movePicker, int nodeType, bool cutNode);
	/*
	探索分岐の情報（スレッド固有、共有）
	*/
  SplitPoint splitPoints[MAX_SPLITPOINTS_PER_THREAD];
  Material::Table materialTable;
  Endgames endgames;
  Pawns::Table pawnsTable;
  Position* activePosition;
	/*
	スレッド固有ID
	*/
  size_t idx;
  int maxPly;
  SplitPoint* volatile activeSplitPoint;
  volatile int splitPointsSize;
	/*
	初期化ではfalseが設定され
	idle_loop関数でスレッドを目覚めさせたときにtrueとなり(Search::think()を呼び出し探索させるため）
	探索終了後再びfalseとなりidle_loopの中で寝る
	*/
  volatile bool searching;
};


/// MainThread and TimerThread are derived classes used to characterize the two
/// special threads: the main one and the recurring timer.
/*
Threadクラスから派生したMainThreadでこれが探索開始時に動く最初のスレッド
*/
struct MainThread : public Thread {
	/*
	thinkingはgo関数から呼ばれたstart_thmking関数でtrueにされMainThread::idle_loop関数で寝ていた
	スレッドを探索に行かせるためのフラグ
	*/
  MainThread() : thinking(true) {} // Avoid a race with start_thinking()
	/*
	MainThread専用idle_loopで探索以外ではここで寝ている
	*/
  virtual void idle_loop();

  volatile bool thinking;
};
/*
タイマー用スレッド
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
探索分岐するスレッドはここにプールされているスレッドを使う
ThreadPoolクラスはThread.cpp内でグローバル宣言されている
*/
struct ThreadPool : public std::vector<Thread*> {

  void init(); // No c'tor and d'tor, threads rely on globals that should
  void exit(); // be initialized and valid during the whole thread lifetime.
	/*
	ThreadPoolからMainThreadクラスをインスタンスを返す
	*/
  MainThread* main() { return static_cast<MainThread*>((*this)[0]); }
	/*
	uci_optionからスレッドに関するオプションを処理する
	*/
  void read_uci_options();
	/*
	用途不明
	*/
  Thread* available_slave(const Thread* master) const;
	/*
	MainThreadを寝かせるためのスレッド
	多分思考を開始させるために念のためのスレッドを強制的に寝かせる関数？
	*/
  void wait_for_think_finished();
	/*
	uciコマンドから呼び出されMainThreadに探索を開始させる、この関数を読んだスレッドはuciコマンドループに戻る
	*/
  void start_thinking(const Position&, const Search::LimitsType&,
                      const std::vector<Move>&, Search::StateStackPtr&);
	/*
	用途不明
	*/
  bool sleepWhileIdle;
	/*
	用途不明
	*/
  Depth minimumSplitDepth;
	/*
	用途不明
	*/
  size_t maxThreadsPerSplitPoint;
	/*
	スレッドのスリープの制御に必要なミューテックと条件変数
	*/
  std::mutex mutex;
  std::condition_variable sleepCondition;
	/*
	タイマー用スレッドクラスのインスタンス
	*/
  TimerThread* timer;
};

extern ThreadPool Threads;

#endif // #ifndef THREAD_H_INCLUDED
