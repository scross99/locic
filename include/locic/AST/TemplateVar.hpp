#ifndef LOCIC_AST_TEMPLATEVAR_HPP
#define LOCIC_AST_TEMPLATEVAR_HPP

#include <string>

#include <locic/AST/Node.hpp>
#include <locic/AST/TypeDecl.hpp>
#include <locic/Support/String.hpp>

namespace locic {

	namespace AST {
	
		class TemplateVar {
		public:
			static TemplateVar*
			NoSpec(Node<TypeDecl> varType, const String& name);
			
			static TemplateVar*
			WithSpec(Node<TypeDecl> varType, const String& name,
			         Node<TypeDecl> specType);
			
			TemplateVar();
			
			String name() const;
			
			Node<TypeDecl>& type();
			const Node<TypeDecl>& type() const;
			
			Node<TypeDecl>& specType();
			const Node<TypeDecl>& specType() const;
			
			size_t index() const;
			void setIndex(size_t index);
			
			std::string toString() const;
			
		private:
			Node<TypeDecl> type_;
			String name_;
			Node<TypeDecl> specType_;
			size_t index_;
			
		};
		
		typedef std::vector<Node<TemplateVar>> TemplateVarList;
		
	}
	
}

#endif