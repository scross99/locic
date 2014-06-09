#include <assert.h>

#include <set>
#include <stdexcept>
#include <string>

#include <locic/AST.hpp>
#include <locic/SEM.hpp>
#include <locic/SemanticAnalysis/CanCast.hpp>
#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/ConvertForLoop.hpp>
#include <locic/SemanticAnalysis/ConvertScope.hpp>
#include <locic/SemanticAnalysis/ConvertType.hpp>
#include <locic/SemanticAnalysis/ConvertValue.hpp>
#include <locic/SemanticAnalysis/ConvertVar.hpp>

namespace locic {

	namespace SemanticAnalysis {
	
		bool WillStatementReturn(SEM::Statement* statement) {
			switch(statement->kind()) {
				case SEM::Statement::VALUE: {
					return false;
				}
				case SEM::Statement::SCOPE: {
					return WillScopeReturn(statement->getScope());
				}
				case SEM::Statement::INITIALISE: {
					return false;
				}
				case SEM::Statement::IF: {
					for (const auto ifClause: statement->getIfClauseList()) {
						if (!WillScopeReturn(ifClause->scope())) {
							return false;
						}
					}
					return WillScopeReturn(statement->getIfElseScope());
				}
				case SEM::Statement::SWITCH: {
					for (auto switchCase: statement->getSwitchCaseList()) {
						if (!WillScopeReturn(switchCase->scope())) {
							return false;
						}
					}
					return true;
				}
				case SEM::Statement::LOOP: {
					return WillScopeReturn(statement->getLoopIterationScope());
				}
				case SEM::Statement::TRY: {
					// TODO: also consider catch blocks?
					return WillScopeReturn(statement->getTryScope());
				}
				case SEM::Statement::SCOPEEXIT: {
					return false;
				}
				case SEM::Statement::RETURN: {
					return true;
				}
				case SEM::Statement::THROW: {
					// TODO: doesn't seem correct...
					return true;
				}
				case SEM::Statement::BREAK: {
					// TODO: doesn't seem correct...
					return true;
				}
				case SEM::Statement::CONTINUE: {
					return false;
				}
				default: {
					throw std::runtime_error("Unknown statement kind.");
				}
			}
		}
		
		bool CanStatementThrow(SEM::Statement* statement) {
			switch(statement->kind()) {
				case SEM::Statement::VALUE: {
					return CanValueThrow(statement->getValue());
				}
				case SEM::Statement::SCOPE: {
					return CanScopeThrow(statement->getScope());
				}
				case SEM::Statement::INITIALISE: {
					return false;
				}
				case SEM::Statement::IF: {
					for (const auto ifClause: statement->getIfClauseList()) {
						if (CanValueThrow(ifClause->condition()) || CanScopeThrow(ifClause->scope())) {
							return true;
						}
					}
					return CanScopeThrow(statement->getIfElseScope());
				}
				case SEM::Statement::SWITCH: {
					for (auto switchCase: statement->getSwitchCaseList()) {
						if (CanScopeThrow(switchCase->scope())) {
							return true;
						}
					}
					return CanValueThrow(statement->getSwitchValue());
				}
				case SEM::Statement::LOOP: {
					return CanValueThrow(statement->getLoopCondition()) ||
						CanScopeThrow(statement->getLoopIterationScope()) ||
						CanScopeThrow(statement->getLoopAdvanceScope());
				}
				case SEM::Statement::TRY: {
					for (auto catchClause: statement->getTryCatchList()) {
						if (CanScopeThrow(catchClause->scope())) {
							return true;
						}
					}
					return CanScopeThrow(statement->getTryScope());
				}
				case SEM::Statement::SCOPEEXIT: {
					// scope(success) is allowed to throw.
					return statement->getScopeExitState() == "success" && CanScopeThrow(statement->getScopeExitScope());
				}
				case SEM::Statement::RETURN: {
					return false;
				}
				case SEM::Statement::THROW: {
					return true;
				}
				case SEM::Statement::BREAK: {
					return false;
				}
				case SEM::Statement::CONTINUE: {
					return false;
				}
				default: {
					throw std::runtime_error("Unknown statement kind.");
				}
			}
		}
		
		SEM::Statement* ConvertStatementData(Context& context, const AST::Node<AST::Statement>& statement) {
			const auto& location = statement.location();
			
			switch (statement->typeEnum) {
				case AST::Statement::VALUE: {
					const auto value = ConvertValue(context, statement->valueStmt.value);
					if (statement->valueStmt.hasVoidCast) {
						if (value->type()->isBuiltInVoid()) {
							throw ErrorException(makeString("Void explicitly ignored in expression '%s' at position %s.",
								value->toString().c_str(), location.toString().c_str()));
						}
						const auto voidType = getBuiltInType(context.scopeStack(), "void_t")->selfType();
						return SEM::Statement::ValueStmt(SEM::Value::Cast(voidType, value));
					} else {
						if (!value->type()->isBuiltInVoid()) {
							throw ErrorException(makeString("Non-void value result ignored in expression '%s' at position %s.",
								value->toString().c_str(), location.toString().c_str()));
						}
						return SEM::Statement::ValueStmt(value);
					}
				}
				case AST::Statement::SCOPE: {
					return SEM::Statement::ScopeStmt(ConvertScope(context, statement->scopeStmt.scope));
				}
				case AST::Statement::IF: {
					const auto boolType = getBuiltInType(context.scopeStack(), "bool");
					
					std::vector<SEM::IfClause*> clauseList;
					for (const auto& astIfClause: *(statement->ifStmt.clauseList)) {
						const auto condition = ConvertValue(context, astIfClause->condition);
						const auto boolValue = ImplicitCast(condition, boolType->selfType(), location);
						const auto ifTrueScope = ConvertScope(context, astIfClause->scope);
						clauseList.push_back(new SEM::IfClause(boolValue, ifTrueScope));
					}
					
					const auto elseScope = ConvertScope(context, statement->ifStmt.elseScope);
					
					return SEM::Statement::If(clauseList, elseScope);
				}
				case AST::Statement::SWITCH: {
					const auto value = ConvertValue(context, statement->switchStmt.value);
					
					std::set<SEM::TypeInstance*> switchCaseTypeInstances;
					
					std::vector<SEM::SwitchCase*> caseList;
					for (const auto& astCase: *(statement->switchStmt.caseList)) {
						const auto semCase = new SEM::SwitchCase();
						
						{
							PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::SwitchCase(semCase));
							
							const bool isMember = false;
							semCase->setVar(ConvertVar(context, isMember, astCase->var));
							semCase->setScope(ConvertScope(context, astCase->scope));
						}
						
						caseList.push_back(semCase);
						
						const auto insertResult = switchCaseTypeInstances.insert(semCase->var()->constructType()->getObjectType());
						
						// Check for duplicate cases.
						if (!insertResult.second) {
							throw ErrorException(makeString("Duplicate switch case for type '%s' at position %s.",
								(*(insertResult.first))->refToString().c_str(),
								location.toString().c_str()));
						}
					}
					
					if (caseList.empty()) {
						throw ErrorException(makeString("Switch statement must contain at least one case at position %s.",
							location.toString().c_str()));
					}
					
					// Check that all switch cases are based
					// on the same union datatype.
					const auto switchTypeInstance = (*(switchCaseTypeInstances.begin()))->parent();
					for (auto caseTypeInstance: switchCaseTypeInstances) {
						auto caseTypeInstanceParent = caseTypeInstance->parent();
						
						if (caseTypeInstanceParent == nullptr) {
							throw ErrorException(makeString("Switch case type '%s' is not a member of a union datatype at position %s.",
								caseTypeInstance->refToString().c_str(),
								location.toString().c_str()));
						}
						
						if (caseTypeInstanceParent != switchTypeInstance) {
							throw ErrorException(makeString("Switch case type '%s' does not share the same parent as type '%s' at position %s.",
								caseTypeInstance->refToString().c_str(),
								(*(switchCaseTypeInstances.begin()))->refToString().c_str(),
								location.toString().c_str()));
						}
					}
					
					// TODO: implement 'default' case.
					const bool hasDefaultCase = false;
					
					if (!hasDefaultCase) {
						// Check all cases are handled.
						for (auto variantTypeInstance: switchTypeInstance->variants()) {
							if (switchCaseTypeInstances.find(variantTypeInstance) == switchCaseTypeInstances.end()) {
								throw ErrorException(makeString("Union datatype member '%s' not handled in switch at position %s.",
									variantTypeInstance->refToString().c_str(), location.toString().c_str()));
							}
						}
					}
					
					// Case value to switch type.
					// TODO: fix the template arguments for the switch type.
					const auto castValue = ImplicitCast(value, switchTypeInstance->selfType(), location);
					
					return SEM::Statement::Switch(castValue, caseList);
				}
				case AST::Statement::WHILE: {
					const auto condition = ConvertValue(context, statement->whileStmt.condition);
					
					PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Loop());
					
					const auto iterationScope = ConvertScope(context, statement->whileStmt.whileTrue);
					const auto advanceScope = new SEM::Scope();
					const auto loopCondition = ImplicitCast(condition, getBuiltInType(context.scopeStack(), "bool")->selfType(), location);
					return SEM::Statement::Loop(loopCondition, iterationScope, advanceScope);
				}
				case AST::Statement::FOR: {
					const auto& forStmt = statement->forStmt;
					const auto loopScope = ConvertForLoop(context, forStmt.typeVar, forStmt.initValue, forStmt.scope);
					return SEM::Statement::ScopeStmt(loopScope);
				}
				case AST::Statement::TRY: {
					SEM::Scope* tryScope = nullptr;
					
					{
						PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TryScope());
						tryScope = ConvertScope(context, statement->tryStmt.scope);
					}
					
					std::vector<SEM::CatchClause*> catchList;
					
					for (const auto& astCatch: *(statement->tryStmt.catchList)) {
						const auto semCatch = new SEM::CatchClause();
						
						PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::CatchClause(semCatch));
						
						const auto& astVar = astCatch->var;
						
						if (astVar->kind != AST::TypeVar::NAMEDVAR) {
							throw ErrorException(makeString("Try statement catch clauses may only "
								"contain named variables (no pattern matching) at position %s.",
								location.toString().c_str()));
						}
						
						// Special case handling for catch variables,
						// since they don't use lvalues.
						const auto varType = ConvertType(context, astVar->namedVar.type);
						if (!varType->isException()) {
							throw ErrorException(makeString("Type '%s' is not an exception type and therefore "
								"cannot be used in a catch clause at position %s.",
								varType->toString().c_str(), location.toString().c_str()));
						}
						
						const auto semVar = SEM::Var::Basic(varType, varType);
						attachVar(context, astVar->namedVar.name, astVar, semVar);
						
						semCatch->setVar(semVar);
						semCatch->setScope(ConvertScope(context, astCatch->scope));
						
						catchList.push_back(semCatch);
					}
					
					return SEM::Statement::Try(tryScope, catchList);
				}
				case AST::Statement::SCOPEEXIT: {
					const auto& scopeExitState = statement->scopeExitStmt.state;
					if (scopeExitState != "exit" && scopeExitState != "success" && scopeExitState != "failure") {
						throw ErrorException(makeString("Unknown scope-exit state '%s' at position %s.",
								scopeExitState.c_str(), location.toString().c_str()));
					}
					
					PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::ScopeAction(scopeExitState));
					
					const auto scopeExitScope = ConvertScope(context, statement->scopeExitStmt.scope);
					
					// scope(success) is allowed to throw.
					if (scopeExitState != "success" && CanScopeThrow(*scopeExitScope)) {
						throw ErrorException(makeString("Scope exit action (for state '%s') can throw, at position %s.",
								scopeExitState.c_str(), location.toString().c_str()));
					}
					
					return SEM::Statement::ScopeExit(scopeExitState, scopeExitScope);
				}
				case AST::Statement::VARDECL: {
					const auto& astTypeVarNode = statement->varDecl.typeVar;
					const auto& astInitialValueNode = statement->varDecl.value;
					
					const auto semValue = ConvertValue(context, astInitialValueNode);
					
					// Convert the AST type var.
					const bool isMemberVar = false;
					const auto semVar = ConvertInitialisedVar(context, isMemberVar, astTypeVarNode, semValue->type());
					assert(!semVar->isAny());
					
					// Cast the initialise value to the variable's type.
					// (The variable conversion above should have ensured
					// this will work.)
					const auto semInitialiseValue = ImplicitCast(semValue, semVar->constructType(), location);
					assert(!semInitialiseValue->type()->isBuiltInVoid());
					
					// Add the variable to the SEM scope.
					const auto semScope = context.scopeStack().back().scope();
					semScope->variables().push_back(semVar);
					
					// Generate the initialise statement.
					return SEM::Statement::InitialiseStmt(semVar, semInitialiseValue);
				}
				case AST::Statement::RETURNVOID: {
					// Void return statement (i.e. return;)
					if (!getParentFunctionReturnType(context.scopeStack())->isBuiltInVoid()) {
						throw ErrorException(makeString("Cannot return void in function '%s' with non-void return type at position %s.",
							lookupParentFunction(context.scopeStack())->name().toString().c_str(),
							location.toString().c_str()));
					}
					
					return SEM::Statement::ReturnVoid();
				}
				case AST::Statement::RETURN: {
					assert(statement->returnStmt.value.get() != nullptr);
					
					// Check this is not being used inside a scope action.
					for (size_t i = 0; i < context.scopeStack().size(); i++) {
						const auto pos = context.scopeStack().size() - i - 1;
						const auto& element = context.scopeStack().at(pos);
						if (element.isScopeAction()) {
							throw ErrorException(makeString("Cannot 'return' in scope action at position %s.",
								location.toString().c_str()));
						}
					}
					
					const auto semValue = ConvertValue(context, statement->returnStmt.value);
					
					// Cast the return value to the function's
					// specified return type.
					const auto castValue = ImplicitCast(semValue, getParentFunctionReturnType(context.scopeStack()), location);
					
					return SEM::Statement::Return(castValue);
				}
				case AST::Statement::THROW: {
					// Check this is not being used inside a scope action
					// (apart from inside scope(success), which is allowed).
					for (size_t i = 0; i < context.scopeStack().size(); i++) {
						const auto pos = context.scopeStack().size() - i - 1;
						const auto& element = context.scopeStack().at(pos);
						if (element.isScopeAction() && element.scopeActionState() != "success") {
							throw ErrorException(makeString("Cannot 'throw' in scope action with state '%s' at position %s.",
								element.scopeActionState().c_str(), location.toString().c_str()));
						}
					}
					
					const auto semValue = ConvertValue(context, statement->throwStmt.value);
					if (!semValue->type()->isObject() || !semValue->type()->getObjectType()->isException()) {
						throw ErrorException(makeString("Cannot throw non-exception value '%s' at position %s.",
								semValue->toString().c_str(), location.toString().c_str()));
					}
					return SEM::Statement::Throw(semValue);
				}
				case AST::Statement::RETHROW: {
					// Check this is being used inside a catch clause, and
					// is not inside a try clause or a scope action.
					bool foundCatchClause = false;
					
					for (size_t i = 0; i < context.scopeStack().size(); i++) {
						const auto pos = context.scopeStack().size() - i - 1;
						const auto& element = context.scopeStack().at(pos);
						
						if (element.isCatchClause()) {
							foundCatchClause = true;
						} else if (element.isTryScope() && !foundCatchClause) {
							throw ErrorException(makeString("Cannot re-throw exception in try scope (within the relevant catch clause) at position %s.",
								location.toString().c_str()));
						} else if (element.isScopeAction() && element.scopeActionState() != "success") {
							throw ErrorException(makeString("Cannot re-throw exception in scope action with state '%s' at position %s.",
								element.scopeActionState().c_str(), location.toString().c_str()));
						}
					}
					
					if (!foundCatchClause) {
						throw ErrorException(makeString("Cannot re-throw exception outside of catch clause at position %s.",
							location.toString().c_str()));
					}
					
					return SEM::Statement::Rethrow();
				}
				case AST::Statement::BREAK: {
					// Check this is being used inside a loop, and
					// would not leave a scope-exit action.
					bool foundLoop = false;
					for (size_t i = 0; i < context.scopeStack().size(); i++) {
						const auto pos = context.scopeStack().size() - i - 1;
						const auto& element = context.scopeStack().at(pos);
						if (element.isLoop()) {
							foundLoop = true;
							break;
						} else if (element.isScopeAction()) {
							throw ErrorException(makeString("Cannot 'break' from scope exit action at position %s.",
								location.toString().c_str()));
						}
					}
					
					if (!foundLoop) {
						throw ErrorException(makeString("Cannot 'break' outside any control flow statements at position %s.",
							location.toString().c_str()));
					}
					
					return SEM::Statement::Break();
				}
				case AST::Statement::CONTINUE: {
					// Check this is being used inside a loop, and
					// would not leave a scope-exit action.
					bool foundLoop = false;
					for (size_t i = 0; i < context.scopeStack().size(); i++) {
						const auto pos = context.scopeStack().size() - i - 1;
						const auto& element = context.scopeStack().at(pos);
						if (element.isLoop()) {
							foundLoop = true;
							break;
						} else if (element.isScopeAction()) {
							throw ErrorException(makeString("Cannot 'continue' in scope exit action at position %s.",
								location.toString().c_str()));
						}
					}
					
					if (!foundLoop) {
						throw ErrorException(makeString("Cannot 'continue' outside any control flow statements at position %s.",
							location.toString().c_str()));
					}
					
					return SEM::Statement::Continue();
				}
				default:
					throw std::runtime_error("Unknown statement kind.");
			}
		}
		
		Debug::StatementInfo makeStatementInfo(const AST::Node<AST::Statement>& astStatementNode) {
			Debug::StatementInfo statementInfo;
			statementInfo.location = astStatementNode.location();
			return statementInfo;
		}
		
		SEM::Statement* ConvertStatement(Context& context, const AST::Node<AST::Statement>& astStatementNode) {
			const auto semStatement = ConvertStatementData(context, astStatementNode);
			context.debugModule().statementMap.insert(std::make_pair(semStatement, makeStatementInfo(astStatementNode)));
			return semStatement;
		}
		
	}
	
}


