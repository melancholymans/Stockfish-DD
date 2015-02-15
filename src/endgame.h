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

#ifndef ENDGAME_H_INCLUDED
#define ENDGAME_H_INCLUDED

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "position.h"
#include "types.h"


/// EndgameType lists all supported endgames

enum EndgameType {

  // Evaluation functions

  KNNK,  // KNN vs K
  KXK,   // Generic "mate lone king" eval
  KBNK,  // KBN vs K
  KPK,   // KP vs K
  KRKP,  // KR vs KP
  KRKB,  // KR vs KB
  KRKN,  // KR vs KN
  KQKP,  // KQ vs KP
  KQKR,  // KQ vs KR
  KBBKN, // KBB vs KN
  KmmKm, // K and two minors vs K and one or two minors


  // Scaling functions
  SCALE_FUNS,

  KBPsK,   // KB+pawns vs K
  KQKRPs,  // KQ vs KR+pawns
  KRPKR,   // KRP vs KR
  KRPKB,   // KRP vs KB
  KRPPKRP, // KRPP vs KRP
  KPsK,    // King and pawns vs king
  KBPKB,   // KBP vs KB
  KBPPKB,  // KBPP vs KB
  KBPKN,   // KBP vs KN
  KNPK,    // KNP vs K
  KNPKB,   // KNP vs KB
  KPKP     // KP vs KP
};


/// Endgame functions can return a Value or a ScaleFactor, according to EndgameType
template<EndgameType E>
using eg_fun = std::conditional<(E < SCALE_FUNS), Value, ScaleFactor>;


/// Base and derived templates for endgame evaluation and scaling functions

template<typename T>
struct EndgameBase {

  virtual ~EndgameBase() {}
  virtual Color color() const = 0;
  virtual T operator()(const Position&) const = 0;
};


template<EndgameType E, typename T = typename eg_fun<E>::type>
struct Endgame : public EndgameBase<T> {

  explicit Endgame(Color c) : strongSide(c), weakSide(~c) {}
  Color color() const { return strongSide; }
  T operator()(const Position&) const;

private:
  const Color strongSide, weakSide;
};


/// Endgames class stores in two std::map the std::unique_ptr to endgame
/// evaluation and scaling base objects. Then we use polymorphism to invoke
/// the actual endgame function calling its operator() that is virtual.

class Endgames {

  template<typename T> using Map = std::map<Key, std::unique_ptr<T>>;

  template<EndgameType E, typename T = EndgameBase<typename eg_fun<E>::type>>
  void add(const std::string& code);

  template<typename T, int I = std::is_same<T, EndgameBase<ScaleFactor>>::value>
  Map<T>& map() { return std::get<I>(maps); }

  std::pair<Map<EndgameBase<Value>>, Map<EndgameBase<ScaleFactor>>> maps;

public:
  Endgames();

  template<typename T> T* probe(Key key, T** eg)
  { return *eg = map<T>().count(key) ? map<T>()[key].get() : nullptr; }
};

#endif // #ifndef ENDGAME_H_INCLUDED
