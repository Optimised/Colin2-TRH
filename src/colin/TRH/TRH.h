/*
 * TRH.h
 *
 *  Created on: 26 Jan 2016
 *      Author: tony
 */

#ifndef COLIN_TRH_TRH_H_
#define COLIN_TRH_TRH_H_

#include <list>

#include "PDDLState.h"
#include "PDDLStateFactory.h"
#include "../ExtendedMinimalState.h"
#include "../FFEvent.h"
#include "../util/Util.h"
#include "../lpscheduler.h" 

using namespace std;

namespace TRH {
class TRH {
private:
	static const char * H_CMD;
	static const string TEMP_FILE_PATH;
	static const string TEMP_FILE_PREFIX;
	static const string TEMP_DOMAIN_SUFFIX;
	static const string TEMP_FILE_EXT;
	static const string H_VAL_DELIM;
	static const string RELAXED_PLAN_SIZE_DELIM;
	static const string H_STATES_EVAL_DELIM;
	static const string H_PLAN_DELIM;
	static const string TEMP_STATE_PATH;
	static TRH * INSTANCE;

	static int generateNewInstanceID();
	
	/*Used to ensure unique state files per instance*/
	const int TRH_INSTANCE_ID;

	//Singleton
	TRH(int trhInstanceID) : TRH_INSTANCE_ID(trhInstanceID) {
	};
	TRH(TRH const & other) : TRH_INSTANCE_ID(generateNewInstanceID()){
	}
	;
	TRH& operator=(TRH const&) {
	}
	;

	string buildCommand();
	string writeTempState(const Planner::MinimalState & state,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory);
	void writeBadState(const Planner::MinimalState & state,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory, int stateNum);
	void writeStateToFile(const Planner::MinimalState & state,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory, string fileName);
	void removeTempState(string fileName);
	list<Planner::ActionSegment> getRelaxedPlan(string plan, double timestamp);
	static bool evaluateStateAndUpdatePlan(const Planner::ActionSegment & actionToBeApplied,
		Planner::ExtendedMinimalState & state, 
		Planner::ExtendedMinimalState * prevState,
		const auto_ptr<Planner::ParentData> incrementalData,
		std::list<Planner::FFEvent> & plan);
	// static Planner::ExtendedMinimalState * applyActionToState(
	// 	Planner::ActionSegment & actionToApply, const Planner::ExtendedMinimalState & parent, 
	// 	const list<Planner::FFEvent> & plan);

public:
	static TRH * getInstance();
	pair<double, int> getHeuristic(Planner::ExtendedMinimalState & theState,
		std::list<Planner::FFEvent>& plan, double timestamp, double heuristic, 
		PDDL::PDDLStateFactory pddlFactory);
	static double TIME_SPENT_IN_HEURISTIC;
	static double TIME_SPENT_IN_PRINTING_TO_FILE;
	static double TIME_SPENT_CONVERTING_PDDL_STATE;
};
}

#endif /* COLIN_TRH_TRH_H_ */
