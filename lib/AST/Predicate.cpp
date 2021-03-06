#include <assert.h>

#include <memory>
#include <string>

#include <locic/AST/Predicate.hpp>
#include <locic/AST/TemplateVar.hpp>
#include <locic/AST/TemplateVarMap.hpp>
#include <locic/AST/Type.hpp>
#include <locic/AST/Value.hpp>
#include <locic/AST/ValueDecl.hpp>

#include <locic/Support/ErrorHandling.hpp>
#include <locic/Support/Hasher.hpp>
#include <locic/Support/MakeString.hpp>
#include <locic/Support/String.hpp>

namespace locic {
	
	namespace AST {
		
		Predicate Predicate::FromBool(const bool boolValue) {
			return boolValue ? True() : False();
		}
		
		Predicate Predicate::True() {
			return Predicate(TRUE);
		}
		
		Predicate Predicate::False() {
			return Predicate(FALSE);
		}
		
		Predicate Predicate::SelfConst() {
			return Predicate(SELFCONST);
		}
		
		Predicate Predicate::And(Predicate left, Predicate right) {
			if (left.isFalse() || right.isFalse()) {
				return Predicate::False();
			} else if (left.isTrue()) {
				return right;
			} else if (right.isTrue()) {
				return left;
			} else if (left == right) {
				return left;
			} else if (left.isAnd() && (left.andLeft() == right || left.andRight() == right)) {
				return left;
			} else if (right.isAnd() && (right.andLeft() == left || right.andRight() == left)) {
				return right;
			}
			
			Predicate predicate(AND);
			predicate.left_ = std::unique_ptr<Predicate>(new Predicate(std::move(left)));
			predicate.right_ = std::unique_ptr<Predicate>(new Predicate(std::move(right)));
			return predicate;
		}
		
		Predicate Predicate::Or(Predicate left, Predicate right) {
			if (left.isTrue() || right.isTrue()) {
				return Predicate::True();
			} else if (left.isFalse()) {
				return right;
			} else if (right.isFalse()) {
				return left;
			} else if (left == right) {
				return left;
			} else if (left.isOr() && (left.orLeft() == right || left.orRight() == right)) {
				return left;
			} else if (right.isOr() && (right.orLeft() == left || right.orRight() == left)) {
				return right;
			}
			
			Predicate predicate(OR);
			predicate.left_ = std::unique_ptr<Predicate>(new Predicate(std::move(left)));
			predicate.right_ = std::unique_ptr<Predicate>(new Predicate(std::move(right)));
			return predicate;
		}
		
		Predicate Predicate::Satisfies(const Type* const type,
		                               const Type* const requirement) {
			Predicate predicate(SATISFIES);
			predicate.type_ = type;
			predicate.requirement_ = requirement;
			return predicate;
		}
		
		Predicate Predicate::Variable(TemplateVar* templateVar) {
			Predicate predicate(VARIABLE);
			predicate.templateVar_ = templateVar;
			return predicate;
		}
		
		Predicate::Predicate(Kind k) : kind_(k) { }
		
		Predicate Predicate::copy() const {
			switch (kind()) {
				case TRUE:
				{
					return Predicate::True();
				}
				case FALSE:
				{
					return Predicate::False();
				}
				case SELFCONST:
				{
					return Predicate::SelfConst();
				}
				case AND:
				{
					return Predicate::And(andLeft().copy(), andRight().copy());
				}
				case OR:
				{
					return Predicate::Or(orLeft().copy(), orRight().copy());
				}
				case SATISFIES:
				{
					return Predicate::Satisfies(satisfiesType(), satisfiesRequirement());
				}
				case VARIABLE:
				{
					return Predicate::Variable(variableTemplateVar());
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		Predicate Predicate::substitute(const TemplateVarMap& templateVarMap,
		                                const Predicate& selfconst) const {
			switch (kind()) {
				case TRUE:
				{
					return Predicate::True();
				}
				case FALSE:
				{
					return Predicate::False();
				}
				case SELFCONST:
				{
					return selfconst.copy();
				}
				case AND:
				{
					return Predicate::And(andLeft().substitute(templateVarMap,
					                                           selfconst),
					                      andRight().substitute(templateVarMap,
							                            selfconst));
				}
				case OR:
				{
					return Predicate::Or(orLeft().substitute(templateVarMap,
					                                         selfconst),
					                     orRight().substitute(templateVarMap,
							                          selfconst));
				}
				case SATISFIES:
				{
					return Predicate::Satisfies(satisfiesType()->substitute(templateVarMap,
					                                                        selfconst),
					                            satisfiesRequirement()->substitute(templateVarMap,
								                                       selfconst));
				}
				case VARIABLE:
				{
					const auto iterator = templateVarMap.find(variableTemplateVar());
					
					if (iterator == templateVarMap.end()) {
						return Predicate::Variable(variableTemplateVar());
					}
					
					const auto& templateValue = iterator->second;
					return templateValue.makePredicate();
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		bool Predicate::dependsOn(const TemplateVar* const templateVar) const {
			switch (kind()) {
				case TRUE:
				case FALSE:
				case SELFCONST:
				{
					return false;
				}
				case AND:
				{
					return andLeft().dependsOn(templateVar) || andRight().dependsOn(templateVar);
				}
				case OR:
				{
					return orLeft().dependsOn(templateVar) || orRight().dependsOn(templateVar);
				}
				case SATISFIES:
				{
					return satisfiesType()->dependsOn(templateVar) || satisfiesRequirement()->dependsOn(templateVar);
				}
				case VARIABLE:
				{
					return templateVar == variableTemplateVar();
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		bool Predicate::dependsOnAny(const TemplateVarArray& array) const {
			for (const auto& templateVar: array) {
				if (dependsOn(templateVar)) {
					return true;
				}
			}
			return false;
		}
		
		bool Predicate::dependsOnOnly(const TemplateVarArray& array) const {
			switch (kind()) {
				case TRUE:
				case FALSE:
				case SELFCONST:
				{
					return true;
				}
				case AND:
				{
					return andLeft().dependsOnOnly(array) && andRight().dependsOnOnly(array);
				}
				case OR:
				{
					return orLeft().dependsOnOnly(array) && orRight().dependsOnOnly(array);
				}
				case SATISFIES:
				{
					return satisfiesType()->dependsOnOnly(array) && satisfiesRequirement()->dependsOnOnly(array);
				}
				case VARIABLE:
				{
					return array.contains(variableTemplateVar());
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		Predicate Predicate::reduceToDependencies(const TemplateVarArray& array, const bool conservativeDefault) const {
			switch (kind()) {
				case TRUE:
				case FALSE:
				case SELFCONST:
				{
					return copy();
				}
				case AND:
				{
					return And(andLeft().reduceToDependencies(array, conservativeDefault), andRight().reduceToDependencies(array, conservativeDefault));
				}
				case OR:
				{
					return Or(orLeft().reduceToDependencies(array, conservativeDefault), orRight().reduceToDependencies(array, conservativeDefault));
				}
				case SATISFIES:
				{
					if (satisfiesType()->dependsOnOnly(array) && satisfiesRequirement()->dependsOnOnly(array)) {
						return copy();
					} else {
						return Predicate::FromBool(conservativeDefault);
					}
				}
				case VARIABLE:
				{
					if (array.contains(variableTemplateVar())) {
						return copy();
					} else {
						return Predicate::FromBool(conservativeDefault);
					}
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		bool Predicate::implies(const AST::Predicate& other) const {
			if (kind() == FALSE || other.kind() == TRUE) {
				return true;
			}
			
			if (kind() == OR) {
				// (A or B) => C is true iff ((A => C) and (B => C)).
				return orLeft().implies(other) && orRight().implies(other);
			}
			
			if (other.kind() == AND) {
				// A => (B and C) is true iff ((A => B) and (A => C)).
				return implies(other.andLeft()) && implies(other.andRight());
			}
			
			if (kind() == AND) {
				// (A and B) => C is true iff ((A => C) or (B => C)).
				return andLeft().implies(other) || andRight().implies(other);
			}
			
			if (other.kind() == OR) {
				// A => (B or C) is true iff ((A => B) or (A => C)).
				return implies(other.orLeft()) || implies(other.orRight());
			}
			
			if (kind() != other.kind()) {
				return false;
			}
			
			switch (kind()) {
				case SELFCONST:
					return true;
				case SATISFIES:
					// TODO: this needs to be improved to allow covariant
					// treatment of the requirement type (e.g. T : copyable_and_movable<T>
					// implies T : movable).
					return satisfiesType() == other.satisfiesType() && satisfiesRequirement() == other.satisfiesRequirement();
				case VARIABLE:
					return variableTemplateVar() == other.variableTemplateVar();
				default:
					locic_unreachable("Reached unreachable block in 'implies'.");
			}
		}
		
		Predicate::Kind Predicate::kind() const {
			return kind_;
		}
		
		bool Predicate::isTrivialBool() const {
			return isTrue() || isFalse();
		}
		
		bool Predicate::isTrue() const {
			return kind() == TRUE;
		}
		
		bool Predicate::isFalse() const {
			return kind() == FALSE;
		}
		
		bool Predicate::isSelfConst() const {
			return kind() == SELFCONST;
		}
		
		bool Predicate::isAnd() const {
			return kind() == AND;
		}
		
		bool Predicate::isOr() const {
			return kind() == OR;
		}
		
		bool Predicate::isSatisfies() const {
			return kind() == SATISFIES;
		}
		
		bool Predicate::isVariable() const {
			return kind() == VARIABLE;
		}
		
		const Predicate& Predicate::andLeft() const {
			assert(isAnd());
			return *left_;
		}
		
		const Predicate& Predicate::andRight() const {
			assert(isAnd());
			return *right_;
		}
		
		const Predicate& Predicate::orLeft() const {
			assert(isOr());
			return *left_;
		}
		
		const Predicate& Predicate::orRight() const {
			assert(isOr());
			return *right_;
		}
		
		const Type* Predicate::satisfiesType() const {
			assert(isSatisfies());
			return type_;
		}
		
		const Type* Predicate::satisfiesRequirement() const {
			assert(isSatisfies());
			return requirement_;
		}
		
		TemplateVar* Predicate::variableTemplateVar() const {
			assert(isVariable());
			return templateVar_;
		}
		
		bool Predicate::operator==(const Predicate& other) const {
			if (kind() != other.kind()) {
				return false;
			}
			
			switch (kind()) {
				case TRUE:
				case FALSE:
				case SELFCONST:
				{
					return true;
				}
				case AND:
				{
					return andLeft() == other.andLeft() && andRight() == other.andRight();
				}
				case OR:
				{
					return orLeft() == other.orLeft() && orRight() == other.orRight();
				}
				case SATISFIES:
				{
					return satisfiesType() == other.satisfiesType() &&
						satisfiesRequirement() == other.satisfiesRequirement();
				}
				case VARIABLE:
				{
					return variableTemplateVar() == other.variableTemplateVar();
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
		size_t Predicate::hash() const {
			Hasher hasher;
			hasher.add(kind());
			
			switch (kind()) {
				case TRUE:
				case FALSE:
				case SELFCONST:
				{
					break;
				}
				case AND:
				{
					hasher.add(andLeft());
					hasher.add(andRight());
					break;
				}
				case OR:
				{
					hasher.add(orLeft());
					hasher.add(orRight());
					break;
				}
				case SATISFIES:
				{
					hasher.add(satisfiesType());
					hasher.add(satisfiesRequirement());
					break;
				}
				case VARIABLE:
				{
					hasher.add(variableTemplateVar());
					break;
				}
			}
			
			return hasher.get();
		}
		
		std::string Predicate::toString() const {
			switch (kind()) {
				case TRUE:
				{
					return "true";
				}
				case FALSE:
				{
					return "false";
				}
				case SELFCONST:
				{
					return "selfconst";
				}
				case AND:
				{
					return makeString("%s and %s",
						andLeft().toString().c_str(),
						andRight().toString().c_str());
				}
				case OR:
				{
					return makeString("%s or %s",
						orLeft().toString().c_str(),
						orRight().toString().c_str());
				}
				case SATISFIES:
				{
					return makeString("%s : %s",
						satisfiesType()->toDiagString().c_str(),
						satisfiesRequirement()->toDiagString().c_str());
				}
				case VARIABLE:
				{
					return variableTemplateVar()->fullName().last().asStdString();
				}
			}
			
			locic_unreachable("Unknown predicate kind.");
		}
		
	}
	
}

