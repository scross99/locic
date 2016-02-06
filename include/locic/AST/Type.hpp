#ifndef LOCIC_AST_TYPE_HPP
#define LOCIC_AST_TYPE_HPP

#include <string>
#include <vector>

#include <locic/AST/Node.hpp>
#include <locic/Support/PrimitiveID.hpp>
#include <locic/Support/String.hpp>

namespace locic {

	namespace AST {
	
		struct Type;
		
		typedef std::vector<Node<Type>> TypeList;
		
		class Predicate;
		class Symbol;
		struct Value;
		
		struct Type {
			enum SignedModifier {
				NO_SIGNED,
				SIGNED,
				UNSIGNED
			};
			
			enum TypeEnum {
				AUTO,
				CONST,
				CONSTPREDICATE,
				NOTAG,
				LVAL,
				REF,
				STATICREF,
				VOID,
				BOOL,
				INTEGER,
				FLOAT,
				PRIMITIVE,
				OBJECT,
				REFERENCE,
				POINTER,
				STATICARRAY,
				FUNCTION
			} typeEnum;
			
			struct {
				Node<Type> targetType;
			} constType;
			
			struct {
				Node<Predicate> predicate;
				Node<Type> targetType;
			} constPredicateType;
			
			struct {
				Node<Type> targetType;
			} noTagType;
			
			struct {
				Node<Type> targetType;
				Node<Type> lvalType;
			} lvalType;
			
			struct {
				Node<Type> targetType;
				Node<Type> refType;
			} refType;
			
			struct {
				Node<Type> targetType;
				Node<Type> refType;
			} staticRefType;
			
			struct {
				SignedModifier signedModifier;
				String name;
			} integerType;
			
			struct {
				String name;
			} floatType;
			
			struct {
				PrimitiveID primitiveID;
			} primitiveType;
			
			struct {
				Node<Symbol> symbol;
			} objectType;
			
			struct {
				Node<Type> targetType;
			} referenceType;
			
			struct {
				Node<Type> targetType;
			} pointerType;
			
			struct {
				Node<Type> targetType;
				Node<Value> arraySize;
			} staticArrayType;
			
			struct {
				bool isVarArg;
				Node<Type> returnType;
				Node<TypeList> parameterTypes;
			} functionType;
			
			inline Type() : typeEnum(static_cast<TypeEnum>(-1)) { }
			
			inline Type(TypeEnum e) : typeEnum(e) { }
			
			static Type* Auto();
			
			static Type* Void();
			
			static Type* Bool();
			
			static Type* Const(Node<Type> targetType);
			
			static Type* ConstPredicate(Node<Predicate> predicate, Node<Type> targetType);
			
			static Type* NoTag(Node<Type> targetType);
			
			static Type* Lval(Node<Type> targetType, Node<Type> lvalType);
			
			static Type* Ref(Node<Type> targetType, Node<Type> refType);
			
			static Type* StaticRef(Node<Type> targetType, Node<Type> refType);
			
			static Type* Integer(SignedModifier signedModifier, const String& name);
			
			static Type* Float(const String& name);
			
			static Type* Primitive(PrimitiveID primitiveID);
			
			static Type* Object(Node<Symbol> symbol);
			
			static Type* Reference(Node<Type> targetType);
			
			static Type* Pointer(Node<Type> targetType);
			
			static Type* StaticArray(Node<Type> targetType, Node<Value> arraySize);
			
			static Type* Function(Node<Type> returnType, Node<TypeList> parameterTypes);
			
			static Type* VarArgFunction(Node<Type> returnType, Node<TypeList> parameterTypes);
			
			~Type();
			
			inline bool isAuto() const {
				return typeEnum == AUTO;
			}
			
			inline bool isVoid() const {
				return typeEnum == VOID;
			}
			
			inline bool isConst() const {
				return typeEnum == CONST;
			}
			
			const Node<Type>& getConstTarget() const {
				assert(isConst());
				return constType.targetType;
			}
			
			inline bool isConstPredicate() const {
				return typeEnum == CONSTPREDICATE;
			}
			
			const Node<Predicate>& getConstPredicate() const {
				assert(isConstPredicate());
				return constPredicateType.predicate;
			}
			
			const Node<Type>& getConstPredicateTarget() const {
				assert(isConstPredicate());
				return constPredicateType.targetType;
			}
			
			inline bool isNoTag() const {
				return typeEnum == NOTAG;
			}
			
			const Node<Type>& getNoTagTarget() const {
				assert(isNoTag());
				return noTagType.targetType;
			}
			
			inline bool isLval() const {
				return typeEnum == LVAL;
			}
			
			const Node<Type>& getLvalTarget() const {
				assert(isLval());
				return lvalType.targetType;
			}
			
			const Node<Type>& getLvalType() const {
				assert(isLval());
				return lvalType.lvalType;
			}
			
			inline bool isRef() const {
				return typeEnum == REF;
			}
			
			const Node<Type>& getRefTarget() const {
				assert(isRef());
				return refType.targetType;
			}
			
			const Node<Type>& getRefType() const {
				assert(isRef());
				return refType.refType;
			}
			
			inline bool isStaticRef() const {
				return typeEnum == STATICREF;
			}
			
			const Node<Type>& getStaticRefTarget() const {
				assert(isStaticRef());
				return staticRefType.targetType;
			}
			
			const Node<Type>& getStaticRefType() const {
				assert(isStaticRef());
				return staticRefType.refType;
			}
			
			inline bool isReference() const {
				return typeEnum == REFERENCE;
			}
			
			const Node<Type>& getReferenceTarget() const {
				assert(isReference());
				return referenceType.targetType;
			}
			
			inline bool isPointer() const {
				return typeEnum == POINTER;
			}
			
			const Node<Type>& getPointerTarget() const {
				assert(isPointer());
				return pointerType.targetType;
			}
			
			inline bool isStaticArray() const {
				return typeEnum == STATICARRAY;
			}
			
			const Node<Type>& getStaticArrayTarget() const {
				assert(isStaticArray());
				return staticArrayType.targetType;
			}
			
			const Node<Value>& getArraySize() const {
				assert(isStaticArray());
				return staticArrayType.arraySize;
			}
			
			inline bool isFunction() const {
				return typeEnum == FUNCTION;
			}
			
			bool functionIsVarArg() const {
				assert(isFunction());
				return functionType.isVarArg;
			}
			
			const Node<Type>& functionReturnType() const {
				assert(isFunction());
				return functionType.returnType;
			}
			
			const Node<TypeList>& functionParameterTypes() const {
				assert(isFunction());
				return functionType.parameterTypes;
			}
			
			inline bool isInteger() const {
				return typeEnum == INTEGER;
			}
			
			inline SignedModifier integerSignedModifier() const {
				assert(isInteger());
				return integerType.signedModifier;
			}
			
			inline const String& integerName() const {
				assert(isInteger());
				return integerType.name;
			}
			
			inline bool isFloat() const {
				return typeEnum == FLOAT;
			}
			
			inline const String& floatName() const {
				assert(isFloat());
				return floatType.name;
			}
			
			bool isPrimitive() const {
				return typeEnum == PRIMITIVE;
			}
			
			PrimitiveID primitiveID() const {
				assert(isPrimitive());
				return primitiveType.primitiveID;
			}
			
			inline bool isObjectType() const {
				return typeEnum == OBJECT;
			}
			
			const Node<Symbol>& symbol() const {
				return objectType.symbol;
			}
			
			std::string toString() const;
			
		};
		
	}
	
}

#endif
