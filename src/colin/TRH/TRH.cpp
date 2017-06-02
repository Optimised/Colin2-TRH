#include <stdlib.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <memory>
#include <sstream>
#include <limits>
#include <random>

#include "TRH.h"
#include "PDDLDomain.h"
#include "PDDLState.h"
#include "PDDLDomainFactory.h"
#include "PendingAction.h"
#include "Proposition.h"
#include "PDDLObject.h"
#include "PDDLUtils.h"
#include "../FFSolver.h"
#include "instantiation.h"
#include "../temporalanalysis.h"

namespace TRH {

TRH * TRH::INSTANCE = NULL;
const char * TRH::H_CMD = "./lib/colin-clp";
const string TRH::TEMP_FILE_PATH = "";//"/tmp/";
const string TRH::TEMP_FILE_PREFIX = "temp";
const string TRH::TEMP_DOMAIN_SUFFIX = "-domain";
const string TRH::TEMP_FILE_EXT = ".pddl";
const string TRH::H_VAL_DELIM = "State Heuristic Value is: ";
const string TRH::RELAXED_PLAN_SIZE_DELIM = "Relaxed plan length is: ";
const string TRH::H_STATES_EVAL_DELIM = "; States evaluated: ";
const string TRH::H_PLAN_DELIM = "0.000:";
double TRH::TIME_SPENT_IN_HEURISTIC = 0.0;
double TRH::TIME_SPENT_IN_PRINTING_TO_FILE = 0.0;
double TRH::TIME_SPENT_CONVERTING_PDDL_STATE = 0.0;


TRH * TRH::getInstance() {
	if (!INSTANCE) {
		INSTANCE = new TRH(11);//generateNewInstanceID());
	}
	return INSTANCE;
}

int TRH::generateNewInstanceID() {
	static std::random_device rd;
	static std::default_random_engine generator(rd());
	static std::uniform_int_distribution<int> distribution(0,
		std::numeric_limits<int>::max());
	return distribution(generator);
}

pair<double, int> TRH::getHeuristic(Planner::ExtendedMinimalState & theState,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, PDDL::PDDLStateFactory pddlFactory) {

	const Planner::MinimalState & state = theState.getInnerState();

    Planner::FF::STATES_EVALUATED++;
	string stateName = writeTempState(state, plan, timestamp, heuristic, pddlFactory);
	cout << "State Name: " << stateName << endl;
	clock_t begin_time = clock();

	std::shared_ptr<FILE> pipe(popen(buildCommand().c_str(), "r"), pclose);
	if (!pipe)
		return std::make_pair(-1.0, -1.0);
	char buffer[128];
	std::string result = "";
	while (!feof(pipe.get())) {
		if (fgets(buffer, 128, pipe.get()) != NULL)
			cout << buffer;
			result += buffer;
	}
	TRH::TRH::TIME_SPENT_IN_HEURISTIC += double( clock () - begin_time ) /  CLOCKS_PER_SEC;
	//removeTempState(stateName);
	int pos = result.find(H_STATES_EVAL_DELIM);
	if (pos != -1) {
		int posEnd = result.find("\n", pos);
		string statesEvalStr = result.substr(pos + H_STATES_EVAL_DELIM.size(), posEnd-(pos + H_STATES_EVAL_DELIM.size()));
		int statesEval = stoi(statesEvalStr);
		// printf("Heuristic States Eval: %s\n", statesEvalStr.c_str());
		Planner::FF::STATES_EVALUATED_IN_HEURISTIC += statesEval;
	}
	pos = result.find(H_VAL_DELIM);
	if (pos == -1) {
		// cerr << "Problem was unsolvable - therefore heuristic value of -1.0" << endl;
		static int badStateNum = 0;
		//writeBadState(state, plan, timestamp, heuristic, pddlFactory, badStateNum++);
		return std::make_pair(-1.0, -1.0);
	}
	string h_val_str = result.substr(pos + H_VAL_DELIM.size());
	// printf("%s\n", h_val_str.c_str());
	double hval = stod(h_val_str);
	
	int relaxedPlanSize = 0;
	pos = result.find(RELAXED_PLAN_SIZE_DELIM);
	if (pos != -1) {
		int posEnd = result.find("\n", pos);
		string relaxedPlanSizeStr = result.substr(pos + RELAXED_PLAN_SIZE_DELIM.size(), posEnd-(pos + RELAXED_PLAN_SIZE_DELIM.size()));
		relaxedPlanSize = stoi(relaxedPlanSizeStr);
		// printf("Relaxed Plan Length: %s\n", relaxedPlanSizeStr.c_str());
	}
	int planPos = result.find(H_PLAN_DELIM);
	if ((planPos != -1) && (hval == 0.0)) {
		cout << "initial plan size: " << plan.size() << endl;
		Planner::FFEvent::printPlan(plan);
		string rPlan = result.substr(planPos, pos-planPos);
		list<Planner::ActionSegment> relaexedPlan = getRelaxedPlan(rPlan, timestamp);
		list<Planner::ActionSegment>::iterator rPlanItr = relaexedPlan.begin();
		Planner::ExtendedMinimalState * theStatePtr = &theState;
		Planner::ExtendedMinimalState * prevState = 0;

		// std::map<int, std::set<int> >::const_iterator saItr =
		// 	state.startedActions.begin();
		// const std::map<int, std::set<int> >::const_iterator saItrEnd =
		// 	state.startedActions.end();

		// for (; saItr != saItrEnd; saItr++) {
		// 	prevState = theStatePtr;
		// 	Planner::ActionSegment actSeg(Planner::RPGBuilder::getInstantiatedOp(saItr->first), 
		// 		VAL::E_AT_END, theStatePtr->getInnerState().nextTIL, Planner::RPGHeuristic::emptyIntList);
		// 	theStatePtr = Planner::FF::applyActionToState(actSeg, *theStatePtr, plan);
		// 	if (!Planner::FF::checkTemporalSoundness(*theStatePtr, actSeg)) {
		// 		cout << "#####Continuing Prefix is temporally not sound..." << endl;
		// 		break;
		// 	} 
		// 	if (!evaluateStateAndUpdatePlan(actSeg, *theStatePtr, prevState, plan)) {
		// 		cout << "#####Something went wrong continuing prefix..." << endl;
		// 		break;	
		// 	}
		// }

		for (int i = 0; rPlanItr != relaexedPlan.end(); rPlanItr++, i++) {
			//Update divisionID / nextTIL
			rPlanItr->divisionID = theStatePtr->getInnerState().nextTIL;
			cout << "Applying action " << PDDL::getActionName(rPlanItr->first->getID()) 
				<< " - " << rPlanItr->second << endl;
			//Add action to plan
			prevState = theStatePtr;
			//
			theStatePtr = Planner::FF::applyActionToState(*rPlanItr, *theStatePtr, plan);
			
			if (!Planner::FF::checkTemporalSoundness(*theStatePtr, *rPlanItr)) {
				cout << "#####Temporally not sound..." << endl;
				break;
			}
			const auto_ptr<Planner::ParentData> incrementalData(Planner::FF::allowCompressionSafeScheduler ? 0 : 
				Planner::LPScheduler::prime(plan, theStatePtr->getInnerState().temporalConstraints,
                theStatePtr->startEventQueue));
			if (!evaluateStateAndUpdatePlan(*rPlanItr, *theStatePtr, prevState, incrementalData, plan)) {
				cout << "#####Something went wrong updating plan..." << endl;
				break;	
			}
			cout << "updating plan: " << i << " " <<
			theStatePtr->getEditableInnerState().temporalConstraints << " " << plan.size() << endl;
		}
		// Planner::FFEvent::printPlan(plan);
	}
	return std::make_pair (hval, relaxedPlanSize);
}

string TRH::buildCommand() {
	ostringstream cmd;
	cmd << H_CMD << " " 
		<< TEMP_FILE_PATH << TEMP_FILE_PREFIX << TRH_INSTANCE_ID 
			<< TEMP_DOMAIN_SUFFIX << TEMP_FILE_EXT
		<< " " << TEMP_FILE_PATH << TEMP_FILE_PREFIX << TRH_INSTANCE_ID 
			<< TEMP_FILE_EXT;
	cout << cmd.str() << endl;
	return cmd.str();
}

list<Planner::ActionSegment> TRH::getRelaxedPlan(string plan, double timestamp) {
	list<Planner::ActionSegment> rPlan;
	string temp;
	std::istringstream inputStream (plan);
	int tilsEncountered = 0; //need to adjust timings for relaxed tils
	const double tilDuration = EPSILON;
	while (getline(inputStream, temp)) {
		
		int actionNameStartPos = temp.find("(");
		int actionNameEndPos = temp.find(")") + 1;
		string actionInstance = temp.substr(actionNameStartPos, 
			actionNameEndPos - actionNameStartPos);
		string actionName = actionInstance.substr(1, actionInstance.find(" ") - 1);
		Inst::instantiatedOp * op = PDDL::getOperator(actionInstance);
		
		// int startTimePos = temp.find(":");
		// double startTime = stod(temp.substr(0, startTimePos));
		// //Add the timestamp to the action to fit after the prefix
		// startTime += timestamp;
		// //Deduct any previous TIL durations from startTimes
		// startTime -= (tilsEncountered * tilDuration);

		int actionDurationStartPos = temp.find("[") + 1;
		int actionDurationEndPos = temp.find("]");
		double duration = stod(temp.substr(actionDurationStartPos, 
			actionDurationEndPos - actionDurationStartPos));

		if (op == 0){
			//Is it a til action?
			//FIXME: Possibly need to be more thorough here
			if ((PDDL::isTILAction(actionName, 0.0, 0.0)) && 
				(duration == EPSILON)){
				tilsEncountered++;
			}
		} else {
			Planner::ActionSegment start_snap_action = Planner::ActionSegment(op, 
				VAL::E_AT_START, 0, Planner::RPGHeuristic::emptyIntList);
			rPlan.push_back(start_snap_action);
			if (!Planner::RPGBuilder::getRPGDEs(op->getID()).empty()) {
				//Durative Action
				Planner::ActionSegment end_snap_action = Planner::ActionSegment(op, 
					VAL::E_AT_END, 0, Planner::RPGHeuristic::emptyIntList);
				rPlan.push_back(end_snap_action);	
			}			
		}
	}
	return rPlan;
}

bool TRH::evaluateStateAndUpdatePlan(const Planner::ActionSegment & actionToBeApplied,
		Planner::ExtendedMinimalState & state, 
		Planner::ExtendedMinimalState * prevState,
		const auto_ptr<Planner::ParentData> incrementalData,
		list<Planner::FFEvent> & plan)
{

	list<Planner::ActionSegment> helpfulActions;


	Planner::FFEvent extraEvent;
	Planner::FFEvent extraEventTwo;

    bool eventOneDefined = false;
    bool eventTwoDefined = false;

    const bool rawDebug = false;

    map<double, list<pair<int, int> > > * justApplied = 0;
    map<double, list<pair<int, int> > > actualJustApplied;
    double tilFrom = 0.001;

    int stepID = -1;

    if (actionToBeApplied.second == VAL::E_AT_START) {
        cout << "Start Action" << endl;
        if (rawDebug) cout << "RAW start\n";
        extraEvent = Planner::FFEvent(actionToBeApplied.first, 
        	state.startEventQueue.back().minDuration, state.startEventQueue.back().maxDuration);
        eventOneDefined = true;

        assert(extraEvent.time_spec == VAL::E_AT_START);
        Planner::FF::makeJustApplied(actualJustApplied, tilFrom, state, true);
        if (!actualJustApplied.empty()) justApplied = &actualJustApplied;

        // if it's not a non-temporal action
        if (!Planner::RPGBuilder::getRPGDEs(actionToBeApplied.first->getID()).empty()) { 

        	const int endStepID = plan.size() + 1;
        	cout << "end Step ID: " << endStepID << endl;
            const int startStepID = endStepID - 1;
            extraEventTwo = Planner::FFEvent(actionToBeApplied.first, startStepID, 
            	state.startEventQueue.back().minDuration, state.startEventQueue.back().maxDuration);
            extraEvent.pairWithStep = endStepID;
            eventTwoDefined = true;

            if (!Planner::TemporalAnalysis::canSkipToEnd(actionToBeApplied.first->getID())) {
                extraEventTwo.getEffects = false;
            }

            stepID = startStepID;
            cout << "Step ID: " << stepID << endl;
        } else {
        	stepID = state.getInnerState().planLength - 1;
        	cout << "Step ID2: " << stepID << endl;
        }
    } else if (actionToBeApplied.second == VAL::E_AT_END) {
        if (rawDebug) cout << "RAW end\n";
        map<int, list<list<Planner::StartEvent>::iterator > >::iterator tsiOld = state.entriesForAction.find(actionToBeApplied.first->getID());
        assert(tsiOld != state.entriesForAction.end());

        list<Planner::StartEvent>::iterator pairWith = tsiOld->second.front();
        tsiOld->second.pop_front();
        if (tsiOld->second.empty()) state.entriesForAction.erase(tsiOld);

        if (rawDebug || Planner::Globals::globalVerbosity & 1048576) cout << "Pairing with start at step " << pairWith->stepID << ", so activating end at " << pairWith->stepID + 1 << "\n";

        stepID = pairWith->stepID + 1;

        {
            list<Planner::FFEvent>::iterator pwItr = plan.begin();
            for (int sID = 0; sID <= pairWith->stepID; ++sID, ++pwItr) ;
            pwItr->getEffects = true;
        }

        state.startEventQueue.erase(pairWith);

        Planner::FF::makeJustApplied(actualJustApplied, tilFrom, state, false);
        if (!actualJustApplied.empty()) justApplied = &actualJustApplied;
    } else {
        extraEvent = Planner::FFEvent(actionToBeApplied.divisionID);
        eventOneDefined = true;
        stepID = state.getInnerState().planLength - 1;
    }


    list<Planner::FFEvent> nowList;

    if (eventOneDefined) nowList.push_back(extraEvent);
    if (eventTwoDefined) nowList.push_back(extraEventTwo);

    assert(stepID != -1);

    //Update Time
    Planner::LPScheduler tryToSchedule(state.getInnerState(), plan, nowList, stepID, state.startEventQueue, incrementalData.get(), state.entriesForAction, (prevState ? &prevState->getInnerState().secondMin : 0), (prevState ? &prevState->getInnerState().secondMax : 0), &(state.tilComesBefore), Planner::FF::scheduleToMetric);

    //Check if it is valid
    // if (!tryToSchedule.isSolved()) return false;

    vector<double> timeAtWhichValueIsDefined;
    // tryToSchedule.updateStateFluents(state.getEditableInnerState().secondMin, state.getEditableInnerState().secondMax, timeAtWhichValueIsDefined);

    if (eventTwoDefined) {
        extraEventTwo = nowList.back();
        nowList.pop_back();
    }

    if (eventOneDefined) {
        extraEvent = nowList.back();
    }

    #ifdef POPF3ANALYSIS
    #ifndef TOTALORDERSTATES
    if (actionToBeApplied.second == VAL::E_AT_START) {
        if (   !RPGBuilder::getRPGDEs(actionToBeApplied.first->getID()).empty()
            && TemporalAnalysis::canSkipToEnd(actionToBeApplied.first->getID())) { // if it's a compression-safe temporal action

            const int endStepID = state.getInnerState().planLength - 1;

            state.getEditableInnerState().updateWhenEndOfActionIs(actionToBeApplied.first->getID(), endStepID, extraEventTwo.lpMinTimestamp);

        }
    }
    #endif
    #endif

    if (eventOneDefined) {
        plan.push_back(extraEvent);
    }

    if (eventTwoDefined) {
        plan.push_back(extraEventTwo);
    }

    if (actionToBeApplied.second == VAL::E_AT_START // If it's the start of an action...
           && !Planner::RPGBuilder::getRPGDEs(actionToBeApplied.first->getID()).empty() // that is temporal..
           && Planner::TemporalAnalysis::canSkipToEnd(actionToBeApplied.first->getID())) { // and compression-safe

       // we can pull it off the start event queue as we don't need to worry about applying it

       state.startEventQueue.pop_back();
   }
   return true;
}

// Planner::ExtendedMinimalState * TRH::applyActionToState(Planner::ActionSegment & actionToApply, 
// 	const Planner::ExtendedMinimalState & parent, const list<Planner::FFEvent> & plan)
// {

// //  static const double EPSILON = 0.001;
//     const bool localDebug = false;

//     if (localDebug) {
//         if (actionToApply.second == VAL::E_AT) {
//             cout << "Applying TIL " << actionToApply.divisionID << " to state:\n";
//         } else {
//             cout << "Applying action " << *(actionToApply.first);
//             if (actionToApply.second == VAL::E_AT_START) {
//                 cout << " start";
//             } else {
//                 cout << " end";
//             }
//             cout << " to state:\n";
//         }
//         //printState(theState);
//     }

// //  assert(actionToApply.needToFinish.empty());


//     assert(!actionToApply.first || !RPGBuilder::rogueActions[actionToApply.first->getID()]);
//     assert(RPGBuilder::getHeuristic()->testApplicability(parent.getInnerState(), parent.timeStamp, actionToApply, true));

//     static vector<double> minTimestamps(10, 0.0);
//     static int tsVecSize = 10;

//     list<FFEvent>::const_iterator pItr = plan.begin();
//     const list<FFEvent>::const_iterator pEnd = plan.end();

//     for (int s = 0; pItr != pEnd; ++pItr, ++s) {
//         if (s >= tsVecSize) {
//             tsVecSize += 10;
//             minTimestamps.resize(tsVecSize, 0.0);
//         }
//         minTimestamps[s] = pItr->lpMinTimestamp;
//     }


//     if (actionToApply.second == VAL::E_AT) { // til actions
//         assert(actionToApply.divisionID == parent.getInnerState().nextTIL);
//         return parent.applyAction(actionToApply, minTimestamps);

//     }

//     double minDur = 0.0;
//     double maxDur = 0.0;

//     bool nonTemporal = false;

//     RPGBuilder::LinearEffects * const lEffs = RPGBuilder::getLinearDiscretisation()[actionToApply.first->getID()];

//     if (actionToApply.second != VAL::E_AT_END) {

//         pair<double, double> actDur(RPGBuilder::getOpDuration(actionToApply.first->getID(), (actionToApply.second == VAL::E_OVER_ALL ? actionToApply.divisionID + 1 : 0), parent.getInnerState().secondMin, parent.getInnerState().secondMax));

//         minDur = actDur.first;
//         maxDur = actDur.second;

//         nonTemporal = RPGBuilder::getRPGDEs(actionToApply.first->getID()).empty();

//         if (Globals::globalVerbosity & 4096 && !nonTemporal) {
//             cout << "- Calculated duration of new action as being in the range [" << minDur << "," << maxDur << "]\n";
//         }

//         if (!nonTemporal) {
//             if (minDur > maxDur) return 0;
//         }

//     } else {
//         map<int, list<list<StartEvent>::iterator > >::const_iterator tsiOld = parent.entriesForAction.find(actionToApply.first->getID());
//         assert(tsiOld != parent.entriesForAction.end());

//         const list<StartEvent>::iterator pairWith = tsiOld->second.front();
//         minDur = pairWith->minDuration;
//         maxDur = pairWith->maxDuration;

//     }


//     ExtendedMinimalState * const toReturn = parent.applyAction(actionToApply, minTimestamps, minDur, maxDur);

//     if (actionToApply.second == VAL::E_AT_START) {

//         if (!nonTemporal) {
//             if (TemporalAnalysis::canSkipToEnd(actionToApply.first->getID())) {
//                 toReturn->startEventQueue.push_back(StartEvent(actionToApply.first->getID(), 0, toReturn->getInnerState().planLength - 2, minDur, maxDur, 0.0));
//             } else {
//                 toReturn->startEventQueue.push_back(StartEvent(actionToApply.first->getID(), 0, toReturn->getInnerState().planLength - 2, minDur, maxDur, 0.0));
//                 list<StartEvent>::iterator backItr = toReturn->startEventQueue.end();
//                 --backItr;
//                 toReturn->entriesForAction[actionToApply.first->getID()].push_back(backItr);
//                 //cout << "Now " << theState.startedActions[actionToApply.first->getID()][0] << " instances of " << *(actionToApply.first) << " going\n";

//             }
//         }

//     } else if (actionToApply.second == VAL::E_AT_END) {
//         map<int, list<list<StartEvent>::iterator > >::iterator efaItr = toReturn->entriesForAction.find(actionToApply.first->getID());
//         assert(efaItr != toReturn->entriesForAction.end());

//         list<list<StartEvent>::iterator >::iterator efaMatch = efaItr->second.begin();
//         const list<list<StartEvent>::iterator >::iterator efaEnd = efaItr->second.end();

//         if (lEffs) {

//             const int intToMatch = lEffs->divisions - 1;

//             for (; efaMatch != efaEnd; ++efaMatch) {
//                 if ((*efaMatch)->divisionsApplied == intToMatch) break;
//             }

//             assert(efaMatch != efaEnd);


//         }

//         (*efaMatch)->terminated = true;

//     } else {
//         assert(false); // is neither a start nor an end
//     }

//     {
//         set<int> checked;

//         // check numeric invariants are all satisfied
//         list<StartEvent>::const_iterator seqItr = toReturn->startEventQueue.begin();
//         const list<StartEvent>::const_iterator seqEnd = toReturn->startEventQueue.end();

//         for (; seqItr != seqEnd; ++seqItr) {

//             if (seqItr->terminated) {
//                 continue;
//             }

//             const list<int> & numInvs = RPGBuilder::getInvariantNumerics()[seqItr->actID];

//             list<int>::const_iterator niItr = numInvs.begin();
//             const list<int>::const_iterator niEnd = numInvs.end();

//             for (; niItr != niEnd; ++niItr) {
//                 if (checked.insert(*niItr).second) {
//                     const RPGBuilder::RPGNumericPrecondition & currPre = RPGBuilder::getNumericPreTable()[*niItr];
//                     if (!currPre.isSatisfiedWCalculate(toReturn->getInnerState().secondMin, toReturn->getInnerState().secondMax)) {
//                         if (Globals::globalVerbosity & 8192) {
//                             cout << "Failed temporal soundness check: invariant " << currPre << " of " << *(RPGBuilder::getInstantiatedOp(seqItr->actID)) << " is now violated\n";
//                         }
//                         break;
//                     }
//                 }
//             }
//             if (niItr != niEnd) {
//                 break;
//             }
//         }

//         if (seqItr != seqEnd) {
//             delete toReturn;
//             return 0;
//         }

//     }

//     return toReturn;
// }

string TRH::writeTempState(const Planner::MinimalState & state,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory) {
    // static int oneShot = 0;
    // if (oneShot++ > 1) {
    // 	cerr << "Exiting..." << endl;
    // 	exit(0);
    // }

	ostringstream stateFileName;
	stateFileName << TEMP_FILE_PREFIX << TRH_INSTANCE_ID;
	writeStateToFile(state, plan, timestamp, heuristic, 
		pddlFactory, stateFileName.str());
	return stateFileName.str();
}

void TRH::writeBadState(const Planner::MinimalState & state,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory, int stateNum) {
	
	ostringstream stateFileName;
	stateFileName << "BadState" << TRH_INSTANCE_ID << "-" << stateNum;
	writeStateToFile(state, plan, timestamp, heuristic, 
		pddlFactory, stateFileName.str());
}


void TRH::writeStateToFile(const Planner::MinimalState & state,
	std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
	PDDL::PDDLStateFactory pddlFactory, string fileName) {

	clock_t begin_time = clock();

    /*Generate Domain*/

    //Domain
    PDDL::PDDLDomain domain = PDDL::PDDLDomainFactory::getInstance()->getDeTILedDomain(
    		VAL::current_analysis->the_domain, state, timestamp);

    //Shared data
    std::list<PDDL::Proposition> tilRequiredObjects = domain.getTILRequiredObjects();
    std::list<PDDL::Proposition> tilPredicates = domain.getTILPredicates();
    std::list<PDDL::Proposition> pendingActionRequiredObjects = domain.getPendingActionRequiredObjects();
    std::set<PDDL::PDDLObject> domainObjectSymbolTable = domain.getDomainObjectSymbolTable();

    //State
    PDDL::PDDLState pddlState = pddlFactory.getDeTILedPDDLState(state, plan, timestamp, 
        	heuristic, tilPredicates, tilRequiredObjects, pendingActionRequiredObjects, domainObjectSymbolTable);
        		
    TRH::TRH::TIME_SPENT_CONVERTING_PDDL_STATE += double( clock () - begin_time ) /  CLOCKS_PER_SEC;
        
    //Write State/Domain to disk for heuristic computation
    begin_time = clock();
	
	string domainFileName = fileName + TEMP_DOMAIN_SUFFIX;
	pddlState.writeDeTILedStateToFile(TEMP_FILE_PATH, fileName);
	domain.writeToFile(TEMP_FILE_PATH, domainFileName);
	TRH::TRH::TIME_SPENT_IN_PRINTING_TO_FILE += double( clock () - begin_time ) /  CLOCKS_PER_SEC;
}

void TRH::removeTempState(string fileName) {
	ostringstream stateFileName;
	ostringstream domainFileName;

	domainFileName << TEMP_FILE_PATH << fileName
		<< TEMP_DOMAIN_SUFFIX << TEMP_FILE_EXT;
	stateFileName << TEMP_FILE_PATH << fileName << TEMP_FILE_EXT;

	//Remove Domain File
	remove(domainFileName.str().c_str());
	//Remove State File
	remove(stateFileName.str().c_str());
}
  
}
