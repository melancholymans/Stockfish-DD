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

#include <iostream>
#include <string>

#include "bitboard.h"
#include "evaluate.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"

/*test start
コードを見ても理解できない時
実際に動かして理解するためのtestコード
*/

using namespace Bitboards;
#include "rkiss.h"
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include "notation.h"

bool func(const ExtMove& left, const ExtMove& right);

using namespace std;

enum LLType{
	python, ruby, perl
};
void print_board(Position& pos);
void test(void);
template<LLType LT> void print(void);
struct S{
	int value = 42;
	char* str = "takemori";
	double f = 3.14159;
	operator int() const { return value; }
	operator char*() { return str; }
	operator double()  { return f; }
};
/*test end*/


int main(int argc, char* argv[]) {

  std::cout << engine_info() << std::endl;

  UCI::init(Options);
  Bitboards::init();
  Position::init();
  Bitbases::init_kpk();
  Search::init();
  Pawns::init();
  Eval::init();
  Threads.init();
  TT.set_size(Options["Hash"]);

  std::string args;

  for (int i = 1; i < argc; ++i)
      args += std::string(argv[i]) + " ";
	/*
	ゲームループに入る前にtest()
	*/
	test();
	UCI::loop(args);

  Threads.exit();
}

//codeをみただけではわからない
//いろいろ試して理解を促進する
void test(void)
{
	/*
	Score temp = make_score(24,11);
	printf("A1 %d\n",(~SQ_A1));
	printf("A2 %d\n",(~SQ_A2));
	printf("A3 %d\n",(~SQ_A3));
	printf("A4 %d\n",(~SQ_A4));
	printf("A5 %d\n",(~SQ_A5));
	printf("A6 %d\n",(~SQ_A6));
	printf("A7 %d\n",(~SQ_A7));
	printf("A8 %d\n",(~SQ_A8));
	printf("B1 %d\n",(~SQ_B1));
	*/
	/*
	for(File f = FILE_A;f < FILE_NB;++f){
	for(Rank r =  RANK_1;r < RANK_NB;++r){
	printf("square = %d\n",make_square(f,r));
	}
	}
	printf("%d\n",SQ_A1 & 7);
	*/
	//relative_square
	/*
	Square sq;
	for(sq = SQ_A1;sq < SQ_NONE;++sq){
	printf(" %2d",relative_square(WHITE,sq));
	if((sq % 8)==7){
	printf("\n");
	}
	}
	printf("\n");
	for(sq = SQ_A1;sq < SQ_NONE;++sq){
	printf(" %2d",relative_square(BLACK,sq));
	if((sq % 8)==7){
	printf("\n");
	}
	}
	*/
	//relative_rank
	/*
	printf("relative_rank\n");
	for(sq = SQ_A1;sq < SQ_NONE;++sq){
	printf(" %2d",relative_rank(WHITE,sq));
	if((sq % 8)==7){
	printf("\n");
	}
	}
	printf("\n");
	for(sq = SQ_A1;sq < SQ_NONE;++sq){
	printf(" %2d",relative_rank(BLACK,sq));
	if((sq % 8)==7){
	printf("\n");
	}
	}
	*/
	//opposite_colors
	/*
	printf("\n");
	Square s2 = SQ_A1;
	for(sq = SQ_A1;sq < SQ_NONE;++sq){
	printf(" %2d",opposite_colors(sq,SQ_A2));
	if((sq % 8)==7){
	printf("\n");
	}
	}

	File f = FILE_B;
	printf("%c\n",to_char(f,true));
	printf("%c\n",to_char(f,false));

	Rank r = RANK_1;
	printf("%c\n",to_char(r));
	*/
	//templateの実験
	/*
	print<python>();
	*/
	//DistanceRingsBB変数の初期値確認
	/*
	for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1){
	for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2){
	if (s1 != s2)
	{
	printf("s1=%d,s2=%d\n",s1,s2);
	printf("%s",Bitboards::pretty(DistanceRingsBB[s1][s2]).c_str());
	}
	}
	}
	*/
	//StepAttacksBBのなかみ確認
	/*
	Piece piece_code = W_KING;

	sq = SQ_A1;
	printf("piece=%d,sq=%d\n",piece_code,sq);
	printf("%s",Bitboards::pretty(StepAttacksBB[piece_code][sq]).c_str());

	sq = SQ_B1;
	printf("piece=%d,sq=%d\n",piece_code,sq);
	printf("%s",Bitboards::pretty(StepAttacksBB[piece_code][sq]).c_str());

	sq = SQ_C1;
	printf("piece=%d,sq=%d\n",piece_code,sq);
	printf("%s",Bitboards::pretty(StepAttacksBB[piece_code][sq]).c_str());

	sq = SQ_A2;
	printf("piece=%d,sq=%d\n",piece_code,sq);
	printf("%s",Bitboards::pretty(StepAttacksBB[piece_code][sq]).c_str());

	sq = SQ_B2;
	printf("piece=%d,sq=%d\n",piece_code,sq);
	printf("%s",Bitboards::pretty(StepAttacksBB[piece_code][sq]).c_str());
	*/
	//RAttacksのなかみ確認
	/*
	sq = SQ_A1;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_B2;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_C3;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_D4;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_E5;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_F6;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_G7;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());
	sq = SQ_H8;
	printf("piece=%d,sq=%d\n",ROOK,sq);
	printf("%s",Bitboards::pretty(*RAttacks[sq]).c_str());

	sq = SQ_A1;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_B2;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_C3;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_D4;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_E5;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_F6;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_G7;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	sq = SQ_H8;
	printf("piece=%d,sq=%d\n",BISHOP,sq);
	printf("%s",Bitboards::pretty(*BAttacks[sq]).c_str());
	*/
	//flipの確認
	/*
	Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, Threads.main());
	pos.flip();
	*/
	//SquareBBの確認
	/*
	printf("SquareBB\n");
	for (Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("%s",Bitboards::pretty(SquareBB[s]).c_str());
	}
	*/
	//PseudoAttacksの確認
	/*
	printf("PseudoAttacks Rook\n");
	for (Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("%s",Bitboards::pretty(PseudoAttacks[ROOK][s]).c_str());
	}
	*/
	/*
	printf("PseudoAttacks Bishop\n");
	for (Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("%s",Bitboards::pretty(PseudoAttacks[BISHOP][s]).c_str());
	}
	printf("PseudoAttacks Queen\n");
	for (Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("%s",Bitboards::pretty(PseudoAttacks[QUEEN][s]).c_str());
	}
	*/
	//LineBB[sq1][sq2]の確認
	/*
	printf("LineBB[sq1][sq2]\n");
	for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1){
	for(Square s2 = SQ_A1; s2 <= SQ_H8; ++s2){
	printf("%d %d \n %s",s1,s2,Bitboards::pretty(LineBB[s1][s2]).c_str());
	}
	}
	*/
	//BetweenBB[s1][s2]の確認
	/*
	printf("BetweenBB[sq1][sq2]\n");
	for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1){
	for(Square s2 = SQ_A1; s2 <= SQ_H8; ++s2){
	printf("%d %d \n %s",s1,s2,Bitboards::pretty(BetweenBB[s1][s2]).c_str());
	}
	}
	*/
	//pop_lsbの確認、コードが理解できないので、挙動を把握
	//pop_lsbは直接呼び出せないのでlsb関数で呼ぶ
	/*
	Bitboard bb;
	bb = 0xDD9;	//b1101 1101 1001
	printf("sq=%d\n",lsb(bb));
	printf("sq=%d\n",lsb(bb-1));
	*/
	//ForwardBBの確認
	/*
	printf("ForwardBB[color][sq]\n");
	for (Color c = WHITE; c <= BLACK; ++c){
	for(Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("color=%d sq=%d \n %s",c,s,Bitboards::pretty(ForwardBB[c][s]).c_str());
	}
	}
	*/
	//PawnAttackSpanの確認
	/*
	printf("PawnAttackSpan[color][sq]\n");
	for (Color c = WHITE; c <= BLACK; ++c){
	for(Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("color=%d sq=%d \n %s",c,s,Bitboards::pretty(PawnAttackSpan[c][s]).c_str());
	}
	}
	*/
	/*
	printf("PassedPawnMask[color][sq]\n");
	for (Color c = WHITE; c <= BLACK; ++c){
	for(Square s = SQ_A1; s <= SQ_H8; ++s){
	printf("color=%d sq=%d \n %s",c,s,Bitboards::pretty(PassedPawnMask[c][s]).c_str());
	}
	}
	*/
	/*
	printf("FileABB\n");
	printf(" %s", Bitboards::pretty(FileABB).c_str());
	printf("FileHBB\n");
	printf(" %s", Bitboards::pretty(FileHBB).c_str());
	*/
	/*
	printf("SquareBB\n");
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A1]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A2]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A3]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A4]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A5]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A6]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A7]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_A8]).c_str());

	printf(" %s", Bitboards::pretty(SquareBB[SQ_B1]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B2]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B3]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B4]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B5]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B6]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B7]).c_str());
	printf(" %s", Bitboards::pretty(SquareBB[SQ_B8]).c_str());
	*/
	/*
	printf("RankBB\n");
	printf(" %s", Bitboards::pretty(RankBB[RANK_1]).c_str());
	printf(" %s", Bitboards::pretty(RankBB[RANK_8]).c_str());
	printf("FILEBB\n");
	printf(" %s", Bitboards::pretty(FileBB[FILE_A]).c_str());
	printf(" %s", Bitboards::pretty(FileBB[FILE_H]).c_str());

	printf("AdjacentFilesBB\n");
	printf(" %s", Bitboards::pretty(AdjacentFilesBB[FILE_A]).c_str());
	printf(" %s", Bitboards::pretty(AdjacentFilesBB[FILE_B]).c_str());

	printf("InFrontBB\n");
	printf(" %s", Bitboards::pretty(InFrontBB[WHITE][RANK_2]).c_str());
	printf(" %s", Bitboards::pretty(InFrontBB[WHITE][RANK_7]).c_str());
	printf(" %s", Bitboards::pretty(InFrontBB[BLACK][RANK_2]).c_str());
	printf(" %s", Bitboards::pretty(InFrontBB[BLACK][RANK_7]).c_str());
	*/

	//attacks_from関数の機能を詳細に調べる
	const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	/*
	attacks_from関数の調査
	このようなbitboardをかえす
	つまり指定した駒コードが指定した座標にいる場合の利きのbitboardを返す
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   | S |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   | X |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	*/
	/*
	Position pos(StartFEN, false, Threads.main()); // The root position
	Square sq = SQ_D6;
	Bitboard bb = pos.attacks_from(B_BISHOP, sq);
	printf(" %s", Bitboards::pretty(bb).c_str());
	*/
	/*
	attackers_to関数の調査
	*/
	/*
	Position pos_to("r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13", false, Threads.main()); // The root position
	bb = pos_to.attackers_to(SQ_D6);
	printf(" %s", Bitboards::pretty(bb).c_str());
	*/
	/*
	shift_bb関数の調査
	*/
	/*
	Bitboard bb = Rank2BB;
	printf(" %s", Bitboards::pretty(Rank2BB).c_str());
	printf(" %s", Bitboards::pretty(shift_bb<DELTA_NE>(bb)).c_str());
	*/
	RKISS rk;
	/*
	オプション機能の確認
	*/
	/*
	int cf = Options["Contempt Factor"] * PawnValueEg / 100; // From centipawns

	stringstream ss;

	ss << Options["Threads"];
	*/
	/*
	型変換演算子
	*/
	/*
	S s;
	int v = int(s);			//明示的な型変換
	printf("%d\n", v);
	char* str = (char*)s;	//明示的な型変換
	printf("%s\n", str);
	double f = s;		//暗黙の型変換
	printf("%f\n", f);
	*/
	/*
	basic_string
	*/
	/*
	int a = stoi("314");
	cout << a << endl;
	size_t idx;
	int b = stoi("100", &idx);
	cout << b << endl;
	cout << idx << endl;

	string s1 = to_string(27154);
	cout << s1 << endl;
	*/
	/*
	string s2 = "123,456,789";

	vector<string> result = split(move(s2), regex(","));
	*/
	/*
	max_elementの機能の確認
	valueの要素でMAXを決めている？
	codeではless<>()をデフォルトで割りつけているだけのように見える
	でも心配なので比較関数も利用できるバージョンも考えた
	追記
	大丈夫比較演算子のオーバーライドがちゃんと定義してある
	*/
	/*
	ExtMove m[32];
	for (int i= 0; i < 32; i++){
	m[i].move = Move(i);
	m[i].value = Value(rand());
	}
	ExtMove *begin = m;
	ExtMove *end = &m[31];

	for (int i = 0; i < 32; i++){
	cout << m[i].move << "  " << m[i].value << endl;
	}
	cout << "max element " << max_element(begin,end)->move  /*<< "    " << max_element(begin, end)->value << endl;
	cout << "end " << end->move << " " << end->value << endl;
	*/
	/*
	move_to_sanの解明
	*/
	/*
	Position pos(StartFEN, false, Threads.main()); // The root position
	Move m = move_from_uci(pos, string("a2a3"));
	cout << "move_to_san: " << move_to_san(pos, m) << endl;
	*/
	/*
	cout << "msb :" << endl;
	cout << "msb(0x00000)" << msb(0x00000) << endl;
	cout << "msb(0x00001)" << msb(0x00001) << endl;
	cout << "msb(0x00010)" << msb(0x00010) << endl;
	cout << "msb(0x00100)" << msb(0x00100) << endl;
	cout << "msb(0x01000)" << msb(0x01000) << endl;
	cout << "msb(0x10000)" << msb(0x10000) << endl;
	*/
	cout << ~(64 - 1) << endl;
	/*
	bitboard.cpp のpop_lsb関数を調べていたらそのなかで使われているbsf_index関数と
	BSFTable配列がわからない、bsf_index関数のなかは32bit版と64bit版があるようで
	DeBruijn_64とDeBruijn_32とマジックナンバーのような変数が設定されていてよくわからないので
	とりあえずpop_lsb関数にいろいろとbitboardをいれてどんな反応をするか調べる
	*/
	/*
	FileBBB=
	0100 0000
	0100 0000
	0100 0000
	0100 0000
	0100 0000
	0100 0000
	0100 0000
	0100 0000
	*/
	/*
	pop_lsbはbitboardの下位bitから１bitがあるindexを返す
	indexは0から始まる
	*/
	Bitboard bb = FileBBB;
	Square sq = pop_lsb(&bb);
	cout << "pop_lsb(FileABB)=" << sq << endl;		//sq = 1
	print(bb);
	/*
	Rank2BB=
	0000 0000	下位bit
	1111 1111
	0000 0000
	0000 0000
	0000 0000
	0000 0000
	0000 0000
	0000 0000	上位bit
	*/	
	bb = Rank2BB;
	sq = pop_lsb(&bb);
	cout << "pop_lsb(Rank2BB)=" << sq << endl;		//sq = 8
	print(bb);
	//printBSFTable();
	/*
	RMasksの初期化
	*/
	printf("RMask\n");
	Bitboards::print(RMasks[SQ_A1]);
	Bitboards::print(RMasks[SQ_B2]);
	Bitboards::print(RMasks[SQ_C3]);
	Bitboards::print(RMasks[SQ_D4]);
	Bitboards::print(RMasks[SQ_E5]);
	Bitboards::print(RMasks[SQ_F6]);
	Bitboards::print(RMasks[SQ_G7]);
	Bitboards::print(RMasks[SQ_H8]);
	/*
	BMasksの初期化
	*/
	printf("BMasks\n");
	Bitboards::print(BMasks[SQ_A1]);
	Bitboards::print(BMasks[SQ_B2]);
	Bitboards::print(BMasks[SQ_C3]);
	Bitboards::print(BMasks[SQ_D4]);
	Bitboards::print(BMasks[SQ_E5]);
	Bitboards::print(BMasks[SQ_F6]);
	Bitboards::print(BMasks[SQ_G7]);
	Bitboards::print(BMasks[SQ_H8]);
	/*
	RShiftsの初期化
	*/
	printf("RShifts\n");
	printf("%d\n", RShifts[SQ_A1]);
	printf("%d\n", RShifts[SQ_B1]);
	printf("%d\n", RShifts[SQ_C1]);
	printf("%d\n", RShifts[SQ_D1]);
	printf("%d\n", RShifts[SQ_E1]);
	printf("%d\n", RShifts[SQ_F1]);
	printf("%d\n", RShifts[SQ_G1]);
	printf("%d\n", RShifts[SQ_H1]);

	printf("%d\n", RShifts[SQ_A2]);
	printf("%d\n", RShifts[SQ_B2]);
	printf("%d\n", RShifts[SQ_C2]);
	printf("%d\n", RShifts[SQ_D2]);
	printf("%d\n", RShifts[SQ_E2]);
	printf("%d\n", RShifts[SQ_F2]);
	printf("%d\n", RShifts[SQ_G2]);
	printf("%d\n", RShifts[SQ_H2]);

	printf("%d\n", RShifts[SQ_A3]);
	printf("%d\n", RShifts[SQ_B3]);
	printf("%d\n", RShifts[SQ_C3]);
	printf("%d\n", RShifts[SQ_D3]);
	printf("%d\n", RShifts[SQ_E3]);
	printf("%d\n", RShifts[SQ_F3]);
	printf("%d\n", RShifts[SQ_G3]);
	printf("%d\n", RShifts[SQ_H3]);

	printf("%d\n", RShifts[SQ_A4]);
	printf("%d\n", RShifts[SQ_B4]);
	printf("%d\n", RShifts[SQ_C4]);
	printf("%d\n", RShifts[SQ_D4]);
	printf("%d\n", RShifts[SQ_E4]);
	printf("%d\n", RShifts[SQ_F4]);
	printf("%d\n", RShifts[SQ_G4]);
	printf("%d\n", RShifts[SQ_H4]);

	printf("%d\n", RShifts[SQ_A5]);
	printf("%d\n", RShifts[SQ_B5]);
	printf("%d\n", RShifts[SQ_C5]);
	printf("%d\n", RShifts[SQ_D5]);
	printf("%d\n", RShifts[SQ_E5]);
	printf("%d\n", RShifts[SQ_F5]);
	printf("%d\n", RShifts[SQ_G5]);
	printf("%d\n", RShifts[SQ_H5]);

	printf("%d\n", RShifts[SQ_A6]);
	printf("%d\n", RShifts[SQ_B6]);
	printf("%d\n", RShifts[SQ_C6]);
	printf("%d\n", RShifts[SQ_D6]);
	printf("%d\n", RShifts[SQ_E6]);
	printf("%d\n", RShifts[SQ_F6]);
	printf("%d\n", RShifts[SQ_G6]);
	printf("%d\n", RShifts[SQ_H6]);

	printf("%d\n", RShifts[SQ_A7]);
	printf("%d\n", RShifts[SQ_B7]);
	printf("%d\n", RShifts[SQ_C7]);
	printf("%d\n", RShifts[SQ_D7]);
	printf("%d\n", RShifts[SQ_E7]);
	printf("%d\n", RShifts[SQ_F7]);
	printf("%d\n", RShifts[SQ_G7]);
	printf("%d\n", RShifts[SQ_H7]);

	printf("%d\n", RShifts[SQ_A8]);
	printf("%d\n", RShifts[SQ_B8]);
	printf("%d\n", RShifts[SQ_C8]);
	printf("%d\n", RShifts[SQ_D8]);
	printf("%d\n", RShifts[SQ_E8]);
	printf("%d\n", RShifts[SQ_F8]);
	printf("%d\n", RShifts[SQ_G8]);
	printf("%d\n", RShifts[SQ_H8]);
	/*
RShifts
RShiftsはbitboardをbitシフトするunsigned int型の変数として宣言
しているので
RShifts
A1	20
B1	21
C1	21
D1	21
E1	21
F1	21
G1	21
H1	20

A2	21
B2	22
C2	22
D2	22
E2	22
F2	22
G2	22
H2	21

A3	21
B3	22
C3	22
D3	22
E3	22
F3	22
G3	22
H3	21

A4	21
B4	22
C4	22
D4	22
E4	22
F4	22
G4	22
H4	21

A5	21
B5	22
C5	22
D5	22
E5	22
G5	22
F5	22
G5	21

A6	21
B6	22
C6	22
D6	22
E6	22
F6	22
G6	22
H6	21

A7	21
B7	22
C7	22
D7	22
E7	22
F7	22
G7	22
H7	21

A8	20
B8	21
C8	21
D8	21
E8	21
F8	21
G8	21
H8	20
*/
	/*
	BShiftsの初期化
	*/
	printf("BShifts\n");
	printf("%d\n", BShifts[SQ_A1]);
	printf("%d\n", BShifts[SQ_B1]);
	printf("%d\n", BShifts[SQ_C1]);
	printf("%d\n", BShifts[SQ_D1]);
	printf("%d\n", BShifts[SQ_E1]);
	printf("%d\n", BShifts[SQ_F1]);
	printf("%d\n", BShifts[SQ_G1]);
	printf("%d\n", BShifts[SQ_H1]);

	printf("%d\n", BShifts[SQ_A2]);
	printf("%d\n", BShifts[SQ_B2]);
	printf("%d\n", BShifts[SQ_C2]);
	printf("%d\n", BShifts[SQ_D2]);
	printf("%d\n", BShifts[SQ_E2]);
	printf("%d\n", BShifts[SQ_F2]);
	printf("%d\n", BShifts[SQ_G2]);
	printf("%d\n", BShifts[SQ_H2]);

	printf("%d\n", BShifts[SQ_A3]);
	printf("%d\n", BShifts[SQ_B3]);
	printf("%d\n", BShifts[SQ_C3]);
	printf("%d\n", BShifts[SQ_D3]);
	printf("%d\n", BShifts[SQ_E3]);
	printf("%d\n", BShifts[SQ_F3]);
	printf("%d\n", BShifts[SQ_G3]);
	printf("%d\n", BShifts[SQ_H3]);

	printf("%d\n", BShifts[SQ_A4]);
	printf("%d\n", BShifts[SQ_B4]);
	printf("%d\n", BShifts[SQ_C4]);
	printf("%d\n", BShifts[SQ_D4]);
	printf("%d\n", BShifts[SQ_E4]);
	printf("%d\n", BShifts[SQ_F4]);
	printf("%d\n", BShifts[SQ_G4]);
	printf("%d\n", BShifts[SQ_H4]);

	printf("%d\n", BShifts[SQ_A5]);
	printf("%d\n", BShifts[SQ_B5]);
	printf("%d\n", BShifts[SQ_C5]);
	printf("%d\n", BShifts[SQ_D5]);
	printf("%d\n", BShifts[SQ_E5]);
	printf("%d\n", BShifts[SQ_F5]);
	printf("%d\n", BShifts[SQ_G5]);
	printf("%d\n", BShifts[SQ_H5]);

	printf("%d\n", BShifts[SQ_A6]);
	printf("%d\n", BShifts[SQ_B6]);
	printf("%d\n", BShifts[SQ_C6]);
	printf("%d\n", BShifts[SQ_D6]);
	printf("%d\n", BShifts[SQ_E6]);
	printf("%d\n", BShifts[SQ_F6]);
	printf("%d\n", BShifts[SQ_G6]);
	printf("%d\n", BShifts[SQ_H6]);

	printf("%d\n", BShifts[SQ_A7]);
	printf("%d\n", BShifts[SQ_B7]);
	printf("%d\n", BShifts[SQ_C7]);
	printf("%d\n", BShifts[SQ_D7]);
	printf("%d\n", BShifts[SQ_E7]);
	printf("%d\n", BShifts[SQ_F7]);
	printf("%d\n", BShifts[SQ_G7]);
	printf("%d\n", BShifts[SQ_H7]);

	printf("%d\n", BShifts[SQ_A8]);
	printf("%d\n", BShifts[SQ_B8]);
	printf("%d\n", BShifts[SQ_C8]);
	printf("%d\n", BShifts[SQ_D8]);
	printf("%d\n", BShifts[SQ_E8]);
	printf("%d\n", BShifts[SQ_F8]);
	printf("%d\n", BShifts[SQ_G8]);
	printf("%d\n", BShifts[SQ_H8]);
	/*
	BShifts

	A1	26
	B1	27
	C1	27
	D1	27
	E1	27
	F1	27
	G1	27
	H1	26

	A2	27
	B2	27
	C2	27
	D2	27
	E2	27
	F2	27
	G2	27
	H2	27

	A3	27
	B3	27
	C3	25
	D3	25
	E3	25
	F3	25
	G3	27
	H3	27

	A4	27
	B4	27
	C4	25
	D4	23
	E4	23
	F4	25
	G4	27
	H4	27

	A5	27
	B5	27
	C5	25
	D5	23
	E5	23
	F5	25
	G5	27
	H5	27

	A6	27
	B6	27
	C6	25
	D6	25
	E6	25
	F6	25
	G6	27
	H6	27

	A7	27
	B7	27
	C7	27
	D7	27
	E7	27
	F7	27
	G7	27
	H7	27

	A8	26
	B8	27
	C8	27
	D8	27
	E8	27
	F8	27
	G8	27
	H8	26

	*/
	printf("DistanceRingsBB\n");
	Bitboards::print(DistanceRingsBB[SQ_D3][0]);
	Bitboards::print(DistanceRingsBB[SQ_D3][1]);
	Bitboards::print(DistanceRingsBB[SQ_D3][2]);
	Bitboards::print(DistanceRingsBB[SQ_D3][3]);
	Bitboards::print(DistanceRingsBB[SQ_D3][4]);
	Bitboards::print(DistanceRingsBB[SQ_D3][5]);
	Bitboards::print(DistanceRingsBB[SQ_D3][6]);
	Bitboards::print(DistanceRingsBB[SQ_D3][7]);
	/*
	DistanceRingsBB[sq][0-3]
	２次元目の添え字は８まで宣言されているが４までしかいらない
	これはチェビシェフ距離を表した距離
	s座標にいるのがQUEENであればC2に移動するのもD2に移動するにも１手で（距離を手数で表している）
	行ける。
	http://ja.wikipedia.org/wiki/%E3%83%81%E3%82%A7%E3%83%93%E3%82%B7%E3%82%A7%E3%83%95%E8%B7%9D%E9%9B%A2
	DistanceRingsBB[D3][0]
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | S | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   | X | X | X |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	DistanceRingsBB[D3][1]
	+---+---+---+---+---+---+---+---+
	|   | X | X | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   | S |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X |   |   |   | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   | X | X | X | X | X |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	DistanceRingsBB[D3][2]
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	| X |   |   | S |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	| X |   |   |   |   |   | X |   |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	DistanceRingsBB[D3][3]
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   | S |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   | X |
	+---+---+---+---+---+---+---+---+
	| X | X | X | X | X | X | X | X |
	+---+---+---+---+---+---+---+---+
	|   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+
	*/
	printf("SquareDistance\n");
	for (int row = 0; row < 8; row++){
		for (int col = 0; col < 8; col++){
			printf("%d", SquareDistance[SQ_A1][row * 8 + col]);
		}
		printf("\n");
	}
	for (int row = 0; row < 8; row++){
		for (int col = 0; col < 8; col++){
			printf("%d", SquareDistance[SQ_D2][row * 8 + col]);
		}
		printf("\n");
	}
	printf("Color of square\n");
	Bitboards::print(0xAA55AA55AA55AA55);
	/*msb*/
	printf("msb\n");
	bb = 0x44;
	//2進数で->100 0100 16進数で0x44
	printf("%d\n", msb(bb));		//出力は6
	/*lsb*/
	printf("lsb\n");
	printf("%d\n", lsb(bb));		//出力は2
	/*pop_lsb*/
	printf("pop_lsb\n");
	Bitboards::print(bb);
	printf("%d\n", pop_lsb(&bb));		//出力は2
	Bitboards::print(bb);
	return;
}

#include <cstring>

template<LLType LT> void print(void)
{
	switch (LT){
	case python:
		printf("python\n");
		break;
	case ruby:
		printf("ruby\n");
		break;
	case perl:
		printf("perl\n");
		break;
	}
}

