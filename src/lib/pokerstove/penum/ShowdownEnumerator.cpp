/**
 * Copyright (c) 2012 Andrew Prock. All rights reserved.
 */
#include "ShowdownEnumerator.h"

#include <string>

#include <iostream>
#include <vector>

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
    boost_foreach (const string& h, results)
    {
        std::string newhand = extendHandCard(h);
        if (ret.size() > 0)
            ret += ",";
        ret += newhand;
    }

    cout << input << endl << "   ==> " << ret << endl;

    return ret;
}

    string ShowdownEnumerator::extendHandCard(const std::string& hand) const
{
    std::string ret = hand;
    std::set<CardSet> handCards;
    if (hand.find("+") == 2 || hand.find("+") == 3 || hand.find("o") == 2 || hand.find("s") == 2 || hand.length() == 2)
    {
        ret = "";
        Rank r1 = Rank(hand);
        Rank r2 = Rank(hand.substr(1));
        char comp = hand[2];
        if (comp == '\0') comp = '`';
        switch(comp)
        {
            case 'o':
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        if (i == j) continue;
                        CardSet cs = CardSet(r1.str() + Suit(i).str() + r2.str() + Suit(j).str());
                        handCards.insert(cs);
                    }
                }
                if (hand[3] == '+') {
                    Rank r3 = r2; r3++;
                    if (r1 == r2) {
                        for (; r3 <= Rank::Ace(); r3++) {
                            for (int i = 0; i < 4; i++) {
                                for (int j = 0; j < 4; j++) {
                                    if (i == j) continue;
                                    CardSet cs = CardSet(r3.str() + Suit(i).str() + r3.str() + Suit(j).str());
                                    handCards.insert(cs);
                                }
                            }
                        }
                    } else {
                        for (; r3 < r1; r3++) {
                            for (int i = 0; i < 4; i++) {
                                for (int j = 0; j < 4; j++) {
                                    if (i == j) continue;
                                    CardSet cs = CardSet(r1.str() + Suit(i).str() + r3.str() + Suit(j).str());
                                    handCards.insert(cs);
                                }
                            }
                        }
                    }
                }
                break;
            case 's':
                if (r1 == r2) {
                    break;
                }

                for (int i = 0; i < 4; i++) {
                    CardSet cs = CardSet(r1.str() + Suit(i).str() + r2.str() + Suit(i).str());
                    handCards.insert(cs);
                }

                if (hand[3] == '+') {
                    Rank r3 = r2; r3++;
                    for (; r3 < r1; r3++) {
                        for (int i = 0; i < 4; i++) {
                            CardSet cs = CardSet(r1.str() + Suit(i).str() + r3.str() + Suit(i).str());
                            handCards.insert(cs);
                        }
                    }
                }
                break;
            default:
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        if (i == j && r1 == r2) continue;
                        CardSet cs = CardSet(r1.str() + Suit(i).str() + r2.str() + Suit(j).str());
                        handCards.insert(cs);
                    }
                }
                if (hand[2] == '+') {
                    Rank r3 = r2; r3++;
                    if (r1 == r2) {
                        for (; r3 <= Rank::Ace(); r3++) {
                            for (int i = 0; i < 4; i++) {
                                for (int j = 0; j < 4; j++) {
                                    if (i == j) continue;
                                    CardSet cs = CardSet(r3.str() + Suit(i).str() + r3.str() + Suit(j).str());
                                    handCards.insert(cs);
                                }
                            }
                        }
                    } else {
                        for (; r3 < r1; r3++) {
                            for (int i = 0; i < 4; i++) {
                                for (int j = 0; j < 4; j++) {
                                    CardSet cs = CardSet(r1.str() + Suit(i).str() + r3.str() + Suit(j).str());
                                    handCards.insert(cs);
                                }
                            }
                        }
                    }
                }
                break;
        }
    }
    std::set<CardSet>::iterator it = handCards.begin();

    while(it != handCards.end()) {
        if (ret.length() > 0)
            ret += ",";
        ret += it->str();
        it++;
    }
    return ret;
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
