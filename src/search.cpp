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
用語集
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
	ponderとは
	先読みのこと
	たとえばcomputer側が先手で、開始局面から探索を開始して76歩とかに決めてUSIプロトコルで bestmove 76fu
	と送る時、先読みをしたい時相手が指すであろう手を予想してbest move 76fu ponder 34fuと送る
	エンジンの相手（人間であれコンピュータであれUSIインターフェイスを備えているもの）はエンジンが予想した局面を
	生成してエンジン側に送り返す。エンジン側はすぐに予想手で生成された局面を探索する。
	相手側が手を指した場合指した手が予想手と一緒だった場合相手側はponderhitを返してくるエンジンがすでに探索が終了していれば
	bestmove で手を返せばよいし、探索途中ならそのまま探索を継続すればよいponderhitを返した時点で手番が変わっている
	もし予想手と違えば相手側はstopをエンジンにおくり、再度予想手ではない真の指し手（相手の手）で更新された局面を送ってきて
	goコマンドを送ってくるのでエンジン側は通常探索をすればOK


	ponderhitコマンドを受けとったあとの挙動
	- すでに探索を終えている場合
	　すでに探索をおえている場合はthink関数の終わりのほうで止まっている。そこにUCIがponderhitコマンドを送ってくると
	  Search::Signals.stopOnPonderhitがtrueになっているのでSignals.stopフラグがtrueになりwait関数を抜けてUCIに
	  指し手を返すことができる。この時もbestmove XX ponder XXと返すのでずーっとponder探索を繰り返すことができる。

	 - 探索途中であった場合
	 　UCI側がponderhitコマンドを送ってくるとSearch::Limits.ponder = falseとする。つまりponderhitコマンドを
		受け取った瞬間からエンジン側に手番が移動しておりそのまま通常探索に移行するのである。
		＜Limits.ponder=trueとfalseでは探索にどう挙動が違うのか＞
			+ ponderがtrueになっている場合色々な探索制限の条件が成立してもSignals.stopがtrueにならず
			Signals.stopOnPonderhitがtrueになるだけで済む。Signals.stopOnPonderhit変数は探索の挙動には
			関係なくponderがtrueになると（相手手番の時間を利用しているので）制限がなく自由に探索しているといえる
			だからponderhitiを受け取ったら探索に制約がついたという意味で通常の探索になる

		先読み手が異なった場合はUCI側はstopコマンドを送ってくる。コマンドはUCI::loopで受け取られSearch::Signals.stop = trueとする。
		＜Search::Signals.stop = trueとなった場合の探索の挙動＞
		id_loop関数で随時この変数がチエックされており、trueになったら即探索ループを抜ける。RootMoves[0]にとりあえず登録してある手を返すが
		これはponder手が正しいことを前提とした手なのでUCI側で破棄される。
		UCI側はponder手が間違っていることを知っているのでponder手ではない手で更新された局面をpositionコマンドで送ってきたあと
		go ponderコマンドを送ることでエンジンに再度探索を要求する。

	＜将棋所HPより＞
	これを使うときは、必ずgo ponderというように、goのすぐあとにponderを書くことになります。
	ponderという言葉は、辞書では「熟考」と訳されていますが、思考ゲームにおいては、相手の手番中に次の手を考える「先読み」を意味します。
	go ponderは、先読みを開始する合図となります。（先読みを開始すべき局面は、この前にpositionコマンドによって送られてきています。）
	エンジンは、go ponderによって思考を開始する場合、GUI側から次のコマンド（stopまたはponderhit）が送られてくる前にbestmoveで
	指し手を返してはいけません。（たとえ、思考開始の時点で詰んでいるような場合であったとしてもです。）相手が手を指すと、それによって
	stopまたはponderhitが送られて来るので、それを待ってからbestmoveで指し手を返すことになります。（この辺の流れついては、
	後述する「対局における通信の具体例」を読んで下さい。）

	＜「対局における通信の具体例」＞
	次に、先読み機能を説明します。
	エンジンが後手で、先手が平手初期局面から１六歩と指した局面であれば
	>position startpos moves 1g1f
	>go
	エンジンはbestmoveコマンドで指し手を返しますが、この時に先読み要求を出すことができます。エンジンの指し手が4a3bで、
	それに対する相手の指し手を6i7hと予想したのであれば
	<bestmove 4a3b ponder 6i7h
	GUIはこれを受信すると、すぐにpositionコマンドで思考開始局面を送ります。この局面は、現在局面に、エンジンが予想した相手の指し手
	（この場合は6i7h）を追加したものになります。それに続けてgo ponderコマンドを送ります。
	>position startpos moves 1g1f 4a3b 6i7h
	>go ponder
	エンジンはこれを受信すると先読みを開始します。goコマンドの解説にも書きましたが、go ponderによって先読みを開始した場合、
	次にGUIからstopまたはponderhitが送られてくるまで、エンジンはbestmoveを返してはいけません。相手が次の手を指す前に思考が
	終わったとしても、GUIからstopまたはponderhitが送られてくるまで待つことになります。
	やがて、相手が手を指します。その手がエンジンの予想手と一致した場合と、そうでない場合で動作が異なります。
	エンジンの予想手が外れた場合
	この場合、GUIはエンジンにstopを送ります。
	>stop
	エンジンはこれに対し、思考中ならすぐに思考を打ち切って、現時点で最善と考えている手をbestmoveで返します。既に思考が終わっていたなら、
	探索済みの指し手をbestmoveで返します。（bestmoveのあとにponderで相手の予想手を追加しても構いませんが、いずれにしろ無視されます。）
	<bestmove 6a5b ponder 4i5h
	この、stopに対してbestmoveで返された指し手は、外れた予想手（この場合は6i7h）に対する指し手なので、GUIはこの内容を無視して、
	正しい相手の指し手（現在局面）を送ります。続けてgoコマンドも送ります。相手が7g7fと指したのであれば
	>position startpos moves 1g1f 4a3b 7g7f
	>go
	エンジンはこれによって通常の思考を開始します。
	エンジンの予想手が当たった場合
	この場合、GUIはエンジンにponderhitを送ります。
	>ponderhit
	予想手が当たったので、エンジンは引き続き思考を継続して構いません。既に思考が終わっていたら、すぐにその指し手を返すこともできます。
	bestmoveで指し手を返すとき、前回と同様にponderを追加して先読み要求を出すこともできます。
	<bestmove 6a5b ponder 4i5h
	以下、同様にして対局が継続されます。

	Q1 最初にponderを使用すると決めるオプションはどこにある。
	ucioption.cpp内にponderオプションにtrueが与えられている
	このオプションがtrueならoptimumSearchTimeに少し上乗せされる
	*/

	/*
	SignalsTypeは構造体
		bool stopOnPonderhit
		bool firstRootMove
		bool stop
		bool failedLowAtRoot;

	stopOnPonderhitponder UCI側がponderhitコマンドを送ってきた時trueになる
	firstRootMove	探索の最初の手順
	Signals.stopは探索を止めるフラグ
	failedLowAtRootはWinodw探索のLow失敗になるとtrueになる.初期設定はstart_thinking関数内でfalseに設定
	*/
	volatile SignalsType Signals;
	/*
	探索の制限項目（uciオプションで受け取る）
	*/
  LimitsType Limits;
	/*
	ルートでの着手リスト
	*/
	std::vector<RootMove> RootMoves;
	/*
	探索開始局面
	*/
	Position RootPos;
	/*
	ルートでの手番
	*/
	Color RootColor;
	/*
	探索に要した時間をミリセコンドで計測
	start_thinking関数内で現在時間を入れておき、探索終了後の時間と差し引きすることで経過時間を計測する
	*/
	Time::point SearchTime;
	/*
	応手手順を入れる、最初の局面がファイルなどから読み込んだ場合positionクラスにも同じSetupStateがあり
	そのSetupStatesからコピーを受け取る
	*/
	StateStackPtr SetupStates;
}	//namespace Search終了

using std::string;
using Eval::evaluate;
using namespace Search;

namespace {

  // Set to true to force running with one thread. Used for debugging
	/*
	通常はfalseとしマルチスレッドで探索、trueはデバック用途で１スレッドで探索
	*/
  const bool FakeSplit = false;

  // Different node types, used as template parameter
	/*
	search関数のテンプレート引数でノードの種別を表す
	Root:ルート局面の探索
	PV:最適応手手順用の探索ノード
	NonPV:現時点で不明
	SplitPointRoot:ルート局面での探索分岐になったノード
	SplitPointPV:現時点で不明
	SplitPointNonPV:現時点で不明
	*/
	enum NodeType { Root, PV, NonPV, SplitPointRoot, SplitPointPV, SplitPointNonPV };

  // Dynamic razoring margin based on depth
	/*
	https://chessprogramming.wikispaces.com/Razoring
	Razoring枝刈りの時のマージンを計算する、深度が深くなるとマージンは小さくなる
	sockfishのsearch関数内ではdepthは減っていく
	*/
	inline Value razor_margin(Depth d) { return Value(512 + 16 * int(d)); }

  // Futility lookup tables (initialized at startup) and their access functions
	/*
	search::init()で初期化される
	*/
	int FutilityMoveCounts[2][32]; // [improving][depth]
	/*
	枝刈り（Futility Pruning）のときのマージンを決める　100*depth
	*/
	inline Value futility_margin(Depth d) {
    return Value(100 * int(d));
  }

  // Reduction lookup tables (initialized at startup) and their access function
	/*
	Reduction（縮小？）
	用途不明
	*/
	int8_t Reductions[2][2][64][64]; // [pv][improving][depth][moveNumber]
	/*
	Search関数の枝刈り部から呼ばれる
	*/
	template <bool PvNode> inline Depth reduction(bool i, Depth d, int mn) {

    return (Depth) Reductions[PvNode][i][std::min(int(d) / ONE_PLY, 63)][std::min(mn, 63)];
  }

	/*
	PVSize=最善応手手順数を入れる（MultiPV),PVIdx=複数応手手順のインデックス
	*/
	size_t PVSize, PVIdx;
	/*
	時間制御？
	*/
	TimeManager TimeMgr;
	/*
	root局面で最善手を変えた回数
	探索時間制御で使用ー回数が多いと時間延長する
	*/
	double BestMoveChanges;
	/*
	用途不明
	*/
	Value DrawValue[COLOR_NB];
	/*
	Historyはクラスでプライベート変数にValue table[pieceType][SQ]を持っている
	最初はid_loop関数内で0クリアしupdate関数で更新する
	駒が移動した先の座標に得点が与えられ一種の位置評価で多くの駒が移動するほど高得点
	駒の移動履歴のようなもの
	*/
	HistoryStats History;
	/*
	用途不明
	*/
	GainsStats Gains;
	/*
	用途不明
	*/
	CountermovesStats Countermoves;
	/*
	一般探索関数
	*/
	template <NodeType NT>
  Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);
	/*
	たぶん末端用探索関数
	*/
	template <NodeType NT, bool InCheck>
  Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth);
	/*
	無限ループを持っておりこのid_loop関数からsearch関数を呼ぶ
	idle_loop関数->think関数->id_loop関数->search関数と呼ばれるようになっている
	main関数でThreads.init()を呼んでnew_thread関数->thread_create関数->start_routine関数->idle_loop関数で一旦
	sleep状態に遷移する
	UCIからのコマンドgoによりstart_thking関数からsleep状態を解除しidle_loop関数から探索が開始される
	*/
	void id_loop(Position& pos);
	/*
	用途不明
	*/
	Value value_to_tt(Value v, int ply);
	/*
	用途不明
	*/
	Value value_from_tt(Value v, int ply);
  bool allows(const Position& pos, Move first, Move second);
  bool refutes(const Position& pos, Move first, Move second);
	/*
	用途不明
	*/
	string uci_pv(const Position& pos, int depth, Value alpha, Value beta);
	/*
	スキルレベルの管理（ユーザーのchessスキル）
	最高スキルが20最低スキルが0でスキルレベルに合わせて手を調整する
	どうやって調整するかと言うと反復深化深度とスキルレベルがあった時に
	pick_move関数を呼びこのクラスのMove bestに良い手登録しておく
	そこで反復深化はやめず、そのまま探索は続けさせておくがskill変数が
	id_loop関数を抜ける時デストラクタが呼ばれ深い深度で得られた最善手を（RootMoves[0]）
	をbest手と交換することによって若干弱い手を採用する
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
main関数から呼ばれている
search系の初期化
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
	Reductions配列の中身をファイルに書き出してExcellでグラフ化
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
	Excellで計算させてみた結果
	FutilityMoveCounts[0]={2,3,3,4,5,6,8,10,12,14,16,19,22,25,28,31,35,39,43,47,51,56,60,65,70,75,81,86,92,98,104,110}
	FutilityMoveCounts[1]={3,4,5,7,8,11,13,16,19,22,25,29,33,38,42,47,52,57,63,69,75,81,88,94,101,109,116,124,131,140,148,156}
	２次曲線になる
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
同名の関数Search::perftから呼ばれる
展開できるノードの数を返す。
benchmarkから使用される
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
idle_loop関数->think関数->id_loop関数->search関数と呼ばれるようになっている
グローバル変数のRootPos変数はwait_for_think_finished関数で現在の局面をコピーしてもらっている
*/
void Search::think() 
{

  static PolyglotBook book; // Defined static to initialize the PRNG only once

  RootColor = RootPos.side_to_move();
	/*
	時間制御？
	*/
	TimeMgr.init(Limits, RootPos.game_ply(), RootColor);
	/*
	ルートでの合法手の手がなければ
	UCIにinfoコマンドで通達して
	finalize:に飛ぶ
	UCIプロトコルにはエンジン側から負けを通知するコマンドがないようです
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
	Limits.infiniteはgo ponderコマンドのあとにinfiniteオプションをつけると
	stopが掛けられるまで無制限探索を続ける。
	Limits.mateもgo ponderのオプション、王手を探索させる x moveで手数を制限につける
	Limits.mateにその手数が入っている

	定跡Bookを使用するなら（デフォルトはfalse）手を探し、
	その手がRootMoves配列にあればその手をRootMovesの先頭に行ってfinalizeラベルに
	移動すること。つまり探索せず定跡手を優先のこと
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
	Contempt Factor=軽蔑要因?　＝＞デフォルトでは0 取れる値は-50 -> 50
	UCI_AnalyseMode=UCI 解析モード？　＝＞デフォルトではfalse
	引き分けと判断する評価値（閾値）を決める。

	対等の相手なら閾値はVALUE_DRAW=0
	相手が強かったらしき
	PHASE_MIDGAME=128
	game_phase関数は局面の評価値を正規化（評価値を0-128にする）した値にして返す
	ｃｆで決めた評価値をVALUE_DRAW(=0）に加算する
	このDrawValue[]は局面の評価値を見て引き分け状態か判定するものではない
	引き分けと判断された時に返す仮の評価値
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
	Options["Write Search Log"]はデフォルトでfalse
	Limits.time White,Blackそれぞれの持ち時間
	Limits.inc　winc,binc(単位m sec)詳細不明
	Limits.movestogo 探索に制限を設けるもののようだが詳細不明
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
	threadはメインスレッドと探索スレッド(start_routineスレッド)とtimerスレッドがある模様
	あと探索中に複数のスレッドで探索木を探索する手法が実装されている
	*/
	for (Thread* th : Threads)
      th->maxPly = 0;

  Threads.sleepWhileIdle = Options["Idle Threads Sleep"];
  Threads.timer->run = true;
  Threads.timer->notify_one(); // Wake up the recurring timer
	/*
	探索を開始
	*/
	id_loop(RootPos); // Let's start searching !

  Threads.timer->run = false; // Stop the timer
  Threads.sleepWhileIdle = true; // Send idle threads to sleep
	/*
	searchのログを記録するオプションがtrueであればデフォルトではfalse
	ファイル名はSearchLog.txtになる
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
	ponder探索中ならstopOnPonderhitをtrueに、通常探索ならstopをtrueに
	UCIからponderhitコマンドは来ないのか->くるUCI::loopでponderhitコマンドを受け取ったら
	Signals.stopをtrueにして探索を止める処理に入る。
	ここはponder探索中全ての探索が終了したがUCI側が思考を終えていないのにエンジン側が先に指し手を返すのを防ぐための処理
	UCIがstopコマンド（UCIが思考終了のフラグ）かponderhitコマンドを出すのを待っている処理（wait_for関数で待機させる）
	*/
  if (!Signals.stop && (Limits.ponder || Limits.infinite))
  {
      Signals.stopOnPonderhit = true;
      RootPos.this_thread()->wait_for(Signals.stop);
  }

  // Best move could be MOVE_NONE when searching on a stalemate position
	/*
	UCIに探索結果を返している,ponderを許可しているときとそうでない時の場合分けをしていると思ったが
	そのようなことはしていない。常にbest move とponder手（最善応手手順の相手の手pv[1]を返している）
	結果を受け取ったUCI側が判断してponder手を処理しているのかも
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
	think関数から呼び出されている
	ここからsearch関数をNodeType(Root, PV, NonPV)を設定して呼び出す
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
		デフォルトならMultiPVに１を返す（MultiPVを２以上にすると、複数の応手手順を許す）
		デフォルトならskill.levelに２０を返す
		*/
		PVSize = Options["MultiPV"];
		Skill skill(Options["Skill Level"]);

		// Do we have to play with skill handicap? In this case enable MultiPV search
		// that we will use behind the scenes to retrieve a set of possible moves.
		/*
		デフォルトならskill.levelは20になるのでskill.enabled()はfalseをかえすのでMultiPVは1のまま
		但しskill.levelが20未満であれば、PVSize最低でも４以上になる
		*/
		if (skill.enabled() && PVSize < 4)
			PVSize = 4;
		/*
		応手手順はRoot局面の可能指し手数より大きくはできない
		*/
		PVSize = std::min(PVSize, RootMoves.size());

		// Iterative deepening loop until requested to stop or target depth reached
		/*
		反復深化法（Iterative deepening loop）
		指定の深度（MAX_PLY）まで達するか、stopが掛るまで反復探索を行う
		Limits.depthはUCIプロトコルから探索深度を指定してあればそちらに従うがしかしMAX_PLYより大きな深度は意味なし
		depth=1から開始されるMAX_PLYは120
		*/
		printf("Signals.stop = %d\n", Signals.stop);
		printf("Limits.depth = %d\n", Limits.depth);

		while (++depth <= MAX_PLY && /*!Signals.stop && 一時的にコメントアウト2015/5/9*/ (!Limits.depth || depth <= Limits.depth))
		{
			// Age out PV variability metric
			/*
			最初は0.0に初期化（このid_loop関数の冒頭で）
			*/
			BestMoveChanges *= 0.8;

			// Save last iteration's scores before first PV line is searched and all
			// the move scores but the (new) PV are set to -VALUE_INFINITE.
			/*
			RootMovesはコンストラクタのときprevScore変数,score変数とも
			-VALUE_INFINITE(32001)に初期設定されている
			prevScoreはこのあとで変更されるのでここで再初期化されるのかな

			RootMoves自体はstart_thinking関数で初期化されている
			このfor文記法はC++11から
			*/
			for (RootMove& rm : RootMoves)
				rm.prevScore = rm.score;

			// MultiPV loop. We perform a full root search for each PV line
			//複数の応手手順数（MultiPVが有効なら４以上MultiPVがfalseなら1）だけ繰り返す
			for (PVIdx = 0; PVIdx < PVSize /*&& !Signals.stop一時的にコメントアウト2015/5/9*/; ++PVIdx)
			{
				// Reset aspiration window starting size
				/*
				prevScoreは-32001なので
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
				aspiration windowという手法らしい、探索窓を狭くして探索速度を上げるが
				探索失敗（真の評価値が窓の外にあった場合fail high/low)の時、再探索コストが掛るのがデメリット
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
					RootMoves配列を安定ソートを使っている
					ソート数によりインサートソートとマージソートを使い分けているが
					比較関数を指定していないので標準のless関数で比較しているが
					標準のless関数からRootMovesのbool operator<(const RootMove& m) const { return score > m.score; }
					を使ってscore同士を比較している
					*/
					std::stable_sort(RootMoves.begin() + PVIdx, RootMoves.end());

					// Write PV back to transposition table in case the relevant
					// entries have been overwritten during the search.
					/*
					得られたｐｖ（最善手手順）をTTに登録している
					*/
					for (size_t i = 0; i <= PVIdx; ++i)
						RootMoves[i].insert_pv_in_tt(pos);

					// If search has been stopped break immediately. Sorting and
					// writing PV back to TT is safe becuase RootMoves is still
					// valid, although refers to previous iteration.
					/*
					stopがかかればこの永久ループからでる
					*/
					if (Signals.stop)
						break;

					// When failing high/low give some update (without cluttering
					// the UI) before to research.
					/*
					high/low失敗したとき
					uci_pv関数の内容を標準出力に出す
					*/
					if ((bestValue <= alpha || bestValue >= beta) && Time::now() - SearchTime > 3000)
						sync_cout << uci_pv(pos, depth, alpha, beta) << sync_endl;

					// In case of failing low/high increase aspiration window and
					// research, otherwise exit the loop.
					/*
					Low失敗した場合の再探索のための評価値設定
					*/
					if (bestValue <= alpha)
					{
						/*
						alpha値を返ってきた値からさらにdelta(16)下げる、つまりWindowを広げて再探索する、ただしVALUE_INFINITEよりは下げない
						*/
						alpha = std::max(bestValue - delta, -VALUE_INFINITE);
						/*
						failedLowAtRootはLow失敗のフラグ
						check_time関数内で使用されている
						*/
						Signals.failedLowAtRoot = true;
						Signals.stopOnPonderhit = false;
					}
					/*
					High失敗した場合の再探索のための評価値設定
					alphaの反対でbeta値をdelta(16)だけ上げる、つまりWindowを広げて再探索する、ただしVALUE_INFINITEよりは上げない
					*/
					else if (bestValue >= beta)
						beta = std::min(bestValue + delta, VALUE_INFINITE);
					/*
					Low,High失敗がないので真の評価値がWindow内で返ってきたので次の反復深化に移る
					*/
					else
						break;
					/*
					Low,High失敗した場合は１６から
					16->24.0->36.0->54.0->81.0->121.5
					と徐々にdeltaを広げて失敗しない探索を行う
					*/
					delta += delta / 2;

					assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
				}//while(true)終了

				// Sort the PV lines searched so far and update the GUI
				/*
				RootMoveをRootMoveクラスのscore値で安定ソートする
				*/
				std::stable_sort(RootMoves.begin(), RootMoves.begin() + PVIdx + 1);

				if (PVIdx + 1 == PVSize || Time::now() - SearchTime > 3000)
					sync_cout << uci_pv(pos, depth, alpha, beta) << sync_endl;
			}//MultiPV終了

			// Do we need to pick now the sub-optimal best move ?
			/*
			スキルレベルが20未満 かつ　time_to_pick 関数でdepth（反復深化深度）がスキルレベルと同じならtrueそれ以外は
			falseを返す。スキルレベルは最高で２０最低が０なのでスキルレベルが下がればpick_move関数を呼ぶ深度は浅くなる
			skill.pick_move関数はbest手を返すようになっているがそれは使われていない
			このバージョンではpick_move関数は残っているが将来のバージョンのstockfishでは消されている
			*/
			if (skill.enabled() && skill.time_to_pick(depth))
				skill.pick_move();
			/*
			search Logが設定してあれば(デフォルトではfalse）SearchLog.txtにログを残す
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
			Limits.mateは王手を探す手を制限するオプションで
			ここにかいてある条件が成立したら探索中止であるが
			その条件の意味がよくわからん
			*/
			if (Limits.mate && bestValue >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - bestValue <= 2 * Limits.mate)
				Signals.stop = true;

			// Do we have time for the next iteration? Can we stop searching now?
			/*
			探索にLimitsによる制限、探索を停止するstopフラグなどが掛っていなければ
			*/
			if (Limits.use_time_management() && !Signals.stop && !Signals.stopOnPonderhit)
			{
				bool stop = false; // Local variable, not the volatile Signals.stop

				// Take in account some extra time if the best move has changed
				/*
				BestMoveChangesはroot局面で最善手が変更になった回数
				その回数が多ければ危険な局面と判断して時間制御を伸ばす
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
					stopシグナルが来ていてponder中ならstopOnPonderhitをtrueにして探索停止する
					ponderでなければ通常の停止フラグをセット
					*/
					if (Limits.ponder)
						Signals.stopOnPonderhit = true;
					else
						Signals.stop = true;
				}
			}
		}//反復深化終了
	}


  // search<>() is the main search function for both PV and non-PV nodes and for
  // normal and SplitPoint nodes. When called just after a split point the search
  // is simpler because we have already probed the hash table, done a null move
  // search, and searched the first move before splitting, we don't have to repeat
  // all this work again. We also don't need to store anything to the hash table
  // here: This is taken care of after we return from the split point.
	/*
	NodeTypeとは？　
	SpNodeとは　splitのSpかも（探索分岐）
	id_loopからsearch関数を呼ぶ時はNodeType=Root,SpNode=falseで呼ばれる
	Depth depthは反復深化のたびに2->4->6と増えていく

	探索中search,qsearch関数内ではdepthはONE_PLY(=2)づつ減っていき
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
		ここはノードの初期化？
		*/
		Thread* thisThread = pos.this_thread();
		/*
		pos.checkers()は手番側のKINGに王手をかけている駒のbitboardを返す
		つまりinCheckがtrueなら手番側に王手がかかっている
		*/
    inCheck = pos.checkers();
		/*
		Root、PvNodeの時はここはとおらない
		探索分岐するときにとおると思われる
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
		ssは最初の２つはカットしてindex２からsearch関数に渡される
		ここでstruct Stackの残りの初期化（探索分岐に関係のない部分）
		ss->currentMoveは現在着目されている指し手を示す指し手であるが最初はMOVE_NONE
		(ss+1)->excludedMoveこの指し手は他の兄弟手より異常に点数が高く地平線効果が疑われるような危険な手
		ss->plyは一つ前の深度に+１する。
		(ss+1)->skipNullMove = falseはnull_moveの時２手連続でnull_moveにならないようにするフラグ、なので１つ次の深度になっている
		(ss+1)->reduction = DEPTH_ZERO;は不明、reduction（削減）
		moveCountは不明
		quietCountは不明
		(ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONEキラー手を初期化しているがなぜ２手先のキラー手を初期化する
		*/
		moveCount = quietCount = 0;
    bestValue = -VALUE_INFINITE;
    ss->currentMove = threatMove = (ss+1)->excludedMove = bestMove = MOVE_NONE;
    ss->ply = (ss-1)->ply + 1;
    (ss+1)->skipNullMove = false; (ss+1)->reduction = DEPTH_ZERO;
    (ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE;

    // Used to send selDepth info to GUI
		/*
		PｖNodeでかつこのスレッドが持っているmaxPlyよりss->plyが深い場合ss->plyに合わせる
		*/
		if (PvNode && thisThread->maxPly < ss->ply)
        thisThread->maxPly = ss->ply;

		/*
		RootNode以外のみ
		あとでコメント入れ
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
		ttMoveとはトランスポジションテーブルから取り出した指し手
		但しルートノードではRootMoves[PVIdx].pv[0]から取り出した指し手
		これはルートノードではまだトランスポジションテーブルに手が登録されていないからかな
		Rootノードでなくトランスポジションテーブルに手がなかった場合はMOVE_NONE
		トランスポジションテーブルの手の評価値はvalue_from_tt関数で設定、手自体がなかった場合はVALUE_NONE
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
		用途不明
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
		//Step5はこの局面の静止評価値をトランスポジションテーブルのエントリーと比較して決める
		//さらにこの局面の親のGainを更新する（Gainはなんなのかわかっていない）
		/*
		王手がかかているならmoves_loppラベルにとんで探索を始めろ
		ここからmoves_loppラベルまでは枝刈りの処理なので王手がかかっている場合は無意味
		*/
		if (inCheck)
    {
        ss->staticEval = eval = VALUE_NONE;
        goto moves_loop;
    }
		/*
		王手がかかっていなくて定跡手があり、その評価値がVALUE_NONEなら現局面の評価値を
		evalとss->staticEvalに与える。
		*/
		else if (tte)
    {
        // Never assume anything on values stored in TT
        if ((ss->staticEval = eval = tte->eval_value()) == VALUE_NONE)
            eval = ss->staticEval = evaluate(pos);

        // Can ttValue be used as a better position evaluation?
				/*
				トランスポジションテーブルに登録してあったエントリーの評価値が現局面の評価値より大きくてトランスポジションテーブルに
				登録された時のbound値がBOUND_LOWER(下限値）ならトランスポジションテーブルの評価値を信用する

				トランスポジションテーブルに登録してあったエントリーの評価値が現局面の評価値より小さくトランスポジションテーブルに
				登録された時のbound値がBOUND_UPPER(上限値）ならトランスポジションテーブルの評価値を採用する
				*/
        if (ttValue != VALUE_NONE)
            if (tte->bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER))
                eval = ttValue;
    }
    else
    {
				/*
				現局面がトランスポジションテーブルのエントリーになかった場合評価値は評価関数を呼んで設定する
				と同時にトランスポジションテーブルに現局面を登録しておく、この時評価値はVALUE_NONEに設定してあるのは
				この局面の探索がまだ済んでいなく（静止評価値はss->staticEvalで与えてあるこの静止評価値を取り出す関数はtte->eval_value()）
				*/
        eval = ss->staticEval = evaluate(pos);
        TT.store(posKey, VALUE_NONE, BOUND_NONE, DEPTH_NONE, MOVE_NONE, ss->staticEval);
    }
		/*
		直前の手が捕獲手でない（手の指し手パターンがノーマルであることをチエックしているのならこのチエックはいらないのでは）
		現局面の静止評価がVALUE_NONEでない
		ひとつ前の手の静止評価値がVALUE_NONEでない
		一つ前の手をmoveに代入しておきさらにそれがMOVE_NULLでないこと（null_moveでない）
		直前の手が捕獲でもなく只の移動であったとき

		Gainsのアップデートを行う、特定の駒種がto座標に移動することによって生じた評価値が以前の評価値より高ければその評価値にupdateする
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
		Razoring枝刈り条件
		PvNodeでないこと
		残り深さが4*ONE_PLY(=2)をきっていること（そういう末端に近いノードをプレフロンティアノードと呼んでいる？）
		現在の評価値にマージン（razor_margin=512+depth*16 残り深さが少なくなっていくつまり末端になっていくほど枝刈りの条件は緩くなる）
		トランスポジションテーブルの手がない
		ベータ値がVALUE_MATE_IN_MAX_PLY(=29900)より小さい
		手番側のPAWNがあと一歩でQUEENにならない

		ベータ値よりマージン値（razor_margin）だけ下げてqsearch関数を呼び出している（窓を狭くして）
		つまり本来であればあと数手探索木を展開してqsearch関数を呼び出さなくてはならないが
		現在の評価値がベータ値より離れていれば省略して良いという枝切り？
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
		Futility Pruning は，チェスで広く用いられている
		枝刈り手法である．本来末端において判定されるαβ
		法の枝刈り条件の判定をその親ノード (frontier node)
		で仮の値を用いて行うことにより，不要な静止探索
		ノードの展開と静的評価関数の呼び出しを削減する．

		すなわち，親ノード P における評価値に，指し手 m
		に対して可変なマージン値 Vdiff(m) を加え，なお仮
		の最小値に満たない場合は p 以下を枝刈りすることが
		可能となる．最適な Vdiff(m) の値は評価関数によっ
		て異なり，小さいほど枝刈りが有効に働く．
		http://www-als.ics.nitech.ac.jp/paper/H18-B/kanai.pdf

		evalは現局面の評価値これよりfutility_margin関数が返すマージンを引いた値
		が予想評価値でこの予想評価値がbeta値を超えるのでカットする

		PvNodeでないこと
		この枝刈りは残り深さが2*7 = 14 より小さいこと（末端に近いこと）が条件のひとつ
		現在の評価値（あくまで静止評価値、探索後の評価値ではない）がfutility_margin値を引いてもベータ値より高いー＞ならベータカットしてもOK
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
		ヌルムーブ（枝刈り）
		ヌルムーブの条件：
		PvNodeでないこと
		前の手がnull_Moveでないこと（２回null_moveでは意味がない）
		のこり探索深さが2*ONE_PLYより多きこと、つまり末端局面（フロントノード）以外でヌルムーブＯＫ
		non_pawn_materialはPAWN以外の駒評価値の合計を返すー＞つまり駒が極端にすくなった局面ではないことー＞駒が少ない局面でnull_Moveすると極端な評価になる
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
				Null　Moveの探索深度を決める
				*/
				Depth R = 3 * ONE_PLY + depth / 4;

        // Null move dynamic reduction based on value
				/*
				現在の静止評価値からPawn１個分の駒評価値を引いてもベーター値より十分大きいなら探索深さをONE_PLY増やす
				浅い探索だとnull_moveが影響してbetacutしてしまうので慎重にもう一段深くする？
				*/
        if (eval - PawnValueMg > beta)
            R += ONE_PLY;

				/*
				パスの手を実行（局面は更新しない）
				*/
				pos.do_null_move(st);
				/*
				パスの手の次はパスしない
				*/
				(ss + 1)->skipNullMove = true;
				/*
				ヌルムーブの深度が現在の残りの深度より深いならqsearch関数（末端探索）そうでなければsearch関数（一般探索）で
				探索する
				現在手番(null move)->相手手番(通常move)->現在手番(null move)

				*/
				nullValue = depth - R < ONE_PLY ? -qsearch<NonPV, false>(pos, ss + 1, -beta, -alpha, DEPTH_ZERO)
                                      : - search<NonPV>(pos, ss+1, -beta, -alpha, depth-R, !cutNode);
				/*
				skipNullMoveをもとに戻す
				null moveを元に戻す
				*/
				(ss + 1)->skipNullMove = false;
        pos.undo_null_move();
				/*
				手をパスしてもその評価値がbetaより大きいなら通常に手を指し手も
				beta Cutを起こすと推測される
				*/
				if (nullValue >= beta)
        {
            // Do not return unproven mate scores
            if (nullValue >= VALUE_MATE_IN_MAX_PLY)
                nullValue = beta;
						/*
						残り深さが12*ONE_PLAYより小さいなら遠慮なく枝切り（末端局面に近いなら）
						*/
						if (depth < 12 * ONE_PLY)
                return nullValue;

            // Do verification search at high depths
						/*
						残り深さが12*ONE_PLYより高い位置でnull_moveに成功した場合
						再度探索させている
						*/
						ss->skipNullMove = true;
            Value v = search<NonPV>(pos, ss, alpha, beta, depth-R, false);
            ss->skipNullMove = false;
						/*
						条件を変えて再探索して,それでもbeta値を超えるようであれば遠慮なくNull Move Cut
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
						null_moveに失敗した場合
						(ss+1)->currentにはこちらの優位をひっくり返す手（敵側の）が入っているはずなので
						その手をthreatMove（脅威の手）に入れておく
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
		ProbCut とはオセロプログラムLogistello の開発者M.Buro 発案の前向き枝刈手法で、発展版のMulti-ProbCut という手法もある。

		基本アイデアは、浅い探索は深い探索の近似になるということで、深い探索をする前に浅い探索を行い、結果が探索窓から
		外れたらカットしても良いんじゃない？という手法。

		残り深さが既定のd になったら、深さd' の探索を行う
		評価値にマージンm を取り、探索窓から外れていないかチェック
		外れた=>カット
		外れない=>深さd の探索を行う
		なお、窓から外れた外れないのチェックは、null-window search が賢く速い。

		細かいことを考えずに、いい加減なコードを書く(なお、D_probcut はProbCut
		を行うとあらかじめ決めておいた深さで、上でいうd, D_shallow は浅い探索の深さで、上でいうd')。
		http://d.hatena.ne.jp/tawake/20060710/1152520755 引用箇所
		カットというのが
		if (value >= rbeta)
		return value;
		の部分だと判断される
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
				pos.captured_piece_type()はとった駒種（do_move関数でst->capturedTypeに登録される)
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
		用途不明
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
		countermovesはここで初期化されている
		*/
		Square prevMoveSq = to_sq((ss - 1)->currentMove);
    Move countermoves[] = { Countermoves[pos.piece_on(prevMoveSq)][prevMoveSq].first,
                            Countermoves[pos.piece_on(prevMoveSq)][prevMoveSq].second };
		/*
		着手リスト生成、一般探索で使用される指し手オーダリング。
		*/
		MovePicker mp(pos, ttMove, depth, History, countermoves, ss);
    CheckInfo ci(pos);
    value = bestValue; // Workaround a bogus 'uninitialized' warning under gcc
		/*
		局面が2手前より優位になっているか判断（静止評価値で判断）して良くなっているならtrue
		*/
    improving =   ss->staticEval >= (ss-2)->staticEval
               || ss->staticEval == VALUE_NONE
               ||(ss-2)->staticEval == VALUE_NONE;
		/*
		Singular Extensionとはそのノードの評価値が兄弟ノードの評価値より大きい場合地平線
		効果などが疑われるのでそのノードの探索を延長する手法
		探索延長が有効になる条件はRootNodeではないこと　＆＆　
		SpNodeではないこと（SpNodeは探索分岐のこと？）　＆＆
		depthが8*ONE_PLYより大きいこと（つまりRootNodeに近く、末端近くではないこと　＆＆
		!excludedMoveは他の兄弟手に比べ異常に評価値が高く地平線効果が疑われるような危険な手でないこと　＆＆
		トランスポジションテーブルの評価値が下限値　＆＆
		トランスポジションテーブルの指し手の深度が現在の深度より3*ONE_PLAY引いたものよりおおきいこと

		兄弟のノードの評価値とか全然出てこないのは何故
		*/
		singularExtensionNode = !RootNode
                           && !SpNode
                           &&  depth >= 8 * ONE_PLY
                           &&  ttMove != MOVE_NONE
                           && !excludedMove  // Recursive(再帰的) singular search is not allowed(許された)->再帰的なシンギラー拡張は許されない？
                           && (tte->bound() & BOUND_LOWER)
                           &&  tte->depth() >= depth - 3 * ONE_PLY;

    // Step 11. Loop through moves
    // Loop through all pseudo-legal moves until no moves remain or a beta cutoff occurs
		/*
		ここからがメインの探索
		*/
		while ((move = mp.next_move<SpNode>()) != MOVE_NONE)
    {
      assert(is_ok(move));

			/*
			用途不明
			excludedMoveのexcludedは遮断する、拒否するという意味
			excludedMoveは他の兄弟手より評価値が高く地平線効果が疑われる手なのでそのような手はパスして次の兄弟手へ
			*/
			if (move == excludedMove)
          continue;

      // At root obey the "searchmoves" option and skip moves not listed in Root
      // Move List, as a consequence any illegal move is also skipped. In MultiPV
      // mode we also skip PV moves which have been already searched.
			/*
			RootNodeモードでnext_move関数がRootMovesにない手をだしてきたらそれはパスする
			RootMovesにある手しか読まない
			*/
			if (RootNode && !std::count(RootMoves.begin() + PVIdx, RootMoves.end(), move))
          continue;
			/*
			RootNodeのときはSpNodeはfalse
			SpNodeは探索分岐のこと？
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
			RootNodeモード専用
			RootNodeで第1手目のときSignals.firstRootMoveをtrueにする
			用途不明
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
			指し手パターンがノーマルで敵KINGへの利きを邪魔している駒がない場合
			移動後に敵KINGに利きをきかす駒のbitboardをgivesCheckに与えるgivesCheckはbool型なのでbitboardがあればtrueになる、もちろんなしということもある
			そうでなければ gives_check関数を呼び王手がないか厳重にチエックする
			*/
			givesCheck = pos.gives_check(move, ci);
			/*
			dangerous自体は「危険な」と言う意味
			王手が可能な手　OR　指し手パターンがNORMAL以外(PROMOTION,ENPASSANT,CASTLING)
			PAWNがRANK4以上の位置にいる.(RANK4はWHITE側からみた位置、BLACKからみるとRANK5以下）
			*/
			dangerous = givesCheck
                 || pos.passed_pawn_push(move)
                 || type_of(move) == CASTLE;

      // Step 12. Extend checks
			/*
			王手が可能な状態にある(givesCheck=true),静止探索しても評価値が0以上である
			ことを条件にONE_PLYだけ探索延長する
			*/
			if (givesCheck && pos.see_sign(move) >= 0)
          ext = ONE_PLY;

      // Singular extension search. If all moves but one fail low on a search of
      // (alpha-s, beta-s), and just one fails high on (alpha, beta), then that move
      // is singular and should be extended. To verify this we do a reduced search
      // on all the other moves but the ttMove, if result is lower than ttValue minus
      // a margin then we extend ttMove.
			/*
			Singular extension searchとは
			そのノードの評価値が兄弟ノードの評価値より大きい場合地平線
			効果などが疑われるのでそのノードの探索を延長する手法

			ここのif文で別探索を現深度の半分で行っている
			この部分での評価値によってext変数（深度延長変数）
			にONE_PLY延長をかけている部分ではないか
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
					地平線効果が疑われる手はexcludedMoveとして登録しておき、この手が見つかった時はパスできる。
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
			最終ここがSingular extensionの探索延長をかけているところ
			強制的にONE_PLY引いているのは何故
			*/
			newDepth = depth - ONE_PLY + ext;

      // Step 13. Pruning at shallow depth (exclude PV nodes)
			/*
			枝刈り
			PｖNodeではない　＆＆
			王手がかかっていない　＆＆（王手がかかっているようなノードを枝刈りしては危険）
			王手が可能な手等重要な手ではない　＆＆
			bestValue > VALUE_MATED_IN_MAX_PLY(= マイナス31880)極端に評価値が悪いわけではないがよくもない？
			*/
			if (!PvNode
          && !captureOrPromotion
          && !inCheck
          && !dangerous
          &&  bestValue > VALUE_MATED_IN_MAX_PLY)
      {
          // Move count based pruning
					/*
					残り深さが16より小さくて、手数がFutilityMoveCounts[improving][depth]より多い
					FutilityMoveCounts[improving][depth]配列はdepthが深くなっていくと累乗関数のように増えていくつまりdepthが増えていけば
					枝刈りの閾値も上がるということ
					脅威手がない（手番側の良い評価一気にひっくり返すような手が存在するとき登録される 脅威手はmoveが指された後に指される手）

					つまり、手数も深度も深く読んだが大した手ではなさそうなので枝刈りする
					*/
					if (depth < 16 * ONE_PLY
              && moveCount >= FutilityMoveCounts[improving][depth]
              && (!threatMove || !refutes(pos, move, threatMove)))
          {
              if (SpNode)
                  splitPoint->mutex.lock();
							/*
							この手をパスして次の兄弟に行く
							*/
              continue;
          }
					/*
					reductionはmoveCount(1～63までの数を取る）が大きくなるほど大きな数を返す(0～18程度）
					またdepth（１～６３までの数をとる）が大きいほど大きな数を返す
					つまりたくさん読めば残り深さを減らしてくれる、depthが大きいとき（まだ浅い読みの時）ときも
					たくさん減らしてくれる。
					Move count based pruningは横方向の枝刈り、このreductionは縦方向の枝刈りをしてくれる
					*/
					predictedDepth = newDepth - reduction<PvNode>(improving, depth, moveCount);

          // Futility pruning: parent node
					/*
					https://chessprogramming.wikispaces.com/Futility+Pruning
					 Futility枝刈りとはおおざっぱな評価値にマージンを持たせそのマージンがalpha-betaの範囲に入っていなかったら
					 枝刈りする手法
					 おおざっぱな評価値＝ss->staticEvalこの局面の駒評価値だけの評価値
					 futility_margin関数＝マージン futility_margin関数=渡されたdepthに100をかけているだけ
					 そうして得られたfutilityValueがalpha値を下回るならこの手はパスして次の兄弟にいく
					 Gainsクラスは配列ではないように見えるが内部にtable[][]配列を保持しており[]演算子オーバーロードで
					 そのtable配列の値を返す
					 Gainsは駒種ごとの、移動した座標ごとの直前評価値との差分評価値です
					 Futility枝刈りのマージンに追加している,Gainsの利用は、ここだけ
					 マージンを大きくすれば枝刈りしやすく、小さくすれば枝刈りしにくい
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
					残り深さが４と末端局面で、静止探索した結果取り合い負けしているならこの手は
					あきらめて次の兄弟にいく枝刈り
					*/
					if (predictedDepth < 4 * ONE_PLY && pos.see_sign(move) < 0)
          {
              if (SpNode)
                  splitPoint->mutex.lock();

              continue;
          }

      }
			/*
			ここまで枝刈り
			*/

      // Check for legality only before to do the move

			/*
			合法手であるかのチエック、合法手でなければこのノードはパス
			何故ここでチエックなのかもっと早くできないのかな
			*/
			if (!RootNode && !SpNode && !pos.legal(move, ci.pinned))
      {
          moveCount--;
          continue;
      }
			/*
			PvNodeノードでかつ第１手目ならpvMoveをtrueにしておく
			第１手目なので必ず通ってほしいルートに設定してある
			*/
      pvMove = PvNode && moveCount == 1;
			/*
			ssは1手先の局面から前の手の参照によくつかわれる
			*/
      ss->currentMove = move;
			/*
			captureOrPromotion　=　捕獲もしくは成るではない　
			quietCountはsearch関数の冒頭で0セットされていてこの部分だげでインクリメントされている
			つまり捕獲しない、成らない＝穏やかな手を６４手までquietsSearched配列に登録しておける
			quietsSearched配列はsearch関数の局所変数、値を更新しているのはここだけ
			後でHistory配列（移動履歴評価）の得点を下げるのに使用される
			*/
      if (!SpNode && !captureOrPromotion && quietCount < 64)
          quietsSearched[quietCount++] = move;

      // Step 14. Make the move
			/*
			ここで局面更新
			*/
			pos.do_move(move, st, ci, givesCheck);

      // Step 15. Reduced depth search (LMR). If the move fails high will be
      // re-searched at full depth.
			/*
			https://chessprogramming.wikispaces.com/Late+Move+Reductions
			LRMは探索深さを短縮することで枝刈りを行う。
			LRMは全てのノードで実施するのではなく
				- 残り深さが３以上　
				- pvMoveノードないこと
				- この局面での指し手が置換表から得た手ではないこと
				- キラー手ではないこと
			つまりそれほど重要そうな手ではないこと
			このLRM枝刈りでないときはdoFullDepthSearch（フルDepthをする）=trueとなる。但しPvNodeで第一手めのときは
			falseになる。
			*/
			if (depth >= 3 * ONE_PLY
          && !pvMove
          && !captureOrPromotion
          &&  move != ttMove
          &&  move != ss->killers[0]
          &&  move != ss->killers[1])
      {
					/*
					improving,depth,moveCountパラメータを指定してReductions配列の値を取ってくる
					Reductions配列の形はReductions.xlsxにグラフ化してある
					depth,moveCountが増えると段階的にss->reductionに返す数値が大きくなる
					つまり浅い探索段階ではそれほどからない、moveCountが少ない場合もそんなにたくさん刈らない
					*/
          ss->reduction = reduction<PvNode>(improving, depth, moveCount);
					/*
						- PvNodeノードの時ではない
						- cutNode=id_loop関数から初期値falseで渡される

						cutNode変数を使用しているのはsearch関数の中で１か所だけで
						LRM枝切りのところでPvNodeでないこととcutNodeがtrueであれば探索深さをONE_PLY追加で削減できる。
						つまりcutNodeとはLRMをより強化するためのフラグ
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
			ここが下の階層に降りて行くところ
			qsearch関数かsearch関数かを選択している
			SpNode＝探索分岐、doFullDepthSearch＝探索深さの削減をせず通常の深さ探索をする時のフラグ
			doFullDepthSearchはseach関数の自動変数でbool型でstep15で設定されている
			*/
			if (doFullDepthSearch)
      {
          if (SpNode)
              alpha = splitPoint->alpha;
					/*
					newDepth < ONE_PLYが成立すれば
					givesCheck ? -qsearch<NonPV,  true>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO) : -qsearch<NonPV, false>(pos, ss+1, -(alpha+1), -alpha, DEPTH_ZERO)
					を実行する
					newDepth < ONE_PLYが成立しなければ
					-search<NonPV, false>(pos, ss+1, -(alpha+1), -alpha, newDepth, !cutNode)
					を実行する。
					つまり新しく設定された探索深さがONE_PLY(2)より小さい場合はqsearch関数を
					ONE_PLYより大きい場合はsearch関数を呼ぶ
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
			newDepth < ONE_PLYが成立すれば（つまりあと１手で末端局面ならqsearch関数）
			givesCheck ? -qsearch<PV,  true>(pos, ss+1, -beta, -alpha, DEPTH_ZERO) : -qsearch<PV, false>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
			を実行し
			newDepth < ONE_PLYが成立しなければ（つまりまだ末端局面ではないなら）
			-search<PV, false>(pos, ss+1, -beta, -alpha, newDepth, false)
			を実行する
			つまり新しく設定された探索深さがONE_PLY(2)より小さい場合はqsearch関数を
			ONE_PLYより大きい場合はsearch関数を呼ぶ
			上と同じような構造であるが、違うのはNonPVかPVかの違いだと思う
			*/
			if (PvNode && (pvMove || (value > alpha && (RootNode || value < beta))))
          value = newDepth < ONE_PLY ?
                          givesCheck ? -qsearch<PV,  true>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
                                     : -qsearch<PV, false>(pos, ss+1, -beta, -alpha, DEPTH_ZERO)
                                     : - search<PV>(pos, ss+1, -beta, -alpha, newDepth, false);
      // Step 17. Undo move
			/*
			ここで局面復元
			*/
			pos.undo_move(move);

      assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

      // Step 18. Check for new best move
			/*
			探索分岐しているThreadならここで共有データからbestValueとalpha値をもらっておく
			まだ詳細不明
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
			探索中止なら評価値を持って返ります　もしくはcutoff_occurred関数が返す値がtrueなら返ります
			cutoff_occurred関数の機能は不明
			探索の中止はUCIインターフェイスからの探索中止コマンドかcheck_time関数で時間制限に引っ掛かったらなる
			*/
			if (/*Signals.stop || 一時的にコメントアウト*/thisThread->cutoff_occurred())
          return value; // To avoid returning VALUE_INFINITE
			/*
			ノードがRootNodeであったら現在の指し手がroot局面の着手リストにあればそれをrmに保存する
			*/
      if (RootNode)
      {
          RootMove& rm = *std::find(RootMoves.begin(), RootMoves.end(), move);

          // PV move or new best move ?
					//pvMoveまたはalpha値を更新したら
					if (pvMove || value > alpha)
          {
              rm.score = value;
              rm.extract_pv_from_tt(pos);

              // We record how often the best move has been changed in each
              // iteration. This information is used for time management: When
              // the best move changes frequently, we allocate some more time.
							/*
							最善応手が変更になった回数をカウントしておき時間制御に使用する
							（あまりにも煩雑に最善手が変わるようなら危険な局面として慎重に探索する必要がある）
							BestMoveChangesはid_loopで0に初期化されている
							*/
              if (!pvMove)
                  ++BestMoveChanges;
          }
          else
              // All other moves but the PV are set to the lowest value, this
              // is not a problem when sorting becuase sort is stable and move
              // position in the list is preserved, just the PV is pushed up.
							/*
							pvMoveでなくalpha値でもない手は最低の評価値を与えておく
							*/
              rm.score = -VALUE_INFINITE;
      }

      if (value > bestValue)
      {
					/*
					SpNodeならbestValueを超えた評価値は共有データのbestValueを更新しておく
					bestValueとalpha,beta,valueの関係は
					*/
          bestValue = SpNode ? splitPoint->bestValue = value : value;

          if (value > alpha)
          {
							/*
							valueがalpha値を更新したらこの指し手を共有データに登録すると同時にbestMoveにも登録
							*/
              bestMove = SpNode ? splitPoint->bestMove = move : move;
							/*
							- PvMoveである
							- beta値を超えない
							- alpha-beta窓のなかでalpha値を更新したらこの指し手の評価値をalpha値にする
							  同時に共有データのalhpa値も更新する
							*/
              if (PvNode && value < beta) // Update alpha! Always alpha < beta
                  alpha = SpNode ? splitPoint->alpha = value : value;
              else
              {
									/*
									そうではない、alpha-beta窓を超えた場合はbeta cutを起こさせる
									whileループを出て後始末をしてこのノードをでて親の局面に帰っていく
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
			まず最初にMainThreadがここにくる
			MainThreadはPvNodeなのでSpNodeではなく　かつ　反復深化の深さがThreads.minimumSplitDepth(=8)より深いこと（浅い探索だと探索分岐の効果が少ない？）

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
    }//ここがメイン探索の終了
		/*
		1つの親の全部の子供の探索が（探索分岐も含めて）終了したらここにくる
		SpNodeのスレッドならここに到達したらbestValueを返してThread::idle_loop関数に返る
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
		moveCountが0ということは合法手がなかった＝チエックメイト　か　ステイルメイト
		excludedMove手は地平線効果が疑われる手であるが他に手がないならその手を返す（excludedMove手は他の兄弟手より評価が高いので評価値はalpha値かな）
		そんな手もなく王手がかかっていたら詰みの評価値を返す、王手も掛っていない＋合法手がない＝ステルメイトなので引き分け
		*/
		if (!moveCount)
        return  excludedMove ? alpha
              : inCheck ? mated_in(ss->ply) : DrawValue[pos.side_to_move()];

    // If we have pruned all the moves without searching return a fail-low score
    if (bestValue == -VALUE_INFINITE)
        bestValue = alpha;
		/*
		トランスポジションテーブルに登録
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
				bestmove（一番良い手）はHistory配列（移動履歴評価）の評価を高め
				そうではない穏やかな手（捕獲しない、成らない平凡な手）はquietsSearched配列に
				登録してあるのでその移動履歴評価を下げておく
				*/
        Value bonus = Value(int(depth) * int(depth));
        History.update(pos.moved_piece(bestMove), to_sq(bestMove), bonus);
        for (int i = 0; i < quietCount - 1; ++i)
        {
            Move m = quietsSearched[i];
            History.update(pos.moved_piece(m), to_sq(m), -bonus);
        }
				/*
				prevMoveSqは直前の敵駒の移動先の座標が入っている
				*/
        if (is_ok((ss-1)->currentMove))
            Countermoves.update(pos.piece_on(prevMoveSq), prevMoveSq, bestMove);
    }

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);
		/*
		返る
		*/
		return bestValue;
  }//ここがsearch関数の終了


  // qsearch() is the quiescence search function, which is called by the main
  // search function when the remaining depth is zero (or, to be more precise,
  // less than ONE_PLY).
	/*
	末端局面専用探索関数のはず
	search関数に比べるとだいぶ行数が少ない
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
		探索分岐ならalphaをoldalphaに退避？
		*/
		if (PvNode)
        oldAlpha = alpha;
		/*
		bestMove,ss->currentMoveを初期化
		*/
    ss->currentMove = bestMove = MOVE_NONE;
    ss->ply = (ss-1)->ply + 1;

    // Check for an instant draw or maximum ply reached
		/*
		引き分け判定して引き分け　OR 最大深度まできたなら　評価値をもらって帰る
		*/
		if (pos.is_draw() || ss->ply > MAX_PLY)
        return DrawValue[pos.side_to_move()];

    // Decide whether or not to include checks, this fixes also the type of
    // TT entry depth that we are going to use. Note that in qsearch we use
    // only two types of depth in TT: DEPTH_QS_CHECKS or DEPTH_QS_NO_CHECKS.
		/*
		用途不明
		*/
		ttDepth = InCheck || depth >= DEPTH_QS_CHECKS ? DEPTH_QS_CHECKS
                                                  : DEPTH_QS_NO_CHECKS;

    // Transposition table lookup
		/*
		トランスポジションテーブルに手があるか調べる、あればttMoveに手を入れておく
		*/
		posKey = pos.key();
    tte = TT.probe(posKey);
    ttMove = tte ? tte->move() : MOVE_NONE;
    ttValue = tte ? value_from_tt(tte->value(),ss->ply) : VALUE_NONE;
		/*
		トランスポジションテーブルの手の評価値が真値ならトランスポジションテーブルの評価値を返す
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
		この状態で王手がかかっている場合はマイナス32001を返す
		*/
		if (InCheck)
    {
        ss->staticEval = VALUE_NONE;
        bestValue = futilityBase = -VALUE_INFINITE;
    }
    else
    {
			/*
			王手がかかっていない場合
			*/
			if (tte)
        {
            // Never assume anything on values stored in TT
						/*
						用途不明
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
		ここで着手リストをMovePickrerにつくらせる
		このコンストラクタはqsearch専用です。
		最後の引数のsqには１手前の敵の駒が移動した升の座標
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
	用途不明
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
	トランスポジションテーブルから取り出した指し手についていた評価値と、トランスポジションテーブルを呼び出したときの
	探索深さを引数として、再評価した評価値を返す
	評価値がVALUE_NONEならVALUE_NONEのまま、VALUE_MATE_IN_MAX_PLY（29900）より大きければ
	評価値より現在深度を引く、VALUE_MATED_IN_MAX_PLY(-29900)より小さければ現在深度を評価値に加算する
	29900>v>-29900ならそのまま評価値を返す
	しかし29900のような大きな数値に現在深度を表すような小さな数字を加算、減算してなにか効果があるのか不明
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
	firstで示される指し手でsecondで示される手を防御できるか判定する
	もうちょっと詳しく調べる
	firstは最初に指される手で、secondはその後に指される手でこの関数の機能はseconnd手がfirst側にとって脅威と
	なる場合first手がその脅威手の対応手となるか判定するもの
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
		first駒を取る手をあらかじめ防いでいるならtrue
		*/
    if (m1from == m2to)
        return true;

    // If the threatened piece has value less than or equal to the value of the
    // threat piece, don't prune moves which defend it.
		/*
		脅威手が捕獲手でかつ　捕獲する駒の評価値が捕獲される駒の価値より大きい（大駒で小駒を取っている)　｜｜　捕獲する駒種がKING
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
				first駒の移動後その利きがsecond駒を捕獲することが可能ならtrue
				*/
        if (pos.attacks_from(pc, m1to, occ) & m2to)
            return true;

        // Scan for possible X-ray attackers behind the moved piece
				/*
				脅威手の移動先座標からROOKとBISHOPの利き上にfirst側のQUEEN,ROOK,BISHOPがいたらxrayに入れる
				*/
        Bitboard xray =  (attacks_bb<  ROOK>(m2to, occ) & pos.pieces(color_of(pc), QUEEN, ROOK))
                       | (attacks_bb<BISHOP>(m2to, occ) & pos.pieces(color_of(pc), QUEEN, BISHOP));

        // Verify attackers are triggered by our move and not already existing
				/*
				よくわからん、脅威の駒を手番側のROOKかBISHOPで捕獲可能ならtrue?
				*/
        if (unlikely(xray) && (xray & ~pos.attacks_from<QUEEN>(m2to)))
            return true;
    }

    // Don't prune safe moves which block the threat path
		/*
		first駒の移動によってsecond駒の移動を阻止できかつ静止探索の結果が0以上であればtrue
		*/
    if ((between_bb(m2from, m2to) & m1to) && pos.see_sign(first) >= 0)
        return true;
		/*
		脅威手を防御できなければfalseを返す
		*/
    return false;
  }


  // When playing with strength handicap choose best move among the MultiPV set
  // using a statistical rule dependent on 'level'. Idea by Heinz van Saanen.
	/*
	クラスSkillのデストラクタで呼ばれる、id_loop関数から呼ばれる
	レベル(スキルレベル)に応じて指し手を変える（最善手ではなく）
	*/
	Move Skill::pick_move() 
	{

    static RKISS rk;

    // PRNG sequence should be not deterministic
		/*
		乱数をランダムな回数（時刻で決める　５０未満）
		*/
    for (int i = Time::now() % 50; i > 0; --i)
        rk.rand<unsigned>();

    // RootMoves are already sorted by score in descending order
    int variance = std::min(RootMoves[0].score - RootMoves[PVSize - 1].score, PawnValueMg);
		/*
		デフォルトのlevelは20なのでweakness=120-2*20=80
		*/
    int weakness = 120 - 2 * level;
    int max_s = -VALUE_INFINITE;
    best = MOVE_NONE;

    // Choose best move. For each move score we add two terms both dependent on
    // weakness, one deterministic and bigger for weaker moves, and one random,
    // then we choose the move with the resulting highest score.
		/*
		マルチ最善応手数だけRootMoves配列をチエックして最も評価値が高い手をbest手に登録する
		*/
    for (size_t i = 0; i < PVSize; ++i)
    {
        int s = RootMoves[i].score;

        // Don't allow crazy blunders even at very low skills
				/*
				RootMoves配列に並んでいる指し手リストのスコアが１つ前のスコアよりPawn駒評価値２個分以上離れている場合
				このループを終了させる
				*/
        if (i > 0 && RootMoves[i-1].score > s + 2 * PawnValueMg)
            break;

        // This is our magic formula
				/*
				ランダムな要素も取り入れてs変数にスコアを入れる
				*/
        s += (  weakness * int(RootMoves[0].score - s)
              + variance * (rk.rand<unsigned>() % weakness)) / 128;
				/*
				ｓが十分大きかったらbestにRootMoves[i]番目の手を入れる
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
	詳細不明であるが,UCI向けに現段階の局面情報（score,nodes,npsなど）を出力する
	id_loop関数からのみ呼び出される
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
				PVIdxはこのファイル内のグローバル変数なので関数内でもアクセスできる
				PVIdxはマルチPVのインデックス、updateフラグは今のPVIdxより小さいindexは表示
				済みなのでパスするフラグ、このフラグがtrueなら表示、falseなら非表示
				*/
        bool updated = (i <= PVIdx);

        if (depth == 1 && !updated)
            continue;

        int d   = updated ? depth : depth - 1;
        Value v = updated ? RootMoves[i].score : RootMoves[i].prevScore;
				/*
				return count of buffered input characters
				in_avail関数は文字数を返す？
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
				move_to_uci関数はMove形式の指し手データを棋譜形式の文字列にする
				RootMoves配列の中に入っている手をマルチPV数だけ出力する
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
search関数からのみ呼ばれる
PV登録、評価値更新時に呼ばれる
pv[0]の手を取り出してその手を実行し局面を更新する
探索の結果が置換表にあるので（あるはず）その手を取り出しつつ
合法手チエックもしながらpv（最応手手順）を再構築している
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
ここにくるタイミングは反復深化でsearch関数から返ってきたところなので
pvには１つづのRootMoveに対して最善手手順が登録されている
ここではｐｖに入っている手をTT（トランスポジションテーブル）に登録している
初手だけではなくPV全てTTに登録している
*/
void RootMove::insert_pv_in_tt(Position& pos) 
{

  StateInfo state[MAX_PLY_PLUS_6], *st = state;
  const TTEntry* tte;
  int ply = 0;
	/*
	TTはトランスポジションテーブルを表すグローバル変数
	pv:（最善手手順）を入れる為の１次元の可変長配列で１つのRootMoveクラスに格納されている
	*/
	do {
      tte = TT.probe(pos.key());
			/*
			渡された局面で、TTに登録された手がないまたはあってもｐｖの手とは異なるとき
			ｐｖ手をTTに登録する
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
Thread::split関数から呼ばれる
optionのo["Threads"]を複数にしないと呼ばれない、デフォルトは1
つまり通常では呼ばれない
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
					split関数で目覚めさせられたスレーブスレッドは全員mutexをロックして待機、
					そしてMainThreadがsplit関数でThreads.mutexをunlockするのを待つ
					*/
          Threads.mutex.lock();

          assert(searching);
          assert(activeSplitPoint);
          SplitPoint* sp = activeSplitPoint;

          Threads.mutex.unlock();

          Stack stack[MAX_PLY_PLUS_6], *ss = stack+2; // To allow referencing (ss-2)
					/*
					ここで渡されたpositionクラスをコピーしてスレッドに渡す(activePositionにアドレスを渡す）
					スレッドごとにコピーを渡す
					*/
          Position pos(*sp->pos, this);

          std::memcpy(ss-2, sp->ss-2, 5 * sizeof(Stack));
          ss->splitPoint = sp;

          sp->mutex.lock();

          assert(activePosition == nullptr);

          activePosition = &pos;
					/*
					nodeTypeに応じて分岐する
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
					探索分岐したスレッドはここに帰ってくる。serach関数が返す評価値を全然利用していないのは
					探索中に得た評価値はすべて共有データーに更新したからと思われる
					*/
          assert(searching);

          searching = false;
          activePosition = nullptr;
					/*
					自分のスレッドIDを消している
					*/
          sp->slavesMask &= ~(1ULL << idx);
					/*
					探索分岐が探索したノード数を合算している
					*/
          sp->nodes += pos.nodes_searched();

          // Wake up master thread so to allow it to return from the idle loop
          // in case we are the last slave of the split point.
					/*
					MainThreadを起こしておく
					- Threads.sleepWhileIdleはThink関数で探索前にfalseとしてしまうのでここが実行されることはない
					- このスレッドがMainThreadではない（探索分岐スレッド）
					- このスレッドが最後のスレッドなら
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
			while永久ループの出口の一つ
			全てのスレッドがなくなったらこのidle_loop関数から脱出できる
			*/
      if (this_sp && !this_sp->slavesMask)
      {
          this_sp->mutex.lock();
          bool finished = !this_sp->slavesMask; // Retest under lock protection
          this_sp->mutex.unlock();
          if (finished)
              return;
      }
  }//while(true)終了
}


/// check_time() is called by the timer thread when the timer triggers. It is
/// used to print debug info and, more important, to detect when we are out of
/// available time and so stop the search.
/*
TimerThread::idle_loop()からのみ呼ばれる
探索開始から所定の時間を超えたら（他にも条件はあるが詳細不明）探索停止のフラグを立てる
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
	SearchTimeはstart_thking関数で時刻をセットして、ここで経過時間elapsedを測る
	この経過時間がTimeMgr.available_time()に設定してある時間を超えたらなどの条件で
	探索停止のフラグを立てる
	停止の詳細は不明
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
