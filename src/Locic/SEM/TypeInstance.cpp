#include <cstdio>
#include <string>
#include <Locic/Log.hpp>
#include <Locic/Map.hpp>
#include <Locic/Name.hpp>
#include <Locic/String.hpp>
#include <Locic/SEM/Namespace.hpp>
#include <Locic/SEM/TemplateVar.hpp>
#include <Locic/SEM/TypeInstance.hpp>
#include <Locic/SEM/Var.hpp>

namespace Locic {

	namespace SEM {
		
		std::string TypeInstance::refToString() const {
			switch(kind()) {
				case PRIMITIVE:
					return makeString("PrimitiveType(name: %s)",
							name().toString().c_str());
				case STRUCTDECL:
					return makeString("StructDeclType(name: %s)",
							name().toString().c_str());
				case STRUCTDEF:
					return makeString("StructDefType(name: %s)",
							name().toString().c_str());
				case CLASSDECL:
					return makeString("ClassDeclType(name: %s)",
							name().toString().c_str());
				case CLASSDEF:
					return makeString("ClassDefType(name: %s)",
							name().toString().c_str());
				case INTERFACE:
					return makeString("InterfaceType(name: %s)",
							name().toString().c_str());
				case TEMPLATETYPE:
					return makeString("TemplateType(name: %s)",
							name().toString().c_str());
				default:
					return "[UNKNOWN TYPE INSTANCE]";
			}
		}
		
		std::string TypeInstance::toString() const {
			return makeString("TypeInstance(ref: %s, "
				"templateVariables: %s, variables: %s, "
				"functions: %s)",
				refToString().c_str(),
				makeArrayString(templateVariables_).c_str(),
				makeArrayString(variables_).c_str(),
				makeArrayString(functions_).c_str()); 
		}
		
	}
	
}

