#ifndef LOCIC_PARSER_STATEMENTBUILDER_HPP
#define LOCIC_PARSER_STATEMENTBUILDER_HPP

#include <locic/AST.hpp>

namespace locic {
	
	class Constant;
	class String;
	
	namespace Debug {
		
		class SourcePosition;
		
	}
	
	namespace Parser {
		
		class TokenReader;
		
		class StatementBuilder {
		public:
			StatementBuilder(const TokenReader& reader);
			~StatementBuilder();
			
			AST::Node<AST::Statement>
			makeScopeStatement(AST::Node<AST::Scope> scope,
			                   const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeIfStatement(AST::IfClauseList ifClauseList,
			                const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeIfElseStatement(AST::IfClauseList ifClauseList,
			                    AST::Node<AST::Scope> elseClause,
			                    const Debug::SourcePosition& start);
			
			AST::Node<AST::IfClause>
			makeIfClause(AST::Node<AST::Value> value,
			             AST::Node<AST::Scope> scope,
			             const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeSwitchStatement(AST::Node<AST::Value> value,
			                    AST::Node<AST::SwitchCaseList> switchCaseList,
			                    AST::Node<AST::DefaultCase> defaultCase,
			                    const Debug::SourcePosition& start);
			
			AST::Node<AST::SwitchCaseList>
			makeSwitchCaseList(AST::SwitchCaseList switchCaseList,
			                   const Debug::SourcePosition& start);
			
			AST::Node<AST::SwitchCase>
			makeSwitchCase(AST::Node<AST::TypeVar> var,
			               AST::Node<AST::Scope> scope,
			               const Debug::SourcePosition& start);
			
			AST::Node<AST::DefaultCase>
			makeEmptyDefaultSwitchCase(const Debug::SourcePosition& start);
			
			AST::Node<AST::DefaultCase>
			makeDefaultSwitchCase(AST::Node<AST::Scope> scope,
			                      const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeWhileStatement(AST::Node<AST::Value> condition,
			                   AST::Node<AST::Scope> scope,
			                   const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeForStatement(AST::Node<AST::TypeVar> var,
			                 AST::Node<AST::Value> value,
			                 AST::Node<AST::Scope> scope,
			                 const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeAssertNoexceptStatement(AST::Node<AST::Scope> scope,
			                            const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeAssertStatement(AST::Node<AST::Value> value,
			                    const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeVarDeclStatement(AST::Node<AST::TypeVar> var,
			                     AST::Node<AST::Value> value,
			                     const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeIncrementStatement(AST::Node<AST::Value> value,
			                       const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeDecrementStatement(AST::Node<AST::Value> value,
			                       const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeValueStatement(AST::Node<AST::Value> value,
			                       const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeUnusedResultValueStatement(AST::Node<AST::Value> value,
			                               const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeReturnStatement(AST::Node<AST::Value> value,
			                    const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeBreakStatement(const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeContinueStatement(const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeUnreachableStatement(const Debug::SourcePosition& start);
			
			AST::Node<AST::Statement>
			makeAssignStatement(AST::Node<AST::Value> lvalue,
			                    AST::Node<AST::Value> rvalue,
			                    AST::AssignKind kind,
			                    const Debug::SourcePosition& start);
			
		private:
			TokenReader& reader_;
			
		};
		
	}
	
}

#endif