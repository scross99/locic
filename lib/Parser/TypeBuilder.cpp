#include <locic/AST.hpp>
#include <locic/Debug/SourcePosition.hpp>
#include <locic/Parser/TokenReader.hpp>
#include <locic/Parser/TypeBuilder.hpp>
#include <locic/Support/PrimitiveID.hpp>

namespace locic {
	
	namespace Parser {
		
		TypeBuilder::TypeBuilder(const TokenReader& reader)
		: reader_(reader) { }
		
		TypeBuilder::~TypeBuilder() { }
		
		AST::Node<AST::Type>
		TypeBuilder::makeTypeNode(AST::Type* const type,
		                          const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, std::move(type));
		}
		
		AST::Node<AST::TypeList>
		TypeBuilder::makeTypeList(AST::TypeList list,
		                          const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, new AST::TypeList(std::move(list)));
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makePrimitiveType(const PrimitiveID primitiveID,
		                               const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Primitive(primitiveID), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeSymbolType(AST::Node<AST::Symbol> symbol,
		                            const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Object(std::move(symbol)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeConstPredicateType(AST::Node<AST::Predicate> predicate,
		                                    AST::Node<AST::Type> targetType,
		                                    const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::ConstPredicate(std::move(predicate), std::move(targetType)),
			                    start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeConstType(AST::Node<AST::Type> targetType,
		                           const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Const(std::move(targetType)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeNoTagType(AST::Node<AST::Type> targetType,
		                           const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::NoTag(std::move(targetType)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeLvalType(AST::Node<AST::Type> targetType,
		                          AST::Node<AST::Type> type,
		                          const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Lval(std::move(targetType), std::move(type)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeRefType(AST::Node<AST::Type> targetType,
		                         AST::Node<AST::Type> type,
		                         const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Ref(std::move(targetType), std::move(type)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeStaticRefType(AST::Node<AST::Type> targetType,
		                               AST::Node<AST::Type> type,
		                               const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::StaticRef(std::move(targetType), std::move(type)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeFunctionPointerType(AST::Node<AST::Type> returnType,
		                                     AST::Node<AST::TypeList> paramTypes,
		                                     const bool isVarArg,
		                                     const Debug::SourcePosition& start) {
			if (isVarArg) {
				return makeTypeNode(AST::Type::VarArgFunction(std::move(returnType), std::move(paramTypes)), start);
			} else {
				return makeTypeNode(AST::Type::Function(std::move(returnType), std::move(paramTypes)), start);
			}
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeAutoType(const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Auto(), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeReferenceType(AST::Node<AST::Type> targetType,
		                               const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Reference(std::move(targetType)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makePointerType(AST::Node<AST::Type> targetType,
		                             const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::Pointer(std::move(targetType)), start);
		}
		
		AST::Node<AST::Type>
		TypeBuilder::makeStaticArrayType(AST::Node<AST::Type> targetType,
		                                 AST::Node<AST::Value> sizeValue,
		                                 const Debug::SourcePosition& start) {
			return makeTypeNode(AST::Type::StaticArray(std::move(targetType),
			                                           std::move(sizeValue)), start);
		}
		
	}
	
}
