/*
 * PDDLMMCRDomainFactory.h
 *
 *  Created on: 18 Jul 2015
 *      Author: tony
 */

#ifndef COLIN_PDDL_MMCRDOMAINFACTORY_H_
#define COLIN_PDDL_MMCRDOMAINFACTORY_H_

#include "Proposition.h"
#include "TIL.h"
#include "PendingAction.h"

namespace PDDL {
class MMCRDomainFactory {
private:
	MMCRDomainFactory() {
	}
	; //Private constructor
	static std::string getHeader(bool deTILed);
	static std::string getTypes();
	static std::string getPredicates(const std::list<PendingAction> & pendingActions = std::list<PendingAction>(),
			const std::list<TIL> & tils = std::list<TIL>());
	static std::string getFunctions();
	static std::string getLoadAction();
	static std::string getUnloadAction();
	static std::string getMoveAction();
	static std::string getInitialAction();
	static std::string getdeTILedActions(std::list<TIL> tils);
	static std::string getdeTILedAction(const TIL & til,
			std::list<PDDL::Proposition> * tilActionPreconditions);
	static std::string getPendingActions(
			const std::list<PendingAction> & pendingActions);
	static std::string getTerminationString();
public:
	static const std::string TIL_ACHIEVED_PROPOSITION;
	static const std::string REQUIRED_PROPOSITION;
	static const std::string INITIAL_ACTION_REQUIRED_PROPOSITION;
	static const std::string INITIAL_ACTION_COMPLETE_PROPOSITION;
	static std::string getMMCRDomain(
			const std::list<PendingAction> & pendingActions);
	static std::string getDeTILedMMCRDomain(const std::list<TIL> & tils,
			const std::list<PendingAction> & pendingActions);
};
}

#endif /* COLIN_PDDL_MMCRDOMAINFACTORY_H_ */
