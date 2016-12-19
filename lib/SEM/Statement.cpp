#include <assert.h>

#include <string>
#include <vector>

#include <locic/AST/Type.hpp>
#include <locic/AST/ValueDecl.hpp>
#include <locic/AST/Var.hpp>

#include <locic/Support/ErrorHandling.hpp>
#include <locic/Support/String.hpp>

#include <locic/SEM/CatchClause.hpp>
#include <locic/SEM/IfClause.hpp>
#include <locic/SEM/Scope.hpp>
#include <locic/SEM/Statement.hpp>
#include <locic/SEM/SwitchCase.hpp>
#include <locic/AST/Value.hpp>

namespace locic {

	namespace SEM {
	
		Statement Statement::ValueStmt(AST::Value value) {
			Statement statement(VALUE, value.exitStates());
			statement.valueStmt_.value = std::move(value);
			return statement;
		}
		
		Statement Statement::ScopeStmt(std::unique_ptr<Scope> scope) {
			Statement statement(SCOPE, scope->exitStates());
			statement.scopeStmt_.scope = std::move(scope);
			return statement;
		}
		
		Statement Statement::InitialiseStmt(AST::Var& var, AST::Value value) {
			Statement statement(INITIALISE, value.exitStates());
			statement.initialiseStmt_.var = &var;
			statement.initialiseStmt_.value = std::move(value);
			return statement;
		}
		
		Statement Statement::If(const std::vector<IfClause*>& ifClauses, std::unique_ptr<Scope> elseScope) {
			assert(elseScope != nullptr);
			
			AST::ExitStates exitStates = AST::ExitStates::None();
			
			for (const auto& ifClause: ifClauses) {
				const auto conditionExitStates = ifClause->condition().exitStates();
				assert(conditionExitStates.onlyHasNormalOrThrowingStates());
				exitStates.add(conditionExitStates.throwingStates());
				exitStates.add(ifClause->scope().exitStates());
			}
			
			exitStates.add(elseScope->exitStates());
			
			Statement statement(IF, exitStates);
			statement.ifStmt_.clauseList = ifClauses;
			statement.ifStmt_.elseScope = std::move(elseScope);
			return statement;
		}
		
		Statement Statement::Switch(AST::Value value, const std::vector<SwitchCase*>& caseList, std::unique_ptr<Scope> defaultScope) {
			AST::ExitStates exitStates = AST::ExitStates::None();
			exitStates.add(value.exitStates().throwingStates());
			
			for (const auto& switchCase: caseList) {
				exitStates.add(switchCase->scope().exitStates());
			}
			
			if (defaultScope.get() != nullptr) {
				exitStates.add(defaultScope->exitStates());
			}
			
			Statement statement(SWITCH, exitStates);
			statement.switchStmt_.value = std::move(value);
			statement.switchStmt_.caseList = caseList;
			statement.switchStmt_.defaultScope = std::move(defaultScope);
			return statement;
		}
		
		Statement Statement::Loop(AST::Value condition, std::unique_ptr<Scope> iterationScope, std::unique_ptr<Scope> advanceScope) {
			// If the loop condition can be exited normally then the loop
			// can be exited normally (i.e. because the condition can be false).
			AST::ExitStates exitStates = condition.exitStates();
			
			auto iterationScopeExitStates = iterationScope->exitStates();
			
			// Block any 'continue' exit state.
			iterationScopeExitStates.remove(AST::ExitStates::Continue());
			
			// A 'break' exit state means a normal return from the loop.
			if (iterationScopeExitStates.hasBreakExit()) {
				exitStates.add(AST::ExitStates::Normal());
				iterationScopeExitStates.remove(AST::ExitStates::Break());
			}
			
			exitStates.add(iterationScopeExitStates);
			
			auto advanceScopeExitStates = advanceScope->exitStates();
			assert(!advanceScopeExitStates.hasBreakExit() && !advanceScopeExitStates.hasContinueExit());
			
			// Block 'normal' exit from advance scope, since this just
			// goes back to the beginning of the loop.
			advanceScopeExitStates.remove(AST::ExitStates::Normal());
			
			exitStates.add(advanceScopeExitStates);
			
			Statement statement(LOOP, exitStates);
			statement.loopStmt_.condition = std::move(condition);
			statement.loopStmt_.iterationScope = std::move(iterationScope);
			statement.loopStmt_.advanceScope = std::move(advanceScope);
			return statement;
		}
		
		Statement Statement::For(AST::Var& var, AST::Value initValue,
		                         std::unique_ptr<Scope> scope) {
			// TODO: get exit states of skip_front() method.
			auto exitStates = initValue.exitStates();
			assert(!exitStates.hasBreakExit() && !exitStates.hasContinueExit() &&
			       !exitStates.hasReturnExit());
			
			// Normal exit from the init value means executing the loop.
			exitStates.remove(AST::ExitStates::Normal());
			
			auto scopeExitStates = scope->exitStates();
			
			// Block any 'continue' exit state.
			scopeExitStates.remove(AST::ExitStates::Continue());
			
			// A 'break' exit state means a normal return from the loop.
			if (scopeExitStates.hasBreakExit()) {
				exitStates.add(AST::ExitStates::Normal());
				scopeExitStates.remove(AST::ExitStates::Break());
			}
			
			exitStates.add(scopeExitStates);
			
			Statement statement(FOR, exitStates);
			statement.forStmt_.var = &var;
			statement.forStmt_.initValue = std::move(initValue);
			statement.forStmt_.scope = std::move(scope);
			return statement;
		}
		
		Statement Statement::Try(std::unique_ptr<Scope> scope, const std::vector<CatchClause*>& catchList) {
			AST::ExitStates exitStates = AST::ExitStates::None();
			
			exitStates.add(scope->exitStates());
			
			for (const auto& catchClause: catchList) {
				auto catchExitStates = catchClause->scope().exitStates();
				
				// Turn 'rethrow' into 'throw'.
				if (catchExitStates.hasRethrowExit()) {
					exitStates.add(AST::ExitStates::ThrowAlways());
					catchExitStates.remove(AST::ExitStates::Rethrow());
				}
				
				exitStates.add(catchExitStates);
			}
			
			Statement statement(TRY, exitStates);
			statement.tryStmt_.scope = std::move(scope);
			statement.tryStmt_.catchList = catchList;
			return statement;
		}
		
		Statement Statement::ScopeExit(const String& state, std::unique_ptr<Scope> scope) {
			// The exit actions here is for when we first visit this statement,
			// which itself actually has no effect; the effect occurs on unwinding
			// and so this is handled by the owning scope.
			Statement statement(SCOPEEXIT, AST::ExitStates::Normal());
			statement.scopeExitStmt_.state = state;
			statement.scopeExitStmt_.scope = std::move(scope);
			return statement;
		}
		
		Statement Statement::ReturnVoid() {
			return Statement(RETURNVOID, AST::ExitStates::Return());
		}
		
		Statement Statement::Return(AST::Value value) {
			assert(value.exitStates().onlyHasNormalOrThrowingStates());
			
			AST::ExitStates exitStates = AST::ExitStates::Return();
			exitStates.add(value.exitStates().throwingStates());
			
			Statement statement(RETURN, exitStates);
			statement.returnStmt_.value = std::move(value);
			return statement;
		}
		
		Statement Statement::Throw(AST::Value value) {
			Statement statement(THROW, AST::ExitStates::ThrowAlways());
			statement.throwStmt_.value = std::move(value);
			return statement;
		}
		
		Statement Statement::Rethrow() {
			return Statement(RETHROW, AST::ExitStates::Rethrow());
		}
		
		Statement Statement::Break() {
			return Statement(BREAK, AST::ExitStates::Break());
		}
		
		Statement Statement::Continue() {
			return Statement(CONTINUE, AST::ExitStates::Continue());
		}
		
		Statement Statement::Assert(AST::Value value, const String& name) {
			Statement statement(ASSERT, value.exitStates());
			statement.assertStmt_.value = std::move(value);
			statement.assertStmt_.name = name;
			return statement;
		}
		
		Statement Statement::AssertNoExcept(std::unique_ptr<Scope> scope) {
			AST::ExitStates exitStates = scope->exitStates();
			exitStates.remove(AST::ExitStates::AllThrowing());
			
			Statement statement(ASSERTNOEXCEPT, exitStates);
			statement.assertNoExceptStmt_.scope = std::move(scope);
			return statement;
		}
		
		Statement Statement::Unreachable() {
			return Statement(UNREACHABLE, AST::ExitStates::None());
		}
		
		Statement::Statement(const Kind argKind, const AST::ExitStates argExitStates)
		: kind_(argKind), exitStates_(argExitStates) { }
			
		Statement::Kind Statement::kind() const {
			return kind_;
		}
		
		AST::ExitStates Statement::exitStates() const {
			return exitStates_;
		}
		
		bool Statement::isValueStatement() const {
			return kind() == VALUE;
		}
		
		const AST::Value& Statement::getValue() const {
			assert(isValueStatement());
			return valueStmt_.value;
		}
		
		bool Statement::isScope() const {
			return kind() == SCOPE;
		}
		
		Scope& Statement::getScope() const {
			assert(isScope());
			return *(scopeStmt_.scope);
		}
		
		bool Statement::isInitialiseStatement() const {
			return kind() == INITIALISE;
		}
		
		AST::Var& Statement::getInitialiseVar() const {
			assert(isInitialiseStatement());
			return *(initialiseStmt_.var);
		}
		
		const AST::Value& Statement::getInitialiseValue() const {
			assert(isInitialiseStatement());
			return initialiseStmt_.value;
		}
		
		bool Statement::isIfStatement() const {
			return kind() == IF;
		}
		
		const std::vector<IfClause*>& Statement::getIfClauseList() const {
			assert(isIfStatement());
			return ifStmt_.clauseList;
		}
		
		Scope& Statement::getIfElseScope() const {
			assert(isIfStatement());
			return *(ifStmt_.elseScope);
		}
		
		bool Statement::isSwitchStatement() const {
			return kind() == SWITCH;
		}
		
		const AST::Value& Statement::getSwitchValue() const {
			assert(isSwitchStatement());
			return switchStmt_.value;
		}
		
		const std::vector<SwitchCase*>& Statement::getSwitchCaseList() const {
			assert(isSwitchStatement());
			return switchStmt_.caseList;
		}
		
		Scope* Statement::getSwitchDefaultScope() const {
			assert(isSwitchStatement());
			return switchStmt_.defaultScope.get();
		}
		
		bool Statement::isLoopStatement() const {
			return kind() == LOOP;
		}
		
		const AST::Value& Statement::getLoopCondition() const {
			assert(isLoopStatement());
			return loopStmt_.condition;
		}
		
		Scope& Statement::getLoopIterationScope() const {
			assert(isLoopStatement());
			return *(loopStmt_.iterationScope);
		}
		
		Scope& Statement::getLoopAdvanceScope() const {
			assert(isLoopStatement());
			return *(loopStmt_.advanceScope);
		}
		
		bool Statement::isFor() const {
			return kind() == FOR;
		}
		
		AST::Var& Statement::getForVar() const {
			assert(isFor());
			return *(forStmt_.var);
		}
		
		const AST::Value& Statement::getForInitValue() const {
			assert(isFor());
			return forStmt_.initValue;
		}
		
		Scope& Statement::getForScope() const {
			assert(isFor());
			return *(forStmt_.scope);
		}
		
		bool Statement::isTryStatement() const {
			return kind() == TRY;
		}
		
		Scope& Statement::getTryScope() const {
			assert(isTryStatement());
			return *(tryStmt_.scope);
		}
		
		const std::vector<CatchClause*>& Statement::getTryCatchList() const {
			assert(isTryStatement());
			return tryStmt_.catchList;
		}
		
		bool Statement::isScopeExitStatement() const {
			return kind() == SCOPEEXIT;
		}
		
		const String& Statement::getScopeExitState() const {
			assert(isScopeExitStatement());
			return scopeExitStmt_.state;
		}
		
		Scope& Statement::getScopeExitScope() const {
			assert(isScopeExitStatement());
			return *(scopeExitStmt_.scope);
		}
		
		bool Statement::isReturnStatement() const {
			return kind() == RETURN;
		}
		
		const AST::Value& Statement::getReturnValue() const {
			assert(isReturnStatement());
			return returnStmt_.value;
		}
		
		bool Statement::isThrowStatement() const {
			return kind() == THROW;
		}
		
		const AST::Value& Statement::getThrowValue() const {
			assert(isThrowStatement());
			return throwStmt_.value;
		}
		
		bool Statement::isRethrowStatement() const {
			return kind() == RETHROW;
		}
		
		bool Statement::isBreakStatement() const {
			return kind() == BREAK;
		}
		
		bool Statement::isContinueStatement() const {
			return kind() == CONTINUE;
		}
		
		bool Statement::isAssertStatement() const {
			return kind() == ASSERT;
		}
		
		const AST::Value& Statement::getAssertValue() const {
			assert(isAssertStatement());
			return assertStmt_.value;
		}
		
		const String& Statement::getAssertName() const {
			assert(isAssertStatement());
			return assertStmt_.name;
		}
		
		bool Statement::isAssertNoExceptStatement() const {
			return kind() == ASSERTNOEXCEPT;
		}
		
		const Scope& Statement::getAssertNoExceptScope() const {
			assert(isAssertNoExceptStatement());
			return *(assertNoExceptStmt_.scope);
		}
		
		bool Statement::isUnreachableStatement() const {
			return kind() == UNREACHABLE;
		}
		
		void Statement::setDebugInfo(Debug::StatementInfo newDebugInfo) {
			debugInfo_ = make_optional(newDebugInfo);
		}
		
		Optional<Debug::StatementInfo> Statement::debugInfo() const {
			return debugInfo_;
		}
		
		std::string Statement::toString() const {
			switch (kind_) {
				case VALUE: {
					return makeString("ValueStatement(value: %s)",
						valueStmt_.value.toString().c_str());
				}
				
				case SCOPE: {
					return makeString("ScopeStatement(scope: %s)",
						scopeStmt_.scope->toString().c_str());
				}
				
				case INITIALISE: {
					return makeString("InitialiseStatement(var: %s, value: %s)",
						initialiseStmt_.var->toString().c_str(),
						initialiseStmt_.value.toString().c_str());
				}
				
				case IF: {
					return makeString("IfStatement(clauseList: %s, elseScope: %s)",
						makeArrayPtrString(ifStmt_.clauseList).c_str(),
						ifStmt_.elseScope->toString().c_str());
				}
				
				case SWITCH: {
					return makeString("SwitchStatement(value: %s, caseList: %s, defaultScope: %s)",
						switchStmt_.value.toString().c_str(),
						makeArrayPtrString(switchStmt_.caseList).c_str(),
						switchStmt_.defaultScope != nullptr ?
							switchStmt_.defaultScope->toString().c_str() :
							"[NONE]");
				}
				
				case LOOP: {
					return makeString("LoopStatement(condition: %s, iteration: %s, advance: %s)",
						loopStmt_.condition.toString().c_str(),
						loopStmt_.iterationScope->toString().c_str(),
						loopStmt_.advanceScope->toString().c_str());
				}
				
				case FOR: {
					return makeString("ForStatement(var: %s, initValue: %s, scope: %s)",
						getForVar().toString().c_str(),
						getForInitValue().toString().c_str(),
						getForScope().toString().c_str());
				}
				
				case TRY: {
					return makeString("TryStatement(scope: %s, catchList: %s)",
						tryStmt_.scope->toString().c_str(),
						makeArrayPtrString(tryStmt_.catchList).c_str());
				}
				
				case SCOPEEXIT: {
					return makeString("ScopeExitStatement(state: %s, scope: %s)",
						getScopeExitState().c_str(),
						getScopeExitScope().toString().c_str());
				}
				
				case RETURN: {
					return makeString("ReturnStatement(value: %s)",
						returnStmt_.value.toString().c_str());
				}
				
				case RETURNVOID: {
					return "ReturnVoidStatement";
				}
				
				case THROW: {
					return makeString("ThrowStatement(value: %s)",
						throwStmt_.value.toString().c_str());
				}
				
				case RETHROW: {
					return "RethrowStatement";
				}
				
				case BREAK: {
					return "BreakStatement";
				}
				
				case CONTINUE: {
					return "ContinueStatement";
				}
				
				case ASSERT: {
					return makeString("AssertStatement(value: %s, name: %s)",
						getAssertValue().toString().c_str(),
						getAssertName().c_str());
				}
				
				case ASSERTNOEXCEPT: {
					return makeString("AssertNoExceptStatement(scope: %s)",
						getAssertNoExceptScope().toString().c_str());
				}
				
				case UNREACHABLE: {
					return "UnreachableStatement";
				}
			}
			
			locic_unreachable("Unknown SEM::Statement kind.");
		}
		
	}
	
}

