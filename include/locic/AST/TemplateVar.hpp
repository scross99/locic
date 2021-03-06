#ifndef LOCIC_AST_TEMPLATEVAR_HPP
#define LOCIC_AST_TEMPLATEVAR_HPP

#include <string>

#include <locic/AST/Node.hpp>
#include <locic/AST/TypeDecl.hpp>
#include <locic/Debug/TemplateVarInfo.hpp>
#include <locic/Support/Optional.hpp>
#include <locic/Support/String.hpp>

namespace locic {
	
	namespace AST {
		
		class Context;
		class Type;
		class Value;
		
		class TemplateVar {
		public:
			static TemplateVar*
			NoSpec(Node<TypeDecl> varType, const String& name);
			
			static TemplateVar*
			WithSpec(Node<TypeDecl> varType, const String& name,
			         Node<TypeDecl> specType);
			
			TemplateVar();
			
			Context& context() const;
			void setContext(Context& context);
			
			String name() const;
			
			const Name& fullName() const;
			void setFullName(Name fullName);
			
			size_t index() const;
			void setIndex(size_t index);
			
			Node<TypeDecl>& typeDecl();
			const Node<TypeDecl>& typeDecl() const;
			
			Node<TypeDecl>& specType();
			const Node<TypeDecl>& specType() const;
			
			const Type* type() const;
			void setType(const Type* type);
			
			Value selfRefValue() const;
			const Type* selfRefType() const;
			
			void setDebugInfo(Debug::TemplateVarInfo debugInfo);
			Optional<Debug::TemplateVarInfo> debugInfo() const;
			
			std::string toString() const;
			
		private:
			Context* context_;
			Node<TypeDecl> typeDecl_;
			const Type* type_;
			const Type* selfRefType_;
			String name_;
			Name fullName_;
			size_t index_;
			Node<TypeDecl> specType_;
			Optional<Debug::TemplateVarInfo> debugInfo_;
			
		};
		
		typedef std::vector<Node<TemplateVar>> TemplateVarList;
		
	}
	
}

#endif
