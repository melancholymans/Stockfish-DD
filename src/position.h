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

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <cassert>
#include <cstddef>

#include "bitboard.h"
#include "types.h"


/// The checkInfo struct is initialized at c'tor time and keeps info used
/// to detect if a move gives check.
class Position;
struct Thread;
/*
check�ɖ𗧂�bitboard��Ԃ�
pinned��pin�����ꂽ��bitboard��Ԃ�
dcCandidates�͓GKING�ւ̗������ז����Ă��鎩�w��bitboard��Ԃ�
checkSq[]�͓GKING�֗����𗘂����Ă��鎩�w��bitboard��Ԃ�
*/
struct CheckInfo {

  explicit CheckInfo(const Position&);

  Bitboard dcCandidates;						//�GKING��pin�t�����ꂽ���w��bitboard �܂�GKING�ւ̗������ז����Ă��鎩�w��̂���
  Bitboard pinned;									//���wKING��pin�����ꂽ���w��bitboard
  Bitboard checkSq[PIECE_TYPE_NB];
  Square ksq;												//�GKING�̍��W
};


/// The StateInfo struct stores information needed to restore a Position
/// object to its previous state when we retract a move. Whenever a move
/// is made on the board (by calling Position::do_move), a StateInfo
/// object must be passed as a parameter.
/*
position�͋ǖʂ�ێ�����N���X�ŁAdo_move�ɂ����
�ύX���������邪�i�ǖʍX�V�j���Ƃ̋ǖʂɖ߂�����
�g�p�����������Ă����\����
*/
struct StateInfo {
	/*
	PAWN�����̋ǖʏ��Ɋ�Â����n�b�V���l��ۊǂ���ϐ�
	*/
	Key pawnKey;
	/*
	�J���[�A��킲�Ƃ̋�ł��܂����j�[�N�Ȑ��i�n�b�V���l�Ƃ��Ďg���j
	*/
	Key	materialKey;
	/*
	npMaterial=non-pawn-material�@PAWN�����Ȃ����̋�̕]���l�̏W�v
	*/
  Value npMaterial[COLOR_NB];
	/*
	castleRights�F�L���X�����O
	rule50�F50�胋�[�����݂�����Ƃ�Ȃ��ȂǏ�����������������������ɂ��郋�[��
					�r���ŋ���������L�����Z������Ȃ�do_move�֐��Ȃ��ŏ������Ă���
	http://ja.wikipedia.org/wiki/50%E6%89%8B%E3%83%AB%E3%83%BC%E3%83%AB
	pliesFromNull�Fdo_move�֐��ŃJ�E���g�A�b�v�Ado_null_move�Ń[���N���A
	is_draw�֐��Ńh���[���肷��Ƃ�rule50��pliesFromNull���r���ď������ق��Ŕ��肵�Ă���
	*/
  int castleRights, rule50, pliesFromNull;
	/*
	�ʒu�]���l�̏W�v�l
	*/
  Score psq;
	/*
	�A���p�b�T��
	*/
  Square epSquare;
	/*
	���̋ǖʂ̃n�b�V���l
	*/
  Key key;
	/*
	��ԑ���KING�֗����𗘂����Ă���G�̋��bitboard
	*/
  Bitboard checkersBB;
	/*
	���̋ǖʂŎ�������ido_move�֐��ōX�V�j
	*/
  PieceType capturedType;
	/*
	�ЂƂ�̃��x���ւ̃����N
	*/
  StateInfo* previous;
};


/// When making a move the current StateInfo up to 'key' excluded is copied to
/// the new one. Here we calculate the quad words (64bits) needed to be copied.
/*
offsetof�̋@�\
�\���̂̃����o�̃o�C�g�ʒu������������Ԃ�
�p�r�s��
*/
const size_t StateCopySize64 = offsetof(StateInfo, key) / sizeof(uint64_t) + 1;


/// The Position class stores the information regarding the board representation
/// like pieces, side to move, hash keys, castling info, etc. The most important
/// methods are do_move() and undo_move(), used by the search to update node info
/// when traversing the search tree.
/*
�ǖʂ�\������N���X
*/
class Position {
public:
  Position() {}
	/*
	���̋ǖʂ��R�s�[���Đ�������R���X�g���N�^
	*/
  Position(const Position& p, Thread* t) { *this = p; thisThread = t; }
	/*
	fen�������琶���R���X�g���N�^,c960�͕ό`���[����K�p����ǂ�����flag
	*/
  Position(const std::string& f, bool c960, Thread* t) { set(f, c960, t); }
	/*������Z�q�I�[�o�[���[�h*/
  Position& operator=(const Position&);
	/*
	�������APosition�S�̂̏������ŁA�Ֆʂ̏������͂��Ƃ�set�֐��ōs��
	main�֐������x�����Ă΂��
	*/
  static void init();

  // Text input/output
	/*
	fenStr�͋ǖʂ𕶎���ŕ\����������
	���၄
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	R--rook,N--night,B--bishop,Q--queen,K--king,P--pawn,�啶����white�i���j
	r--rook,n--night,b--bishop,q--queen,k--king,p--pawn,��������black�i���j
	�����͋󔒂̐�,/�͍s�̏I��� �ǖʂ�\�����镶����̂��Ƌ󔒂����Ă��̋ǖʂŎ���
	�w���J���[��w/b�ŕ\���A���̎���KQkq�͕s�� -���s���@0 1���s��
	�i�ǋL�j
	KQkq�̓L���X�����O�Ɋ֌W����Ȃɂ�
	*/
	/*
	set�֐���Position�N���X�̃R���X�g���N�^����Ă΂�
	fenStr����͂��ē����f�[�^���X�V���Ă���B
	�X�V���������f�[�^��
	board[s]
	byTypeBB[ALL_PIECES]
	byColorBB[c]
	index[s]
	pieceList[c][pt][index[s]]
	���ɂ����낢��ǖʕێ��A�ǖʍX�V�ɕK�v�Ȃ��̂����������Ă��邪
	�ڍוs��
	*/
	void set(const std::string& fen, bool isChess960, Thread* th);
	/*
	���݂̋ǖʂ�fenStr������ɕϊ�����
	*/
	const std::string fen() const;
	/*
	�w������ƌ��݂̋ǖʂ𕶎���ɂ��ĕԂ�
	*/
	const std::string pretty(Move m = MOVE_NONE) const;
  // Position representation
	/*
	�S�Ă̋�̃r�b�g��������bitboard��Ԃ�
	�֐��̐錾�̂��Ƃɂ��Ă���const�͂���
	position�N���X�̃����o�[�ϐ��͕ύX�ł��Ȃ�
	���Ƃ������Ă���B�ibitboard��ǂ݂����ĕԂ�
	�֐��Ȃ̂ŕύX���邱�Ƃ��Ȃ��j
	*/
	Bitboard pieces() const;
	/*
	�w�肵����킪������bitboard��Ԃ�
	*/
	Bitboard pieces(PieceType pt) const;
	/*
	pt1,pt2�Ŏw�肵����킪������bitboard��Ԃ�
	*/
	Bitboard pieces(PieceType pt1, PieceType pt2) const;
	/*
	�w�肵���J���[�̋�����Ă���bitboard��Ԃ�
	*/
	Bitboard pieces(Color c) const;
	/*
	�w�肵�����A�w�肵���J���[�̋�����Ă���bitboard��Ԃ�
	*/
	Bitboard pieces(Color c, PieceType pt) const;
	/*
	�w�肵�����pt1,pt2�A�w�肵���J���[�̋�����Ă���bitboard��Ԃ�
	*/
	Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
	/*
	�����o�[�ϐ�board[64]�̍��W���w�肵�ċ�R�[�h��Ԃ�
	*/
	Piece piece_on(Square s) const;
	/*
	�J���[���w�肵��pieceList[COLOR][PIECE_TYPE][16]�ϐ�����king�̍��W��Ԃ�
	*/
	Square king_square(Color c) const;
	/*
	�A���p�b�T���̍��W��Ԃ�
	*/
	Square ep_square() const;
	/*
	�w�肵�����W�ɋ�Ȃ�������true��Ԃ�
	*/
	bool empty(Square s) const;
	/*
	�J���[�Ƌ����w�肷���pieceCount[COLOR][PIECE_TYPE]��
	�Ԃ��BpieceCount�z��̓J���[���Ƌ�킱�Ƃ̋���L�^���Ă���z��
	�Ƃ����template<PieceType Pt>�Ƃ͂Ȃ񂾂낤
	�����炭��킲�Ƃ�count�֐����e���v���[�g�����Ă���̂ł�
	���ꂾ�������ɓ��삷��H
	*/
	template<PieceType Pt> int count(Color c) const;
	/*
	�J���[�Ƌ����w�肷���pieceList[COLOR][PIECE_TYPE]��
	���W���X�g�i�z��j��Ԃ��Ă���
	���̊֐�����킲�ƂɃe���v���[�g������Ă���H
	*/
	template<PieceType Pt> const Square* list(Color c) const;

  // Castling
	/*
	�p�r�s��
	�����A�L���X�e�C���O�̂��Ƃ��Ǝv��
	*/
	int can_castle(CastleRight f) const;
	/*
	�p�r�s��
	�����A�L���X�e�C���O�̂��Ƃ��Ǝv��
	*/
	int can_castle(Color c) const;
	/*
	�p�r�s��
	�����A�L���X�e�C���O�̂��Ƃ��Ǝv��
	*/
	bool castle_impeded(Color c, CastlingSide s) const;
	/*
	�p�r�s��
	�����A�L���X�e�C���O�̂��Ƃ��Ǝv��
	*/
	Square castle_rook_square(Color c, CastlingSide s) const;

  // Checking
	/*
	StateInfo�̃����o�[checkersBB��Ԃ��Ă���
	��ԑ���KING�ɉ���������Ă���G����bitboard��Ԃ�
	*/
	Bitboard checkers() const;
	/*
	Color�Ɏ��w�J���[ kingColor�ɓG�w�J���[���w�肷���
	�GKING�ɓB�t�����ꂽ��i���w���̋�A�G�w�̋�ł͂Ȃ��j��Ԃ�
	�A��pin���ꂽ��͂P�����łQ���܂��Ă����pin�Ƃ͔��f����Ȃ�
	�܂�G��KING�ւ̗������ז����Ă��鎩�w���̋��Ԃ�	
	*/
	Bitboard discovered_check_candidates() const;
	/*
	�p�r�s��
	�����Aking�ւ̃`�G�b�N�Ɋւ���Ȃɂ�
	*/
	Bitboard pinned_pieces(Color toMove) const;

  // Attacks to/from a given square
	/*
	�����v�Z���Ƃ�����
	*/
	Bitboard attackers_to(Square s) const;
  Bitboard attackers_to(Square s, Bitboard occ) const;
  Bitboard attacks_from(Piece p, Square s) const;
  static Bitboard attacks_from(Piece p, Square s, Bitboard occ);
  template<PieceType> Bitboard attacks_from(Square s) const;
  template<PieceType> Bitboard attacks_from(Square s, Color c) const;

  // Properties of moves
	/*
	�n���ꂽ�肪���@�肩�`�G�b�N����
	*/
	bool legal(Move m, Bitboard pinned) const;
	/*
	�p�r�s��
	*/
	bool pseudo_legal(const Move m) const;
	/*
	���肪�������Ȃ�true��Ԃ�
	*/
	bool capture(Move m) const;
	/*
	�p�r�s��
	����̎�ʂ��`�G�b�N���Ă��邪
	�֐������画�f����ɋ���Ƃ��
	�������͐����𔻒肵�Ă���悤����
	�H
	*/
	bool capture_or_promotion(Move m) const;
	/*
	�p�r�s��
	���@��̔���H
	*/
	bool gives_check(Move m, const CheckInfo& ci) const;
	/*
	����f�[�^�̋�킪PAWN�ł��s��Rank_4�ȏゾ������true��Ԃ�
	�p�r�s��
	*/
	bool passed_pawn_push(Move m) const;
	/*
	����f�[�^�����R�[�h���擾����
	*/
	Piece moved_piece(Move m) const;
	/*
	������������Ă���
	*/
	PieceType captured_piece_type() const;

  // Piece specific
	/*
	�p�r�s��
	*/
	bool pawn_passed(Color c, Square s) const;
	/*
	PAWN������w�n�̍ŉ��i�ɂ��邩�`�G�b�N�A������
	true��Ԃ��APAWN�����邽�߂̃`�G�b�N�Ɏg�p���Ă���̂����H
	*/
	bool pawn_on_7th(Color c) const;
	/*
	�p�r�s��
	�����J���[��bishop���Q�ȏ゠���āi�܂�Ƃ��Ă��Ȃ��j
	board�̃J���[�i�s���͗l�̐F�j���قȂ��Ă����true
	�����������m��bishop�݂͌��ɈقȂ�board�J���[�ɔz�u�����̂�
	true���Ԃ��Ă���͓̂�����O�̂悤�ȋC�����邪�Achess960�ł�
	�Ⴄ�̂����H
	*/
	bool bishop_pair(Color c) const;
	/*
	WHITE�ABLACK����bishop���݂��Ⴂ��board�J���[�Ɉʒu�����
	true��Ԃ�
	�܂�Ⴄboard�J���[�̏ꍇ���݂��Ɏ�荇�����Ƃ͂Ȃ���
	���f�ł���
	*/
	bool opposite_bishops() const;

  // Doing and undoing moves
	/*
	�ǖʍX�V
	�������炢�낢�������ǉ����ĉ���
	do_move���Ă�ł��郉�b�p�[
	*/
	void do_move(Move m, StateInfo& st);
	/*
	�ǖʍX�V
	*/
	void do_move(Move m, StateInfo& st, const CheckInfo& ci, bool moveIsCheck);
	/*
	�ǖʕ���
	*/
	void undo_move(Move m);
	/*
	null move�𓮂���
	*/
	void do_null_move(StateInfo& st);
	/*
	do_null_move�̕���
	*/
	void undo_null_move();

  // Static exchange evaluation
	/*
	SEE�̖��O���炵�ĐÎ~�T���H
	*/
	int see(Move m, int asymmThreshold = 0) const;
	/*
	�p�r�s��
	*/
	int see_sign(Move m) const;

  // Accessing hash keys
	/*
	�p�r�s��
	StateInfo�̃����o�[key��Ԃ�����
	*/
	Key key() const;
	/*
	�p�r�s��
	*/
	Key exclusion_key() const;
	/*
	�p�r�s��
	StateInfo�̃����o�[pawnKey��Ԃ�����
	*/
	Key pawn_key() const;
	/*
	�p�r�s��
	StateInfo�̃����o�[materialKey��Ԃ�����
	*/
	Key material_key() const;

  // Incremental piece-square evaluation
	/*
	�p�r�s��
	StateInfo�̃����o�[psq��Ԃ�
	*/
	Score psq_score() const;
	/*
	�p�r�s��
	StateInfo�̃����o�[npMaterial
	*/
	Value non_pawn_material(Color c) const;

  // Other properties of the position
	/*
	�����o�[�ϐ�sideToMove��Ԃ�
	*/
	Color side_to_move() const;
	/*
	�Q�[���̉���ڂ���Ԃ�
	*/
	int game_ply() const;
	/*
	�`�F�X960�iChess 960�j�́A�ϑ��`�F�X�̈��
	chess960���ǂ�����Ԃ�
	*/
	bool is_chess960() const;
	/*
	�p�r�s��
	�����o�[�ϐ�thisThread��Ԃ�
	*/
	Thread* this_thread() const;
	/*
	�T���؂̃m�[�h����Ԃ�
	*/
	int64_t nodes_searched() const;
	/*
	search�֐��ŒT���؂𕪊����ĒT�������ꍇ���ꂼ��̕����؂ł̃m�[�h����
	���v����Ƃ��ɌĂ΂��
	*/
	void set_nodes_searched(int64_t n);
	/*
	�p�r�s��
	���������̔�������Ă���H
	*/
	bool is_draw() const;

  // Position consistency check, for debugging
	/*
	�ǖʂ̕s�����Ȃǂ��`�G�b�N���Ă���
	�ڍוs���ȂƂ��������
	*/
	bool pos_is_ok(int* failedStep = nullptr) const;
	/*
	�J���[���t�]����fen������������position�N���X������������
	*/
	void flip();

private:
  // Initialization helpers (used while setting up a position)
	/*
	position�N���X���N���A�ɂ���
	�ꕔ�p�r�s������
	*/
	void clear();
	/*
	�p�r�s��
	*/
	void set_castle_right(Color c, Square rfrom);

  // Helper functions
  void do_castle(Square kfrom, Square kto, Square rfrom, Square rto);
  Bitboard hidden_checkers(Square ksq, Color c, Color toMove) const;
  void put_piece(Square s, Color c, PieceType pt);
  void remove_piece(Square s, Color c, PieceType pt);
  void move_piece(Square from, Square to, Color c, PieceType pt);

  // Computing hash keys from scratch (for initialization and debugging)
  Key compute_key() const;
  Key compute_pawn_key() const;
  Key compute_material_key() const;

  // Computing incremental evaluation scores and material counts
  Score compute_psq_score() const;
  Value compute_non_pawn_material(Color c) const;

  // Board and pieces
	/*
	board�֌W�̎�v�ϐ�
	*/
	//Piece�^�̔z��A�U�S����
	Piece board[SQUARE_NB];
	//��킲�Ƃ�bitboard�A���͂U��ނ����Ȃ����W�܂ŗv�f������
	Bitboard byTypeBB[PIECE_TYPE_NB];
	//�J���[���Ƃ�bitboard
	Bitboard byColorBB[COLOR_NB];
	//�J���[���ƁA��킲�Ƃ̋���L�����Ă����z��
	int pieceCount[COLOR_NB][PIECE_TYPE_NB];
	//�J���[���ƁA��킲�ƁA�ǂ̍��W�ɂ���̂��L�����Ă����z��
	Square pieceList[COLOR_NB][PIECE_TYPE_NB][16];
	//�p�r�s��
	int index[SQUARE_NB];

  // Other info
	/*
	�L���X�����O�֌W�̕ϐ�����
	�p�r�s��
	*/
	int castleRightsMask[SQUARE_NB];
  Square castleRookSquare[COLOR_NB][CASTLING_SIDE_NB];
  Bitboard castlePath[COLOR_NB][CASTLING_SIDE_NB];
	/*
	�J�n�ǖʂ̂��߂�StateInfo�ϐ��APosition::clear�֐�����
	StateInfo* m_st����|�C���^�[�����
	*/
  StateInfo startState;
	/*
	do_move���Ăяo������
	�܂�search�ł̃m�[�h��
	*/
	int64_t nodes;
	/*
	do_move�֐���++����undo_move��--�����̂ŒT���ł͂Ȃ��Q�[���̎萔
	*/
	int gamePly;
	/*
	���
	*/
	Color sideToMove;
	/*
	�܂��AThread�֌W�ɂ͎肪�łȂ��̂ŕۗ�
	*/
	Thread* thisThread;
	/*
	�ǖʕ����̂��߂ɕK�v�ȏ��(StateInfo�j��ێ����邽�߂̃|�C���^�[
	StateInfo�͊e���x���̎����ϐ� StateInfo st;��do_move�֐����ŏ���ݒ肵�Ă���
	m_st�͏�ɍŐ[����StateInfo���w���Ă���
	StateInfo��previous��H�邱�ƂłP��Ԃ̃��x���̎w������ɃA�N�Z�X�ł���
	*/
	StateInfo* m_st;	//���܂�ɂ��������O�����Ă��ĕ��炵���̂ŕύX2015/3 st->m_st
	/*
	�ϑ�chess960���ǂ����̃t���O
	*/
	int chess960;
};
/*
search�̃m�[�h����Ԃ�
*/
inline int64_t Position::nodes_searched() const {
  return nodes;
}
/*
search�֐��ŒT���؂𕪊����ĒT�������ꍇ���ꂼ��̕����؂ł̃m�[�h����
���v����Ƃ��ɌĂ΂��
*/
inline void Position::set_nodes_searched(int64_t n) {
  nodes = n;
}
/*
board�z��̎w����W�ɂ����R�[�h��Ԃ�
*/
inline Piece Position::piece_on(Square s) const {
  return board[s];
}
/*
����f�[�^�����R�[�h���擾����
���ł͂Ȃ�
*/
inline Piece Position::moved_piece(Move m) const {
  return board[from_sq(m)];
}
/*
board[]�z��̎w�肵�����W�ɋ�Ȃ����true��Ԃ�
*/
inline bool Position::empty(Square s) const {
  return board[s] == NO_PIECE;
}
/*
��Ԃ�Ԃ�
*/
inline Color Position::side_to_move() const {
  return sideToMove;
}
/*
�S�Ă̋���bitboard��Ԃ�
*/
inline Bitboard Position::pieces() const {
  return byTypeBB[ALL_PIECES];
}
/*
��킲�Ƃ�bitboard��Ԃ�
�S�Ă̋���bitboard��Ԃ��ɂ�ALL_PIECES
*/
inline Bitboard Position::pieces(PieceType pt) const {
  return byTypeBB[pt];
}
/*
pt1,pt2����bitboard��Ԃ�
*/
inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
  return byTypeBB[pt1] | byTypeBB[pt2];
}
/*
�J���[���Ƃ�bitboard��Ԃ�
*/
inline Bitboard Position::pieces(Color c) const {
  return byColorBB[c];
}
/*
�J���[�Ƌ����w�肵��bitboard��Ԃ�
*/
inline Bitboard Position::pieces(Color c, PieceType pt) const {
  return byColorBB[c] & byTypeBB[pt];
}
/*
�w�肵���J���[�A���pt1,pt2��bitboard��Ԃ�
*/
inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
  return byColorBB[c] & (byTypeBB[pt1] | byTypeBB[pt2]);
}
/*
�w�肵���J���[�A���̐���Ԃ�
*/
template<PieceType Pt> inline int Position::count(Color c) const {
  return pieceCount[c][Pt];
}
/*
�w�肵���J���[�A���̍��W���X�g��Ԃ�
*/
template<PieceType Pt> inline const Square* Position::list(Color c) const {
  return pieceList[c][Pt];
}
/*
�p�r�s��
*/
inline Square Position::ep_square() const {
  return m_st->epSquare;
}
/*
�w�肵���J���[��KING�̍��W��Ԃ�
*/
inline Square Position::king_square(Color c) const {
  return pieceList[c][KING][0];
}
/*
�p�r�s��
*/
inline int Position::can_castle(CastleRight f) const {
  return m_st->castleRights & f;
}
/*
�p�r�s��
*/
inline int Position::can_castle(Color c) const {
  return m_st->castleRights & ((WHITE_OO | WHITE_OOO) << (2 * c));
}
/*
�p�r�s��
*/
inline bool Position::castle_impeded(Color c, CastlingSide s) const {
  return byTypeBB[ALL_PIECES] & castlePath[c][s];
}
/*
�p�r�s��
*/
inline Square Position::castle_rook_square(Color c, CastlingSide s) const {
  return castleRookSquare[c][s];
}
/*
�w�肵�����W�Ɏw�肵�����i�e���v���[�g�����Ŏw��j�̗���bitboard
�����Ă���bitboard��Ԃ��i�����w��ł���A�w�肵�Ă��Ȃ�������
���ы�̗�����Ԃ��j
*/
template<PieceType Pt>
inline Bitboard Position::attacks_from(Square s) const {

  return  Pt == BISHOP || Pt == ROOK ? attacks_bb<Pt>(s, pieces())
        : Pt == QUEEN  ? attacks_from<ROOK>(s) | attacks_from<BISHOP>(s)
        : StepAttacksBB[Pt][s];
}
/*
�w�肵�����W�ɗ����Ă�����bitboard��Ԃ�
�e���v���[�g��PAWN��p�ɂ��Ă���
template<>�ɂȂ��Ă���̂�PAWN���w�肵�Ă��邩��
*/
template<>
inline Bitboard Position::attacks_from<PAWN>(Square s, Color c) const {
  return StepAttacksBB[make_piece(c, PAWN)][s];
}
/*
�w�肵����w�肵�����W�ɂ���ꍇ�̗�����bitboard��Ԃ�
�܂�from�͎w�肵�����W����(from�j�����Ă���iattacks)�Ƃ����Ӗ�
���΂�attackers_to�͎w�肵�����Wto�ɗ��Ă���(to)�������o���Ă���
*/
inline Bitboard Position::attacks_from(Piece p, Square s) const {
  return attacks_from(p, s, byTypeBB[ALL_PIECES]);
}
/*
�w�肵�����W�ɗ����Ă�����bitboard��Ԃ��A�J���[�͖��֌W
*/
inline Bitboard Position::attackers_to(Square s) const {
  return attackers_to(s, byTypeBB[ALL_PIECES]);
}
/*
checkersBB�͎��w��KING�ɉ���Check���|���Ă�����bitboard
����bitboard��Ԃ�
checkersBB�͋ǖʃN���X���������ꂽ���Aset_state�֐��ōŏ��̏�����������
���̌��do_move�֐��ōX�V����
*/
inline Bitboard Position::checkers() const {
  return m_st->checkersBB;
}
/*
Color�Ɏ��w�J���[ kingColor�ɓG�w�J���[���w�肷���
�GKING�ɓB�t�����ꂽ��i���w���̋�A�G�w�̋�ł͂Ȃ��j��Ԃ�
�A��pin���ꂽ��͂P�����łQ���܂��Ă����pin�Ƃ͔��f����Ȃ�
�܂�G��KING�ւ̗������ז����Ă��鎩�w���̋��Ԃ�
*/
inline Bitboard Position::discovered_check_candidates() const {
  return hidden_checkers(king_square(~sideToMove), sideToMove, sideToMove);
}
/*
���w�T�C�h�itoMove�j��pin������Ă�����bitboard��Ԃ�
*/
inline Bitboard Position::pinned_pieces(Color toMove) const {
  return hidden_checkers(king_square(toMove), ~toMove, toMove);
}
/*
passed_pawn_mask�֐��́H
�w�肵�����W�ɂ���PAWN���ړ��\�Ȕ͈͂̒��ɓG����PAWN�Ƃ�AND
�Ȃ̂ł��ꂩ���邱�Ƃ��\��PAWN��bitboard��Ԃ�
*/
inline bool Position::pawn_passed(Color c, Square s) const {
  return !(pieces(~c, PAWN) & passed_pawn_mask(c, s));
}
/*
����f�[�^�̋�킪PAWN�ł��s��Rank_4�ȏゾ������true��Ԃ�
�p�r�s��
*/
inline bool Position::passed_pawn_push(Move m) const {

  return   type_of(moved_piece(m)) == PAWN
        && pawn_passed(sideToMove, to_sq(m));
}
/*
���ǖʂ̃n�b�V���l
*/
inline Key Position::key() const {
  return m_st->key;
}
/*
�ǖʏ��PAWN�������g���ē���ꂽ�n�b�V���l
*/
inline Key Position::pawn_key() const {
  return m_st->pawnKey;
}
/*
����ł̏��œ���ꂽ�n�b�V���l
key()�ƈ���Ď�ԁA�L���X�����O�A�A���p�b�T���̏��͓����Ă��Ȃ�
*/
inline Key Position::material_key() const {
  return m_st->materialKey;
}
/*
���ǖʂł̈ʒu�]���l�̏W�v�l
*/
inline Score Position::psq_score() const {
  return m_st->psq;
}
/*
PAWN����������]���l
*/
inline Value Position::non_pawn_material(Color c) const {
  return m_st->npMaterial[c];
}
/*
�Q�[���̉���ڂ���Ԃ�
*/
inline int Position::game_ply() const {
  return gamePly;
}
/*
WHITE�ABLACK����bishop���݂��Ⴂ��board�J���[�Ɉʒu�����
true��Ԃ�
�܂�Ⴄboard�J���[�̏ꍇ���݂��Ɏ�荇�����Ƃ͂Ȃ���
���f�ł���
*/
inline bool Position::opposite_bishops() const {

  return   pieceCount[WHITE][BISHOP] == 1
        && pieceCount[BLACK][BISHOP] == 1
        && opposite_colors(pieceList[WHITE][BISHOP][0], pieceList[BLACK][BISHOP][0]);
}
/*
�����J���[��bishop���Q�ȏ゠���āi�܂�Ƃ��Ă��Ȃ��j
board�̃J���[�i�s���͗l�̐F�j���قȂ��Ă����true
�����������m��bishop�݂͌��ɈقȂ�board�J���[�ɔz�u�����̂�
true���Ԃ��Ă���͓̂�����O�̂悤�ȋC�����邪�Achess960�ł�
�Ⴄ�̂����H
*/
inline bool Position::bishop_pair(Color c) const {

  return   pieceCount[c][BISHOP] >= 2
        && opposite_colors(pieceList[c][BISHOP][0], pieceList[c][BISHOP][1]);
}
/*
PAWN��RANK_7�ɂ���bitboard(BLACK�ɂƂ��Ă�Rank_2�j
�܂莟�̂P���QUEEN�ɂȂ����Ƃ�������
*/
inline bool Position::pawn_on_7th(Color c) const {
  return pieces(c, PAWN) & rank_bb(relative_rank(c, RANK_7));
}
/*
�ό`chess960���ǂ�����Ԃ�
*/
inline bool Position::is_chess960() const {
  return chess960;
}
/*
�ʏ�̓���(NORMAL=0)�{����Ƃ铮��Ȃ�true
promoto�@�b�b�@�A���p�b�T���Ȃ�true�i�m�[�}���A�L���X�����O�ȊO�j
*/
inline bool Position::capture_or_promotion(Move m) const {

  assert(is_ok(m));
  return type_of(m) ? type_of(m) != CASTLE : !empty(to_sq(m));
}
/*
�P���ɋ������{�A���p�b�T���Ȃ�true�@
*/
inline bool Position::capture(Move m) const {

  // Note that castle is coded as "king captures the rook"
  assert(is_ok(m));
  return (!empty(to_sq(m)) && type_of(m) != CASTLE) || type_of(m) == ENPASSANT;
}
/*
�������̋������Ă����Ado_move�֐��ōX�V�����
*/
inline PieceType Position::captured_piece_type() const {
  return m_st->capturedType;
}
/*
���̒T���p�̃X���b�h��Ԃ�
*/
inline Thread* Position::this_thread() const {
  return thisThread;
}
/*
���u�����Ƃɂ���Đ�����bitboard�̍X�V
��̈ړ��ł͂Ȃ��A�����̋�z�u�Ɏg�p����
*/
inline void Position::put_piece(Square s, Color c, PieceType pt) {

  board[s] = make_piece(c, pt);
  byTypeBB[ALL_PIECES] |= s;
  byTypeBB[pt] |= s;
  byColorBB[c] |= s;
  pieceCount[c][ALL_PIECES]++;
  index[s] = pieceCount[c][pt]++;
  pieceList[c][pt][index[s]] = s;
}
/*
��̈ړ��ɂ��ǖ�(bitboard�Ȃǁj�̍X�V
do_move,undo_move�֐�����Ă΂��
*/
inline void Position::move_piece(Square from, Square to, Color c, PieceType pt) {

  // index[from] is not updated and becomes stale. This works as long
  // as index[] is accessed just by known occupied squares.
  Bitboard from_to_bb = SquareBB[from] ^ SquareBB[to];
  byTypeBB[ALL_PIECES] ^= from_to_bb;
  byTypeBB[pt] ^= from_to_bb;
  byColorBB[c] ^= from_to_bb;
  board[from] = NO_PIECE;
  board[to] = make_piece(c, pt);
  index[to] = index[from];
  pieceList[c][pt][index[to]] = to;
}
/*
������ꂽ���̋ǖʂ̍X�V
byTypeBB,byTypeBB,byColorBB���X�V
index[],pieceList[][][],pieceCount[][]�z����X�V����֐�
do_move,undo_move,do_castle�֐�����Ă΂��
*/
inline void Position::remove_piece(Square s, Color c, PieceType pt) {

  // WARNING: This is not a reversible operation. If we remove a piece in
  // do_move() and then replace it in undo_move() we will put it at the end of
  // the list and not in its original place, it means index[] and pieceList[]
  // are not guaranteed to be invariant to a do_move() + undo_move() sequence.
  byTypeBB[ALL_PIECES] ^= s;
  byTypeBB[pt] ^= s;
  byColorBB[c] ^= s;
  /* board[s] = NO_PIECE; */ // Not needed, will be overwritten by capturing
  pieceCount[c][ALL_PIECES]--;
  Square lastSquare = pieceList[c][pt][--pieceCount[c][pt]];
  index[lastSquare] = index[s];
  pieceList[c][pt][index[lastSquare]] = lastSquare;
  pieceList[c][pt][pieceCount[c][pt]] = SQ_NONE;
}

#endif // #ifndef POSITION_H_INCLUDED
