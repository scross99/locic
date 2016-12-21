#include <stdio.h>

#include <stdexcept>

#include <locic/AST/Type.hpp>
#include <locic/Debug.hpp>
#include <locic/Frontend/OptionalDiag.hpp>
#include <locic/Support/Name.hpp>
#include <locic/Support/String.hpp>
#include <locic/SEM.hpp>

#include <locic/SemanticAnalysis/CallValue.hpp>
#include <locic/SemanticAnalysis/Cast.hpp>
#include <locic/SemanticAnalysis/ConvertPredicate.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/GetMethod.hpp>
#include <locic/SemanticAnalysis/GetMethodSet.hpp>
#include <locic/SemanticAnalysis/Lval.hpp>
#include <locic/SemanticAnalysis/MethodSet.hpp>
#include <locic/SemanticAnalysis/MethodSetSatisfies.hpp>
#include <locic/SemanticAnalysis/Ref.hpp>
#include <locic/SemanticAnalysis/Template.hpp>
#include <locic/SemanticAnalysis/TypeCapabilities.hpp>

namespace locic {

	namespace SemanticAnalysis {

		static const AST::Type*
		ImplicitCastTypeFormatOnlyChain(Context& context, const AST::Type* sourceType,
		                                const AST::Type* destType, bool hasParentConstChain,
		                                const Debug::SourceLocation& location, bool isTopLevel = false);

		class CannotCastTemplateTypeDiag : public Error {
		public:
			CannotCastTemplateTypeDiag(const AST::Type* sourceType, const AST::Type* destType)
			: message_(makeString("Cannot cast from template type '%s' to template type '%s'.",
			                      sourceType->toDiagString().c_str(), destType->toDiagString().c_str())) {}

			std::string toString() const { return message_; }

		private:
			std::string message_;
		};

		static const AST::Type*
		ImplicitCastTypeFormatOnlyChainCheckType(Context& context, const AST::Type* sourceType,
		                                         const AST::Type* destType, bool hasConstChain,
		                                         const Debug::SourceLocation& location) {
			if (sourceType == destType) {
				return sourceType;
			}
			
			if (destType->isAuto()) {
				// 'auto' is pattern matching, so in this
				// case it can match the source type.
				return sourceType->withoutTags();
			}
			
			if (sourceType->kind() != destType->kind()) {
				// At this point types need to be in the same group.
				return nullptr;
			}
			
			switch (destType->kind()) {
				case AST::Type::AUTO: {
					locic_unreachable("Source type can't be auto.");
				}
				case AST::Type::ALIAS: {
					locic_unreachable("Aliases should already be resolved.");
				}
				case AST::Type::OBJECT: {
					if (sourceType->getObjectType() != destType->getObjectType()) {
						if (!sourceType->isInterface() || !destType->isInterface()) {
							// Non-interface objects can only be cast to
							// the same object type.
							return nullptr;
						}
						
						const auto sourceMethodSet = getTypeMethodSet(context, sourceType);
						const auto requireMethodSet = getTypeMethodSet(context, destType);
						
						if (methodSetSatisfiesRequirement(context, sourceMethodSet, requireMethodSet)) {
							return destType;
						} else {
							return nullptr;
						}
					}
					
					// Need to check template arguments.
					const auto sourceNumArgs = sourceType->templateArguments().size();
					const auto destNumArgs = destType->templateArguments().size();
					
					if (sourceNumArgs != destNumArgs) {
						return nullptr;
					}
					
					AST::ValueArray templateArgs;
					templateArgs.reserve(sourceType->templateArguments().size());
					
					for (size_t i = 0; i < sourceType->templateArguments().size(); i++) {
						const auto& sourceTemplateArg = sourceType->templateArguments()[i];
						const auto& destTemplateArg = destType->templateArguments()[i];
						
						if (sourceTemplateArg.isTypeRef() && destTemplateArg.isTypeRef()) {
							const auto templateArg =
							    ImplicitCastTypeFormatOnlyChain(context, sourceTemplateArg
							                                                 .typeRefType(),
							                                    destTemplateArg
							                                        .typeRefType(),
							                                    hasConstChain, location);
							if (templateArg == nullptr) return nullptr;
							
							templateArgs.push_back(AST::Value::TypeRef(templateArg, sourceTemplateArg.type()));
						} else {
							if (sourceTemplateArg != destTemplateArg) {
								return nullptr;
							}
							templateArgs.push_back(sourceTemplateArg.copy());
						}
					}
					
					return AST::Type::Object(sourceType->getObjectType(), std::move(templateArgs));
				}
				case AST::Type::TEMPLATEVAR: {
					if (sourceType->getTemplateVar() != destType->getTemplateVar()) {
						context.issueDiag(CannotCastTemplateTypeDiag(sourceType, destType),
						                  location);
						throw SkipException();
					}
					return sourceType->withoutTags();
				}
			}
			
			locic_unreachable("Unknown type kind.");
		}

		static const AST::Type*
		ImplicitCastTypeFormatOnlyChainCheckTags(Context& context, const AST::Type* sourceType,
		                                         const AST::Type* destType, bool hasParentConstChain,
		                                         const Debug::SourceLocation& location, bool isTopLevel) {
			// TODO: fix this to evaluate the const predicates.
			const bool isSourceConst = !sourceType->constPredicate().isFalse();
			const bool isDestConst = !destType->constPredicate().isFalse();
			
			// Can't cast const to non-const, unless the destination type is
			// 'auto', since that can match 'const T'.
			if (!sourceType->constPredicate().implies(destType->constPredicate()) && !destType->isAuto()) {
				// No copying can be done now, so this is just an error.
				return nullptr;
			}
			
			if (!hasParentConstChain && sourceType->constPredicate().implies(destType->constPredicate()) && !isSourceConst && isDestConst) {
				// Must be a const chain for mutable-to-const cast to succeed.
				// For example, the following cast is invalid:
				//         ptr<T> -> ptr<const T>
				// It can be made valid by changing it to:
				//         const ptr<T> -> const ptr<const T>
				return nullptr;
			}
			
			// There is a chain of const if all parents of the destination type are const,
			// and the destination type itself is const.
			const bool hasConstChain = isTopLevel || (hasParentConstChain && isDestConst);
			
			bool isLval = false;
			
			if (sourceType->isLval() || destType->isLval()) {
				if (!(sourceType->isLval() && destType->isLval())) {
					// If one type is lval, both types must be lvals.
					return nullptr;
				}
				
				isLval = true;
			}
			
			const AST::Type* refTarget = nullptr;
			
			if (sourceType->isRef() || destType->isRef()) {
				if (!(sourceType->isRef() && destType->isRef())) {
					// If one type is ref, both types must be refs.
					return nullptr;
				}
				
				// Must perform substitutions for the ref target type.
				refTarget =
				    ImplicitCastTypeFormatOnlyChain(context, sourceType->refTarget(),
				                                    destType->refTarget(), hasConstChain, location);
				if (refTarget == nullptr) return nullptr;
			}
			
			const AST::Type* staticRefTarget = nullptr;
			
			if (sourceType->isStaticRef() || destType->isStaticRef()) {
				if (!(sourceType->isStaticRef() && destType->isStaticRef())) {
					// If one type is ref, both types must be refs.
					return nullptr;
				}
				
				// Must perform substitutions for the ref target type.
				staticRefTarget =
				    ImplicitCastTypeFormatOnlyChain(context, sourceType->staticRefTarget(),
				                                    destType->staticRefTarget(), hasConstChain,
				                                    location);
				if (staticRefTarget == nullptr) return nullptr;
			}
			
			// Generate the 'untagged' type.
			auto resultType = ImplicitCastTypeFormatOnlyChainCheckType(context, sourceType, destType,
			                                                           hasConstChain, location);
			if (resultType == nullptr) return nullptr;
			
			// Add the substituted tags.
			if (isLval) {
				resultType = resultType->createLvalType();
			}
			
			if (refTarget != nullptr) {
				resultType = resultType->createRefType(refTarget);
			}
			
			if (staticRefTarget != nullptr) {
				resultType = resultType->createStaticRefType(staticRefTarget);
			}
			
			// Non-const 'auto' can match 'const T', and in that case
			// the resulting type must be const.
			return resultType->createTransitiveConstType(AST::Predicate::Or(sourceType->constPredicate().copy(), destType->constPredicate().copy()));
		}

		inline static const AST::Type*
		ImplicitCastTypeFormatOnlyChain(Context& context, const AST::Type* sourceType,
		                                const AST::Type* destType, bool hasParentConstChain,
		                                const Debug::SourceLocation& location, bool isTopLevel) {
			return ImplicitCastTypeFormatOnlyChainCheckTags(context, sourceType, destType,
			                                                hasParentConstChain, location, isTopLevel);
		}

		const AST::Type*
		ImplicitCastTypeFormatOnly(Context& context, const AST::Type* sourceType, const AST::Type* destType,
		                           const Debug::SourceLocation& location) {
			// Needed for the main format-only cast function to ensure the
			// const chaining rule from root is followed; since this
			// is root there is a valid chain of (zero) const parent types.
			const bool hasParentConstChain = true;
			
			const bool isTopLevel = true;

			return ImplicitCastTypeFormatOnlyChain(context, sourceType->resolveAliases(),
			                                       destType->resolveAliases(), hasParentConstChain,
			                                       location, isTopLevel);
		}

		Optional<AST::Value>
		ImplicitCastFormatOnly(Context& context, AST::Value value, const AST::Type* destType,
		                       const Debug::SourceLocation& location) {
			auto resultType = ImplicitCastTypeFormatOnly(context, value.type(), destType, location);
			if (resultType == nullptr) {
				return Optional<AST::Value>();
			}
			
			// The value's type needs to reflect the successful cast, however
			// this shouldn't be added unless necessary.
			if (value.type() != resultType) {
				return make_optional(AST::Value::Cast(resultType, std::move(value)));
			} else {
				return make_optional(std::move(value));
			}
		}

		Optional<AST::Value>
		ImplicitCastConvert(Context& context, std::vector<std::string>& errors, AST::Value value,
		                    const AST::Type* destType, const Debug::SourceLocation& location, bool allowBind,
		                    bool formatOnly = false);

		static Optional<AST::Value> PolyCastRefValueToType(Context& context, AST::Value value, const AST::Type* destType) {
			const auto sourceType = value.type();
			assert(sourceType->isRef() && destType->isRef());
			
			const auto sourceTargetType = sourceType->refTarget();
			const auto destTargetType = destType->refTarget();
			
			const auto sourceMethodSet = getTypeMethodSet(context, sourceTargetType);
			const auto destMethodSet = getTypeMethodSet(context, destTargetType);
			
			return methodSetSatisfiesRequirement(context, sourceMethodSet, destMethodSet) ?
				make_optional(AST::Value::PolyCast(destType, std::move(value))) :
				Optional<AST::Value>();
		}
		
		static Optional<AST::Value> PolyCastStaticRefValueToType(Context& context, AST::Value value, const AST::Type* destType) {
			const auto sourceType = value.type();
			assert(sourceType->isStaticRef() && destType->isStaticRef());
			
			const auto sourceTargetType = sourceType->staticRefTarget();
			const auto destTargetType = destType->staticRefTarget();
			
			const auto sourceMethodSet = getTypeMethodSet(context, sourceTargetType);
			const auto destMethodSet = getTypeMethodSet(context, destTargetType);
			
			return methodSetSatisfiesRequirement(context, sourceMethodSet, destMethodSet) ?
				make_optional(AST::Value::PolyCast(destType, std::move(value))) :
				Optional<AST::Value>();
		}
		
		// User-defined casts.
		static Optional<AST::Value> ImplicitCastUser(Context& context, std::vector<std::string>& errors,
		                                             AST::Value rawValue, const AST::Type* destType,
		                                             const Debug::SourceLocation& location, bool allowBind) {
			auto value = derefValue(std::move(rawValue));
			const auto sourceDerefType = getDerefType(value.type());
			
			// Use notag() to make destination type non-const so that it's movable.
			const auto destDerefType = getDerefType(destType)->createNoTagType();
			
			if (sourceDerefType->isObject() && destDerefType->isObjectOrTemplateVar() &&
			    TypeCapabilities(context).supportsImplicitCast(sourceDerefType)) {
				if (destDerefType->isObject() && sourceDerefType->getObjectType() == destDerefType->getObjectType()) {
					// Can't cast to same type.
					return Optional<AST::Value>();
				}
				
				const auto& castFunction = sourceDerefType->getObjectType()->getFunction(context.getCString("implicitcast"));
				
				const auto& requiresPredicate = castFunction.requiresPredicate();
				
				auto combinedTemplateVarMap = sourceDerefType->generateTemplateVarMap();
				const auto& castTemplateVar = castFunction.templateVariables().front();
				combinedTemplateVarMap.insert(std::make_pair(castTemplateVar, AST::Value::TypeRef(destDerefType, castTemplateVar->type()->createStaticRefType(destDerefType))));
				
				if (evaluatePredicate(context, requiresPredicate, combinedTemplateVarMap)) {
					auto boundValue = bindReference(context, std::move(value));
					auto method = GetTemplatedMethod(context, std::move(boundValue), context.getCString("implicitcast"), makeTemplateArgs(context, { destDerefType }), location);
					auto castValue = CallValue(context, std::move(method), {}, location);
					
					// There still might be some aspects to cast with the constructed type.
					return ImplicitCastConvert(context, errors, std::move(castValue), destType, location, allowBind);
				} else {
					errors.push_back(makeString("user cast failed from type '%s' to type '%s'",
					                            sourceDerefType->toString().c_str(),
					                            destDerefType->toString().c_str()));
				}
			}
			
			return Optional<AST::Value>();
		}
		
		static bool isStructurallyEqual(const AST::Type* firstType, const AST::Type* secondType) {
			if (firstType->kind() != secondType->kind()) {
				return false;
			}
			
			if (firstType->isObject()) {
				return firstType->getObjectType() == secondType->getObjectType();
			} else if (firstType->isTemplateVar()) {
				return firstType->getTemplateVar() == secondType->getTemplateVar();
			} else {
				return false;
			}
		}
		
		static bool canTreatConstantAsUnsigned(const AST::Value& value, const AST::Type* const destType) {
			assert(value.isConstant());
			
			const auto sourceType = value.type()->resolveAliases();
			
			if (!sourceType->isPrimitive() || !destType->isPrimitive()) {
				return false;
			}
			
			const auto sourcePrimitiveID = sourceType->primitiveID();
			const auto destPrimitiveID = destType->primitiveID();
			
			if (!sourcePrimitiveID.isSignedInteger()) {
				// Looking for constants of signed integer type.
				return false;
			}
			
			if (!destPrimitiveID.isUnsignedInteger()) {
				// Looking to cast to an unsigned integer type.
				return false;
			}
			
			// Finally, we need to able to represent the constant
			// in the destination type.
			return sourcePrimitiveID.asUnsigned().isSubsetOf(destPrimitiveID);
		}

		class FormatOnlyCastFailedDiag : public Error {
		public:
			FormatOnlyCastFailedDiag(const AST::Type* sourceType, const AST::Type* destType)
			: message_(makeString("Format only cast failed from type %s to type %s.",
			                      sourceType->toDiagString().c_str(), destType->toDiagString().c_str())) {}

			std::string toString() const { return message_; }

		private:
			std::string message_;
		};

		Optional<AST::Value> ImplicitCastConvert(Context& context, std::vector<std::string>& errors, const AST::Value value, const AST::Type* destType, const Debug::SourceLocation& location, bool allowBind, bool formatOnly) {
			{
				// Try a format only cast first, since
				// this requires no transformations.
				auto castResult = ImplicitCastFormatOnly(context, value.copy(), destType, location);
				if (castResult) {
					return castResult;
				} else if (formatOnly) {
					context.issueDiag(FormatOnlyCastFailedDiag(value.type(), destType), location);
					throw SkipException();
				}
			}
			
			const auto sourceType = value.type()->resolveAliases();
			
			if (value.isConstant() &&
			    canTreatConstantAsUnsigned(value, destType)) {
				// Allow positive signed integer constants to be
				// treated as unsigned.
				return make_optional(AST::Value::Constant(value.constant(),
				                                          destType));
			}
			
			// Try to cast datatype to its parent union datatype.
			if (sourceType->isDatatype()) {
				const auto destDerefType = getDerefType(destType);
				if (destDerefType->isUnionDatatype()) {
					bool found = false;
					for (const auto variant: destDerefType->getObjectType()->variants()) {
						if (sourceType->getObjectType() == variant) {
							found = true;
							break;
						}
					}
					
					if (found) {
						auto castValue = AST::Value::Cast(destDerefType, value.copy());
						auto castResult = ImplicitCastConvert(context, errors, std::move(castValue), destType, location, allowBind);
						if (castResult) {
							return castResult;
						}
					}
				}
			}
			
			// Try to dereference the source ref type enough times
			// so that it matches the destination ref type.
			{
				const auto sourceCount = getRefCount(sourceType);
				const auto destCount = getRefCount(destType);
				
				// Can only ever reduce a reference to another
				// reference count without implicitCopy.
				if (sourceCount > destCount && destCount > 0) {
					auto reducedValue = value.copy();
					
					const auto numReduce = sourceCount - destCount;
					for (size_t i = 0; i < numReduce; i++) {
						reducedValue = derefOne(std::move(reducedValue));
					}
					
					auto castResult = ImplicitCastConvert(context, errors, std::move(reducedValue), destType, location, allowBind);
					if (castResult) {
						return castResult;
					}
				}
			}
			
			// Try to dissolve the source lval type.
			if (getDerefType(sourceType)->isLval() && !getDerefType(destType)->isLval()) {
				auto reducedValue = dissolveLval(context, derefValue(value.copy()), location);
				
				auto castResult = ImplicitCastConvert(context, errors, std::move(reducedValue), destType, location, allowBind);
				if (castResult) {
					return castResult;
				}
			}
			
			// Try to use a polymorphic ref cast.
			if (sourceType->isRef() && destType->isRef() && sourceType->refTarget()->isObject() && destType->refTarget()->isInterface()) {
				// TODO: add support for custom ref types.
				if (sourceType->isBuiltInReference() && destType->isBuiltInReference()) {
					const auto sourceTarget = sourceType->refTarget();
					const auto destTarget = destType->refTarget();
					
					if (doesPredicateImplyPredicate(context, sourceTarget->constPredicate(), destTarget->constPredicate())) {
						auto castResult = PolyCastRefValueToType(context, value.copy(), destType);
						if (castResult) {
							return castResult;
						}
					}
				}
			}
			
			// Try to use a polymorphic staticref cast.
			if (sourceType->isStaticRef() && destType->isStaticRef() && sourceType->staticRefTarget()->isObject() && destType->staticRefTarget()->isInterface()) {
				const auto sourceTarget = sourceType->staticRefTarget();
				const auto destTarget = destType->staticRefTarget();
				
				if (doesPredicateImplyPredicate(context, sourceTarget->constPredicate(), destTarget->constPredicate())) {
					auto castResult = PolyCastStaticRefValueToType(context, value.copy(), destType);
					if (castResult) {
						return castResult;
					}
				}
			}
			
			// Try to use implicitCopy-by-reference to turn a
			// reference into a basic value.
			if (sourceType->isRef() && (!destType->isRef() || !isStructurallyEqual(sourceType->refTarget(), destType->refTarget()))) {
				const auto sourceDerefType = getDerefType(sourceType);
				if (TypeCapabilities(context).supportsImplicitCopy(sourceDerefType)) {
					auto copyValue = CallValue(context, GetSpecialMethod(context, derefValue(value.copy()), context.getCString("implicitcopy"), location), {}, location);
					
					auto copyRefValue = sourceDerefType->isStaticRef() ?
						AST::Value::StaticRef(sourceDerefType->staticRefTarget(), std::move(copyValue)) :
						std::move(copyValue);
					
					const bool nextAllowBind = false;
					auto convertCast = ImplicitCastConvert(context, errors, std::move(copyRefValue), destType, location, nextAllowBind);
					if (convertCast) {
						return convertCast;
					}
				} else if (sourceDerefType->isObjectOrTemplateVar() && CanDoImplicitCast(context, sourceDerefType, destType, location)) {
					// This almost certainly would have worked
					// if implicitCopy was available, so let's
					// report this error to the user.
					errors.push_back(makeString("Unable to copy type '%s' because it doesn't have "
					                            "a valid 'implicitcopy' method, "
					                            "in cast from type %s to type %s.",
					                            sourceDerefType->toString().c_str(),
					                            sourceType->toString().c_str(),
					                            destType->toString().c_str()));
				}
			}
			
			// Try to use implicitCopy to make a value non-const.
			if (getRefCount(sourceType) == getRefCount(destType) &&
			    !doesPredicateImplyPredicate(context, sourceType->constPredicate(), destType->constPredicate()) &&
			    sourceType->isObjectOrTemplateVar() && TypeCapabilities(context).supportsImplicitCopy(sourceType)) {
				auto boundValue = bindReference(context, value.copy());
				auto copyValue = CallValue(context, GetSpecialMethod(context, std::move(boundValue), context.getCString("implicitcopy"), location), {}, location);
				assert(doesPredicateImplyPredicate(context, copyValue.type()->constPredicate(), destType->constPredicate()));
				
				auto copyLvalValue = sourceType->isLval() ?
						AST::Value::Lval(std::move(copyValue)) :
						std::move(copyValue);
				
				auto copyRefValue = sourceType->isRef() ?
						AST::Value::Ref(sourceType->refTarget(), std::move(copyLvalValue)) :
						std::move(copyLvalValue);
				
				auto copyStaticRefValue = sourceType->isStaticRef() ?
						AST::Value::StaticRef(sourceType->staticRefTarget(), std::move(copyRefValue)) :
						std::move(copyRefValue);
				
				const bool nextAllowBind = false;
				auto convertCast = ImplicitCastConvert(context, errors, std::move(copyStaticRefValue), destType, location, nextAllowBind);
				if (convertCast) {
					return convertCast;
				}
			}
			
			// Try to bind value to reference (e.g. T -> T&).
			if (allowBind && !sourceType->isLval() && !sourceType->isRef() && destType->isRef() &&
					destType->isBuiltInReference() &&
					doesPredicateImplyPredicate(context, sourceType->constPredicate(), destType->refTarget()->constPredicate()) &&
					isStructurallyEqual(sourceType, destType->refTarget())) {
				auto refValue = bindReference(context, value.copy());
				auto castResult = ImplicitCastConvert(context, errors, std::move(refValue), destType, location, allowBind);
				if (castResult) {
					return castResult;
				}
			}
			
			// Try a user cast.
			{
				auto castResult = ImplicitCastUser(context, errors, value.copy(), destType, location, allowBind);
				if (castResult) {
					return castResult;
				}
			}
			
			return Optional<AST::Value>();
		}

		class CannotImplicitlyCastTypeDiag : public Error {
		public:
			CannotImplicitlyCastTypeDiag(const AST::Type* sourceType, const AST::Type* destType)
			: message_(makeString("Can't implicitly cast type '%s' to type '%s'.",
			                      sourceType->toDiagString().c_str(), destType->toDiagString().c_str())) {}

			std::string toString() const { return message_; }

		private:
			std::string message_;
		};

		class CannotImplicitlyCastValueToTypeDiag : public Error {
		public:
			CannotImplicitlyCastValueToTypeDiag(const AST::Type* sourceType, const AST::Type* destType)
			: message_(makeString("Can't implicitly cast value of type '%s' to type '%s'.",
			                      sourceType->toDiagString().c_str(), destType->toDiagString().c_str())) {}

			std::string toString() const { return message_; }

		private:
			std::string message_;
		};

		class CastErrorDiag : public Error {
		public:
			CastErrorDiag(std::string message) : message_(std::move(message)) {}

			std::string toString() const { return message_; }

		private:
			std::string message_;
		};

		AST::Value ImplicitCast(Context& context, AST::Value value, const AST::Type* destType, const Debug::SourceLocation& location, bool formatOnly) {
			std::vector<std::string> errors;
			const auto valueKind = value.kind();
			const auto valueType = value.type();
			const bool allowBind = true;
			auto result = ImplicitCastConvert(context, errors, std::move(value), destType->resolveAliases(), location, allowBind, formatOnly);
			if (result) {
				return std::move(*result);
			}
			
			if (errors.empty()) {
				if (valueKind == AST::Value::CASTDUMMYOBJECT) {
					context.issueDiag(CannotImplicitlyCastTypeDiag(valueType, destType), location);
					throw SkipException();
				} else {
					context.issueDiag(CannotImplicitlyCastValueToTypeDiag(valueType, destType),
					                  location);
					throw SkipException();
				}
			} else {
				context.issueDiag(CastErrorDiag(errors.front()), location);
				throw SkipException();
			}
		}
		
		bool CanDoImplicitCast(Context& context, const AST::Type* sourceType, const AST::Type* destType, const Debug::SourceLocation& location) {
			const bool allowBind = true;
			const bool formatOnly = false;
			std::vector<std::string> errors;
			const auto result = ImplicitCastConvert(context, errors, AST::Value::CastDummy(sourceType), destType, location, allowBind, formatOnly);
			return result;
		}
		
		namespace {
			
			const AST::Type* getUnionDatatypeParent(const AST::Type* type) {
				while (type->isRef()) {
					type = type->refTarget();
				}
				
				if (!type->isDatatype()) {
					return nullptr;
				}
				
				if (type->getObjectType()->parentTypeInstance() == nullptr) {
					return nullptr;
				}
				
				return type->getObjectType()->parentTypeInstance()->selfType()->substitute(type->generateTemplateVarMap());
			}
			
		}
		
		const AST::Type* UnifyTypes(Context& context, const AST::Type* first, const AST::Type* second, const Debug::SourceLocation& location) {
			// Try to convert both types to their parent (if any).
			const auto firstParent = getUnionDatatypeParent(first);
			if (firstParent != nullptr &&
				CanDoImplicitCast(context, first, firstParent, location) &&
				CanDoImplicitCast(context, second, firstParent, location)) {
				return firstParent;
			}
			
			if (CanDoImplicitCast(context, first, second, location)) {
				return second;
			} else {
				return first;
			}
		}
		
	}
	
}





