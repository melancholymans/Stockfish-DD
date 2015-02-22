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

#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "misc.h"
#include "types.h"

/// The TTEntry is the 128 bit transposition table entry, defined as below:
/*
全エントリーで128bit
*/
///
/// key: 32 bit							ハッシュキー
/// move: 16 bit						着手データ
/*
置換表に評価値を保存するときにその評価値がどのような評価値なのかも一緒に
保存する。ベータカットを起こした時の評価値はbetaから上に真の評価値があるので
置換表に保存された評価値は下限値(BOUND_LOWER)をしめしている
反対にアルファカットを起こした時はalphaか下側に真の評価値があるので
置換表に保存された評価値は上限値（BOUND_UPPER)を示している
おそらく真の評価値の時はBOUND_EXACTだと思われる
*/
/// bound type: 8 bit				評価値の種別(BOUND_LOWER,BOUND_UPPER,BOUND_EXACT)
/// generation: 8 bit				世代
/// value: 16 bit						評価値
/// depth: 16 bit						探索深さ
/// static value: 16 bit		不明（静的な評価値）
/// static margin: 16 bit		使われていない

struct TTEntry {
	/*
	エントリーに値を保存するメソッド関数
	*/
  void save(uint32_t k, Value v, Bound b, Depth d, Move m, int g, Value ev) {

    key32        = (uint32_t)k;
    move16       = (uint16_t)m;
    bound8       = (uint8_t)b;
    generation8  = (uint8_t)g;
    value16      = (int16_t)v;
    depth16      = (int16_t)d;
    evalValue    = (int16_t)ev;
  }
  void set_generation(uint8_t g) { generation8 = g; }
	/*
	エントリーの値を取り出すメソッド関数群
	*/
  uint32_t key() const      { return key32; }
  Depth depth() const       { return (Depth)depth16; }
  Move move() const         { return (Move)move16; }
  Value value() const       { return (Value)value16; }
  Bound bound() const       { return (Bound)bound8; }
  int generation() const    { return (int)generation8; }
  Value eval_value() const  { return (Value)evalValue; }

private:
  uint32_t key32;
  uint16_t move16;
  uint8_t bound8, generation8;
  int16_t value16, depth16, evalValue;
};


/// A TranspositionTable consists of a power of 2 number of clusters and each
/// cluster consists of ClusterSize number of TTEntry. Each non-empty entry
/// contains information of exactly one position. Size of a cluster shall not be
/// bigger than a cache line size. In case it is less, it should be padded to
/// guarantee always aligned accesses.
/*
置換エントリを管理するテーブル
*/
class TranspositionTable {

  static const unsigned ClusterSize = 4; // A cluster is 64 Bytes

public:
	~TranspositionTable() { free(mem); }
	/*
	用途不明
	*/
	void new_search() { ++generation; }
	/*
	置換表をkeyで探して,あればエントリーへのポインタを返す
	なければnullptrを返す（NULLではなくnullptrを返すのがC++11らしい）
	*/
	const TTEntry* probe(const Key key) const;
	TTEntry* first_entry(const Key key) const;
	void refresh(const TTEntry* tte) const;
	void set_size(size_t mbSize);
	void clear();
	void store(const Key key, Value v, Bound type, Depth d, Move m, Value statV);

	private:
	uint32_t hashMask;
	TTEntry* table;
	void* mem;
	uint8_t generation; // Size must be not bigger than TTEntry::generation8
};
/*
置換表のグローバル変数
*/
extern TranspositionTable TT;


/// TranspositionTable::first_entry() returns a pointer to the first entry of
/// a cluster given a position. The lowest order bits of the key are used to
/// get the index of the cluster.
/*
受け取ったkey(64bit)の下位32bitを使って最初のエントリーへのアドレスを取得して返す
*/
inline TTEntry* TranspositionTable::first_entry(const Key key) const {

  return table + ((uint32_t)key & hashMask);
}


/// TranspositionTable::refresh() updates the 'generation' value of the TTEntry
/// to avoid aging. Normally called after a TT hit.
/*
TranspositionTableの世代を設定値（generation）にする
*/
inline void TranspositionTable::refresh(const TTEntry* tte) const {
  const_cast<TTEntry*>(tte)->set_generation(generation);
}

#endif // #ifndef TT_H_INCLUDED
