/*
 * Util.h
 *
 *  Created on: 27 Apr 2015
 *      Author: tony
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#include "minimalstate.h"
#include "SearchQueueItem.h"
#include "instantiation.h"

namespace Planner {

const static int TOO_MANY_VARIABLES = 10000000;
const static double ERRONEOUS_TIME_STAMP_MIN = 0.000001;
const static double ERRONEOUS_TIME_STAMP_MAX = 99999999;
const static double ERRONEOUS_HEURISTIC_MIN = 0.000001;

void printAllLiterals();

void printState(const Planner::MinimalState & theState, double timeStamp);

void printErrorState(SearchQueueItem & searchNode, int stateNum);

void printLiteral(Inst::Literal * literal);

void printSearchNodeHeuristic(const SearchQueueItem & searchNode);

bool isSearchNodeValid(SearchQueueItem & searchNode);

}

#endif /* UTIL_UTIL_H_ */
