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

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "bitcount.h"
#include "pawns.h"
#include "position.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Doubled pawn penalty by file
	/*
	同列にPAWNが２つ以上いると防御上の弱点とされる（ルール違反ではない）
	同一列にいるとPAWN同士で守ることができないから
	このpawns.cppに定義してあるevaluate関数で使用されている
	http://en.wikipedia.org/wiki/Doubled_pawns
	*/
  const Score Doubled[FILE_NB] = {
    S(13, 43), S(20, 48), S(23, 48), S(23, 48),
    S(23, 48), S(23, 48), S(20, 48), S(13, 43) };

  // Isolated pawn penalty by opposed flag and file
	/*
	孤立したpawnへのペナルティ
	このpawns.cppに定義してあるevaluate関数で使用されている
	http://en.wikipedia.org/wiki/Isolated_pawn
	*/
  const Score Isolated[2][FILE_NB] = {
  { S(37, 45), S(54, 52), S(60, 52), S(60, 52),
    S(60, 52), S(60, 52), S(54, 52), S(37, 45) },
  { S(25, 30), S(36, 35), S(40, 35), S(40, 35),
    S(40, 35), S(40, 35), S(36, 35), S(25, 30) } };

  // Backward pawn penalty by opposed flag and file
	/*
	PAWNの斜め利きから守られているPAWNは強いが
	守っている後ろのPAWN（Backward pawn）自体は弱い
	このpawns.cppに定義してあるevaluate関数で使用されている
	http://en.wikipedia.org/wiki/Backward_pawn
	*/
  const Score Backward[2][FILE_NB] = {
  { S(30, 42), S(43, 46), S(49, 46), S(49, 46),
    S(49, 46), S(49, 46), S(43, 46), S(30, 42) },
  { S(20, 28), S(29, 31), S(33, 31), S(33, 31),
    S(33, 31), S(33, 31), S(29, 31), S(20, 28) } };

  // Pawn chain membership bonus by file and rank (initialized by formula)
	/*
	下のURLに書いてあるような配置になればボーナスがでる
	https://chessprogramming.wikispaces.com/Pawn+chain
	*/
  Score ChainMember[FILE_NB][RANK_NB];

  // Candidate passed pawn bonus by rank
	/*
	用途不明だが、この評価項目はプラスされているのでペナリティではなくボーナスのようだ
	https://chessprogramming.wikispaces.com/Candidate+Passed+Pawn
	*/
  const Score CandidatePassed[RANK_NB] = {
    S( 0, 0), S( 6, 13), S(6,13), S(14,29),
    S(34,68), S(83,166), S(0, 0), S( 0, 0) };

  // Weakness of our pawn shelter in front of the king indexed by [rank]
	/*
	用途不明
	*/
  const Value ShelterWeakness[RANK_NB] =
  { V(100), V(0), V(27), V(73), V(92), V(101), V(101) };

  // Danger of enemy pawns moving toward our king indexed by
  // [no friendly pawn | pawn unblocked | pawn blocked][rank of enemy pawn]
	/*
	king周辺の危険、用途不明
	*/
  const Value StormDanger[3][RANK_NB] = {
  { V( 0),  V(64), V(128), V(51), V(26) },
  { V(26),  V(32), V( 96), V(38), V(20) },
  { V( 0),  V( 0), V( 64), V(25), V(13) } };

  // Max bonus for king safety. Corresponds to start position with all the pawns
  // in front of the king and no enemy pawn on the horizon.
	/*
	kingの安全？、用途不明
	*/
  const Value MaxSafetyBonus = V(263);

  #undef S
  #undef V
	/*
	PAWNは非対称な駒なので関数入口で方向子を揃えている
	手番のカラーはUs,敵はThem
	WHITEにとって：Upは座標値大きくなる方向、つまりBLACK側に向かっている
	RightはBLACK側に向かいながら右側（相手から見て右側？）
	LeftはBLACK側に向かいながら左側（相手から見て左側）

	BLACKにとってUpは座標が少なくなる方向、つまりWHITE側に向かっている
	RightはWHITE側に向かいながら右側（相手からみて右側？）
	leftはWHITE側に向かいながら左側（相手からみて左側？）

	ここでPAWN特有の評価項目を加算して（減算）していく
	最終value値に集約して返す
	*/
  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    const Color  Them  = (Us == WHITE ? BLACK    : WHITE);
    const Square Up    = (Us == WHITE ? DELTA_N  : DELTA_S);
    const Square Right = (Us == WHITE ? DELTA_NE : DELTA_SW);
    const Square Left  = (Us == WHITE ? DELTA_NW : DELTA_SE);

    Bitboard b;
    Square s;
    File f;
    bool passed, isolated, doubled, opposed, chain, backward, candidate;
    Score value = SCORE_ZERO;
		/*
		手番側のPAWNの座標のリスト
		*/
    const Square* pl = pos.list<PAWN>(Us);
		/*
		手番側のPAWNのbitboard
		*/
    Bitboard ourPawns = pos.pieces(Us, PAWN);
		/*
		敵側のPAWNのbitboard
		*/
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us] = e->candidatePawns[Us] = 0;
    e->kingSquares[Us] = SQ_NONE;
    e->semiopenFiles[Us] = 0xFF;
		/*
		手番側のPAWNを取る方向（両斜め前）に動かしたあとのbitboardを入れておく
		pawnAttacks=PAWNのアタッカー駒
		*/
    e->pawnAttacks[Us] = shift_bb<Right>(ourPawns) | shift_bb<Left>(ourPawns);
		/*
		DarkSquares（ダーク升：盤の升の色が暗い色になっている升のこと）にいる手番側のPAWNの数
		*/
    e->pawnsOnSquares[Us][BLACK] = popcount<Max15>(ourPawns & DarkSquares);
		/*
		DarkSquares（ダーク升）に乗っていない手番側のPAWNの数
		*/
    e->pawnsOnSquares[Us][WHITE] = pos.count<PAWN>(Us) - e->pawnsOnSquares[Us][BLACK];

    // Loop through all pawns of the current color and score each pawn
		/*
		手番側のPAWNの座標をs変数に入れて
		*/
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));
				/*手番側のPAWNの列番号*/
        f = file_of(s);

        // This file cannot be semi-open
				/*
				semiopenFiles[]配列の初期値は0xFF、８bitが全て立っている状態に設定される
				ここではPAWNがいる列のbitを反転させビットANDをとることでPAWNがいる列を０に
				いない列を１として表現している
				*/
        e->semiopenFiles[Us] &= ~(1 << f);

        // Our rank plus previous one. Used for chain detection
				/*
				PAWNがいる座標の行のbitboardとそのすぐ後ろの行（PAWNの進行方向とは逆）のbitboardをOR結合
				*/
        b = rank_bb(s) | rank_bb(s - pawn_push(Us));

        // Flag the pawn as passed, isolated, doubled or member of a pawn
        // chain (but not the backward one).
				/*
				pawn chainを検出
				chainに手番側のPAWNのbitboardと現在着目しているPAWNがいる列の両脇の列を示すadjacent_files_bbとPAWNがいる行の
				一つ後ろの行のbitboardとのbit ANDによって、注目しているPAWNの左斜め後ろか右斜め後ろにいるPAWNを入れる
				*/
        chain    =   ourPawns   & adjacent_files_bb(f) & b;
				/*
				isolated＝孤立したという意味
				注目しているPAWNの両脇にPAWNがいない時,isolated変数はtrueになり、いる時はfalseとなる
				*/
        isolated = !(ourPawns   & adjacent_files_bb(f));
				/*
				注目しているPAWNの前方に同カラーのPAWNがいたらtrue、いなかったらfalse
				*/
        doubled  =   ourPawns   & forward_bb(Us, s);
				/*
				doubled＝対立と言う意味
				注目しているPAWNの前方にいる敵PAWNがいればtrue,いなければfalse
				*/
        opposed  =   theirPawns & forward_bb(Us, s);
				/*
				手番側のPAWNが敵PAWNをとれるならfalse、取れないならtrue
				*/
        passed   = !(theirPawns & passed_pawn_mask(Us, s));

        // Test for backward pawn.
        // If the pawn is passed, isolated, or member of a pawn chain it cannot
        // be backward. If there are friendly pawns behind on adjacent files
        // or if can capture an enemy pawn it cannot be backward either.
				/*
				敵PAWNをとれない　||　味方のPAWNから孤立している（同じ行にいない）　||　後方のPAWNとチエインでつながっている　||
				注目のPAWNの後方に味方のPAWNがいる　||　敵のPAWNを取ることが可能
				ならbackward変数はfalse
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   | P |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   | P |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				|   |   |   |   |   |   |   |   |
				+---+---+---+---+---+---+---+---+
				P->PAWN
				*/
        if (   (passed | isolated | chain)
            || (ourPawns & pawn_attack_span(Them, s))
            || (pos.attacks_from<PAWN>(s, Us) & theirPawns))
            backward = false;
        else
        {
            // We now know that there are no friendly pawns beside or behind this
            // pawn on adjacent files. We now check whether the pawn is
            // backward by looking in the forward direction on the adjacent
            // files, and picking the closest pawn there.
					/*
					上のif文の条件が成立しない時（おそらくまだPAWNを余り動かしていなくPAWN同士が隣接しているような状態を想定？）
					注目しているPAWNの利きの中にいる全てのPAWN（敵、味方全て）をbitboardにいれ、その手番側からビットスキャンして
					そのPAWNがいる行bitboardと注目のPAWNの利きのビットアンドを取る
					ここの処理の目的がよくわからん
					https://chessprogramming.wikispaces.com/Backward+Pawn
					*/
            b = pawn_attack_span(Us, s) & (ourPawns | theirPawns);
            b = pawn_attack_span(Us, s) & rank_bb(backmost_sq(Us, b));

            // If we have an enemy pawn in the same or next rank, the pawn is
            // backward because it cannot advance without being captured.
						/*
						ここの処理もよくわからんが、最終的にbackward変数をtrueにするのかfalseにするのかが目的
						*/
            backward = (b | shift_bb<Up>(b)) & theirPawns;
        }

        assert(opposed | passed | (pawn_attack_span(Us, s) & theirPawns));

        // A not passed pawn is a candidate to become passed, if it is free to
        // advance and if the number of friendly pawns beside or behind this
        // pawn on adjacent files is higher or equal than the number of
        // enemy pawns in the forward direction on the adjacent files.
				/*
				（前方に敵PAWNがいる　||　直進できる　||　backward（？）　|| 孤立したPAWNである）でないこと　&&
				注目しているPAWNの直前にいる敵PAWNが味方のPAWNを取ることができる &&
				前方にいる敵PAWNの数より後方にいる味方PAWNの数が同数以上
				ならcandidateはtrue
				*/
        candidate =   !(opposed | passed | backward | isolated)
                   && (b = pawn_attack_span(Them, s + pawn_push(Us)) & ourPawns) != 0
                   &&  popcount<Max15>(b) >= popcount<Max15>(pawn_attack_span(Us, s) & theirPawns);

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate passed pawns. Only the frontmost passed
        // pawn on each file is considered a true passed pawn.
				/*
				敵PAWNを取れる　&&　味方同士のPAWNが縦列になっていないなら
				passedPawns bitboardに注目しているPAWNを追加する
				このbitboardはこの関数内では活用しないがevaluate.cppの関数で使用されている
				*/
        if (passed && !doubled)
            e->passedPawns[Us] |= s;

        // Score this pawn
				/*
				孤立したPAWNならIsolated配列に設定してある値を評価値から引く（ペナルティ）
				列によってペナルティは異なる。端は小さく中央は大きくなる。値は中盤と終盤のセットになっている
				*/
        if (isolated)
            value -= Isolated[opposed][f];
				/*
				味方のPAWN同士が縦列になっている場合のペナルティ
				列によってペナルティ値は異なる。端より中央がペナルティがきつい
				*/
        if (doubled)
            value -= Doubled[f];
				/*
				注目しているPAWNに後続する味方のPAWNがいない、両脇に味方のPAWNがいない時のペナルティ
				列によってペナルティ値は異なる。端より中央がペナルティがきつい
				*/
        if (backward)
            value -= Backward[opposed][f];
				/*
				味方のPAWNがチエーン状になっていればボーナス
				ChainMember配列はinit関数内で設定
				*/
        if (chain)
            value += ChainMember[f][relative_rank(Us, s)];
				/*
				敵PAWNを取って有利になりそうなPAWNの可能性がある場合はボーナス
				*/
        if (candidate)
        {
            value += CandidatePassed[relative_rank(Us, s)];
						/*
						このbitboardはこの関数内では活用しないがevaluate.cppの関数で使用されている
						*/
            if (!doubled)
                e->candidatePawns[Us] |= s;
        }
    }

    return value;
  }

} // namespace

namespace Pawns {

/// init() initializes some tables by formula instead of hard-code their values
/*
ChainMember配列の初期化
*/
void init() {

  const int chainByFile[8] = { 1, 3, 3, 4, 4, 3, 3, 1 };
  int bonus;

  for (Rank r = RANK_1; r < RANK_8; ++r)
      for (File f = FILE_A; f <= FILE_H; ++f)
      {
          bonus = r * (r-1) * (r-2) + chainByFile[f] * (r/2 + 1);
          ChainMember[f][r] = make_score(bonus, bonus);
      }
}


/// probe() takes a position object as input, computes a Entry object, and returns
/// a pointer to it. The result is also stored in a hash table, so we don't have
/// to recompute everything when the same pawn structure occurs again.
/*
*/
Entry* probe(const Position& pos, Table& entries) {

  Key key = pos.pawn_key();
  Entry* e = entries[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->value = evaluate<WHITE>(pos, e) - evaluate<BLACK>(pos, e);
  return e;
}


/// Entry::shelter_storm() calculates shelter and storm penalties for the file
/// the king is on, as well as the two adjacent files.

template<Color Us>
Value Entry::shelter_storm(const Position& pos, Square ksq) {

  const Color Them = (Us == WHITE ? BLACK : WHITE);

  Value safety = MaxSafetyBonus;
  Bitboard b = pos.pieces(PAWN) & (in_front_bb(Us, rank_of(ksq)) | rank_bb(ksq));
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);
  Rank rkUs, rkThem;
  File kf = std::max(FILE_B, std::min(FILE_G, file_of(ksq)));

  for (File f = kf - File(1); f <= kf + File(1); ++f)
  {
      b = ourPawns & file_bb(f);
      rkUs = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;
      safety -= ShelterWeakness[rkUs];

      b  = theirPawns & file_bb(f);
      rkThem = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;
      safety -= StormDanger[rkUs == RANK_1 ? 0 : rkThem == rkUs + 1 ? 2 : 1][rkThem];
  }

  return safety;
}


/// Entry::update_safety() calculates and caches a bonus for king safety. It is
/// called only when king square changes, about 20% of total king_safety() calls.

template<Color Us>
Score Entry::update_safety(const Position& pos, Square ksq) {

  kingSquares[Us] = ksq;
  castleRights[Us] = pos.can_castle(Us);
  minKPdistance[Us] = 0;

  Bitboard pawns = pos.pieces(Us, PAWN);
  if (pawns)
      while (!(DistanceRingsBB[ksq][minKPdistance[Us]++] & pawns)) {}

  if (relative_rank(Us, ksq) > RANK_4)
      return kingSafety[Us] = make_score(0, -16 * minKPdistance[Us]);

  Value bonus = shelter_storm<Us>(pos, ksq);

  // If we can castle use the bonus after the castle if it is bigger
  if (pos.can_castle(make_castle_right(Us, KING_SIDE)))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_G1)));

  if (pos.can_castle(make_castle_right(Us, QUEEN_SIDE)))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_C1)));

  return kingSafety[Us] = make_score(bonus, -16 * minKPdistance[Us]);
}

// Explicit template instantiation
template Score Entry::update_safety<WHITE>(const Position& pos, Square ksq);
template Score Entry::update_safety<BLACK>(const Position& pos, Square ksq);

} // namespace Pawns
