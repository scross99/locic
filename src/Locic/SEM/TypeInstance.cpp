#include <cstdio>
#include <string>
#include <Locic/Log.hpp>
#include <Locic/Map.hpp>
#include <Locic/Name.hpp>
#include <Locic/SEM/Namespace.hpp>
#include <Locic/SEM/TypeInstance.hpp>
#include <Locic/SEM/Var.hpp>

namespace Locic {

	namespace SEM {
		
		bool TypeInstance::supportsNullConstruction() const {
			return typeProperties_.nullConstructor != NULL;
		}
		
		SEM::Function* TypeInstance::getNullConstructor() const {
			assert(supportsNullConstruction());
			return typeProperties_.nullConstructor;
		}
		
		void TypeInstance::setNullConstructor(SEM::Function* function) {
			assert(typeProperties_.nullConstructor == NULL);
			typeProperties_.nullConstructor = function;
		}
		
		bool TypeInstance::supportsImplicitCopy() const {
			return typeProperties_.implicitCopy != NULL;
		}
		
		Type* TypeInstance::getImplicitCopyType() const {
			assert(supportsImplicitCopy());
			return typeProperties_.implicitCopy->type()->getFunctionReturnType();
		}
		
		void TypeInstance::setImplicitCopy(SEM::Function* function) {
			assert(typeProperties_.implicitCopy == NULL);
			typeProperties_.implicitCopy = function;
		}
	
		/*bool TypeInstance::supportsNullConstruction() const {
			const std::string functionName = "Null";
			Locic::Optional<Function*> result = functions().tryGet(functionName);
			
			if(!result.hasValue()) return false;
			
			Function* function = result.getValue();
			assert(function != NULL);
			
			// Looking for static method.
			if(function->isMethod()) return false;
			
			Type* type = function->type();
			assert(type->isFunction());
			
			if(type->isFunctionVarArg()) return false;
			
			// One argument for the 'this' pointer.
			if(type->getFunctionParameterTypes().size() != 1) return false;
			
			return type->getFunctionReturnType()->isTypeInstance(this);
		}
		
		bool TypeInstance::supportsImplicitCopy() const {
			const std::string functionName = "implicitCopy";
			Locic::Optional<Function*> result = functions().tryGet(functionName);
			
			if(!result.hasValue()) return false;
			
			Function* function = result.getValue();
			assert(function != NULL);
			
			// Looking for non-static method.
			if(!function->isMethod()) return false;
			
			Type* type = function->type();
			assert(type->isFunction());
			
			if(type->isFunctionVarArg()) return false;
			
			if(type->getFunctionParameterTypes().size() != 0) return false;
			
			return type->getFunctionReturnType()->isTypeInstance(this);
		}
		
		Type* TypeInstance::getFunctionReturnType(const std::string& functionName) {
			Locic::Optional<Function*> result = functions().tryGet(functionName);
			assert(result.hasValue() && "Function must exist to get its return type");
			
			Function* function = result.getValue();
			assert(function != NULL);
			
			Type* type = function->type();
			assert(type->isFunction());
			
			return type->getFunctionReturnType();
		}
		
		Type* TypeInstance::getImplicitCopyType() {
			assert(supportsImplicitCopy());
			return getFunctionReturnType("implicitCopy");
		}*/
		
		std::string TypeInstance::toString() const {
			switch(kind()) {
				case PRIMITIVE:
					return makeString("PrimitiveType(%s)",
							name().c_str());
				case STRUCTDECL:
					return makeString("StructDeclType(%s)",
							name().c_str());
				case STRUCTDEF:
					return makeString("StructDefType(%s)",
							name().c_str());
				case CLASSDECL:
					return makeString("ClassDeclType(%s)",
							name().c_str());
				case CLASSDEF:
					return makeString("ClassDefType(%s)",
							name().c_str());
				case INTERFACE:
					return makeString("InterfaceType(%s)",
							name().c_str());
				default:
					return "[UNKNOWN TYPE INSTANCE]";
			}
		}
		
	}
	
}

