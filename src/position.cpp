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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "bitcount.h"
#include "movegen.h"
#include "notation.h"
#include "position.h"
#include "psqtab.h"
#include "rkiss.h"
#include "thread.h"
#include "tt.h"

using std::string;
using std::cout;
using std::endl;
/*
notation.cppにも同じ名前で定義されているがこちら側はstring型、notation.cppはchar型
駒種を文字１文字で表現するための文字列
*/
static const string PieceToChar(" PNBRQK  pnbrqk");
/*
アラメントを64bitに揃えている
*/
CACHE_LINE_ALIGNMENT
/*
psqという名前の変数はあっちこっちにある、Piece-Squareのように駒種別の座標によってある値をもつ
ような意味に使われることがおおいようだ。
なので名前をpiece_sq_scoreに変更する
初期化はPosition::initでする
do_move関数でこの値を集計してStateInfo->psq変数に格納している
あとcompute_psq_score関数で使用されている。
*/
Score piece_sq_score[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
/*
駒自身の価値、中盤での価値が**Mg,終盤での価値が**Egになっている
white側のみでPosition::initでBLACK側にコピーする
PieceValue[Mg=0][0-5]でVALUE_ZEROからQueenまでの中盤のwhile側駒評価値が入る
PieceValue[Eg=1][0-5]でVALUE_ZEROからQueenまでの終盤のwhile側駒評価値が入る
init関数で残りの
PieceValue[Mg=0][8-13]でVALUE_ZEROからQueenまでの中盤のblack側駒評価値が入る
PieceValue[Eg=1][8-13]でVALUE_ZEROからQueenまでの終盤のblack側駒評価値が入る
配列の全てに値が入っているわけではない
*/
Value PieceValue[PHASE_NB][PIECE_NB] = {
{ VALUE_ZERO, PawnValueMg, KnightValueMg, BishopValueMg, RookValueMg, QueenValueMg },
{ VALUE_ZERO, PawnValueEg, KnightValueEg, BishopValueEg, RookValueEg, QueenValueEg } };
/*
chessなどの局面の状態を１つのハッシュ値で代表させる方法
参考HP:http://hackemdown.blogspot.jp/2014/06/zobrist-hashing.html
初期化しているのはPosition::init()関数内
*/
namespace Zobrist {
	/*
	COLORごと、駒種ごと、盤座標ごとに乱数を発生させ（rkiss.hにあるclass RKISSで発生させている）
	一意のハッシュ値を得る
	*/
  Key psq[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
	/*
	アンパッサンのときのハッシュ値
	*/
  Key enpassant[FILE_NB];
	/*
	キャスリングのときのハッシュ値
	*/
  Key castle[CASTLE_RIGHT_NB];
	/*
	手番のときのハッシュ値
	*/
  Key side;
	/*
	用途不明、単語の意味は除外
	*/
  Key exclusion;
}
/*
用途不明、単語の意味は除外
*/
Key Position::exclusion_key() const { return m_st->key ^ Zobrist::exclusion; }

namespace {

// min_attacker() is an helper function used by see() to locate the least
// valuable attacker for the side to move, remove the attacker we just found
// from the bitboards and scan for new X-ray attacks behind it.
/*
min_attacker関数はsee関数のヘルパー関数
でsee関数は静止探索
const Bitboard* bb[]は駒種ごとのbitboard
const Square& to取り合いになっている座標
const Bitboard& stmAttackers敵のアタッカー駒のbitboard
Bitboard& occupied　from座標の駒を除いた全駒のbitboard
Bitboard& attackers 全アタッカー（カラーに関係なく）
*/
template<int Pt> FORCE_INLINE
PieceType min_attacker(const Bitboard* bb, const Square& to, const Bitboard& stmAttackers,
                       Bitboard& occupied, Bitboard& attackers) 
{
	/*
	テンプレートパラメータで指定してある駒種でアタッカー駒を抽出
	最初の駒種はPAWNからーつまり価値の低い駒から行う
	指定した駒種がなかった場合min_attacker関数を駒種を上げて(PAWN->NIGHT->...)再帰呼び出しする

	*/
  Bitboard b = stmAttackers & bb[Pt];
  if (!b)
      return min_attacker<Pt+1>(bb, to, stmAttackers, occupied, attackers);
	/*
	取り合う駒種の駒を全体のbitboardから取り除く
	*/
  occupied ^= b & ~(b - 1);
	/*
	取り合う駒種がPAWN || BISHOP || QUEENだったとき、座標toに影の利きを利かせている
	BISHOP,QUEEN駒があるようならattackersビットボードに追加する

	取り合う駒種がROOK || QUEENだったとき、座標toに影の利きを利かせている
	ROOK,QUEEN駒があるようならattackersビットボードに追加する

	つまり駒を使ったらその後ろにまだto座標で取り合いできる駒があれば追加せよということ
	*/
	if (Pt == PAWN || Pt == BISHOP || Pt == QUEEN)
      attackers |= attacks_bb<BISHOP>(to, occupied) & (bb[BISHOP] | bb[QUEEN]);

  if (Pt == ROOK || Pt == QUEEN)
      attackers |= attacks_bb<ROOK>(to, occupied) & (bb[ROOK] | bb[QUEEN]);
	/*
	X-ray=直訳するとレントゲン線？
	*/
  attackers &= occupied; // After X-ray that may add already processed pieces
	/*
	取り合える駒種を返す、引数のbitboardは参照渡しなのでこの関数内で更新すみ
	*/
  return (PieceType)Pt;
}
/*
テンプレート関数（KINGの明示化）
*/
template<> FORCE_INLINE
PieceType min_attacker<KING>(const Bitboard*, const Square&, const Bitboard&, Bitboard&, Bitboard&) 
{
  return KING; // No need to update bitboards, it is the last cycle
}

} // namespace


/// CheckInfo c'tor
/*
このCheckInfoクラスはコンストラクタしかない
局面クラスpositionを受け取って現局面で王手をかけている駒種ごとのbitboard（もちろんチエックがかかっていない場合は0）
敵KINGに対してpin付けされている駒のbitboardを返す
*/
CheckInfo::CheckInfo(const Position& pos) 
{

  Color them = ~pos.side_to_move();
  ksq = pos.king_square(them);

  pinned = pos.pinned_pieces(pos.side_to_move());
  dcCandidates = pos.discovered_check_candidates();

  checkSq[PAWN]   = pos.attacks_from<PAWN>(ksq, them);
  checkSq[KNIGHT] = pos.attacks_from<KNIGHT>(ksq);
  checkSq[BISHOP] = pos.attacks_from<BISHOP>(ksq);
  checkSq[ROOK]   = pos.attacks_from<ROOK>(ksq);
  checkSq[QUEEN]  = checkSq[BISHOP] | checkSq[ROOK];
  checkSq[KING]   = 0;
}


/// Position::init() initializes at startup the various arrays used to compute
/// hash keys and the piece square tables. The latter is a two-step operation:
/// First, the white halves of the tables are copied from PSQT[] tables. Second,
/// the black halves of the tables are initialized by flipping and changing the
/// sign of the white scores.
/*
Zobrist（局面ごとのハッシュ値を生成するクラス）を初期化している
そのあと評価値（駒評価値と位置評価値）を初期している
*/
void Position::init() 
{

  RKISS rk;
	/*
	升目、駒種ごとに乱数をあらかじめ設定しておき
	局面の状態に応じて１意のハッシュ値（異局面で同一のハッシュ値がでる可能性はある）
	*/
	for (Color c = WHITE; c <= BLACK; ++c)
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          for (Square s = SQ_A1; s <= SQ_H8; ++s)
              Zobrist::psq[c][pt][s] = rk.rand<Key>();
	/*
	アンパッサンのハッシュ値
	*/
	for (File f = FILE_A; f <= FILE_H; ++f)
      Zobrist::enpassant[f] = rk.rand<Key>();
	/*
	キャスリングのハッシュ値
	*/
	for (int cr = CASTLES_NONE; cr <= ALL_CASTLES; ++cr)
  {
      Bitboard b = cr;
      while (b)
      {
          Key k = Zobrist::castle[1ULL << pop_lsb(&b)];
          Zobrist::castle[cr] ^= k ? k : rk.rand<Key>();
      }
  }
	/*
	手番のハッシュ値
	*/
	Zobrist::side = rk.rand<Key>();
  Zobrist::exclusion  = rk.rand<Key>();
	/*
	WHITE側の駒評価値は直接設定している（このposition.cppの冒頭部分）
	ここではWHITE側の評価値をBLACK側にコピーしている
	*/
	for (PieceType pt = PAWN; pt <= KING; ++pt)
  {
      PieceValue[MG][make_piece(BLACK, pt)] = PieceValue[MG][pt];
      PieceValue[EG][make_piece(BLACK, pt)] = PieceValue[EG][pt];

      Score v = make_score(PieceValue[MG][pt], PieceValue[EG][pt]);
			/*
			PSQTはpsqtab.hに定義してある配列でScore変数が（32bitの上位16bitにミドルゲーム駒評価値を、下位16bitにエンドゲーム駒評価値を設定してある）
			盤位置に応じて格納されている。位置評価値の基本位置評価値と言える
			psq[BLACK][pt][~s]の~sは演算子のオーバーロードで座標変換している (例A1->A8,B2->B7）
			white側（先手）がプラス、BLACK側が（後手）マイナスをもつ
			基本位置評価値に駒評価値を加算してpsq配列を初期化している
			ｐｓｑ配列はstatic Score psq[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];と宣言されている
			おそらく初期化のされかた、ネーミングから駒評価値と位置評価値を組み合わせたもの
			*/
			for (Square s = SQ_A1; s <= SQ_H8; ++s)
      {
         piece_sq_score[WHITE][pt][ s] =  (v + PSQT[pt][s]);
				 piece_sq_score[BLACK][pt][~s] = -(v + PSQT[pt][s]);
      }
  }
}


/// Position::operator=() creates a copy of 'pos'. We want the new born Position
/// object do not depend on any external data so we detach state pointer from
/// the source one.
/*
局面クラスpositionをコピーする演算子のオーバーロード
startStateを受け取る
*/
Position& Position::operator=(const Position& pos) 
{

  std::memcpy(this, &pos, sizeof(Position));
  startState = *m_st;
  m_st = &startState;
  nodes = 0;

  assert(pos_is_ok());

  return *this;
}


/// Position::set() initializes the position object with the given FEN string.
/// This function is not very robust - make sure that input FENs are correct,
/// this is assumed to be the responsibility of the GUI.
/*
FEN stringを読み込んで局面を設定している
*/
void Position::set(const string& fenStr, bool isChess960, Thread* th) 
{
/*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1; within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") while Black take lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. This is recorded regardless of whether
      there is a pawn in position to make an en passant capture.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

  char col, row, token;
  size_t p;
  Square sq = SQ_A8;
  std::istringstream ss(fenStr);

  clear();
	//空白文字をスキップさせない設定
	ss >> std::noskipws;

  // 1. Piece placement
	/*
	FEN stringのスキャンはA8->B8->...->H8
	A7->B7..->H7
	A1->B1->..H1と読み取ってい行く
	*/
	while ((ss >> token) && !isspace(token))
  {
			//数字は空白を表すので数値だけ座標を加算する
			if (isdigit(token))
          sq += Square(token - '0'); // Advance the given number of files
			/*
			FEN stringの見本
			"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
			*/
			else if (token == '/')
          sq -= Square(16);
			//ここはFEN stringを駒コードに変換してput_pieceを呼んで内部データを更新している
			else if ((p = PieceToChar.find(token)) != string::npos)
      {
          put_piece(sq, color_of(Piece(p)), type_of(Piece(p)));
          ++sq;
      }
  }

  // 2. Active color
  ss >> token;
  sideToMove = (token == 'w' ? WHITE : BLACK);
  ss >> token;

  // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
  // Shredder-FEN that uses the letters of the columns on which the rooks began
  // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
  // if an inner rook is associated with the castling right, the castling tag is
  // replaced by the file letter of the involved rook, as for the Shredder-FEN.
	//キャスリングに関することのようだが詳細不明
	while ((ss >> token) && !isspace(token))
  {
      Square rsq;
      Color c = islower(token) ? BLACK : WHITE;

      token = char(toupper(token));

      if (token == 'K')
          for (rsq = relative_square(c, SQ_H1); type_of(piece_on(rsq)) != ROOK; --rsq) {}

      else if (token == 'Q')
          for (rsq = relative_square(c, SQ_A1); type_of(piece_on(rsq)) != ROOK; ++rsq) {}

      else if (token >= 'A' && token <= 'H')
          rsq = File(token - 'A') | relative_rank(c, RANK_1);

      else
          continue;

      set_castle_right(c, rsq);
  }

  // 4. En passant square. Ignore if no pawn capture is possible
	//アンパサン
	if (((ss >> col) && (col >= 'a' && col <= 'h'))
      && ((ss >> row) && (row == '3' || row == '6')))
  {
      m_st->epSquare = File(col - 'a') | Rank(row - '1');

      if (!(attackers_to(m_st->epSquare) & pieces(sideToMove, PAWN)))
          m_st->epSquare = SQ_NONE;
  }

  // 5-6. Halfmove clock and fullmove number
	/*
	Halfmove
	https://chessprogramming.wikispaces.com/Halfmove+Clock

	std::skipwsはマニピュレータ（先頭の空白を読み飛ばす）
	m_st->rule50は５０手ルールのフラグかと思っていたが違うようだ＝
	この変数で50手を数えている、do_move関数内でカウントアップし99手に達しそうになったら
	５０手ルールを適用している
	gamePlyも手数手数などのssに入力している
	*/
	ss >> std::skipws >> m_st->rule50 >> gamePly;

  // Convert from fullmove starting from 1 to ply starting from 0,
  // handle also common incorrect FEN with fullmove = 0.
	gamePly = std::max(2 * (gamePly - 1), 0) + int(sideToMove == BLACK);
	//局面復元のための情報
	m_st->key = compute_key();
  m_st->pawnKey = compute_pawn_key();
  m_st->materialKey = compute_material_key();
  m_st->psq = compute_psq_score();
  m_st->npMaterial[WHITE] = compute_non_pawn_material(WHITE);
  m_st->npMaterial[BLACK] = compute_non_pawn_material(BLACK);
  m_st->checkersBB = attackers_to(king_square(sideToMove)) & pieces(~sideToMove);
  chess960 = isChess960;
  thisThread = th;

  assert(pos_is_ok());
}


/// Position::set_castle_right() is an helper function used to set castling
/// rights given the corresponding color and the rook starting square.
/*
多分キャスリングに関するなにか
*/
void Position::set_castle_right(Color c, Square rfrom) 
{

  Square kfrom = king_square(c);
  CastlingSide cs = kfrom < rfrom ? KING_SIDE : QUEEN_SIDE;
  CastleRight cr = make_castle_right(c, cs);

  m_st->castleRights |= cr;
  castleRightsMask[kfrom] |= cr;
  castleRightsMask[rfrom] |= cr;
  castleRookSquare[c][cs] = rfrom;

  Square kto = relative_square(c, cs == KING_SIDE ? SQ_G1 : SQ_C1);
  Square rto = relative_square(c, cs == KING_SIDE ? SQ_F1 : SQ_D1);

  for (Square s = std::min(rfrom, rto); s <= std::max(rfrom, rto); ++s)
      if (s != kfrom && s != rfrom)
          castlePath[c][cs] |= s;

  for (Square s = std::min(kfrom, kto); s <= std::max(kfrom, kto); ++s)
      if (s != kfrom && s != rfrom)
          castlePath[c][cs] |= s;
}


/// Position::fen() returns a FEN representation of the position. In case
/// of Chess960 the Shredder-FEN notation is used. Mainly a debugging function.
/*
Position::fen() returns a FEN representation of the position. In case of
Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.
内部データよりFEN stringを作り上げる
*/
const string Position::fen() const 
{

  std::ostringstream ss;

  for (Rank rank = RANK_8; rank >= RANK_1; --rank)
  {
      for (File file = FILE_A; file <= FILE_H; ++file)
      {
          Square sq = file | rank;

          if (empty(sq))
          {
              int emptyCnt = 1;

              for ( ; file < FILE_H && empty(++sq); ++file)
                  ++emptyCnt;

              ss << emptyCnt;
          }
          else
              ss << PieceToChar[piece_on(sq)];
      }

      if (rank > RANK_1)
          ss << '/';
  }

  ss << (sideToMove == WHITE ? " w " : " b ");

  if (can_castle(WHITE_OO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(WHITE,  KING_SIDE)), false) : 'K');

  if (can_castle(WHITE_OOO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(WHITE, QUEEN_SIDE)), false) : 'Q');

  if (can_castle(BLACK_OO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK,  KING_SIDE)),  true) : 'k');

  if (can_castle(BLACK_OOO))
      ss << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK, QUEEN_SIDE)),  true) : 'q');

  if (m_st->castleRights == CASTLES_NONE)
      ss << '-';

  ss << (ep_square() == SQ_NONE ? " - " : " " + square_to_string(ep_square()) + " ")
      << m_st->rule50 << " " << 1 + (gamePly - int(sideToMove == BLACK)) / 2;

  return ss.str();
}


/// Position::pretty() returns an ASCII representation of the position to be
/// printed to the standard output together with the move's san notation.
/*
内部データ（board[]など）と渡された指し手情報を表示する
*/
const string Position::pretty(Move move) const 
{

  const string dottedLine =            "\n+---+---+---+---+---+---+---+---+";
  const string twoRows =  dottedLine + "\n|   | . |   | . |   | . |   | . |"
                        + dottedLine + "\n| . |   | . |   | . |   | . |   |";

  string brd = twoRows + twoRows + twoRows + twoRows + dottedLine;

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      brd[513 - 68 * rank_of(s) + 4 * file_of(s)] = PieceToChar[piece_on(s)];
  }

  std::ostringstream ss;

  if (move)
      ss << "\nMove: " << (sideToMove == BLACK ? ".." : "")
         << move_to_san(*const_cast<Position*>(this), move);

  ss << brd << "\nFen: " << fen() << "\nKey: " << std::hex << std::uppercase
     << std::setfill('0') << std::setw(16) << m_st->key << "\nCheckers: ";

  for (Bitboard b = checkers(); b; )
      ss << square_to_string(pop_lsb(&b)) << " ";

  ss << "\nLegal moves: ";
  for (const ExtMove& ms : MoveList<LEGAL>(*this))
      ss << move_to_san(*const_cast<Position*>(this), ms.move) << " ";

  return ss.str();
}


/// Position:hidden_checkers() returns a bitboard of all pinned / discovery check
/// pieces, according to the call parameters. Pinned pieces protect our king,
/// discovery check pieces attack the enemy king.
/*
ksqは自陣のKINGの座標
Colorは敵サイドのカラー
toMoveは自陣サイドのカラー

敵の大駒にpinされている自駒を返す
*/
Bitboard Position::hidden_checkers(Square ksq, Color c, Color toMove) const 
{

  Bitboard b, pinners, result = 0;

  // Pinners are sliders that give check when pinned piece is removed
	/*
	敵側QUEEN,ROOK,BISHOP駒で自陣KINGに影の利きをとうしている駒をpinners(bitboard)に入れておく
	*/
  pinners = (  (pieces(  ROOK, QUEEN) & PseudoAttacks[ROOK  ][ksq])
             | (pieces(BISHOP, QUEEN) & PseudoAttacks[BISHOP][ksq])) & pieces(c);
	/*
	その影の利きを掛けている駒とKINGの間にある駒をb(bitboard)に入れて
	その駒が１つである自駒を返す（more_than_one関数が判定する）
	*/
  while (pinners)
  {
      b = between_bb(ksq, pop_lsb(&pinners)) & pieces();

      if (!more_than_one(b))
          result |= b & pieces(toMove);
  }
  return result;
}


/// Position::attackers_to() computes a bitboard of all pieces which attack a
/// given square. Slider attacks use occ bitboard as occupancy.
/*
attackers_to関数の機能
指定した座標に利いている全ての駒（カラーに関係なく）を検出してビットを立てた
bitboardを返す
*/
Bitboard Position::attackers_to(Square s, Bitboard occ) const 
{

  return  (attacks_from<PAWN>(s, BLACK) & pieces(WHITE, PAWN))
        | (attacks_from<PAWN>(s, WHITE) & pieces(BLACK, PAWN))
        | (attacks_from<KNIGHT>(s)      & pieces(KNIGHT))
        | (attacks_bb<ROOK>(s, occ)     & pieces(ROOK, QUEEN))
        | (attacks_bb<BISHOP>(s, occ)   & pieces(BISHOP, QUEEN))
        | (attacks_from<KING>(s)        & pieces(KING));
}


/// Position::attacks_from() computes a bitboard of all attacks of a given piece
/// put in a given square. Slider attacks use occ bitboard as occupancy.
/*
attacks_from<>関数の機能
指定した駒コード、盤座標にある駒の利きのbitboardを返す。ただしPAWNは進む方向と
駒を取る利きが違う、attacks_fromはあくまで駒をとる利きのみかえす
attacks_from関数は３つオーバーロードしている
attacks_from(Square s)はテンプレート引数で駒種を指定して、
指定した座標から利きのbitboardを返す。
飛び駒だけでなく非飛び駒の利きbitboardも返せる。
attacks_from<PAWN>(Square s, Color c)
駒種はPAWNだけで座標とカラーが指定できて利きbitboardを返す
attacks_from(Piece pc, Square s)
指定した座標、指定した駒種から利きのbitboardを返す。
非飛び駒は対応していない
*/
Bitboard Position::attacks_from(Piece p, Square s, Bitboard occ) 
{

  assert(is_ok(s));

  switch (type_of(p))
  {
  case BISHOP: return attacks_bb<BISHOP>(s, occ);
  case ROOK  : return attacks_bb<ROOK>(s, occ);
  case QUEEN : return attacks_bb<BISHOP>(s, occ) | attacks_bb<ROOK>(s, occ);
  default    : return StepAttacksBB[p][s];
  }
}


/// Position::legal() tests whether a pseudo-legal move is legal
/*
引数Move mが合法手か検査する合法手かどうかは
アンパッサンだったら

動いた駒がKINGだったら
移動先に敵の利きが利いていたらNG、キャスリングはOK
pinがかかっていないこと,pinがかかっていてもpinがはずれない動きならOK
*/
bool Position::legal(Move m, Bitboard pinned) const 
{

  assert(is_ok(m));
  assert(pinned == pinned_pieces(sideToMove));

  Color us = sideToMove;
  Square from = from_sq(m);

  assert(color_of(moved_piece(m)) == us);
  assert(piece_on(king_square(us)) == make_piece(us, KING));

  // En passant captures are a tricky special case. Because they are rather
  // uncommon, we do it simply by testing whether the king is attacked after
  // the move is made.
  if (type_of(m) == ENPASSANT)
  {
      Color them = ~us;
      Square to = to_sq(m);
      Square capsq = to + pawn_push(them);
      Square ksq = king_square(us);
      Bitboard b = (pieces() ^ from ^ capsq) | to;

      assert(to == ep_square());
      assert(moved_piece(m) == make_piece(us, PAWN));
      assert(piece_on(capsq) == make_piece(them, PAWN));
      assert(piece_on(to) == NO_PIECE);

      return   !(attacks_bb<  ROOK>(ksq, b) & pieces(them, QUEEN, ROOK))
            && !(attacks_bb<BISHOP>(ksq, b) & pieces(them, QUEEN, BISHOP));
  }

  // If the moving piece is a king, check whether the destination
  // square is attacked by the opponent. Castling moves are checked
  // for legality during move generation.
	/*
	指し手がKINGなら着手パターンがキャスリングであること　|| KINGの移動先に敵の利きがないことでOK
	*/
  if (type_of(piece_on(from)) == KING)
      return type_of(m) == CASTLE || !(attackers_to(to_sq(m)) & pieces(~us));

  // A non-king move is legal if and only if it is not pinned or it
  // is moving along the ray towards or away from the king.
	/*
	指し手駒がpinされていなければOK、BISHOP、ROOKの利き方向への移動ならOK
	*/
	return   !pinned
        || !(pinned & from)
        ||  aligned(from, to_sq(m), king_square(us));
}


/// Position::pseudo_legal() takes a random move and tests whether the move is
/// pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.
/*
合法手であるかテストする

置換表からの手は必ずしも現局面において合法かどうかは検査するまで
わからない、同様に兄弟手の水平展開＝キラー手も検査する
*/
bool Position::pseudo_legal(const Move m) const 
{

  Color us = sideToMove;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);

  // Use a slower but simpler function for uncommon cases
	/*
	現局面でのMoveListクラスで合法手を生成
	contains関数は渡された指し手が合法手リストにあるかチエックしている
	type_of(m) != NORMALなのでPROMOTIONかEN PASSANTかCASTINGである
	*/
  if (type_of(m) != NORMAL)
      return MoveList<LEGAL>(*this).contains(m);

  // Is not a promotion, so promotion piece must be empty
	/*
	ここにくるということは着手パターンは
	NORMALなのでpromotion_typeに手があったらNG
	*/
  if (promotion_type(m) - 2 != NO_PIECE_TYPE)
      return false;

  // If the from square is not occupied by a piece belonging to the side to
  // move, the move is obviously not legal.
	/*
	指し手の駒に駒コードが入っていない、カラーが違うのは当然NG
	*/
  if (pc == NO_PIECE || color_of(pc) != us)
      return false;

  // The destination square cannot be occupied by a friendly piece
	/*
	指し手の移動先に自陣の駒があるのはNG
	*/
  if (pieces(us) & to)
      return false;

  // Handle the special case of a pawn move
	/*
	PAWNに関するチエック
	*/
  if (type_of(pc) == PAWN)
  {
      // Move direction must be compatible with pawn color
			/*
			WHITE側PAWNの移動はプラスとなりBLACK側PAWNの移動はマイナスとなる
			従って手番がWHITEの時directionがプラスではないのはNG
			*/
      int direction = to - from;
      if ((us == WHITE) != (direction > 0))
          return false;

      // We have already handled promotion moves, so destination
      // cannot be on the 8/1th rank.
			/*
			PAWNの移動先がRANK_8またはRANK_1ではNG
			（ここにいるということは着手パターンがPMOTOTIONではないので）
			*/
      if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1)
          return false;

      // Proceed according to the square delta between the origin and
      // destination squares.
			/*
			directionはPAWNの移動距離（WHITEならプラス、BLACKならマイナス）
			DELTA_NW、DELTA_NE、DELTA_SW、DELTA_SEは方向子
			*/
      switch (direction)
      {
      case DELTA_NW:
      case DELTA_NE:
      case DELTA_SW:
      case DELTA_SE:
      // Capture. The destination square must be occupied by an enemy
      // piece (en passant captures was handled earlier).
			/*
			斜めの方向子は取る動きなのでその移動先に駒がない　||　あっても自陣の駒ではNG
			ここにいるということはアンパッサンの心配はいらない
			*/
      if (piece_on(to) == NO_PIECE || color_of(piece_on(to)) != ~us)
          return false;

      // From and to files must be one file apart, avoids a7h5
			/*
			PAWNの取る動作は左右どちらでも列１分しか動かない
			*/
      if (abs(file_of(from) - file_of(to)) != 1)
          return false;
      break;

      case DELTA_N:
      case DELTA_S:
      // Pawn push. The destination square must be empty.
			/*
			１升だけ動くターンの時は駒は取らない
			*/
      if (!empty(to))
          return false;
      break;

      case DELTA_NN:
      // Double white pawn push. The destination square must be on the fourth
      // rank, and both the destination square and the square between the
      // source and destination squares must be empty.
				/*
				http://chess.plala.jp/p2-6.html
				PAWNは最初の位置からの移動のみ２升動ける
				ここはWHITE側PAWNなので移動先がRANK_4ではない　||　移動先に駒がある　||　移動先との間に駒があるはNG
				*/
      if (    rank_of(to) != RANK_4
          || !empty(to)
          || !empty(from + DELTA_N))
          return false;
      break;

      case DELTA_SS:
      // Double black pawn push. The destination square must be on the fifth
      // rank, and both the destination square and the square between the
      // source and destination squares must be empty.
				/*
				PAWNは最初の位置からの移動のみ２升動ける
				ここはBLACK側PAWNなので移動先がRANK_5ではない　 || 移動先に駒がある　 || 移動先との間に駒があるはNG
				*/
					if (rank_of(to) != RANK_5
          || !empty(to)
          || !empty(from + DELTA_S))
          return false;
      break;

      default:
          return false;
      }
  }	//PAWNの場合のチエックここまで、その他の駒のチエックは駒の利き上に移動先があることを見ている
  else if (!(attacks_from(pc, from) & to))
      return false;

  // Evasions generator already takes care to avoid some kind of illegal moves
  // and pl_move_is_legal() relies on this. So we have to take care that the
  // same kind of moves are filtered out here.
	/*
	手番側のKINGに王手をかけている駒があって、指し手の駒がKINGでなくって他の駒で、王手をかけているのが
	単独ではなく開き王手であった場合、開き王手の場合、王自身の移動以外正解はないのでその時点でNG
	*/
  if (checkers())
  {
      if (type_of(pc) != KING)
      {
          // Double check? In this case a king move is required
          if (more_than_one(checkers()))
              return false;

          // Our move must be a blocking evasion or a capture of the checking piece
					/*
					王手をかけている駒とKINGの間に入って利きを遮断する手か王手をかけている駒自身を
					取る手以外の手はNG
					*/
          if (!((between_bb(lsb(checkers()), king_square(us)) | checkers()) & to))
              return false;
      }
      // In case of king moves under check we have to remove king so to catch
      // as invalid moves like b1a1 when opposite queen is on c1.
			/*
			ここにくるのはKNIGで、その移動先に敵駒の利きが利いていたらNG
			*/
      else if (attackers_to(to, pieces() ^ from) & pieces(~us))
          return false;
  }
  return true;
}


/// Position::move_gives_check() tests whether a pseudo-legal move gives a check
/*
指し手が王手であればtrueを返す（チエックしている）
呼んでいる関数
generate_castle
move_to_san
do_move
perft
search
qsearch
*/
bool Position::gives_check(Move m, const CheckInfo& ci) const 
{

  assert(is_ok(m));
  assert(ci.dcCandidates == discovered_check_candidates());
  assert(color_of(moved_piece(m)) == sideToMove);

  Square from = from_sq(m);
  Square to = to_sq(m);
  PieceType pt = type_of(piece_on(from));

  // Direct check ?
	//駒の移動先に移動した結果王手が掛けれる位置ならtrue
	if (ci.checkSq[pt] & to)
      return true;

  // Discovery check ?
	/*
	移動することで王手が掛けれるなら（開き王手）trueを返す
	*/
	if (unlikely(ci.dcCandidates) && (ci.dcCandidates & from))
  {
      // For pawn and king moves we need to verify also direction
      if (   (pt != PAWN && pt != KING)
          || !aligned(from, to, king_square(~sideToMove)))
          return true;
  }

  // Can we skip the ugly special cases ?
	/*
	指し手パターンがノーマルならfalse？
	*/
  if (type_of(m) == NORMAL)
      return false;

  Color us = sideToMove;
  Square ksq = king_square(~us);
	/*
	指し手パターンによって判断、NORMALなら即false
	指し手パターンが成りで成った先で王手ができるようならtrue
	*/
	switch (type_of(m))
  {
  case PROMOTION:
      return attacks_from(Piece(promotion_type(m)), to, pieces() ^ from) & ksq;

  // En passant capture with check ? We have already handled the case
  // of direct checks and ordinary discovered check, the only case we
  // need to handle is the unusual case of a discovered check through
  // the captured pawn.
	/*
	アンパッサン関係のようだが詳細不明
	*/
	case ENPASSANT:
  {
      Square capsq = file_of(to) | rank_of(from);
      Bitboard b = (pieces() ^ from ^ capsq) | to;

      return  (attacks_bb<  ROOK>(ksq, b) & pieces(us, QUEEN, ROOK))
            | (attacks_bb<BISHOP>(ksq, b) & pieces(us, QUEEN, BISHOP));
  }
	/*
	キャスリングのようだが詳細不明
	*/
	case CASTLE:
  {
      Square kfrom = from;
      Square rfrom = to; // 'King captures the rook' notation
      Square kto = relative_square(us, rfrom > kfrom ? SQ_G1 : SQ_C1);
      Square rto = relative_square(us, rfrom > kfrom ? SQ_F1 : SQ_D1);

      return   (PseudoAttacks[ROOK][rto] & ksq)
            && (attacks_bb<ROOK>(rto, (pieces() ^ kfrom ^ rfrom) | rto | kto) & ksq);
  }
  default:
      assert(false);
      return false;
  }
}


/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.
/*
局面を更新する関数、下の関数do_moveのオーバーロード
*/
void Position::do_move(Move m, StateInfo& newSt) 
{
	/*
	CheckInfoで手番側に王手がかかっているかチエックしている
	反対にgives_checkはこれから指す手が王手かどうかをチエックしている
	*/
  CheckInfo ci(*this);
  do_move(m, newSt, ci, gives_check(m, ci));
}
/*
局面を更新する唯一の関数
*/
void Position::do_move(Move m, StateInfo& newSt, const CheckInfo& ci, bool moveIsCheck) 
{

  assert(is_ok(m));
  assert(&newSt != m_st);
	/*
	展開したノード数をカウントしている
	think関数で探索が終了したあとnodes_searched関数を呼び出し
	このnodes数を表示させる
	*/
	++nodes;
	/*
	StateInfo.keyに局面のハッシュ値が記録されている
	*/
	Key k = m_st->key;

  // Copy some fields of old state to our new StateInfo object except the ones
  // which are going to be recalculated from scratch anyway, then switch our state
  // pointer to point to the new, ready to be updated, state.
	/*
	StateCopySize64はStateInfo構造体のなかでkeyアイテムまでのオフセット（byte単位）数を返す
	つまりStateInfo構造体の一部だけnewStにコピーする
	コピーされるアイテムは
	Key pawnKey;
	Key	materialKey;
	Value npMaterial[COLOR_NB];
	int castleRights, rule50, pliesFromNull;
	Score psq;
	Square epSquare;
	Key key;
	*/
	std::memcpy(&newSt, m_st, StateCopySize64 * sizeof(uint64_t));
	/*
	StateInfoをつないでいる
	*/
	newSt.previous = m_st;
  m_st = &newSt;

  // Update side to move
	/*
	局面のハッシュ値を更新している
	*/
	k ^= Zobrist::side;

  // Increment ply counters.In particular rule50 will be later reset it to zero
  // in case of a capture or a pawn move.
	/*
	gamePlyはゲーム手数のカウントアップ
	rule50のためのカウントアップ
	pliesFromNullは今のところ不明do_null_move関数では0に初期化する
	*/
	++gamePly;
  ++m_st->rule50;
  ++m_st->pliesFromNull;

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = piece_on(from);
  PieceType pt = type_of(pc);
  PieceType captured = type_of(m) == ENPASSANT ? PAWN : type_of(piece_on(to));

  assert(color_of(pc) == us);
  assert(piece_on(to) == NO_PIECE || color_of(piece_on(to)) == them || type_of(m) == CASTLE);
  assert(captured != KING);

  if (type_of(m) == CASTLE)
  {
      assert(pc == make_piece(us, KING));

      bool kingSide = to > from;
      Square rfrom = to; // Castle is encoded as "king captures friendly rook"
      Square rto = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
      to = relative_square(us, kingSide ? SQ_G1 : SQ_C1);
      captured = NO_PIECE_TYPE;

      do_castle(from, to, rfrom, rto);

			m_st->psq += piece_sq_score[us][ROOK][rto] - piece_sq_score[us][ROOK][rfrom];
      k ^= Zobrist::psq[us][ROOK][rfrom] ^ Zobrist::psq[us][ROOK][rto];
  }
	/*
	capturedは取った駒の駒種、もし取る手ではなければcapturedは0
	*/
	if (captured)
  {
      Square capsq = to;

      // If the captured piece is a pawn, update pawn hash key, otherwise
      // update non-pawn material.
			/*
			もしとった駒種がPAWNで取り方がアンパッサンなら
			とった駒は移動先の真後ろになる（アンパッサンのルール確認）ので
			capsqはtoではなくto+pawn_push(them)となる
			*/
			if (captured == PAWN)
      {
          if (type_of(m) == ENPASSANT)
          {
              capsq += pawn_push(them);

              assert(pt == PAWN);
              assert(to == m_st->epSquare);
              assert(relative_rank(us, to) == RANK_6);
              assert(piece_on(to) == NO_PIECE);
              assert(piece_on(capsq) == make_piece(them, PAWN));

              board[capsq] = NO_PIECE;
          }

          m_st->pawnKey ^= Zobrist::psq[them][PAWN][capsq];
      }
      else
          m_st->npMaterial[them] -= PieceValue[MG][captured];

      // Update board and piece lists
			/*
			駒を取ったことによって変更になる
			byTypeBB,byTypeBB,byColorBBを更新
			index[],pieceList[][][],pieceCount[][]配列を更新する関数
			*/
			remove_piece(capsq, them, captured);

      // Update material hash key and prefetch access to materialTable
			/*
			局面のハッシュ値から取られた駒のハッシュ値を除去している
			同時にmaterialKeyも変更している
			*/
			k ^= Zobrist::psq[them][captured][capsq];
      m_st->materialKey ^= Zobrist::psq[them][captured][pieceCount[them][captured]];
      prefetch((char*)thisThread->materialTable[m_st->materialKey]);

      // Update incremental scores
			/*
			m_st->psqは位置評価値の集計値なので取られた駒の差分をしている
			*/
			m_st->psq -= piece_sq_score[them][captured][capsq];

      // Reset rule 50 counter
			/*
			駒を取ったのでrule50は一旦キャンセルとなる
			*/
			m_st->rule50 = 0;
  }
	/*
	これ以降は駒を取られていない指し手の更新
	*/
	// Update hash key
	/*
	移動前のハッシュ値を除去し、移動後のハッシュ値を更新している
	*/
	k ^= Zobrist::psq[us][pt][from] ^ Zobrist::psq[us][pt][to];

  // Reset en passant square
	/*
	アンパッサン関係だと思うが詳細不明
	*/
	if (m_st->epSquare != SQ_NONE)
  {
      k ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      m_st->epSquare = SQ_NONE;
  }

  // Update castle rights if needed
	/*
	キャスリング関係かな、詳細不明
	*/
	if (m_st->castleRights && (castleRightsMask[from] | castleRightsMask[to]))
  {
      int cr = castleRightsMask[from] | castleRightsMask[to];
      k ^= Zobrist::castle[m_st->castleRights & cr];
      m_st->castleRights &= ~cr;
  }

  // Prefetch TT access as soon as we know the new hash key
  prefetch((char*)TT.first_entry(k));

  // Move the piece. The tricky Chess960 castle is handled earlier
	/*
	move_piece関数の機能は
	byTypeBB,byTypeBB,byColorBBの更新
	board,index[],pieceList[][][]配列の更新
	駒取りはないのでpieceCount[]配列の更新はない
	*/
	if (type_of(m) != CASTLE)
      move_piece(from, to, us, pt);

  // If the moving piece is a pawn do some special extra work
  if (pt == PAWN)
  {
      // Set en-passant square, only if moved pawn can be captured
		/*
		(int(to) ^ int(from)) == 16となるPAWNの動きは２段とびのみ
		でかつアンパッサンできる条件が成立している場合の処理
		*/
		if ((int(to) ^ int(from)) == 16
          && (attacks_from<PAWN>(from + pawn_push(us), us) & pieces(them, PAWN)))
      {
          m_st->epSquare = Square((from + to) / 2);
          k ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      }
		/*
		PAWNがなる場合の処理
		*/
		if (type_of(m) == PROMOTION)
      {
          PieceType promotion = promotion_type(m);

          assert(relative_rank(us, to) == RANK_8);
          assert(promotion >= KNIGHT && promotion <= QUEEN);
					/*
					一旦PAWNを除去する処理
					*/
					remove_piece(to, us, PAWN);
					/*
					なった駒を移動先に置く処理
					*/
					put_piece(to, us, promotion);

          // Update hash keys
					/*
					局面のハッシュ値をを更新、pawn専用のハッシュ値も更新
					*/
					k ^= Zobrist::psq[us][PAWN][to] ^ Zobrist::psq[us][promotion][to];
          m_st->pawnKey ^= Zobrist::psq[us][PAWN][to];
          m_st->materialKey ^=  Zobrist::psq[us][promotion][pieceCount[us][promotion]-1]
                            ^ Zobrist::psq[us][PAWN][pieceCount[us][PAWN]];

          // Update incremental score
					//位置評価値も更新
					m_st->psq += piece_sq_score[us][promotion][to] - piece_sq_score[us][PAWN][to];

          // Update material
					/*
					駒評価値も更新
					*/
					m_st->npMaterial[us] += PieceValue[MG][promotion];
      }

      // Update pawn hash key and prefetch access to pawnsTable
      m_st->pawnKey ^= Zobrist::psq[us][PAWN][from] ^ Zobrist::psq[us][PAWN][to];
      prefetch((char*)thisThread->pawnsTable[m_st->pawnKey]);

      // Reset rule 50 draw counter
			/*
			引き分け条件をクリア
			*/
			m_st->rule50 = 0;
  }
	/*
	これ以降は駒を取らない指し手でPAWN以外の駒種の処理、共通処理かな
	*/
	// Update incremental scores
	/*
	位置評価値の更新
	*/
	m_st->psq += piece_sq_score[us][pt][to] - piece_sq_score[us][pt][from];

  // Set capture piece
	/*
	とった駒種
	*/
	m_st->capturedType = captured;

  // Update the key with the final value
	/*
	最終ハッシュ値を登録
	*/
	m_st->key = k;

  // Update checkers bitboard, piece must be already moved
  m_st->checkersBB = 0;
	/*
	moveIsCheckはdo_move関数の引数の１つ
	王手の手があるならtrue
	*/
	if (moveIsCheck)
  {
      if (type_of(m) != NORMAL)
          m_st->checkersBB = attackers_to(king_square(them)) & pieces(us);
      else
      {
          // Direct checks
					/*
					ci.checkSq[pt]には駒種ごとに敵KINGに王手をかけることで出来るbitboardがはいっている
					今回のMoveによってその場所に移動できたかをci.checkSq[pt] & toでチエックしている
					そしてチエックが可能であればcheckerBBに追加している
					*/
					if (ci.checkSq[pt] & to)
						m_st->checkersBB |= to;

          // Discovery checks
					/*
					ROOKまたはBISHOPではない駒が動いたことでROOK,BISHOPの利きが敵KINGに届いたのではないかチエックしている
					*/
					if (ci.dcCandidates && (ci.dcCandidates & from))
          {
              if (pt != ROOK)
                  m_st->checkersBB |= attacks_from<ROOK>(king_square(them)) & pieces(us, QUEEN, ROOK);

              if (pt != BISHOP)
                  m_st->checkersBB |= attacks_from<BISHOP>(king_square(them)) & pieces(us, QUEEN, BISHOP);
          }
      }
  }
	/*
	手番の変更
	*/
	sideToMove = ~sideToMove;

  assert(pos_is_ok());
}


/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.
/*
手を戻す関数
do_move関数にくらべすごくコード量が少ない
*/
void Position::undo_move(Move m) 
{

  assert(is_ok(m));
	/*
	手番反転
	*/
  sideToMove = ~sideToMove;

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  PieceType pt = type_of(piece_on(to));
  PieceType captured = m_st->capturedType;

  assert(empty(from) || type_of(m) == CASTLE);
  assert(captured != KING);
	/*
	もし指し手パターンが成るパターンなら
	*/
  if (type_of(m) == PROMOTION)
  {
			/*
			promotionにはPAWNがなった駒種を入れる->QUEENかNIGHT
			*/
      PieceType promotion = promotion_type(m);

      assert(promotion == pt);
      assert(relative_rank(us, to) == RANK_8);
      assert(promotion >= KNIGHT && promotion <= QUEEN);
			/*
			remove_piece関数は駒を取り除く時の処理
			ひとまず、なった駒をもとに戻し通常の移動の処理と共通化する
			（remove_piece関数で成った駒を取り除く処理をして、PAWNを置く処理＝成ったPAWNを元に戻す）
			*/
			remove_piece(to, us, promotion);
      put_piece(to, us, PAWN);
      pt = PAWN;
  }
	/*
	キャスリング関係の戻しかな
	*/
	if (type_of(m) == CASTLE)
  {
      bool kingSide = to > from;
      Square rfrom = to; // Castle is encoded as "king captures friendly rook"
      Square rto = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
      to = relative_square(us, kingSide ? SQ_G1 : SQ_C1);
      captured = NO_PIECE_TYPE;
      pt = KING;
      do_castle(to, from, rto, rfrom);
  }
  else
		/*
		移動の戻し（from,toをテレコにしている）
		*/
		move_piece(to, from, us, pt); // Put the piece back at the source square
	/*
	取った駒があれば(captured>0)、取られた場所に戻す
	但し取った方法がアンパッサンなら取られた場所はto座標ではないので注意
	*/
	if (captured)
  {
      Square capsq = to;

      if (type_of(m) == ENPASSANT)
      {
          capsq -= pawn_push(us);

          assert(pt == PAWN);
          assert(to == m_st->previous->epSquare);
          assert(relative_rank(us, to) == RANK_6);
          assert(piece_on(capsq) == NO_PIECE);
      }

      put_piece(capsq, them, captured); // Restore the captured piece
  }

  // Finally point our state pointer back to the previous state
	/*
	StateInfoを１手戻している,m_stが指していたStateInfoは自動変数なのでメモリーリークの心配はいらない
	gamePlyも戻している
	*/
  m_st = m_st->previous;
  --gamePly;

  assert(pos_is_ok());
}


/// Position::do_castle() is a helper used to do/undo a castling move. This
/// is a bit tricky, especially in Chess960.
/*
キャスリング関係、詳細不明
*/
void Position::do_castle(Square kfrom, Square kto, Square rfrom, Square rto) 
{

  // Remove both pieces first since squares could overlap in Chess960
  remove_piece(kfrom, sideToMove, KING);
  remove_piece(rfrom, sideToMove, ROOK);
  board[kfrom] = board[rfrom] = NO_PIECE; // Since remove_piece doesn't do it for us
  put_piece(kto, sideToMove, KING);
  put_piece(rto, sideToMove, ROOK);
}


/// Position::do(undo)_null_move() is used to do(undo) a "null move": It flips
/// the side to move without executing any move on the board.
/*
ヌルムーブ用の局面更新関数
*/
void Position::do_null_move(StateInfo& newSt) 
{

  assert(!checkers());
	/*
	手番はパスするがStateInfoはコピーが必要
	*/
  std::memcpy(&newSt, m_st, sizeof(StateInfo)); // Fully copy here

  newSt.previous = m_st;
  m_st = &newSt;
	/*
	アンパッサン？
	*/
  if (m_st->epSquare != SQ_NONE)
  {
      m_st->key ^= Zobrist::enpassant[file_of(m_st->epSquare)];
      m_st->epSquare = SQ_NONE;
  }
	/*
	局面は変わらないが手番は変わるのでハッシュ値は更新
	*/
  m_st->key ^= Zobrist::side;
  prefetch((char*)TT.first_entry(m_st->key));

  ++m_st->rule50;
	/*
	pliesFromNullはヌルムーブからのカウントなのでここでカウントゼロ
	*/
  m_st->pliesFromNull = 0;
	/*
	手番切り替え
	*/
  sideToMove = ~sideToMove;

  assert(pos_is_ok());
}
/*
ヌルムーブ用の局面復元関数
*/
void Position::undo_null_move() 
{

  assert(!checkers());

  m_st = m_st->previous;
  sideToMove = ~sideToMove;
}


/// Position::see() is a static exchange evaluator: It tries to estimate the
/// material gain or loss resulting from a move. Parameter 'asymmThreshold' takes
/// tempi into account. If the side who initiated the capturing sequence does the
/// last capture, he loses a tempo and if the result is below 'asymmThreshold'
/// the capturing sequence is considered bad.
/*
静止探索
駒の取り合いの評価、探索ルーチンを使用せず駒の取り合いがなくなるまで評価を続ける
*/
int Position::see_sign(Move m) const 
{

  assert(is_ok(m));

  // Early return if SEE cannot be negative because captured piece value
  // is not less then capturing one. Note that king moves always return
  // here because king midgame value is set to 0.
	/*
	手番の指し手の駒価値（中盤）がこれから取ろうとしている駒の価値（中盤）より小さいまたは同等なら
	１で帰れ（より価値の小さい駒で価値の大きな駒を取ることはOKらしい
	つまりこれ以降の処理(see関数を呼ぶ処理）は指し手の駒が取られる駒より価値が高い場合の
	処理と言うことになる
	*/
  if (PieceValue[MG][moved_piece(m)] <= PieceValue[MG][piece_on(to_sq(m))])
      return 1;

  return see(m);
}
/*
静止探索
*/
int Position::see(Move m, int asymmThreshold) const 
{

  Square from, to;
  Bitboard occupied, attackers, stmAttackers;
  int swapList[32], slIndex = 1;
  PieceType captured;
  Color stm;

  assert(is_ok(m));

  from = from_sq(m);
  to = to_sq(m);
	/*
	swapListの最初にはto座標にいる駒の価値を入れておく（最初に取られる駒）
	*/
  swapList[0] = PieceValue[MG][piece_on(to)];
  stm = color_of(piece_on(from));
	/*
	カラーに関係なく全駒のbitboard、但しfromにいる駒を除く
	*/
  occupied = pieces() ^ from;

  // Castle moves are implemented as king capturing the rook so cannot be
  // handled correctly. Simply return 0 that is always the correct value
  // unless in the rare case the rook ends up under attack.
	/*
	キャスリング関係なら価値0で返る
	*/
	if (type_of(m) == CASTLE)
      return 0;
	/*
	指し手パターンがアンパッサンならswapListにpawnを入れておく
	*/
	if (type_of(m) == ENPASSANT)
  {
      occupied ^= to - pawn_push(stm); // Remove the captured pawn
      swapList[0] = PieceValue[MG][PAWN];
  }

  // Find all attackers to the destination square, with the moving piece
  // removed, but possibly an X-ray attacker added behind it.
	/*
	指した手の移動先に利いている駒のbitboardをattackersに入れる（カラーに関係なく）
	*/
	attackers = attackers_to(to, occupied) & occupied;

  // If the opponent has no attackers we are finished
	/*
	stmAttackersに敵の駒だけを入れる
	もし敵の駒がなければ（つまり取り合いがなければ）
	そこで中断
	*/
	stm = ~stm;
  stmAttackers = attackers & pieces(stm);
  if (!stmAttackers)
      return swapList[0];

  // The destination square is defended, which makes things rather more
  // difficult to compute. We proceed by building up a "swap list" containing
  // the material gain or loss at each stop in a sequence of captures to the
  // destination square, where the sides alternately capture, and always
  // capture with the least valuable piece. After each capture, we look for
  // new X-ray attacks from behind the capturing piece.
	/*
	slIndexは１から始まる、0は最初にto座標にいる駒が入っている
	swapList[]は取り合い駒リスト
	*/
	captured = type_of(piece_on(from));
	do {
			/*
			swapList配列は３２まで
			*/
      assert(slIndex < 32);

      // Add the new entry to the swap list
			/*
			取り合いになっていくがその駒評価値をswapListに記録する
			直前に取った駒の価値との差分を記録する
			
			*/
			swapList[slIndex] = -swapList[slIndex - 1] + PieceValue[MG][captured];
      ++slIndex;

      // Locate and remove the next least valuable attacker
			/*
			まず取り合いはPAWNからおこない、to座標にきている駒を取り合い
			その駒種を返す、
			*/
			captured = min_attacker<PAWN>(byTypeBB, to, stmAttackers, occupied, attackers);
      stm = ~stm;
      stmAttackers = attackers & pieces(stm);

      // Stop before processing a king capture
			/*
			取り合いになり徐々に駒種がPAWNから上がっていくがKNIGになったらそこでやめる
			もしくはbitboardがなくなったらやめる
			*/
			if (captured == KING && stmAttackers)
      {
          swapList[slIndex++] = QueenValueMg * 16;
          break;
      }

  } while (stmAttackers);	//アタッカー駒があれば続ける

  // If we are doing asymmetric SEE evaluation and the same side does the first
  // and the last capture, he loses a tempo and gain must be at least worth
  // 'asymmThreshold', otherwise we replace the score with a very low value,
  // before negamaxing.
	/*
	asymmThresholdはsee関数」の引数、デフォルト数は0
	0以上であれば　swapList[i]を繰ってasymmThresholdより小さければ
	もしasymmThresholdが0より大きいならそれより価値の低い駒取っていたなら
	その価値をさらに低くする（クイーン１６個分より低く）
	*/
  if (asymmThreshold)
      for (int i = 0; i < slIndex; i += 2)
          if (swapList[i] < asymmThreshold)
              swapList[i] = - QueenValueMg * 16;

  // Having built the swap list, we negamax through it to find the best
  // achievable score from the point of view of the side to move.
	/*
	swapListを遡り最小の評価値を得る？
	*/
	while (--slIndex)
      swapList[slIndex - 1] = std::min(-swapList[slIndex], swapList[slIndex - 1]);

  return swapList[0];
}


/// Position::clear() erases the position object to a pristine state, with an
/// empty board, white to move, and no castling rights.
/*
positionクラスをクリアにする、
*/
void Position::clear() 
{

  std::memset(this, 0, sizeof(Position));
  startState.epSquare = SQ_NONE;
  m_st = &startState;

  for (int i = 0; i < PIECE_TYPE_NB; ++i)
      for (int j = 0; j < 16; ++j)
          pieceList[WHITE][i][j] = pieceList[BLACK][i][j] = SQ_NONE;
}


/// Position::compute_key() computes the hash key of the position. The hash
/// key is usually updated incrementally as moves are made and unmade, the
/// compute_key() function is only used when a new position is set up, and
/// to verify the correctness of the hash key when running in debug mode.
/*
局面の全ての駒と手番とキャスリング、アンパッサンに元ずいてハッシュ値を決める
pos_is_ok関数とset関数のみから呼ばれる
*/
Key Position::compute_key() const 
{

  Key k = Zobrist::castle[m_st->castleRights];

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      k ^= Zobrist::psq[color_of(piece_on(s))][type_of(piece_on(s))][s];
  }

  if (ep_square() != SQ_NONE)
      k ^= Zobrist::enpassant[file_of(ep_square())];

  if (sideToMove == BLACK)
      k ^= Zobrist::side;

  return k;
}

/// Position::compute_pawn_key() computes the hash key of the position. The
/// hash key is usually updated incrementally as moves are made and unmade,
/// the compute_pawn_key() function is only used when a new position is set
/// up, and to verify the correctness of the pawn hash key when running in
/// debug mode.
/*
局面上のPAWNだけの情報に基づいてハッシュ値を決める
pos_is_ok関数とset関数のみから呼ばれる
*/
Key Position::compute_pawn_key() const 
{

  Key k = 0;

  for (Bitboard b = pieces(PAWN); b; )
  {
      Square s = pop_lsb(&b);
      k ^= Zobrist::psq[color_of(piece_on(s))][PAWN][s];
  }

  return k;
}

/// Position::compute_material_key() computes the hash key of the position.
/// The hash key is usually updated incrementally as moves are made and unmade,
/// the compute_material_key() function is only used when a new position is set
/// up, and to verify the correctness of the material hash key when running in
/// debug mode.
/*
Zobrist::psq[2][8][64]と最後の升は本来、座標を表すものだが
駒数で乱数表を引いている
デバックチエックのpos_is_ok関数からと、set関数からのみ呼ばれている
駒だけ（material）の情報でハッシュ値を決めている
*/
Key Position::compute_material_key() const 
{

  Key k = 0;

  for (Color c = WHITE; c <= BLACK; ++c)
      for (PieceType pt = PAWN; pt <= QUEEN; ++pt)
          for (int cnt = 0; cnt < pieceCount[c][pt]; ++cnt)
              k ^= Zobrist::psq[c][pt][cnt];

  return k;
}


/// Position::compute_psq_score() computes the incremental scores for the middle
/// game and the endgame. These functions are used to initialize the incremental
/// scores when a new position is set up, and to verify that the scores are correctly
/// updated by do_move and undo_move when the program is running in debug mode.
/*
位置評価値の集計
*/
Score Position::compute_psq_score() const 
{

  Score score = SCORE_ZERO;

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(&b);
      Piece pc = piece_on(s);
			score += piece_sq_score[color_of(pc)][type_of(pc)][s];
  }

  return score;
}


/// Position::compute_non_pawn_material() computes the total non-pawn middle
/// game material value for the given side. Material values are updated
/// incrementally during the search, this function is only used while
/// initializing a new Position object.
/*
PAWNを除いた駒評価値の集計（中盤評価値）
*/
Value Position::compute_non_pawn_material(Color c) const 
{

  Value value = VALUE_ZERO;

  for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt)
      value += pieceCount[c][pt] * PieceValue[MG][pt];

  return value;
}


/// Position::is_draw() tests whether the position is drawn by material,
/// repetition, or the 50 moves rule. It does not detect stalemates, this
/// must be done by the search.
/*
ドロー（引き分け）
次の場合は、「自動的」にドローとなる。
ステイルメイト ： 自分の手番で、自分のキングにチェックされてはいないが、合法手がない状況を指す。
ドロー・オファー： 片方がドローを提案し、もう片方がそれを承諾した場合。
デッド・ポジション[8]： 駒の兵力不足のため、双方が相手のキングをチェックメイトできなくなった状況を指す。次の駒の組合せの時は、
たとえ敵の駒がキング一つだけであってもチェックメイトすることはできない。[9]
	キング + ビショップ1個
	キング + ナイト1個
	（キング + ナイト2個

次の場合、一方のプレーヤーの「申請（クレーム）」によりドローとなる
50手ルール ： 50手連続して両者ともポーンが動かず、またお互いに駒を取らない場合。
スリーフォールド・レピティション（同形三復）： 同一の局面が3回現れた場合。
*/
bool Position::is_draw() const 
{

  // Draw by material?
	/*
	カラーに関係なくPAWNがない　&&　両カラーの駒価値の合計がBishopValueMg以下=>デッド・ポジションかな
	*/
  if (   !pieces(PAWN)
      && (non_pawn_material(WHITE) + non_pawn_material(BLACK) <= BishopValueMg))
      return true;

  // Draw by the 50 moves rule?
	/*
	指し手数が５０手（先手、後手合わせて１手と棋譜には表記する）を
	超えて、手番での王手がない　||　合法手はあるが膠着状態と判断できたらドロー
	*/
  if (m_st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
      return true;

  int i = 4, e = std::min(m_st->rule50, m_st->pliesFromNull);
	/*
	スリーフォールド・レピティションの判断をしている
	e:現在の手数をm_st->rule50, m_st->pliesFromNullで判断している
	これは何故かというとrule50は駒を取ったりするとカウントクリアしているので
	同形が出やすい
	*/
  if (i <= e)
  {
			/*
			現局面より２手遡る
			*/
      StateInfo* stp = m_st->previous->previous;

      do {
					/*
					２手づつ遡る
					*/
          stp = stp->previous->previous;
					/*
					手を遡って１回ハッシュ値で同形と判断したらドロー判定
					ルールでは同形３回とあるが何故
					*/
          if (stp->key == m_st->key)
              return true; // Draw after first repetition

          i += 2;

      } while (i <= e);
  }

  return false;
}


/// Position::flip() flips position with the white and black sides reversed. This
/// is only useful for debugging especially for finding evaluation symmetry bugs.
/*
渡された文字が小文字なら大文字に変換し大文字なら小文字にする
すぐ下にあるflip関数のヘルパー、先手、後手を反転させる
*/
static char toggle_case(char c) 
{
  return char(islower(c) ? toupper(c) : tolower(c));
}
/*
getline(is,str,delim);
is:文字列の抽出元となる入力ストリーム。
str:入力ストリームから抽出した文字の読み込み先となる文字列。
delim:行の区切り記号。
splitの代わりみたいな関数
string.insert(index,string)
indexの位置に文字列を挿入する
std::transform(start,end,result,func)
startからendまでの範囲に関数funcを適用してresultに結果を返す
関数toggle_caseは文字（文字列ではない）を受け取りそれが小文字なら
大文字にして返す、大文字だったら小文字にして返す
*/
void Position::flip() 
{

  string f, token;
  std::stringstream ss(fen());
	/*
	fen文字列を逆にしている
	rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNRが入ってくる文字
	fには"RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbqkbnr "
	と反対に構築する
	*/
	for (Rank rank = RANK_8; rank >= RANK_1; --rank) // Piece placement
  {
      std::getline(ss, token, rank > RANK_1 ? '/' : ' ');
      f.insert(0, token + (f.empty() ? " " : "/"));
  }
	/*
	カラーを変えている
	*/
	ss >> token; // Active color
  f += (token == "w" ? "B " : "W "); // Will be lowercased later

  ss >> token; // Castling availability
  f += token + " ";
	/*
	大文字を小文字に、小文字を大文字に変換
	つまりWHITEをBLACKにBLACKをWHITEにする
	*/
	std::transform(f.begin(), f.end(), f.begin(), toggle_case);

  ss >> token; // En passant square
  f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

  std::getline(ss, token); // Half and full moves
  f += token;

  set(f, is_chess960(), this_thread());

  assert(pos_is_ok());
}


/// Position::pos_is_ok() performs some consitency checks for the position object.
/// This is meant to be helpful when debugging.
/*
デバック用のチエックするもの？
*/
bool Position::pos_is_ok(int* failedStep) const 
{

  int dummy, *step = failedStep ? failedStep : &dummy;

  // What features of the position should be verified?
  const bool all = false;

  const bool debugBitboards       = all || false;
  const bool debugKingCount       = all || false;
  const bool debugKingCapture     = all || false;
  const bool debugCheckerCount    = all || false;
  const bool debugKey             = all || false;
  const bool debugMaterialKey     = all || false;
  const bool debugPawnKey         = all || false;
  const bool debugIncrementalEval = all || false;
  const bool debugNonPawnMaterial = all || false;
  const bool debugPieceCounts     = all || false;
  const bool debugPieceList       = all || false;
  const bool debugCastleSquares   = all || false;

  *step = 1;
	/*
	sideToMoveがWHITE,BLACKになっているか
	ちゃんとking_square配列に正しくkingの座標が入っているか
	*/
	if (sideToMove != WHITE && sideToMove != BLACK)
      return false;

  if ((*step)++, piece_on(king_square(WHITE)) != W_KING)
      return false;

  if ((*step)++, piece_on(king_square(BLACK)) != B_KING)
      return false;
	/*
	debugKingCountがtrueになっていればこの検査をする
	盤をスキャンしてWHITE,BLACKのKINGが１つづつでなければNG
	*/
  if ((*step)++, debugKingCount)
  {
      int kingCount[COLOR_NB] = {};

      for (Square s = SQ_A1; s <= SQ_H8; ++s)
          if (type_of(piece_on(s)) == KING)
              ++kingCount[color_of(piece_on(s))];

      if (kingCount[0] != 1 || kingCount[1] != 1)
          return false;
  }
	/*
	debugKingCaptureがtrueになっていれば検査する
	相手側のKINGに王手がかかっているとNG
	このpos_is_ok関数は局面をセットした時やdo_move関数で局面を更新した時の最後の呼ばれるので
	その時点でKINGに王手がかかっていましたはNGである
	*/
  if ((*step)++, debugKingCapture)
      if (attackers_to(king_square(~sideToMove)) & pieces(sideToMove))
          return false;
	/*
	debugCheckerCountがtrueでかつ手番側のKINGに王手を掛けている駒が３つ以上ある
	王手は最大でも２駒までしかかけられないのでNG
	*/
  if ((*step)++, debugCheckerCount && popcount<Full>(m_st->checkersBB) > 2)
      return false;
	/*
	debugBitboardsがtrueなら検査
	*/
  if ((*step)++, debugBitboards)
  {
      // The intersection of the white and black pieces must be empty
		/*
		お互いのカラーのANDをとると必ず0になるはず、成らなかったら
		おかしい
		*/
		if (pieces(WHITE) & pieces(BLACK))
          return false;

      // The union of the white and black pieces must be equal to all
      // occupied squares
		/*
		WHITEとBLACKのORは全ての駒と一緒のはず
		そうでなければおかしい
		*/
		if ((pieces(WHITE) | pieces(BLACK)) != pieces())
          return false;

      // Separate piece type bitboards must have empty intersections
		/*
		駒種が違うもの同士は重ならない
		重なったらおかしい
		*/
		for (PieceType p1 = PAWN; p1 <= KING; ++p1)
          for (PieceType p2 = PAWN; p2 <= KING; ++p2)
              if (p1 != p2 && (pieces(p1) & pieces(p2)))
                  return false;
  }
	/*
	アンパッサンのチエック、アンパッサンのルール上アンパサンが起こるランクはBLACK側からみたらRANK3
	WHITE側から見たらRANK6になるのでこのようなチエックをしている
	*/
  if ((*step)++, ep_square() != SQ_NONE && relative_rank(sideToMove, ep_square()) != RANK_6)
      return false;
	/*
	debugKeyがtrueなら検査する
	局面のハッシュ値が正しいかチエック
	*/
  if ((*step)++, debugKey && m_st->key != compute_key())
      return false;
	/*
	debugPawnKeyがtrueなら検査する
	pawnKeyが正しいかチエック
	*/
  if ((*step)++, debugPawnKey && m_st->pawnKey != compute_pawn_key())
      return false;
	/*
	debugMaterialKeyがtrueなら検査する
	materialKeyが正しいかチエック
	*/
  if ((*step)++, debugMaterialKey && m_st->materialKey != compute_material_key())
      return false;
	/*
	debugIncrementalEvalがtrueなら検査する
	位置評価値が正しいかチエック
	*/
  if ((*step)++, debugIncrementalEval && m_st->psq != compute_psq_score())
      return false;
	/*
	debugNonPawnMaterialがtrueなら検査する
	npMaterialが正しいかチエック
	*/
  if ((*step)++, debugNonPawnMaterial)
      if (   m_st->npMaterial[WHITE] != compute_non_pawn_material(WHITE)
          || m_st->npMaterial[BLACK] != compute_non_pawn_material(BLACK))
          return false;
	/*
	debugPieceCountsがtrueなら検査する
	pieceCount[][]に正しい数が入っているかチエック
	*/
  if ((*step)++, debugPieceCounts)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (PieceType pt = PAWN; pt <= KING; ++pt)
              if (pieceCount[c][pt] != popcount<Full>(pieces(c, pt)))
                  return false;
	/*
	debugPieceListがtrueなら検査する
	pieceCount[][]とpieceList[][][]とboard[]があっている
	*/
  if ((*step)++, debugPieceList)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (PieceType pt = PAWN; pt <= KING; ++pt)
              for (int i = 0; i < pieceCount[c][pt];  ++i)
                  if (   board[pieceList[c][pt][i]] != make_piece(c, pt)
                      || index[pieceList[c][pt][i]] != i)
                      return false;
	/*
	debugCastleSquaresがtrueなら検査する
	なんかキャスリングのチエック
	*/
  if ((*step)++, debugCastleSquares)
      for (Color c = WHITE; c <= BLACK; ++c)
          for (CastlingSide s = KING_SIDE; s <= QUEEN_SIDE; s = CastlingSide(s + 1))
          {
              CastleRight cr = make_castle_right(c, s);

              if (!can_castle(cr))
                  continue;

              if (  (castleRightsMask[king_square(c)] & cr) != cr
                  || piece_on(castleRookSquare[c][s]) != make_piece(c, ROOK)
                  || castleRightsMask[castleRookSquare[c][s]] != cr)
                  return false;
          }

  *step = 0;
  return true;
}
