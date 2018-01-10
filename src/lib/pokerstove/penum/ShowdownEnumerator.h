/**
 * Copyright (c) 2012 Andrew Prock. All rights reserved.
 */
#ifndef PENUM_SHOWDOWNENUMERATOR_H_
#define PENUM_SHOWDOWNENUMERATOR_H_

#include <boost/shared_ptr.hpp>
#include <pokerstove/peval/PokerHandEvaluator.h>
#include <pokerstove/util/combinations.h>
#include <pokerstove/peval/Card.h>
#include <pokerstove/peval/Suit.h>
#include <pokerstove/peval/Rank.h>
#include "CardDistribution.h"

#include <set>

using namespace std;

namespace pokerstove
{
    class ShowdownEnumerator
    {
        public:
            ShowdownEnumerator ();

            /**
             * enumerate a poker scenario, with board support
             */
            vector<EquityResult> calculateEquity (const std::vector<CardDistribution>& dists,
                                                       const CardSet& board,
                                                       boost::shared_ptr<PokerHandEvaluator> peval) const;

            /**
             * enumerate a poker scenario, with board support and fuzz input
             */
            vector<EquityResult> calculateEquityFuzz (const vector<std::string>& dists,
                                                       const CardSet& board,
                                                       boost::shared_ptr<PokerHandEvaluator> peval) const;

        private:
            /**
             * translate input into fuzz match hands cards.
             */
            string parseFuzzInput(const std::string& input) const;

            /**
             * translate input into fuzz match hands cards.
             */
            set<CardSet> extendHandCard(const std::string& input) const;

            /**
             * generate the cardset within the range.
             */
            set<CardSet> generateRange(Rank a, Rank l, Rank r, char m, bool pair) const;
    };
}

#endif  // PENUM_SHOWDOWNENUMERATOR_H_
