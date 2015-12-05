/*
 * PNE.h
 *
 *  Created on: 2 Jul 2015
 *      Author: tony
 */

#ifndef COLIN_PDDL_PNE_H_
#define COLIN_PDDL_PNE_H_

#include <list>
#include <map>
#include <string>

#include <PDDLObject.h>

namespace PDDL {

class PNE {

private:
	std::string name;
	std::list<std::string> arguments;
	double value;

public:
	PNE(std::string name, std::list<std::string> arguments, double value) : name(name), arguments(arguments), value(value) {};
	const std::string & getName() const { return name; };
	const std::list<std::string> & getArguments() const {return arguments; };
	const double & getValue() const { return value; };
	std::string toActionEffectString(const std::map<const PDDLObject *, std::string> & parameterTable) const;
	friend std::ostream & operator<<(std::ostream & output, const PNE & pne);
	bool operator==(const PNE & other);

};

}



#endif /* COLIN_PDDL_PNE_H_ */
