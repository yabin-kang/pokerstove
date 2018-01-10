/**
 * Copyright (c) 2012 Andrew Prock. All rights reserved.
 */
#include "ShowdownEnumerator.h"

#include "Odometer.h"
#include "PartitionEnumerator.h"
#include "SimpleDeck.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/math/special_functions/binomial.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <pokerstove/util/combinations.h>
#include <pokerstove/peval/Card.h>
#include <pokerstove/peval/Suit.h>

#define boost_foreach BOOST_FOREACH

namespace pokerstove {

ShowdownEnumerator::ShowdownEnumerator ()
{

}

vector<EquityResult> ShowdownEnumerator::calculateEquityFuzz (const vector<string>& inputs,
                                                              const CardSet& board,
                                                              boost::shared_ptr<PokerHandEvaluator> peval) const
{
    vector<CardDistribution> handDists;
    for (const string& input : inputs) {
        string hand = parseFuzzInput(input);
        handDists.emplace_back();
        handDists.back().parse(hand);
    }

    // calcuate the results and print them
    return calculateEquity(handDists, board, peval);
}

string ShowdownEnumerator::parseFuzzInput(const std::string& input) const
{
    string ret;
    vector<string> results;
    boost::split (results, input, boost::is_any_of(","));
    set<CardSet> handCardAll;
    boost_foreach (const string& h, results)
    {
        set<CardSet> handCards = extendHandCard(h);
        handCardAll.insert(handCards.begin(), handCards.end());
    }

    std::set<CardSet>::iterator it = handCardAll.begin();

    while(it != handCardAll.end()) {
        if (ret.length() > 0)
            ret += ",";
        ret += it->str();

        if (ret.find(".") < ret.length()) {
            ret = ".";
            break;
        }

        it++;
    }
    cout << input << endl << "   ==> " << ret << endl;
    return ret;
}

std::set<CardSet> ShowdownEnumerator::generateRange(Rank a, Rank l, Rank r, char m, bool pair) const
{
    std::set<CardSet> handCards;
    for (Rank b = l; b <= r; b++) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if ((m == 'o' && i == j) || (m == 's' && i != j) || (pair && i == j)) continue;
                if (pair) a = b;
                CardSet cs = CardSet(a.str() + Suit(i).str() + b.str() + Suit(j).str());
                handCards.insert(cs);
            }
        }
    }

    return handCards;
}

set<CardSet> ShowdownEnumerator::extendHandCard(const std::string& hand) const
{
    set<CardSet> handCards;
    if (hand == ".") {
        CardSet handCard;
        handCards.insert(handCard);
        return handCards;
    }

    // try parse as regular hand card first.
    if (hand.length() == 4) {
        CardSet cs;
        for (size_t i = 0; i < hand.length(); i += 2)
        {
            Card c;
            if (!c.fromString(hand.substr(i,i+2)))
                break;
            if (cs.contains(c))
                break;

            cs.insert(c);
        }

        if (cs.size() > 0) {
            handCards.insert(cs);
            return handCards;
        }
    }

    Rank r1 = Rank(hand);
    Rank r2 = Rank(hand.substr(1));
    bool pair = r1 == r2;
    Rank r3 = r1;
    Rank r4 = r2;
    char comp = 'a';
    if (hand.length() == 2) {
        handCards = generateRange(r1, r4, r3, comp, pair);
    } else {
        comp = hand[2];
        if (comp == '+' && hand.length() == 3) {
            comp = 'a';
            r3--;
            if (pair) r3 = Rank::Ace();
            handCards = generateRange(r1, r2, r3, comp, pair);
        } else if (comp == '-' && hand.length() == 5) {
            comp = 'a';
            r3 = Rank(hand.substr(3));
            r4 = Rank(hand.substr(4));
            if ((pair == (r3 == r4)) && (pair || r1 == r3)) {
                if (r2 > r4) {
                    r3 = r4;
                    r4 = r2;
                    r2 = r3;
                }
                handCards = generateRange(r1, r2, r4, comp, pair);
            }
        } else if (comp == 'o' || comp == 's') {
            char newcomp = hand[3];
            if (newcomp == '+' && hand.length() == 4) {
                r3--;
                if (pair) r3 = Rank::Ace();
                handCards = generateRange(r1, r2, r3, comp, pair);
            } else if (newcomp == '-' && hand.length() == 7) {
                r3 = Rank(hand.substr(4));
                r4 = Rank(hand.substr(5));
                newcomp = hand[6];
                if (comp == newcomp) {
                    if ((pair == (r3 == r4)) && (pair || r1 == r3)) {
                        if (r2 > r4) {
                            r3 = r4;
                            r4 = r2;
                            r2 = r3;
                        }
                        handCards = generateRange(r1, r2, r4, comp, pair);
                    }
                }
            } else if (hand.length() == 3) {
                handCards = generateRange(r1, r2, r4, comp, pair);
            }
        }
    }

    return handCards;
}

vector<EquityResult> ShowdownEnumerator::calculateEquity (const vector<CardDistribution>& dists,
                                                          const CardSet& board,
                                                          boost::shared_ptr<PokerHandEvaluator> peval) const
{
    if (peval.get() == NULL)
        throw runtime_error("ShowdownEnumerator, null evaluator");
    assert(dists.size() > 0);
    const size_t ndists = dists.size();
    vector<EquityResult> results(ndists, EquityResult());
    size_t handsize = peval->handSize();

    // the dsizes vector is a list of the sizes of the player hand
    // distributions
    vector<size_t> dsizes;
    for (size_t i=0; i<ndists; i++)
    {
        assert(dists[i].size() > 0);
        dsizes.push_back (dists[i].size());
    }

    // need to figure out the board stuff, we'll be rolling the board into
    // the partitions to make enumeration easier down the line.
    size_t nboards = 0;
    size_t boardsize = peval->boardSize();
    if (boardsize > 0)
        nboards++;

    // for the most part, these are allocated here to avoid contant stack
    // reallocation as we cycle through the inner loops
    SimpleDeck deck;
    CardSet dead;
    double weight;
    vector<CardSet>             ehands         (ndists+nboards);
    vector<size_t>              parts          (ndists+nboards);
    vector<CardSet>             cardPartitions (ndists+nboards);
    vector<PokerHandEvaluation> evals          (ndists);         // NO BOARD

    // copy quickness
    CardSet * copydest = &ehands[0];
    CardSet * copysrc = &cardPartitions[0];
    size_t ncopy = (ndists+nboards)*sizeof(CardSet);
    Odometer o(dsizes);
    do
    {
        // colect all the cards being used by the players, skip out in the
        // case of card duplication
        bool disjoint = true;
        dead.clear ();
        weight = 1.0;
        for (size_t i=0; i<ndists+nboards; i++)
        {
            if (i<ndists)
            {
                cardPartitions[i] = dists[i][o[i]];
                parts[i]          = handsize-cardPartitions[i].size();
                weight           *= dists[i][cardPartitions[i]];
            }
            else
            {
                // this allows us to have board distributions in the future
                cardPartitions[i] = board;
                parts[i]          = boardsize-cardPartitions[i].size();
            }
            disjoint = disjoint && dead.disjoint(cardPartitions[i]);
            dead |= cardPartitions[i];
        }

        if (disjoint)
        {
            deck.reset ();
            deck.remove (dead);
            PartitionEnumerator2 pe(deck.size(), parts);
            do
            {
                // we use memcpy here for a little speed bonus
                memcpy (copydest, copysrc, ncopy);
                for (size_t p=0; p<ndists+nboards; p++)
                    ehands[p] |= deck.peek(pe.getMask (p));

                // TODO: do we need this if/else, or can we just use the if
                // clause? A: need to rework tracking of whether a board is needed
                if (nboards > 0)
                    peval->evaluateShowdown (ehands, ehands[ndists], evals, results, weight);
                else
                    peval->evaluateShowdown (ehands, board, evals, results, weight);
            }
            while (pe.next ());
        }
    }
    while (o.next ());

    return results;
}

}
