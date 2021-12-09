#include "opcode.h"

const char * OpCode_Str[] = {
	"MOV",

	"SETNIL",
	"SETFALSE",
	"SETTRUE",

	"LOADK",

	"ADD",
	"MINUS",
	"MULTI",
	"DIV",

	"REMAINDER",

	"And",
	"OR",
	"CONCATENATE",

	"GT",
	"LT",
	"GE",
	"LE",
	"EQ",
	"NE",

	"NOT",

	"LEN",

	"CMP",
	"JMP",
	"JTRUE",
	"JFALSE",

	"JGT",
	"JLT",
	"JGE",
	"JLE",
	"JEQ",
	"JNE",

	"GETGLOBAL",
	"SETGLOBAL",
	"SETGLOBALNIL",

	"INIT_CALL",
	"CALL",
	
	"RETURN",

	"CLOSE",

	"CLOSURE",

	"GETUPVAL",
	"SETUPVAL",
	"SETUPVALNIL",

	"GETTABLE",
	"SETTABLE",
	"SETTABLE NEXT NUMBER",
	"SETTABLE NIL",
	"NEWTABLE",
	
	"VALUE",

	"FOREACH BEGIN",
	"FOREACH",
	"FOREACH NO KEY",
};

const struct OpInfo OpCode_Info[] = {
	{1, 1, 2, 0, 0},// MOV

	{1, 1, 1, 2, 0},// SetNil
	{1, 1, 1, 0, 0},// SetFalse
	{1, 1, 1, 0, 0},// SetTrue

	{1, 1, 2, 0, 0}, // LoadK

	{1, 3, 1, 2, 0}, // Add
	{1, 3, 1, 2, 0}, // Minus
	{1, 3, 1, 2, 0}, // Multi
	{1, 3, 1, 2, 0}, // Div

	{1, 3, 1, 2, 0}, // Remainder

	{1, 3, 1, 2, 0}, // And
	{1, 3, 1, 2, 0}, // Or

	{1, 3, 1, 2, 0}, // Concatenate

	{1, 3, 1, 2, 0}, // GT
	{1, 3, 1, 2, 0}, // LT
	{1, 3, 1, 2, 0}, // GE
	{1, 3, 1, 2, 0}, // LE
	{1, 3, 1, 2, 0}, // EQ
	{1, 3, 1, 2, 0}, // NE

	{1, 1, 1, 0, 0}, // NOT
	{1, 1, 1, 0, 0}, // LEN

	{1, 3, 1, 2, 0}, // CMP

	{0, 0, 0, 0, 0}, // JMP
	{0, 0, 1, 0, 0}, // JTRUE
	{0, 0, 1, 0, 0}, // JFALSE

	{0, 0, 1, 2, 0}, // JGT
	{0, 0, 1, 2, 0}, // JLT
	{0, 0, 1, 2, 0}, // JGE
	{0, 0, 1, 2, 0}, // JLE
	{0, 0, 1, 2, 0}, // JEQ
	{0, 0, 1, 2, 0}, // JNE

	{1, 3, 2, 0, 0}, // GetGlobal
	{0, 0, 2, 0, 0}, // SetGlobal
	{0, 0, 2, 0, 0}, // SetGlobalNil
	
	{0, 0, 1, 0, 0}, // InitCall
	{0, 0, 1, 0, 0}, // CAll

	{0, 0, 1, 2, 0}, // Return
	{0, 0, 0, 0, 0}, // Close

	{1, 2, 1, 0, 0}, // CLOSURE

	{1, 3, 2, 0, 0}, // GetUpVal
	{0, 0, 2, 0, 0}, // SetUpVal
	{0, 0, 2, 0, 0}, // SetUpValNil

	{1, 3, 1, 2, 0}, // GetTable
	{0, 0, 1, 2, 3}, // SetTable
	{0, 0, 1, 2, 3}, // SetTableNum
	{0, 0, 1, 2, 0}, // SetTableNil

	{1, 3, 0, 0, 0}, // NewTable	-- NewTable 的优化要复杂些
	{1, 1, 1, 0, 0}, // Value

	{0, 0, 1, 0, 0}, // ForeachBegin
	{0, 0, 1, 2, 3}, // Foreach
	{0, 0, 1, 2, 3}, // Foreach_Nokey

};

int OpCode_get_output(Instruction i)
{
	int op = GetOP(i);
	if(!OpCode_Info[op].output) {
		return -1;
	}

	switch(OpCode_Info[op].output_reg) {
		case 1:
			return GetA(i);

		case 2:
			return GetB(i);

		case 3:
			return GetC(i);

		case 4:
			return GetBx(i);

		default:
			break;
	}

	return -1;
}

#define generate_get_input(x)	\
int OpCode_get_input##x(Instruction i)	\
{	\
	int op = GetOP(i);	\
	switch(OpCode_Info[op].input##x) {	\
		case 1:	\
			return GetA(i);	\
		case 2:	\
			return GetB(i);	\
		case 3:	\
			return GetC(i);	\
		case 4:	\
			return GetBx(i);\
		default:	\
			break;	\
	}	\
	return -1;	\
}

generate_get_input(1)
generate_get_input(2)
generate_get_input(3)
