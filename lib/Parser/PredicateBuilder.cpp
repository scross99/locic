#include <locic/AST.hpp>
#include <locic/Debug/SourcePosition.hpp>
#include <locic/Parser/PredicateBuilder.hpp>
#include <locic/Parser/TokenReader.hpp>

namespace locic {
	
	namespace Parser {
		
		PredicateBuilder::PredicateBuilder(const TokenReader& reader)
		: reader_(reader) { }
		
		PredicateBuilder::~PredicateBuilder() { }
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makePredicateNode(AST::Predicate* const predicate,
		                                    const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, predicate);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeTruePredicate(const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::True(), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeFalsePredicate(const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::False(), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeBracketPredicate(AST::Node<AST::Predicate> predicate,
		                                       const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::Bracket(std::move(predicate)), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeTypeSpecPredicate(AST::Node<AST::TypeDecl> type,
		                                        AST::Node<AST::TypeDecl> capabilityType,
		                                        const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::TypeSpec(std::move(type),
			                                                  std::move(capabilityType)), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeSymbolPredicate(AST::Node<AST::Symbol> symbol,
		                                      const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::Symbol(std::move(symbol)), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeAndPredicate(AST::Node<AST::Predicate> leftPredicate,
		                                   AST::Node<AST::Predicate> rightPredicate,
		                                   const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::And(std::move(leftPredicate),
			                                             std::move(rightPredicate)), start);
		}
		
		AST::Node<AST::Predicate>
		PredicateBuilder::makeOrPredicate(AST::Node<AST::Predicate> leftPredicate,
		                                  AST::Node<AST::Predicate> rightPredicate,
		                                  const Debug::SourcePosition& start) {
			return makePredicateNode(AST::Predicate::Or(std::move(leftPredicate),
			                                            std::move(rightPredicate)), start);
		}
		
	}
	
}
