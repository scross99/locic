#ifndef LOCIC_AST_VALUE_H
#define LOCIC_AST_VALUE_H

#include <Locic/AST/List.h>
#include <Locic/AST/Var.h>

typedef enum AST_ConstantType{
	AST_CONSTANT_BOOL,
	AST_CONSTANT_INT,
	AST_CONSTANT_FLOAT
} AST_ConstantType;

typedef struct AST_Constant{
	AST_ConstantType type;
	
	union{
		int boolConstant;
		int intConstant;
		float floatConstant;
	};
} AST_Constant;

typedef struct AST_VarAccess{
	AST_Var * var;
} AST_VarAccess;

typedef enum AST_UnaryType{
	AST_UNARY_PLUS,
	AST_UNARY_MINUS,
	AST_UNARY_ADDRESSOF,
	AST_UNARY_DEREF,
	AST_UNARY_NEGATE
} AST_UnaryType;
	
typedef struct AST_Unary{
	AST_UnaryType type;
	struct AST_Value * value;
} AST_Unary;

typedef enum AST_BinaryType{
	AST_BINARY_ADD,
	AST_BINARY_SUBTRACT,
	AST_BINARY_MULTIPLY,
	AST_BINARY_DIVIDE,
	AST_BINARY_ISEQUAL,
	AST_BINARY_NOTEQUAL,
	AST_BINARY_GREATEROREQUAL,
	AST_BINARY_LESSOREQUAL,
} AST_BinaryType;

typedef struct AST_Binary{
	AST_BinaryType type;
	struct AST_Value * left;
	struct AST_Value * right;
} AST_Binary;

typedef struct AST_Ternary{
	struct AST_Value * condition;
	struct AST_Value * ifTrue;
	struct AST_Value * ifFalse;
} AST_Ternary;

typedef struct AST_Construct{
	char * typeName;
	char * constructorName; // NULL for default constructor.
	AST_List * parameters;
} AST_Construct;

typedef enum AST_ValueType{
	AST_VALUE_CONSTANT,
	AST_VALUE_VARACCESS,
	AST_VALUE_UNARY,
	AST_VALUE_BINARY,
	AST_VALUE_TERNARY,
	AST_VALUE_CONSTRUCT
} AST_ValueType;
	
typedef struct AST_Value{
	AST_ValueType type;
			
	union{
		AST_Constant constant;
		AST_VarAccess varAccess;
		AST_Unary unary;
		AST_Binary binary;
		AST_Ternary ternary;
		AST_Construct construct;
	};
} AST_Value;

AST_Value * AST_MakeBoolConstant(int val);

AST_Value * AST_MakeIntConstant(int val);

AST_Value * AST_MakeFloatConstant(float val);

AST_Value * AST_MakeVarAccess(AST_Var * var);

AST_Value * AST_MakeUnary(AST_UnaryType type, AST_Value * operand);

AST_Value * AST_MakeBinary(AST_BinaryType type, AST_Value * left, AST_Value * right);

AST_Value * AST_MakeTernary(AST_Value * cond, AST_Value * ifTrue, AST_Value * ifFalse);

AST_Value * AST_MakeConstruct(char * typeName, char * constructorName, AST_List * parameters);

AST_Value * AST_MakeMemberAccess(AST_Value * object, char * memberName);

AST_Value * AST_MakeMethodCall(AST_Value * object, char * methodName, AST_List * parameters);

#endif
