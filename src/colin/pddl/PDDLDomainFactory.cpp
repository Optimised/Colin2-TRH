/*
 * PDDLDomainFactory.cpp
 *
 *  Created on: 18 Jul 2015
 *      Author: tony
 */

#include <string>
#include <sstream>
#include <algorithm>
#include <list>

#include "ptree.h"

#include "PDDLDomainFactory.h"
#include "TIL.h"
#include "PDDLUtils.h"
#include "PropositionFactory.h"
#include "PNEFactory.h"

#include "../FakeTILAction.h"
#include "../RPGBuilder.h"

using namespace std;

namespace PDDL {

const string PDDLDomainFactory::TIL_ACHIEVED_PROPOSITION = "til-achieved";
const string PDDLDomainFactory::REQUIRED_PROPOSITION = "required";
const string PDDLDomainFactory::INITIAL_ACTION_COMPLETE_PROPOSITION_NAME =
		"initial-action-complete";
const PDDL::Proposition PDDLDomainFactory::INITIAL_ACTION_COMPLETE_PROPOSITION(
		PDDLDomainFactory::INITIAL_ACTION_COMPLETE_PROPOSITION_NAME,
		list<string>());

PDDLDomainFactory * PDDLDomainFactory::INSTANCE = NULL;

PDDLDomainFactory * PDDLDomainFactory::getInstance() {
	if (!INSTANCE) {
		INSTANCE = new PDDLDomainFactory(VAL::current_analysis->the_domain);
	}
	return INSTANCE;
}

PDDLDomainFactory::PDDLDomainFactory(const VAL::domain * domain) {
	types = getTypes(domain->types);
	domainPredicates = getDomainPredicates(domain->predicates);
	functions = getFunctions(domain->functions);
	constants = getConstantsFromDomain(domain->constants);
	domainOperators = getDomainOperators(domain->ops);
}

PDDL::PDDLDomain PDDLDomainFactory::getDomain(const VAL::domain * domain,
		const std::list<PendingAction> & pendingActions) {
	string name = domain->name;
	list<string> requirements = getDomainRequirements(domain->req, false);
	list<PDDL::Proposition> predicates = getPredicates(pendingActions);
	return PDDLDomain(name, requirements, types, predicates, functions,
			constants, domainOperators);
}

PDDL::PDDLDomain PDDLDomainFactory::getDeTILedDomain(const VAL::domain * domain,
		const Planner::MinimalState & state, double timestamp) {

	//Construct De-TILed Features
	list<string> deTILedActions;
	std::list<PDDL::Proposition> tilPredicates;
	std::list<PDDL::Proposition> tilRequiredObjects;
	std::set<PDDLObject> domainObjectSymbolTable;
	std::list<PDDL::Proposition> tilRequiredObjectsParameterised;
	list<PDDL::Proposition> pendingActionRequiredObjects;

	std::list<TIL> tils = getTILs(state, timestamp, domainObjectSymbolTable);
	bool hasTils = tils.size();
	if (hasTils) {
		deTILedActions = getdeTILedActions(tils, tilPredicates,
				tilRequiredObjects, tilRequiredObjectsParameterised);
	}
	std::list<PDDL::PendingAction> pendingActions = getPendingActions(state,
			timestamp, domainObjectSymbolTable, pendingActionRequiredObjects);

	//Construct the remainder of the domain
	string name = domain->name;
	list<string> requirements = getDomainRequirements(domain->req, hasTils);
	list<PDDL::PDDLObject> types = getTypes(domain->types);
	list<PDDL::Proposition> predicates = getPredicates(pendingActions,
			tilPredicates, tilRequiredObjectsParameterised);
	list<string> actions = getActions(pendingActions, deTILedActions);

	return PDDLDomain(name, requirements, types, predicates, functions,
			constants, actions, tilPredicates, tilRequiredObjects,
			pendingActionRequiredObjects, domainObjectSymbolTable);
}

list<string> PDDLDomainFactory::getDomainRequirements(VAL::pddl_req_flag flags,
		bool deTILed) {
	list<string> result;
	//Explicitly Add Equality because
	//They are used in de-tiled domains (for de-tiled actions)
	if ((flags & VAL::E_EQUALITY) || deTILed)
		result.push_back(":equality ");
	if (flags & VAL::E_STRIPS)
		result.push_back(":strips ");
	if (flags & VAL::E_TYPING)
		result.push_back(":typing ");
	if (flags & VAL::E_DISJUNCTIVE_PRECONDS)
		result.push_back(":disjunctive-preconditions ");
	if (flags & VAL::E_EXT_PRECS)
		result.push_back(":existential-preconditions ");
	if (flags & VAL::E_UNIV_PRECS)
		result.push_back(":universal-preconditions ");
	if (flags & VAL::E_COND_EFFS)
		result.push_back(":conditional-effects ");
	if (flags & VAL::E_NFLUENTS)
		result.push_back(":fluents ");
	if (flags & VAL::E_OFLUENTS)
		result.push_back(":object-fluents ");
	if (flags & VAL::E_ACTIONCOSTS)
		result.push_back(":action-costs ");
	if (flags & VAL::E_DURATIVE_ACTIONS)
		result.push_back(":durative-actions ");
	if (flags & VAL::E_DURATION_INEQUALITIES)
		result.push_back(":duration-inequalities ");
	if (flags & VAL::E_CONTINUOUS_EFFECTS)
		result.push_back(":continuous-effects ");
	//Explicitly Add Negative PreConditions because
	//They are used in de-tiled domains (for de-tiled actions)
	if ((flags & VAL::E_NEGATIVE_PRECONDITIONS) || deTILed)
		result.push_back(":negative-preconditions ");
	if (flags & VAL::E_DERIVED_PREDICATES)
		result.push_back(":derived-predicates ");
	//If deTiled ignore TIL requirement
	if ((flags & VAL::E_TIMED_INITIAL_LITERALS) && !deTILed)
		result.push_back(":timed-initial-literals ");
	if (flags & VAL::E_PREFERENCES)
		result.push_back(":preferences ");
	if (flags & VAL::E_CONSTRAINTS)
		result.push_back(":constraints ");
	if (flags & VAL::E_TIME)
		result.push_back(":time ");
	return result;
}

list<PDDL::PDDLObject> PDDLDomainFactory::getTypes(
		const VAL::pddl_type_list * types) {
	list<PDDL::PDDLObject> typeList;
	if (types) {
		VAL::pddl_type_list::const_iterator typeItr = types->begin();
		for (; typeItr != types->end(); typeItr++) {
			const VAL::pddl_type * type = *typeItr;
			PDDL::PDDLObject pddlObject = getPDDLObject(type);
			typeList.push_back(pddlObject);
		}
	}
	return typeList;
}

list<PDDL::Proposition> PDDLDomainFactory::getDomainPredicates(
		const VAL::pred_decl_list * predicates) {
	list<PDDL::Proposition> domainPropositions;
	//get standard domain predicates
	VAL::pred_decl_list::const_iterator predItr = predicates->begin();
	for (; predItr != predicates->end(); predItr++) {
		const VAL::pred_decl * predicate = *predItr;
		PDDL::Proposition domProp =
				PropositionFactory::getInstance()->getProposition(predicate);
		domainPropositions.push_back(domProp);
	}
	return domainPropositions;
}

list<PDDL::Proposition> PDDLDomainFactory::getPredicates(
		const std::list<PendingAction> & pendingActions /*=empty list*/,
		const std::list<PDDL::Proposition> & tilPredicates /*=empty list*/,
		const std::list<PDDL::Proposition> & tilRequiredObjects /*=empty list*/) {
	list<PDDL::Proposition> propositions;

	//Add Domain Predicates
	propositions.insert(propositions.end(), domainPredicates.begin(),
			domainPredicates.end());
	//Add Predicates for de-TILed Actions
	//This ensures correct TIL ordering
	propositions.insert(propositions.end(), tilPredicates.begin(),
			tilPredicates.end());
	//Add Required Predicates for Partial Actions
	if (pendingActions.size()) {
		std::list<PendingAction>::const_iterator pendActItr =
				pendingActions.begin();
		for (; pendActItr != pendingActions.end(); pendActItr++) {
			list<PDDL::Proposition> requiredPredicates =
					pendActItr->getRequiredPropositionsParameterised();
			propositions.insert(propositions.end(), requiredPredicates.begin(),
					requiredPredicates.end());
		}
	}
	//Add Required Predicates for De-TILed Actions
	//This ensures the correct Objects are used for a TIL
	propositions.insert(propositions.end(), tilRequiredObjects.begin(),
			tilRequiredObjects.end());

	//Add Initial Action Predicate
	propositions.push_back(INITIAL_ACTION_COMPLETE_PROPOSITION);
	return propositions;
}

list<PDDL::Proposition> PDDLDomainFactory::getFunctions(
		const VAL::func_decl_list * functions) {
	list<PDDL::Proposition> functionList;
	VAL::func_decl_list::const_iterator funcItr = functions->begin();
	for (; funcItr != functions->end(); funcItr++) {
		PDDL::Proposition function = getFunction(*funcItr);
		functionList.push_back(function);
	}
	return functionList;
}

std::list<std::pair<std::string, std::string> > PDDLDomainFactory::getConstantsFromDomain(
		const VAL::const_symbol_list * constants) {
	std::list<std::pair<std::string, std::string> > constantList;
	if (constants) {
		VAL::const_symbol_list::const_iterator constItr = constants->begin();
		for (; constItr != constants->end(); constItr++) {
			const VAL::const_symbol * con = *constItr;
			string name = con->getName();
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);
			std::pair<std::string, std::string> constant(name,
					getPDDLTypeString(con));
			constantList.push_back(constant);
		}
	}
	return constantList;
}

list<string> PDDLDomainFactory::getDomainOperators(
		const VAL::operator_list * operators) {
	list<string> domOperators;
	VAL::operator_list::const_iterator opsItr = operators->begin();
	for (; opsItr != operators->end(); opsItr++) {
		const VAL::operator_ * op = *opsItr;
		//Determine type of action
		const VAL::durative_action * dur_action =
				dynamic_cast<const VAL::durative_action *>(op);
		const VAL::action * action = dynamic_cast<const VAL::action *>(op);
		if (dur_action) {
			domOperators.push_back(getDurativeAction(dur_action));
		} else {
			domOperators.push_back(getAction(action));
		}
	}
	return domOperators;
}

list<string> PDDLDomainFactory::getActions(
		const std::list<PendingAction> & pendingActions,
		std::list<string> deTILedActions) {
	list<string> actions;
	//Add Domain Operators
	actions.insert(actions.end(), domainOperators.begin(),
			domainOperators.end());
	//Add Initial Action
	actions.push_back(getInitialAction());
	//Add Pending Actions
	list<string> pendingActionList = getPendingActions(pendingActions);
	actions.insert(actions.end(), pendingActionList.begin(),
			pendingActionList.end());
	//Add deTILed Actions
	actions.insert(actions.end(), deTILedActions.begin(), deTILedActions.end());
	return actions;
}

string PDDLDomainFactory::getDurativeAction(
		const VAL::durative_action * action) {
	ostringstream output;
	//get header
	output << "\t(:durative-action " << action->name->getName() << endl;
	//get parameters
	output << "\t\t:parameters (" << getArgumentString(action->parameters)
			<< ")" << endl;
	//get duration
	const VAL::timed_goal * duration =
			dynamic_cast<const VAL::timed_goal *>(action->dur_constraint);
	const VAL::comparison * durationalGoal =
			dynamic_cast<const VAL::comparison *>(duration->getGoal());
	output << "\t\t:duration " << getExpressionString(durationalGoal) << endl;
	//get conditions
	output << "\t\t:condition" << endl
			<< getConditions(action->precondition, true) << endl;
	//get effects
	output << "\t\t:effect" << endl << getEffectsString(action->effects)
			<< endl;
	output << "\t)";
	return output.str();
}

string PDDLDomainFactory::getAction(const VAL::action * action) {
	ostringstream output;
	//get header
	output << "\t(:action " << action->name->getName() << endl;
	//get parameters
	output << "\t\t:parameters (" << getArgumentString(action->parameters)
			<< ")" << endl;
	//get preconditions
	output << "\t\t:precondition" << endl
			<< getConditions(action->precondition, false) << endl;
	//get effects
	output << "\t\t:effect" << endl << getEffectsString(action->effects)
			<< endl;
	output << "\t)";
	return output.str();
}

std::string PDDLDomainFactory::getConditions(const VAL::goal * goal,
		bool isForDurativeAction) {
	std::ostringstream output;
	const VAL::conj_goal * conjGoal = dynamic_cast<const VAL::conj_goal *>(goal);
	if (conjGoal) {
		output << "\t\t\t(and " << endl;
		if (isForDurativeAction) {
			output << "\t\t\t\t(at start "
					<< INITIAL_ACTION_COMPLETE_PROPOSITION << ")" << std::endl;
		} else {
			output << "\t\t\t\t" << INITIAL_ACTION_COMPLETE_PROPOSITION
					<< std::endl;
		}
		VAL::goal_list::const_iterator goalItr = conjGoal->getGoals()->begin();
		for (; goalItr != conjGoal->getGoals()->end(); goalItr++) {
			const VAL::goal * aGoal = *goalItr;
			output << "\t\t\t\t" << getGoalString(aGoal) << endl;
		}
		output << "\t\t\t)" << endl;
	} else {
		output << "\t\t\t(and " << endl;
		output << "\t\t\t\t(at start " << INITIAL_ACTION_COMPLETE_PROPOSITION
				<< ")" << std::endl;
		output << getGoalString(goal);
	}
	return output.str();
}

std::string PDDLDomainFactory::getInitialAction() {
	ostringstream output;
	output << "\t(:action init-action" << endl;
	output << "\t\t:parameters()" << endl << "\t\t:precondition ( )" << endl
			<< "\t\t:effect " << endl << "\t\t\t"
			<< PDDLDomainFactory::INITIAL_ACTION_COMPLETE_PROPOSITION << endl
			<< "\t)";
	return output.str();
}

list<string> PDDLDomainFactory::getdeTILedActions(std::list<TIL> tils,
		std::list<PDDL::Proposition> & tilActionPreconditions,
		std::list<PDDL::Proposition> & tilRequiredObjects,
		std::list<PDDL::Proposition> & tilRequiredObjectsParameterised) {
	//sort list by timestamp
	tils.sort(PDDL::TIL::TILTimestampComparator);
	//Look at each TIL one at a time;
	list<string> actions;
	std::list<TIL>::const_iterator tilItr = tils.begin();
	const std::list<TIL>::const_iterator tilItrEnd = tils.end();
	for (; tilItr != tilItrEnd; tilItr++) {
		string deTILedAction = getdeTILedAction(*tilItr, tilActionPreconditions,
				tilRequiredObjects, tilRequiredObjectsParameterised);
		actions.push_back(deTILedAction);
	}
	return actions;
}

string PDDLDomainFactory::getdeTILedAction(const TIL & til,
		std::list<PDDL::Proposition> & tilActionPreconditions,
		std::list<PDDL::Proposition> & tilRequiredObjects,
		std::list<PDDL::Proposition> & tilRequiredObjectsParameterised) {
	//Create special proposition for this TIL
	std::list<string> arguments;
	PDDL::Proposition tilLit(til.getName(), arguments);
	//Find all parameters for this TIL Action and generate the paramtable
	std::map<PDDLObject, std::string> parameterTable =
			PDDL::generateParameterTable(til.getParameters());
	//List of objects that are required
	std::list<PDDL::Proposition> requiredObjects;

	//Generate action string
	ostringstream output;
	output << "\t(:action " << til.getName() << endl;
	//Add parameters and create the required predicates on the way
	output << "\t\t:parameters( ";
	std::map<PDDLObject, std::string>::const_iterator paramItr =
			parameterTable.begin();
	int paramNum = 1;
	for (; paramItr != parameterTable.end(); paramItr++) {
		output << paramItr->second << " - " << paramItr->first.getTypeString()
				<< " ";
		//Proposition to ensure correct objects are used
		ostringstream propName;
		propName << PDDLDomainFactory::REQUIRED_PROPOSITION << "-"
				<< til.getName() << "-" << paramNum;
		std::list<std::string> args;
		args.push_back(paramItr->first.getName());
		PDDL::Proposition paramRequired(propName.str(), args);
		requiredObjects.push_back(paramRequired);
		tilRequiredObjectsParameterised.push_back(
				paramRequired.getParameterisedProposition(parameterTable,
						true));
		paramNum++;
	}
	output << ")" << endl;

	// Pre-conditions
	output << "\t\t:precondition (and" << endl;
	//Add requirement for initial action
	output << "\t\t\t" << PDDLDomainFactory::INITIAL_ACTION_COMPLETE_PROPOSITION
			<< std::endl;
	//Add requiredment that TIL hasn't happened (one shot)
	//TODO: This is pretty much redundant given the required predicates
	output << "\t\t\t(not " << tilLit << ")" << std::endl;
	//Add requirement for past TILs to have been achieved
	if (tilActionPreconditions.size()) {
		std::list<PDDL::Proposition>::const_iterator preItr =
				tilActionPreconditions.begin();
		const std::list<PDDL::Proposition>::const_iterator preItrEnd =
				tilActionPreconditions.end();
		for (; preItr != preItrEnd; preItr++) {
			output << "\t\t\t" << preItr->toParameterisedString(parameterTable)
					<< endl;
		}
	}

	//Add predicates to ensure correct objects are used
	std::list<PDDL::Proposition>::const_iterator reqObjItr =
			requiredObjects.begin();
	for (; reqObjItr != requiredObjects.end(); reqObjItr++) {
		output << "\t\t\t" << reqObjItr->toParameterisedString(parameterTable)
				<< endl;
	}

	//Add Effects
	output << "\t\t)" << endl;
	output << "\t\t:effect (and" << endl;
	std::list<PDDL::Proposition>::const_iterator addItr =
			til.getAddEffects().begin();
	const std::list<PDDL::Proposition>::const_iterator addItrEnd =
			til.getAddEffects().end();
	for (; addItr != addItrEnd; addItr++) {
		output << "\t\t\t" << addItr->toParameterisedString(parameterTable)
				<< endl;
	}
	// add special predicate to indicate that the til is complete
	// This is the one shot to remove the TIL requirement
	//TODO: This is pretty much redundant given tracking of objects
	output << "\t\t\t" << tilLit << endl;

	// Del Effects
	// Add predicates to make cargo unavailable
	std::list<PDDL::Proposition>::const_iterator delItr =
			til.getDelEffects().begin();
	const std::list<PDDL::Proposition>::const_iterator delItrEnd =
			til.getDelEffects().end();
	for (; delItr != delItrEnd; delItr++) {
		output << "\t\t\t(not " << delItr->toParameterisedString(parameterTable)
				<< ")" << endl;
	}

	//Remove Predicates that required certain objects
	reqObjItr = requiredObjects.begin();
	for (; reqObjItr != requiredObjects.end(); reqObjItr++) {
		output << "\t\t\t(not "
				<< reqObjItr->toParameterisedString(parameterTable) << ")"
				<< endl;
	}

	//Footer
	output << "\t\t)" << endl << "\t)";
	//Add the TIL Proposition to the list of preconditions for future TILS
	//This retains precedence ordering of TILs
	tilActionPreconditions.push_back(tilLit);
	//Add required objects to domain list
	tilRequiredObjects.insert(tilRequiredObjects.end(), requiredObjects.begin(),
			requiredObjects.end());
	return output.str();
}

/**
 * Get the actions in a state that are in the middle of executing.
 * For example the start snap action has been executed, but not the end snap action
 */
std::list<PDDL::PendingAction> PDDLDomainFactory::getPendingActions(
		const Planner::MinimalState & state, double timestamp,
		std::set<PDDLObject> & objectSymbolTable,
		list<PDDL::Proposition> & pendingActionRequiredObjects) {
	std::list<PendingAction> pendingActions;
	//Cycle through Facts held up by executing actions (these are effects coming into play)
	std::map<int, std::set<int> >::const_iterator saItr =
			state.startedActions.begin();
	const std::map<int, std::set<int> >::const_iterator saItrEnd =
			state.startedActions.end();
	for (; saItr != saItrEnd; saItr++) {

		std::set<PDDLObject> parameters;

		//For each action get its conditions
		std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > conditions =
				getConditions(saItr->first, parameters);

		// Get action duration
		double minDur = Planner::RPGBuilder::getOpMinDuration(saItr->first, 1);
		double maxDur = Planner::RPGBuilder::getOpMaxDuration(saItr->first, 1);

		// Literals Added by Action
		std::list<std::pair<PDDL::Proposition, VAL::time_spec> > propositionalAddEffects;
		std::list<Inst::Literal*> adds =
				Planner::RPGBuilder::getEndPropositionAdds()[saItr->first];
		std::list<Inst::Literal*>::const_iterator addItr = adds.begin();
		const std::list<Inst::Literal*>::const_iterator addItrEnd = adds.end();
		for (; addItr != addItrEnd; addItr++) {
			const VAL::proposition * realProp = (*addItr)->toProposition();
			parameters = extractParameters(*addItr, parameters, constants);
			Proposition prop =
					PropositionFactory::getInstance()->getProposition(realProp);
			propositionalAddEffects.push_back(
					std::pair<PDDL::Proposition, VAL::time_spec>(prop,
							VAL::time_spec::E_AT_END));
		}

		// Literals Deleted by Action
		std::list<std::pair<PDDL::Proposition, VAL::time_spec> > propositionalDelEffects;
		std::list<Inst::Literal*> deletes =
				Planner::RPGBuilder::getEndPropositionDeletes()[saItr->first];
		std::list<Inst::Literal*>::const_iterator delItr = deletes.begin();
		const std::list<Inst::Literal*>::const_iterator delItrEnd =
				deletes.end();
		for (; delItr != delItrEnd; delItr++) {
			const VAL::proposition * realProp = (*addItr)->toProposition();
			parameters = extractParameters(*delItr, parameters, constants);
			Proposition prop =
					PropositionFactory::getInstance()->getProposition(realProp);
			propositionalDelEffects.push_back(
					std::pair<PDDL::Proposition, VAL::time_spec>(prop,
							VAL::time_spec::E_AT_END));
		}

		// PNEs Added by Action
		std::list<std::pair<PDDL::PNE, VAL::time_spec> > pneEffects;
		std::list<int> numerics =
				Planner::RPGBuilder::getEndEffNumerics()[saItr->first];
		std::list<int>::const_iterator numItr = numerics.begin();
		const std::list<int>::const_iterator numItrEnd = numerics.end();
		for (; numItr != numItrEnd; numItr++) {
			//Get the numeric effect information
			Planner::RPGBuilder::RPGNumericEffect & effect =
					Planner::RPGBuilder::getNumericEff()[*numItr];
			//Apply the numeric effect to get new fluent values
			pair<double, double> result = effect.applyEffectMinMax(
					state.secondMin, state.secondMax, minDur, maxDur);
			//Determine the PNE that the values belong
			Inst::PNE* aPNE = Planner::RPGBuilder::getPNE(effect.fluentIndex);
			PDDL::PNE pne = PNEFactory::getInstance()->getPNE(aPNE,
					result.first);
			pneEffects.push_back(
					std::pair<PDDL::PNE, VAL::time_spec>(pne,
							VAL::time_spec::E_AT_END));
			//Ensure the parameteres of the PNE are captured
			parameters = extractParameters(aPNE, parameters, constants);
		}
		std::string name = PDDL::getActionName(saItr->first);
		//Get Pending Action Required Objects
		list<PDDL::Proposition> requiredObjects =
				getPendingActionRequiredObjectPropositions(name, parameters);
		//Add required objects to conditions and effects
		list<PDDL::Proposition>::const_iterator reqObjItr =
				requiredObjects.begin();
		for (; reqObjItr != requiredObjects.end(); reqObjItr++) {

			std::pair<VAL::time_spec, bool> required(VAL::time_spec::E_AT_START,
					true);
			VAL::time_spec notRequiredTimeSpec = VAL::time_spec::E_AT_END;

			pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > requiredCondition(
					*reqObjItr, required);
			pair<PDDL::Proposition, VAL::time_spec> notRequiredEffect(
					*reqObjItr, notRequiredTimeSpec);
			conditions.push_back(requiredCondition);
			propositionalDelEffects.push_back(notRequiredEffect);
		}
		//Insert all required objects into master list for state
		pendingActionRequiredObjects.insert(pendingActionRequiredObjects.end(),
				requiredObjects.begin(), requiredObjects.end());

		// insert all parameters into master table
		objectSymbolTable.insert(parameters.begin(), parameters.end());
		//Generate symbol table for this action
		map<PDDLObject, string> parameterTable = PDDL::generateParameterTable(parameters);

		PendingAction pendingAction(name, parameterTable, propositionalAddEffects,
				propositionalDelEffects, pneEffects, conditions, requiredObjects, minDur);

		pendingActions.push_back(pendingAction);
	}
	return pendingActions;
}

list<string> PDDLDomainFactory::getPendingActions(
		const std::list<PendingAction> & pendingActions) {
	list<string> pendingActionList;
	std::list<PendingAction>::const_iterator actItr = pendingActions.begin();
	const std::list<PendingAction>::const_iterator actItrEnd =
			pendingActions.end();
	for (; actItr != actItrEnd; actItr++) {
		ostringstream pendingAction;
		pendingAction << *actItr;
		pendingActionList.push_back(pendingAction.str());
	}
	return pendingActionList;
}

std::list<PDDL::Proposition> PDDLDomainFactory::getPendingActionRequiredObjectPropositions(
		string actionName, std::set<PDDLObject> parameters) {
	list<PDDL::Proposition> requiredObjects;
	std::set<PDDLObject>::const_iterator paramItr = parameters.begin();
	int paramNum = 1;
	for (; paramItr != parameters.end(); paramItr++) {
		ostringstream name;
		name << PDDLDomainFactory::REQUIRED_PROPOSITION << "-" << actionName
				<< "-" << paramNum;
		paramNum++;
		list<string> params;
		string param = paramItr->getName();
		transform(param.begin(), param.end(), param.begin(), ::toupper);
		params.push_back(param);
		PDDL::Proposition prop(name.str(), params);
		requiredObjects.push_back(prop);
	}
	return requiredObjects;
}

std::list<PDDL::TIL> PDDLDomainFactory::getTILs(
		const Planner::MinimalState & state, double timestamp,
		std::set<PDDLObject> & objectSymbolTable) {

	std::list<PDDL::TIL> tils;

	//Cycle thourgh TILs
	list<Planner::FakeTILAction> theTILs = Planner::RPGBuilder::getTILs();
	std::list<Planner::FakeTILAction>::const_iterator tilItr = theTILs.begin();
	const std::list<Planner::FakeTILAction>::const_iterator tilItrEnd =
			theTILs.end();
	for (; tilItr != tilItrEnd; tilItr++) {
		const Planner::FakeTILAction * tilAction = &(*tilItr);
		//Make sure the TIL is still current
		if (tilAction->duration <= timestamp) {
			continue;
		}

		PDDL::extractParameters(tilAction, objectSymbolTable, constants);
		PDDL::TIL til = PDDL::getTIL(*tilAction, timestamp, constants);
		tils.push_back(til);
	}
	return tils;
}

/**
 * Get all conditions and return parameters
 */
std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > PDDLDomainFactory::getConditions(
		int actionID, std::set<PDDLObject> & parameters) {

	std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > conditions;

	/*Postconditions*/
	std::list<Inst::Literal*> positivePostConditions =
			Planner::RPGBuilder::getEndPropositionalPreconditions()[actionID];
	std::list<Inst::Literal*> negativePostConditions =
			Planner::RPGBuilder::getEndNegativePropositionalPreconditions()[actionID];

	std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > tempConditions =
			convertLiterals_AddSignAndTime(positivePostConditions,
					VAL::time_spec::E_AT_END, true, parameters);
	conditions.insert(conditions.end(), tempConditions.begin(),
			tempConditions.end());

	tempConditions = convertLiterals_AddSignAndTime(negativePostConditions,
			VAL::time_spec::E_AT_END, false, parameters);
	conditions.insert(conditions.end(), tempConditions.begin(),
			tempConditions.end());

	/*Invariant Conditions*/
	std::list<Inst::Literal*> positiveInvariantConditions =
			Planner::RPGBuilder::getInvariantPropositionalPreconditions()[actionID];
	std::list<Inst::Literal*> negativeInvariantConditions =
			Planner::RPGBuilder::getInvariantNegativePropositionalPreconditions()[actionID];
	tempConditions = convertLiterals_AddSignAndTime(positiveInvariantConditions,
			VAL::time_spec::E_OVER_ALL, true, parameters);
	conditions.insert(conditions.end(), tempConditions.begin(),
			tempConditions.end());

	tempConditions = convertLiterals_AddSignAndTime(negativeInvariantConditions,
			VAL::time_spec::E_OVER_ALL, false, parameters);
	conditions.insert(conditions.end(), tempConditions.begin(),
			tempConditions.end());
	return conditions;
}

/**
 * Convert Colin's Literals to our datamodel and add their sign and when they apply, with respect to the action
 */
std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > PDDLDomainFactory::convertLiterals_AddSignAndTime(
		std::list<Inst::Literal*> literals, VAL::time_spec timeQualifier,
		bool isPositive, std::set<PDDLObject> & parameters) {
	std::list<pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> > > conditions;

	std::list<Inst::Literal*>::const_iterator condItr = literals.begin();
	std::list<Inst::Literal*>::const_iterator condItrEnd = literals.end();

	for (; condItr != condItrEnd; condItr++) {
		const VAL::proposition * realCond = (*condItr)->toProposition();
		//This is required to ensure the parameters are created
		PDDL::extractParameters(*condItr, parameters, constants);
		PDDL::Proposition literal =
				PropositionFactory::getInstance()->getProposition(realCond);
		conditions.push_back(
				pair<PDDL::Proposition, std::pair<VAL::time_spec, bool> >(
						literal,
						std::pair<VAL::time_spec, bool>(timeQualifier,
								isPositive)));
	}
	return conditions;
}

}
