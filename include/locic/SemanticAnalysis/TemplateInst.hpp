#ifndef LOCIC_SEMANTICANALYSIS_TEMPLATEINST_HPP
#define LOCIC_SEMANTICANALYSIS_TEMPLATEINST_HPP

#include <utility>

#include <locic/AST/TemplateVarMap.hpp>
#include <locic/Debug/SourceLocation.hpp>
#include <locic/SemanticAnalysis/ScopeStack.hpp>

namespace locic {
	
	namespace AST {
		
		class TemplatedObject;
		
	}
	
	namespace SemanticAnalysis {
		
		class TemplateInst {
		public:
			TemplateInst(ScopeStack argScopeStack,
			             AST::TemplateVarMap argTemplateVarMap,
			             const AST::TemplatedObject& argTemplatedObject,
			             Debug::SourceLocation argLocation)
			: scopeStack_(std::move(argScopeStack)),
			templateVarMap_(std::move(argTemplateVarMap)),
			templatedObject_(&argTemplatedObject),
			location_(std::move(argLocation)) { }
			
			TemplateInst(TemplateInst&&) = default;
			TemplateInst& operator=(TemplateInst&&) = default;
			
			ScopeStack& scopeStack() {
				return scopeStack_;
			}
			
			const AST::TemplateVarMap& templateVarMap() const {
				return templateVarMap_;
			}
			
			const AST::TemplatedObject& templatedObject() const {
				return *templatedObject_;
			}
			
			const Debug::SourceLocation& location() const {
				return location_;
			}
			
		private:
			TemplateInst(const TemplateInst&) = delete;
			TemplateInst& operator=(const TemplateInst&) = delete;
			
			ScopeStack scopeStack_;
			AST::TemplateVarMap templateVarMap_;
			const AST::TemplatedObject* templatedObject_;
			Debug::SourceLocation location_;
			
		};
		
	}
	
}

#endif
