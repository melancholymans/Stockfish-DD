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

#include <cassert>

#include "movegen.h"
#include "position.h"

/// Simple macro to wrap a very common while loop, no facny, no flexibility,
/// hardcoded names 'mlist' and 'from'.
/*
bはターゲットとなるbitboardでfrom座標（マクロなのでこのfromは引数ではなくこのマクロが挿入された場所の
自動変数を利用している,mlist変数についても同様）にある駒がターゲットに移動する着手リストを作る

generate_moves関数
generate_all関数
generate<QUIET_CHECKS>関数
generate<EVASIONS>
で使用する
*/
#define SERIALIZE(b) while (b) (mlist++)->move = make_move(from, pop_lsb(&b))

/// Version used for pawns, where the 'from' square is given as a delta from the 'to' square
/*
PAWNの着手リストを作るためのマクロ
第一引数のbはPAWNのターゲット（移動先）、第二引数ｄは方向子

generate_promotions関数
*/
#define SERIALIZE_PAWNS(b, d) while (b) { Square to = pop_lsb(&b); \
                                         (mlist++)->move = make_move(to - (d), to); }
namespace {
	/*
	generate_allからのみ呼び出される
	キャスリング関係の生成関数なのでPASS
	*/
	template<CastlingSide Side, bool Checks, bool Chess960>
  ExtMove* generate_castle(const Position& pos, ExtMove* mlist, Color us) 
	{

    if (pos.castle_impeded(us, Side) || !pos.can_castle(make_castle_right(us, Side)))
        return mlist;

    // After castling, the rook and king final positions are the same in Chess960
    // as they would be in standard chess.
    Square kfrom = pos.king_square(us);
    Square rfrom = pos.castle_rook_square(us, Side);
    Square kto = relative_square(us, Side == KING_SIDE ? SQ_G1 : SQ_C1);
    Bitboard enemies = pos.pieces(~us);

    assert(!pos.checkers());

    const int K = Chess960 ? kto > kfrom ? -1 : 1
                           : Side == KING_SIDE ? -1 : 1;

    for (Square s = kto; s != kfrom; s += (Square)K)
        if (pos.attackers_to(s) & enemies)
            return mlist;

    // Because we generate only legal castling moves we need to verify that
    // when moving the castling rook we do not discover some hidden checker.
    // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
    if (Chess960 && (attacks_bb<ROOK>(kto, pos.pieces() ^ rfrom) & pos.pieces(~us, ROOK, QUEEN)))
        return mlist;

    (mlist++)->move = make<CASTLE>(kfrom, rfrom);

    if (Checks && !pos.gives_check((mlist - 1)->move, CheckInfo(pos)))
        --mlist;

    return mlist;
  }

	/*
	generate_pawn_movesから呼び出され
	指定されたDelta方向子で移動先bitboardをつくり
	座標を取り出し成れる駒に昇格する手を生成する
	*/
	template<GenType Type, Square Delta>
  inline ExtMove* generate_promotions(ExtMove* mlist, Bitboard pawnsOn7,
                                      Bitboard target, const CheckInfo* ci) 
	{

    Bitboard b = shift_bb<Delta>(pawnsOn7) & target;

    while (b)
    {
        Square to = pop_lsb(&b);

        if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
            (mlist++)->move = make<PROMOTION>(to - Delta, to, QUEEN);

        if (Type == QUIETS || Type == EVASIONS || Type == NON_EVASIONS)
        {
            (mlist++)->move = make<PROMOTION>(to - Delta, to, ROOK);
            (mlist++)->move = make<PROMOTION>(to - Delta, to, BISHOP);
            (mlist++)->move = make<PROMOTION>(to - Delta, to, KNIGHT);
        }

        // Knight-promotion is the only one that can give a direct check not
        // already included in the queen-promotion.
				/*
				生成パターンが駒を取らずに王手をかける　かつ　KNIGHTになったあとKINGに王手をかけることが
				できるならばその手を着手リストに追加する
				(void)ci; // Silence a warning under MSVC　はMSVCでワーニングの抑制？
				*/
				if (Type == QUIET_CHECKS && (StepAttacksBB[W_KNIGHT][to] & ci->ksq))
            (mlist++)->move = make<PROMOTION>(to - Delta, to, KNIGHT);
        else
            (void)ci; // Silence a warning under MSVC
    }

    return mlist;
  }

	/*
	PAWNの手生成専用
	PAWN以外はKNIGHT,ROOK,BISHOP,QUEENの動きは対称（動きがWHITEでもBLACKでも同じ）
	その点将棋は動きが非対称なものが多い、対称なものは王、飛車、角行のみ
	*/
	template<Color Us, GenType Type>
  ExtMove* generate_pawn_moves(const Position& pos, ExtMove* mlist,
                               Bitboard target, const CheckInfo* ci) 
	{

    // Compute our parametrized parameters at compile time, named according to
    // the point of view of white side.
		/*
		方向子を相対的に決めることでWHITE,BLACK共用のルーチンを作っている
		たとえばUpとはPAWNが前に進む方向子であるがWHITE側にとってDELTA_N(=8)であり
		BLACK側はDELTA_S(= -8)である
		*/
		const Color    Them = (Us == WHITE ? BLACK : WHITE);
    const Bitboard TRank8BB = (Us == WHITE ? Rank8BB  : Rank1BB);
    const Bitboard TRank7BB = (Us == WHITE ? Rank7BB  : Rank2BB);
    const Bitboard TRank3BB = (Us == WHITE ? Rank3BB  : Rank6BB);
    const Square   Up       = (Us == WHITE ? DELTA_N  : DELTA_S);
    const Square   Right    = (Us == WHITE ? DELTA_NE : DELTA_SW);
    const Square   Left     = (Us == WHITE ? DELTA_NW : DELTA_SE);

    Bitboard b1, b2, dc1, dc2, emptySquares;
		/*
		pawnsOn7はあと１手でQUEENになれるPAWNのbitboard
		*/
		Bitboard pawnsOn7 = pos.pieces(Us, PAWN) &  TRank7BB;
		/*
		まだまだQUEENに成れないPAWN
		*/
		Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;
		/*
		回避の生成パターンなら敵bitboardは敵の駒bitboard & target
		取る生成パターンならそのまま、その他のパターンは敵の駒全部
		*/
		Bitboard enemies = (Type == EVASIONS ? pos.pieces(Them) & target :
                        Type == CAPTURES ? target : pos.pieces(Them));

    // Single and double pawn pushes, no promotions
		/*
		取るパターンではない場合の手生成
		*/
		if (Type != CAPTURES)
    {
			//取らない手なのでtargetは空白となる
			emptySquares = (Type == QUIETS || Type == QUIET_CHECKS ? target : ~pos.pieces());
				//b1はQUEENになれないPAWNが駒をとらずに進める場所のbitboard
				b1 = shift_bb<Up>(pawnsNotOn7)   & emptySquares;
				/*b2はb1が１手動いたPAWNの内3ランク目にいるPAWNをもう１手動かしたもの
				つまりb2はPAWNが初手のみ２RANK動けるPAWNのこと
				b1は１ランクだけ動くb2は２ランク動く手なので、PAWNにとって駒を取らないすべての
				動きを表している。
				*/
				b2 = shift_bb<Up>(b1 & TRank3BB) & emptySquares;
				//取るパターンではない手の内、王手を回避する（PAWNの動きなので、王手をかけている駒をとる、利きをさえぎる手かな）
				if (Type == EVASIONS) // Consider only blocking squares
        {
            b1 &= target;
            b2 &= target;
        }
				//取るパターンではないのうち移動することによって王手をかける
				//pos.attacks_from<PAWN>(ci->ksq, Them);は敵の王からPAWNの利きとを＆演算するのでb1,b2には
				//王手の手が入っているはず
				if (Type == QUIET_CHECKS)
        {
            b1 &= pos.attacks_from<PAWN>(ci->ksq, Them);
            b2 &= pos.attacks_from<PAWN>(ci->ksq, Them);

            // Add pawn pushes which give discovered check. This is possible only
            // if the pawn is not on the same file as the enemy king, because we
            // don't generate captures. Note that a possible discovery check
            // promotion has been already generated among captures.
						/*ci->dcCandidatesは敵KINGへの利きを邪魔している駒のことなので
						そんな駒があったら、前方に空白があり、かつ敵KINGのいるFILEにいないPAWNをdc1へ
						そのdc1が３Rankにいて前方が空白なPAWNをdc2にいれておきb1,b2に追加する
						*/
						if (pawnsNotOn7 & ci->dcCandidates)
            {
                dc1 = shift_bb<Up>(pawnsNotOn7 & ci->dcCandidates) & emptySquares & ~file_bb(ci->ksq);
                dc2 = shift_bb<Up>(dc1 & TRank3BB) & emptySquares;

                b1 |= dc1;
                b2 |= dc2;
            }
        }

        SERIALIZE_PAWNS(b1, Up);
        SERIALIZE_PAWNS(b2, Up + Up);
    }

    // Promotions and underpromotions
		/*
		この上の処理は取るパターンではない、ここからは取る、回避するパターンを生成する
		ランク７にいるPAWNがいて　かつ　（生成パターンが回避パターンでなく　もしくは　移動先が空白）
		なら
		*/
		if (pawnsOn7 && (Type != EVASIONS || (target & TRank8BB)))
    {
        if (Type == CAPTURES)
            emptySquares = ~pos.pieces();

        if (Type == EVASIONS)
            emptySquares &= target;

        mlist = generate_promotions<Type, Right>(mlist, pawnsOn7, enemies, ci);
        mlist = generate_promotions<Type, Left >(mlist, pawnsOn7, enemies, ci);
        mlist = generate_promotions<Type, Up>(mlist, pawnsOn7, emptySquares, ci);
    }

    // Standard and en-passant captures
		/*
		ここは取るパターン　回避するパターン　回避するパターン以外全て
		ランク７にいるPAWNで移動することによって敵駒をとる手を生成し、着手リストに登録する
		*/
		if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        b1 = shift_bb<Right>(pawnsNotOn7) & enemies;
        b2 = shift_bb<Left >(pawnsNotOn7) & enemies;

        SERIALIZE_PAWNS(b1, Right);
        SERIALIZE_PAWNS(b2, Left);

        if (pos.ep_square() != SQ_NONE)
        {
            assert(rank_of(pos.ep_square()) == relative_rank(Us, RANK_6));

            // An en passant capture can be an evasion only if the checking piece
            // is the double pushed pawn and so is in the target. Otherwise this
            // is a discovery check and we are forced to do otherwise.
            if (Type == EVASIONS && !(target & (pos.ep_square() - Up)))
                return mlist;

            b1 = pawnsNotOn7 & pos.attacks_from<PAWN>(pos.ep_square(), Them);

            assert(b1);

            while (b1)
                (mlist++)->move = make<ENPASSANT>(pop_lsb(&b1), pos.ep_square());
        }
    }

    return mlist;
  }

	/*
	PAWN,KING以外の駒の動きはこの関数だけで生成する
	*/
	template<PieceType Pt, bool Checks> FORCE_INLINE
  ExtMove* generate_moves(const Position& pos, ExtMove* mlist, Color us,
                          Bitboard target, const CheckInfo* ci) 
	{

    assert(Pt != KING && Pt != PAWN);
		/*
		指定されたカラー、指定された駒種の現在の座標リスト
		Checksの意味：QUIET_CHECKSであれはtrue
		*/
		const Square* pl = pos.list<Pt>(us);

    for (Square from = *pl; from != SQ_NONE; from = *++pl)
    {
        if (Checks)
        {
						//ChecksはQUIET_CHECKSのときtrueになるがPISHOP,ROOK,QUEENの移動は
						//移動によって王手が掛られるような手でなければ生成しない
						//（多分手が増えすぎるのを抑えるのが目的では）
						if ((Pt == BISHOP || Pt == ROOK || Pt == QUEEN)
                && !(PseudoAttacks[Pt][from] & target & ci->checkSq[Pt]))
                continue;
						/*
						敵陣のKINGへの利きの邪魔になっている駒が移動する手はすでに生成しているので
						ここでは生成しない
						*/
						if (unlikely(ci->dcCandidates) && (ci->dcCandidates & from))
                continue;
        }
				/*
				from座標にいる駒から移動可能な場所のbitboardをbに入れておき
				SERIALIZE(b)マクロで
				*/
        Bitboard b = pos.attacks_from<Pt>(from) & target;
				//QUIET_CHECKSは移動することで王手をかけれる手に移動する手を追加する
				if (Checks)
            b &= ci->checkSq[Pt];
				/*
				bitboard b（ターゲットbitboard）を渡して着手リスト(mlist）を生成している
				*/
        SERIALIZE(b);
    }

    return mlist;
  }

	/*
	駒の動きを生成する
	引数の内targetは移動先のbitboardを表現している
	posは今の局面クラス、テンプレート引数のGenType Typeは
	生成パターンを指定している、パターンはCAPTURES,QUIET,NON_EVASIONSがあるもよう
	CAPTURESとにかく取る手
	QUIETとらず移動する手
	NON_EVASIONS王手を回避する手以外すべて
	*/
	template<Color Us, GenType Type> FORCE_INLINE
  ExtMove* generate_all(const Position& pos, ExtMove* mlist, Bitboard target,
                        const CheckInfo* ci = nullptr) {

    const bool Checks = Type == QUIET_CHECKS;

    mlist = generate_pawn_moves<Us, Type>(pos, mlist, target, ci);
    mlist = generate_moves<KNIGHT, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves<BISHOP, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves<  ROOK, Checks>(pos, mlist, Us, target, ci);
    mlist = generate_moves< QUEEN, Checks>(pos, mlist, Us, target, ci);

    if (Type != QUIET_CHECKS && Type != EVASIONS)
    {
        Square from = pos.king_square(Us);
        Bitboard b = pos.attacks_from<KING>(from) & target;
        SERIALIZE(b);
    }

    if (Type != CAPTURES && Type != EVASIONS && pos.can_castle(Us))
    {
        if (pos.is_chess960())
        {
            mlist = generate_castle< KING_SIDE, Checks, true>(pos, mlist, Us);
            mlist = generate_castle<QUEEN_SIDE, Checks, true>(pos, mlist, Us);
        }
        else
        {
            mlist = generate_castle< KING_SIDE, Checks, false>(pos, mlist, Us);
            mlist = generate_castle<QUEEN_SIDE, Checks, false>(pos, mlist, Us);
        }
    }

    return mlist;
  }


} // namespace


/// generate<CAPTURES> generates all pseudo-legal captures and queen
/// promotions. Returns a pointer to the end of the move list.
///
/// generate<QUIETS> generates all pseudo-legal non-captures and
/// underpromotions. Returns a pointer to the end of the move list.
///
/// generate<NON_EVASIONS> generates all pseudo-legal captures and
/// non-captures. Returns a pointer to the end of the move list.
/*
手を生成するテンプレート関数
ここから駒種、生成パターンに応じて分岐する
*/
template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* mlist) 
{

  assert(Type == CAPTURES || Type == QUIETS || Type == NON_EVASIONS);
  assert(!pos.checkers());

  Color us = pos.side_to_move();
	/*
	CAPTURES=敵側の駒がターゲット
	QUIETS=駒を取るのではなく空白に移動する手（QUIETSの本体の意味＝穏やかな）
	NON_EVASIONS=回避する手ではない＝敵駒＋空白がターゲット
	*/
	Bitboard target = Type == CAPTURES ? pos.pieces(~us)
                  : Type == QUIETS       ? ~pos.pieces()
                  : Type == NON_EVASIONS ? ~pos.pieces(us) : 0;

  return us == WHITE ? generate_all<WHITE, Type>(pos, mlist, target)
                     : generate_all<BLACK, Type>(pos, mlist, target);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);


/// generate<QUIET_CHECKS> generates all pseudo-legal non-captures and knight
/// underpromotions that give check. Returns a pointer to the end of the move list.
/*
まず、敵KINGとの利きの邪魔になっている自陣駒が他の駒を取らずに移動する着手リストを作る
そのあとgenerate_allを呼ぶ、但しtargetは空白とする（駒を取らないため）

QUIET_CHECKSは駒をとらずに王手をかける手のこと？
*/
template<>
ExtMove* generate<QUIET_CHECKS>(const Position& pos, ExtMove* mlist) 
{

  assert(!pos.checkers());

  Color us = pos.side_to_move();
  CheckInfo ci(pos);
  Bitboard dc = ci.dcCandidates;

  while (dc)
  {
     Square from = pop_lsb(&dc);
     PieceType pt = type_of(pos.piece_on(from));

     if (pt == PAWN)
         continue; // Will be generated togheter with direct checks

     Bitboard b = pos.attacks_from(Piece(pt), from) & ~pos.pieces();

     if (pt == KING)
         b &= ~PseudoAttacks[QUEEN][ci.ksq];

     SERIALIZE(b);
  }

  return us == WHITE ? generate_all<WHITE, QUIET_CHECKS>(pos, mlist, ~pos.pieces(), &ci)
                     : generate_all<BLACK, QUIET_CHECKS>(pos, mlist, ~pos.pieces(), &ci);
}


/// generate<EVASIONS> generates all pseudo-legal check evasions when the side
/// to move is in check. Returns a pointer to the end of the move list.
//EVASIONS＝回避
/*
王手を回避する手を生成する
*/
template<>
ExtMove* generate<EVASIONS>(const Position& pos, ExtMove* mlist) 
{

  assert(pos.checkers());

  int checkersCnt = 0;
  Color us = pos.side_to_move();
  Square ksq = pos.king_square(us), from = ksq /* For SERIALIZE */, checksq;
  Bitboard sliderAttacks = 0;
  Bitboard b = pos.checkers();

  assert(pos.checkers());

  // Find squares attacked by slider checkers, we will remove them from the king
  // evasions so to skip known illegal moves avoiding useless legality check later.
  do
  {
      ++checkersCnt;
      checksq = pop_lsb(&b);

      assert(color_of(pos.piece_on(checksq)) == ~us);

      if (type_of(pos.piece_on(checksq)) > KNIGHT) // A slider
          sliderAttacks |= LineBB[checksq][ksq] ^ checksq;

  } while (b);

  // Generate evasions for king, capture and non capture moves
	/*
	KINGの利き　＆　pos.pieces(us) = KINGの移動可能＆空白または敵駒
	（KINGの移動可能＆空白または敵駒）＆　敵の飛び駒の利き以外の場所
	つまりKINGが逃げる場所をmlistに登録している
	*/
	b = pos.attacks_from<KING>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
  SERIALIZE(b);

  if (checkersCnt > 1)
      return mlist; // Double check, only a king move can save the day

  // Generate blocking evasions or captures of the checking piece
  Bitboard target = between_bb(checksq, ksq) | checksq;

  return us == WHITE ? generate_all<WHITE, EVASIONS>(pos, mlist, target)
                     : generate_all<BLACK, EVASIONS>(pos, mlist, target);
}


/// generate<LEGAL> generates all the legal moves in the given position
/*
合法手を生成する
自KINGに王手がかかってれば回避する手を生成
そうでなければ回避する手じゃない手を生成する
*/
template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* mlist) 
{

  ExtMove *end, *cur = mlist;
  Bitboard pinned = pos.pinned_pieces(pos.side_to_move());
  Square ksq = pos.king_square(pos.side_to_move());

  end = pos.checkers() ? generate<EVASIONS>(pos, mlist)
                       : generate<NON_EVASIONS>(pos, mlist);
  while (cur != end)
		//合法手でなかったら手を消す
		if ((pinned || from_sq(cur->move) == ksq || type_of(cur->move) == ENPASSANT)
          && !pos.legal(cur->move, pinned))
          cur->move = (--end)->move;
      else
          ++cur;

  return end;
}
