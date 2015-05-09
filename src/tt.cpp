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
トランスポジションテーブルのグローバル変数
*/
TranspositionTable TT; // Our global transposition table


/// TranspositionTable::set_size() sets the size of the transposition table,
/// measured in megabytes. Transposition table consists of a power of 2 number
/// of clusters and each cluster consists of ClusterSize number of TTEntry.
/*
トランスポジションテーブルの初期化はmain関数から
TT.set_size(Options["Hash"])と呼ばれて初期化する
Options["Hash"]はデフォルトでは32M個のクラスタが用意される
取りえる値は1--16384Mクラスタ

このトランスポジションテーブルはリハッシュ回数がClusterSize=4のオープンハッシュテーブルで
実装されている
*/
void TranspositionTable::set_size(size_t mbSize) 
{

  assert(msb((mbSize << 20) / sizeof(TTEntry)) < 32);
	/*
	必要なメモリーを計算
	TTEEntry = 128bit = 16byte
	TTEntry[ClusterSize=4] = 16*4=64byte
	mbSize=32デフォルトのオプション値 << 20/64byte = 33554432/64 = 524288
	msb(524288) = 19
	4 << 19 = 2,097,152byte

	このメモリーで用意される置換表は
	TTEntryは16byteでこれが４つセット（１クラスタ）なので16*4=64byte
	2,097,152byte/64=32,768
	この32というのがオプションの３２のこと
	*/
  uint32_t size = ClusterSize << msb((mbSize << 20) / sizeof(TTEntry[ClusterSize]));
	/*
	要求されたメモリーが前とおなじならなにもせず帰る
	*/
  if (hashMask == size - ClusterSize)
      return;
	/*
	hashMaskは局面から算出するbit列（Zobristクラスが返す64bit列)を
	TranspositionTableのインデックスにする。
	具体的な数値はー＞1 1111 1111 1111 1111 1100bのようになる。
	下位2bitはクラスタが４つづつなので空いている
	Addres
	0		クラスタ1番 エントリ1目
	1		クラスタ1番 エントリ2目
	2		クラスタ1番 エントリ3目
	3		クラスタ1番 エントリ4目

	4		クラスタ2番 エントリ1目
	5		クラスタ2番 エントリ2目
	6		クラスタ2番 エントリ3目
	7		クラスタ2番 エントリ4目

	8		クラスタ3番 エントリ1目
	9		クラスタ3番 エントリ2目
	10	クラスタ3番 エントリ3目
	11	クラスタ3番 エントリ4目

	12		クラスタ3番 エントリ1目
	13		クラスタ3番 エントリ2目
	14	クラスタ3番 エントリ3目
	15	クラスタ3番 エントリ4目

	クラスタ1番目のエントリ１にアクセスするにはインデックスは0->0000b
	クラスタ2番目のエントリ１にアクセスするにはインデックスは4->100b
	クラスタ3番目のエントリ１にアクセスするにはインデックスは8->1000b
	クラスタ4番目のエントリ１にアクセスするにはインデックスは12->1100b
	のように４つ飛びごとにアクセスするために下位2bitはあけてある
	*/
  hashMask = size - ClusterSize;
  free(mem);
  mem = calloc(size * sizeof(TTEntry) + CACHE_LINE_SIZE - 1, 1);
	/*
	エラーメッセージを出して強制終了
	*/
  if (!mem)
  {
      std::cerr << "Failed to allocate " << mbSize
                << "MB for transposition table." << std::endl;
      exit(EXIT_FAILURE);
  }
	/*
	スマートポインタ(uintptr_t)を使っている
	*/
  table = (TTEntry*)((uintptr_t(mem) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1));
}


/// TranspositionTable::clear() overwrites the entire transposition table
/// with zeroes. It is called whenever the table is resized, or when the
/// user asks the program to clear the table (from the UCI interface).
/*
TranspositionTableをゼロクリアする
benchmark関数、UCIから呼ばれている（設定されているようだが
呼ばれていないような気がする）
*/
void TranspositionTable::clear() 
{

  std::memset(table, 0, (hashMask + ClusterSize) * sizeof(TTEntry));
}


/// TranspositionTable::probe() looks up the current position in the
/// transposition table. Returns a pointer to the TTEntry or NULL if
/// position is not found.
/*
指定されたkeyでTranspositionTableを探索して該当するエントリーが在ったらそのポインターを返す
なかったらnullptrを返す

Zobristクラスが返すbit列は64bitでその下位32bitをTranspositionTableのインデックスに使い
上位32bitをエントリーのkeyに使う

最初にfirst_entry関数を呼び出し下位32bitでテーブルのインデックスを作り、該当するクラスタの最初のアドレス返してもらう
クラウドの数だけ歩進（＝４）しそのエントリのkeyが上位32bitと一致していたらそのエントリーのアドレスを返す
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
TranspositionTableへの登録
局面をハッシュ化したkeyを受け取って上位32bitをクラスタの中のエントリーの識別子に使用
first_entry関数を呼んで下位32bitでTranspositionTableのインデックスを
作ってfirst_entry関数でクラスタの最初のエントリーアドレスをtteに入れる
順番にクラスタを検査して（ちゃんとエントリーは空いているか、もしくはすでに情報は入っているが
上位３２bitも同じ）上書きOKならそのエントリーを更新対象にする
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
			更新＆追加対象のエントリーの取捨選択
			*/
      c1 = (replace->generation() == generation ?  2 : 0);
      c2 = (tte->generation() == generation || tte->bound() == BOUND_EXACT ? -2 : 0);
      c3 = (tte->depth() < replace->depth() ?  1 : 0);

      if (c1 + c2 + c3 > 0)
          replace = tte;
  }
	/*
	エントリー更新
	*/
  replace->save(key32, v, b, d, m, generation, statV);
}
