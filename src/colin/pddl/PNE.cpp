/*
 * PNE.cpp
 *
 *  Created on: 8 Jul 2015
 *      Author: tony
 */

#include <iostream>
#include <sstream>

#include "PNE.h"

using namespace std;
namespace PDDL {

std::string PNE::toActionEffectString() const {
	ostringstream output;
	output << "(assign (" << name;
	std::list<std::string>::const_iterator argItr = arguments.begin();
	const std::list<std::string>::const_iterator argItrEnd = arguments.end();
	for (; argItr != argItrEnd; argItr++) {
		output << " " << (*argItr);
	}
	output << ") " << value << ")";
	return output.str();
}

std::ostream & operator<<(std::ostream & output, const PNE & pne) {
	output << "(= (" << pne.name << " ";
	std::list<std::string>::const_iterator argItr = pne.arguments.begin();
	const std::list<std::string>::const_iterator argItrEnd =
			pne.arguments.end();
	for (; argItr != argItrEnd; argItr++) {
		output << (*argItr) << " ";
	}
	output << ") " << pne.value << ")";
	return output;
}

}

