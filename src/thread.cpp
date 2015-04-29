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

#include <algorithm> // For std::count
#include <cassert>

#include "movegen.h"
#include "search.h"
#include "thread.h"
#include "ucioption.h"

/*
stackfishはメインスレッドとMainThreadスレッド、TimerThreadがある
探索中に発生する探索分岐（split）はどう制御しているのかはまだわからない
クラスの継承関係
ThreadBase->Thread		->ThreadPool
											->MainThread
											->TimerThread
このstockfishDDバージョンはスレッドの制御に標準ライブラリーのThreadクラスを使っている
（C++11から搭載されるようになった）
*/

using namespace Search;

ThreadPool Threads; // Global object

namespace {

 // Helpers to launch a thread after creation and joining before delete. Must be
 // outside Thread c'tor and d'tor because object shall be fully initialized
 // when virtual idle_loop() is called and when joining.
/*
ThreadPool::init関数からTimerThread,MainThreadを作る時に呼ばれる
スレッドを作るヘルパー関数
テンプレートパラメータで渡されたスレッドクラスを生成して標準スレッドライブラリにidle_loop関数を
登録することによって一旦スレッドを待機状態に設定し、生成したスレッドクラスのインスタンスを返す
*/
 template<typename T> T* new_thread() {
   T* th = new T();
   th->nativeThread = std::thread(&ThreadBase::idle_loop, th); // Will go to sleep
   return th;
 }
 /*
 ThreadPool::read_uci_option関数,ThreadPool::exit関数から呼ばれる
 スレッドを殺す
 */
 void delete_thread(ThreadBase* th) {
	 //exitフラグをセット
   th->exit = true; // Search must be already finished
	 //寝ているスレッドは起こしておく
   th->notify_one();
	 //ここでスレッドの終了を待つ
   th->nativeThread.join(); // Wait for thread termination
   delete th;
 }

}

// ThreadBase::notify_one() wakes up the thread when there is some search to do
/*
notify_oneはThreadBaseクラスが持っているミューテック変数をロックして
寝ているスレッドを起こす
*/
void ThreadBase::notify_one() {
  std::unique_lock<std::mutex>(this->mutex);
  sleepCondition.notify_one();
}


// ThreadBase::wait_for() set the thread to sleep until condition 'b' turns true
//渡したｂ変数が条件変数となる。
//この関数を呼び出したスレッドを寝かせる
void ThreadBase::wait_for(volatile const bool& b) {

  std::unique_lock<std::mutex> lk(mutex);
  sleepCondition.wait(lk, [&]{ return b; });
}


// Thread c'tor just inits data but does not launch any thread of execution that
// instead will be started only upon c'tor returns.
/*
スレッドクラスのコンストラクタ
スレッドはtimerThread以外は全てThreadPoolに保持されるので（Threads[0]がMainThreadになる）
スレッド固有IDはPool内の配列のIndexになる
*/
Thread::Thread() /* : splitPoints() */ { // Value-initialization bug in MSVC

  searching = false;
	/*
	このmaxPly,splitPointsSize,activeSplitPoint,activePositionの用途は不明
	*/
  maxPly = splitPointsSize = 0;
  activeSplitPoint = nullptr;
  activePosition = nullptr;
  idx = Threads.size();
}


// TimerThread::idle_loop() is where the timer thread waits msec milliseconds
// and then calls check_time(). If msec is 0 thread sleeps until is woken up.
extern void check_time();
/*
タイマースレッドのアイドルループ
5msごとに探索停止判断をする関数(check_time)を呼び出している
*/
void TimerThread::idle_loop() {

  while (!exit)
  {
      std::unique_lock<std::mutex> lk(mutex);
			/*
			runは探索中はtrueになっている（探索終了後はfalseに）
			Resolution=5msになっているので5msごと起きてcheck_time関数を呼び出して
			探索停止すべきか判断している
			*/
      if (!exit)
          sleepCondition.wait_for(lk, std::chrono::milliseconds(run ? Resolution : INT_MAX));

      lk.unlock();

      if (run)
          check_time();
  }
}


// MainThread::idle_loop() is where the main thread is parked waiting to be started
// when there is a new search. Main thread will launch all the slave threads.
/*
最初main関数からThreads::init()を呼ぶ
ThreadsはThreadPoolクラスなのでinit関数からnew_thread<MainThread>を呼ぶ
最初に来た時はsearchingフラグがfalseなのでsleep状態に遷移

UCIのgoコマンドからThreads.start_thinking(pos, limits, SetupStates)が呼ばれると
notify_one関数を呼びsleep状態から抜け

MainThread::idle_loop関数->think関数->id_loop関数->search関数と呼ばれるようになっている
*/
void MainThread::idle_loop() {

  while (true)
  {
      std::unique_lock<std::mutex> lk(mutex);

      thinking = false;

      while (!thinking && !exit)
      {
          Threads.sleepCondition.notify_one(); // Wake up UI thread if needed
          sleepCondition.wait(lk);
      }

      lk.unlock();

      if (exit)
          return;

      searching = true;

      Search::think();

      assert(searching);

      searching = false;
  }
}


// Thread::cutoff_occurred() checks whether a beta cutoff has occurred in the
// current active split point, or in some ancestor of the split point.
/*
search関数のStep 18（Check for new best move）からのみ呼ばれている
用途不明
*/
bool Thread::cutoff_occurred() const {

  for (SplitPoint* sp = activeSplitPoint; sp; sp = sp->parentSplitPoint)
      if (sp->cutoff)
          return true;

  return false;
}


// Thread::available_to() checks whether the thread is available to help the
// thread 'master' at a split point. An obvious requirement is that thread must
// be idle. With more than two threads, this is not sufficient: If the thread is
// the master of some split point, it is only available as a slave to the slaves
// which are busy searching the split point at the top of slaves split point
// stack (the "helpful master concept" in YBWC terminology).
/*
available_slave関数のみから呼び出されている
探索中は即ないと返事
用途不明
*/
bool Thread::available_to(const Thread* master) const {

  if (searching)
      return false;

  // Make a local copy to be sure doesn't become zero under our feet while
  // testing next condition and so leading to an out of bound access.
  int size = splitPointsSize;

  // No split points means that the thread is available as a slave for any
  // other thread otherwise apply the "helpful master" concept if possible.
  return !size || (splitPoints[size - 1].slavesMask & (1ULL << master->idx));
}


// init() is called at startup to create and launch requested threads, that will
// go immediately to sleep due to 'sleepWhileIdle' set to true. We cannot use
// a c'tor becuase Threads is a static object and we need a fully initialized
// engine at this point due to allocation of Endgames in Thread c'tor.
/*
スレッドの初期化
*/
void ThreadPool::init() {

  sleepWhileIdle = true;
  timer = new_thread<TimerThread>();
  push_back(new_thread<MainThread>());
	/*
	Option["Threads"]に複数設定してあればnew_thread<Thread>で設定数だけスレッドを生成する
	生成されたスレッドはsearch.cppのThread::idle_loop関数に行く
	ただしnew_threadに渡しているテンプレートパラメータ<TimerThread,MainThread>によって
	idle_loopは異なる。
	TimerThread::idle_loop,MainThread::idle_loopとThread::split関数から呼ばれるThread::idle_loopがある。
	そのidle_loopはこのすぐ下にあるread_uci_option関数内で生成されたスレッドとリンクされる
	一旦sleepCondition.wait(mutex)で待機させられる
	探索自体はMainThread::idle_loop関数->think関数->id_loop関数->search関数と呼ばれて行く
	*/
	read_uci_options();
}


// exit() cleanly terminates the threads before the program exits
/*
このプログラムが終了するとき呼ばれスレッドを破壊する
*/
void ThreadPool::exit() {

  delete_thread(timer); // As first because check_time() accesses threads data

  for (Thread* th : *this)
      delete_thread(th);
}


// read_uci_options() updates internal threads parameters from the corresponding
// UCI options and creates/destroys threads to match the requested number. Thread
// objects are dynamically allocated to avoid creating in advance all possible
// threads, with included pawns and material tables, if only few are used.
/*
ThreadPoolのinit関数から呼ばれる（これは１回だけ）、あとuci option からMin Split Depth,Max Threads per Split Point,Threadsの
オプションが変更されたら随時呼ばれる。
uci option設定に従ってプールするスレッド数を決める
*/
void ThreadPool::read_uci_options() {

  maxThreadsPerSplitPoint = Options["Max Threads per Split Point"];	//デフォルトで設定されているのは5スレッド
  minimumSplitDepth       = Options["Min Split Depth"] * ONE_PLY;		//デフォルトで設定されているのは0
  size_t requested        = Options["Threads"];											//デフォルトで設定されているのは1、設定可能スレッド数は1~64

  assert(requested > 0);

  // Value 0 has a special meaning: We determine the optimal minimum split depth
  // automatically. Anyhow the minimumSplitDepth should never be under 4 plies.
	/*
	minimumSplitDepthが0なら4*ONE_PLYとし決して４より小さくならないようにしている
	*/
  if (!minimumSplitDepth)
      minimumSplitDepth = (requested < 8 ? 4 : 7) * ONE_PLY;
  else
      minimumSplitDepth = std::max(4 * ONE_PLY, minimumSplitDepth);
	/*
	ThreadPoolはvectorを継承しているのでsize()はプールしているスレッド数となる
	uci optionで設定されたrequested数がプールされたスレッド数より大きい場合は
	スレッドをここで生成する。
	このread_uci_options関数が呼ばれた時点ではプールにためているスレッドはMainThreadだけなので
	requestedが１より多ければここで生成して準備しておく
	*/
  while (size() < requested)
      push_back(new_thread<Thread>());
	/*
	反対に少なかったら削除する
	*/
  while (size() > requested)
  {
      delete_thread(back());
      pop_back();
  }
}


// slave_available() tries to find an idle thread which is available as a slave
// for the thread 'master'.
/*
search関数,split関数から呼ばれる
利用可能な奴隷（空いているスレッド）を探してあったらそのスレッドのインスタンスを返す
なかったらnullptr(C++11で導入）
available_to関数がどのように探しているのかわからないのでいまいちわからない
*/
Thread* ThreadPool::available_slave(const Thread* master) const {
	/*
	available_to関数用途不明
	*/
  for (Thread* th : *this)
      if (th->available_to(master))
          return th;

  return nullptr;
}


// split() does the actual work of distributing the work at a node between
// several available threads. If it does not succeed in splitting the node
// (because no idle threads are available), the function immediately returns.
// If splitting is possible, a SplitPoint object is initialized with all the
// data that must be copied to the helper threads and then helper threads are
// told that they have been assigned work. This will cause them to instantly
// leave their idle loops and call search(). When all threads have returned from
// search() then split() returns.
/*
search関数のstep19から呼び出される（呼び出し条件いろいろ）
Fakeはfalseで呼び出される
探索分岐のスレッド用であるが完全には理解できていない
*/
template <bool Fake>
void Thread::split(Position& pos, const Stack* ss, Value alpha, Value beta, Value* bestValue,
                   Move* bestMove, Depth depth, Move threatMove, int moveCount,
                   MovePicker* movePicker, int nodeType, bool cutNode) {

  assert(pos.pos_is_ok());
  assert(*bestValue <= alpha && alpha < beta && beta <= VALUE_INFINITE);
  assert(*bestValue > -VALUE_INFINITE);
  assert(depth >= Threads.minimumSplitDepth);
  assert(searching);
  assert(splitPointsSize < MAX_SPLITPOINTS_PER_THREAD);

  // Pick the next available split point from the split point stack
	/*
	MAX_SPLITPOINTS_PER_THREAD=8だけの配列
	おそらくスレッド単位で必要な情報を入れるものと思われる
	*/
	SplitPoint& sp = splitPoints[splitPointsSize];

  sp.masterThread = this;
  sp.parentSplitPoint = activeSplitPoint;
  sp.slavesMask = 1ULL << idx;
  sp.depth = depth;
  sp.bestValue = *bestValue;
  sp.bestMove = *bestMove;
  sp.threatMove = threatMove;
  sp.alpha = alpha;
  sp.beta = beta;
  sp.nodeType = nodeType;
  sp.cutNode = cutNode;
  sp.movePicker = movePicker;
  sp.moveCount = moveCount;
  sp.pos = &pos;
  sp.nodes = 0;
  sp.cutoff = false;
  sp.ss = ss;

  // Try to allocate available threads and ask them to start searching setting
  // 'searching' flag. This must be done under lock protection to avoid concurrent
  // allocation of the same slave by another master.
  Threads.mutex.lock();
  sp.mutex.lock();
	/*
	ここのsplitPointsSizeはThreads[0].splitPointsSizeです（つまりMainThread用の変数なので
	共有変数となるのでmutexのロックがか掛っている
	splitPointsSize変数は探索分岐をしているスレッドの数
	*/
	++splitPointsSize;
  activeSplitPoint = &sp;
  activePosition = nullptr;

  size_t slavesCnt = 1; // This thread is always included
  Thread* slave;

  while (    (slave = Threads.available_slave(this)) != nullptr
         && ++slavesCnt <= Threads.maxThreadsPerSplitPoint && !Fake)
  {
      sp.slavesMask |= 1ULL << slave->idx;
      slave->activeSplitPoint = &sp;
      slave->searching = true; // Slave leaves idle_loop()
      slave->notify_one(); // Could be sleeping
  }

  // Everything is set up. The master thread enters the idle loop, from which
  // it will instantly launch a search, because its 'searching' flag is set.
  // The thread will return from the idle loop when all slaves have finished
  // their work at this split point.
  if (slavesCnt > 1 || Fake)
  {
      sp.mutex.unlock();
      Threads.mutex.unlock();
			/*
			ここから探索分岐
			*/
			Thread::idle_loop(); // Force a call to base class idle_loop()

      // In helpful master concept a master can help only a sub-tree of its split
      // point, and because here is all finished is not possible master is booked.
      assert(!searching);
      assert(!activePosition);

      // We have returned from the idle loop, which means that all threads are
      // finished. Note that setting 'searching' and decreasing splitPointsSize is
      // done under lock protection to avoid a race with Thread::available_to().
			/*
			探索分岐が終了すればここに戻ってくる
			*/
			Threads.mutex.lock();
      sp.mutex.lock();
  }

  searching = true;
  --splitPointsSize;
  activeSplitPoint = sp.parentSplitPoint;
  activePosition = &pos;
  pos.set_nodes_searched(pos.nodes_searched() + sp.nodes);
  *bestMove = sp.bestMove;
  *bestValue = sp.bestValue;

  sp.mutex.unlock();
  Threads.mutex.unlock();
}

// Explicit template instantiations
template void Thread::split<false>(Position&, const Stack*, Value, Value, Value*, Move*, Depth, Move, int, MovePicker*, int, bool);
template void Thread::split< true>(Position&, const Stack*, Value, Value, Value*, Move*, Depth, Move, int, MovePicker*, int, bool);


// wait_for_think_finished() waits for main thread to go to sleep then returns
/*
呼んでいるのはbenchmark関数、start_thinking関数、UCI::loop関数のコマンドループを終了した時点で呼ばれる
MainThreadのthinkingがfalseになったら（つまり終了したら）
MainThreadを寝かせる
*/
void ThreadPool::wait_for_think_finished() {

  std::unique_lock<std::mutex> lk(main()->mutex);
  sleepCondition.wait(lk, [&]{ return !main()->thinking; });
}


// start_thinking() wakes up the main thread sleeping in MainThread::idle_loop()
// so to start a new search, then returns immediately.
/*
go関数->MainThread::idle_loop関数->think関数->id_loop関数->search関数と呼ばれるようになっている
*/
void ThreadPool::start_thinking(const Position& pos, const LimitsType& limits,const std::vector<Move>& searchMoves, StateStackPtr& states) {
	/*
	ここでwait_for_think_finishedを呼んでいるのは探索を実行する前にスレッド完全に初期化させるためでは？
	*/
	wait_for_think_finished();
	/*
	経過時間の測定基準点
	*/
  SearchTime = Time::now(); // As early as possible
	/*
	Signals.stopOnPonderhitは不明
	Signals.firstRootMoveは探索の最初の手順が行われたかのフラグ
	*/
  Signals.stopOnPonderhit = Signals.firstRootMove = false;
  /*
	Signals.stopは探索を停止するためのフラグなのでここでfalseに設定
	Signals.failedLowAtRootはWinodw探索のLow失敗になるとtrueになる
	*/
	Signals.stop = Signals.failedLowAtRoot = false;
	
	/*
	RootMovesはvector変数でRootMoveクラスを配列で保持している
	vectorのclaer関数でクリア
	*/
  RootMoves.clear();
	/*
	局面を渡される
	*/
  RootPos = pos;
	/*
	探索制限を設定、Limits変数はグローバル変数
	*/
  Limits = limits;
	/*
	states.get()はunique_ptr(スマートポインタ）が持っている関数でスマートポインタが保持しているポインタを返す
	http://cpprefjp.github.io/reference/memory/unique_ptr/get.html
	つまりポインタが有効であればSetupStatesに引数のstatesがムーブされる
	*/
  if (states.get()) // If we don't set a new position, preserve current state
  {
      SetupStates = std::move(states); // Ownership transfer here
      assert(!states.get());
  }
	/*
	RootMoveはクラスでそれをRootMovesというvectorに格納している
	渡されたMove形式の指し手はこれまたRootMoveクラス内にあるstd::vector<Move> pv変数に入れている

	合法手リストを作ってsearchMovesが空もしくはsearchMovesの着手リストと合法手が同じなら
	RootMoves着手リストに追加する
	(searchMovesはユーザーが指定した指し手をコマンドラインから指定した着手リスト)
	*/
	for (const ExtMove& ms : MoveList<LEGAL>(pos))
      if (searchMoves.empty() || std::count(searchMoves.begin(), searchMoves.end(), ms.move))
          RootMoves.push_back(RootMove(ms.move));

  main()->thinking = true;
  main()->notify_one(); // Starts main thread
}
