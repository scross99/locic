#include "gtest/gtest.h"

#include <locic/Lex/Lexer.hpp>
#include <locic/Lex/NumericValue.hpp>

#include "MockCharacterSource.hpp"
#include "MockDiagnosticReceiver.hpp"

void testInteger(const std::string& text, const unsigned long long result) {
	locic::Array<locic::Lex::Character, 16> characters;
	for (const auto c: text) {
		characters.push_back(c);
	}
	
	locic::StringHost stringHost;
	MockCharacterSource source(std::move(characters));
	MockDiagnosticReceiver diagnosticReceiver;
	locic::Lex::Lexer lexer(source, diagnosticReceiver);
	const auto token = lexer.lexToken(stringHost);
	EXPECT_TRUE(source.empty());
	EXPECT_TRUE(diagnosticReceiver.hasNoErrorsOrWarnings());
	EXPECT_EQ(token.kind(), locic::Lex::Token::CONSTANT);
	EXPECT_EQ(token.constant().integerValue(), result);
}

TEST(NumericLexTest, Integer) {
	testInteger("0", 0);
	testInteger("1", 1);
	testInteger("2", 2);
	testInteger("3", 3);
	testInteger("4", 4);
	testInteger("5", 5);
	testInteger("6", 6);
	testInteger("7", 7);
	testInteger("8", 8);
	testInteger("9", 9);
	testInteger("10", 10);
	testInteger("123", 123);
}

TEST(NumericLexTest, HexInteger) {
	testInteger("0x0", 0);
	testInteger("0x1", 1);
	testInteger("0x2", 2);
	testInteger("0x3", 3);
	testInteger("0x4", 4);
	testInteger("0x5", 5);
	testInteger("0x6", 6);
	testInteger("0x7", 7);
	testInteger("0x8", 8);
	testInteger("0x9", 9);
	
	testInteger("0xa", 10);
	testInteger("0xb", 11);
	testInteger("0xc", 12);
	testInteger("0xd", 13);
	testInteger("0xe", 14);
	testInteger("0xf", 15);
	
	testInteger("0xA", 10);
	testInteger("0xB", 11);
	testInteger("0xC", 12);
	testInteger("0xD", 13);
	testInteger("0xE", 14);
	testInteger("0xF", 15);
	
	testInteger("0x10", 16);
	testInteger("0x11", 17);
	testInteger("0x12", 18);
	testInteger("0x13", 19);
	testInteger("0x14", 20);
}

void testFloat(const std::string& text, const double result) {
	locic::Array<locic::Lex::Character, 16> characters;
	for (const auto c: text) {
		characters.push_back(c);
	}
	
	locic::StringHost stringHost;
	MockCharacterSource source(std::move(characters));
	MockDiagnosticReceiver diagnosticReceiver;
	locic::Lex::Lexer lexer(source, diagnosticReceiver);
	const auto token = lexer.lexToken(stringHost);
	EXPECT_TRUE(source.empty());
	EXPECT_TRUE(diagnosticReceiver.hasNoErrorsOrWarnings());
	EXPECT_EQ(token.kind(), locic::Lex::Token::CONSTANT);
	EXPECT_EQ(token.constant().floatValue(), result);
}

TEST(NumericLexTest, Float) {
	testFloat("1.0", 1.0);
	testFloat("1.1", 1.1);
	testFloat("1.2", 1.2);
	testFloat("1.3", 1.3);
	testFloat("1.4", 1.4);
	testFloat("1.5", 1.5);
	testFloat("1.6", 1.6);
	testFloat("1.7", 1.7);
	testFloat("1.8", 1.8);
	testFloat("1.9", 1.9);
	testFloat("123.2", 123.2);
}

TEST(NumericLexTest, DISABLED_FloatEmptyIntegerPart) {
	testFloat(".0", 0.0);
	testFloat(".1", 0.1);
	testFloat(".2", 0.2);
	testFloat(".3", 0.3);
	testFloat(".4", 0.4);
	testFloat(".5", 0.5);
	testFloat(".6", 0.6);
	testFloat(".7", 0.7);
	testFloat(".8", 0.8);
	testFloat(".9", 0.9);
	testFloat(".123", 0.123);
}

TEST(NumericLexTest, FloatEmptyFractionalPart) {
	testFloat("0.", 0.0);
	testFloat("1.", 1.0);
	testFloat("2.", 2.0);
	testFloat("3.", 3.0);
	testFloat("4.", 4.0);
	testFloat("5.", 5.0);
	testFloat("6.", 6.0);
	testFloat("7.", 7.0);
	testFloat("8.", 8.0);
	testFloat("9.", 9.0);
	testFloat("10.", 10.0);
	testFloat("20.", 20.0);
}
