#include <string>

#include <locic/Support/String.hpp>

#include <locic/AST/AliasDecl.hpp>
#include <locic/AST/Node.hpp>
#include <locic/AST/RequireSpecifier.hpp>
#include <locic/AST/Value.hpp>

namespace locic {
	
	namespace AST {
		
		AliasDecl::AliasDecl(const String& pName, AST::Node<Value> pValue)
		: name_(pName), templateVariables_(makeDefaultNode<TemplateVarList>()),
		requireSpecifier_(makeNode<RequireSpecifier>(Debug::SourceLocation::Null(), RequireSpecifier::None())),
		value_(std::move(pValue)), alias_(nullptr) { }
		
		AliasDecl::~AliasDecl() { }
		
		String AliasDecl::name() const {
			return name_;
		}
		
		const Node<TemplateVarList>& AliasDecl::templateVariables() const {
			return templateVariables_;
		}
		
		const Node<RequireSpecifier>& AliasDecl::requireSpecifier() const {
			return requireSpecifier_;
		}
		
		const AST::Node<AST::Value>& AliasDecl::value() const {
			return value_;
		}
		
		void AliasDecl::setRequireSpecifier(Node<RequireSpecifier> pRequireSpecifier) {
			requireSpecifier_ = std::move(pRequireSpecifier);
		}
		
		void AliasDecl::setTemplateVariables(Node<TemplateVarList> pTemplateVariables) {
			templateVariables_ = std::move(pTemplateVariables);
		}
		
		void AliasDecl::setAlias(SEM::Alias& semAlias) {
			assert(alias_ == nullptr);
			alias_ = &semAlias;
		}
		
		SEM::Alias& AliasDecl::alias() {
			assert(alias_ != nullptr);
			return *alias_;
		}
		
		const SEM::Alias& AliasDecl::alias() const {
			assert(alias_ != nullptr);
			return *alias_;
		}
		
		std::string AliasDecl::toString() const {
			std::string templateVarString = "";
			
			bool isFirst = true;
			
			for (const auto& node : *templateVariables()) {
				if (!isFirst) {
					templateVarString += ", ";
				}
				
				isFirst = false;
				templateVarString += node.toString();
			}
			
			return makeString("Alias(name: %s, templateVariables: (%s), value: %s)",
				name().c_str(), templateVarString.c_str(), value()->toString().c_str());
		}
		
	}
	
}

