#ifndef LLVM_CLANG_COMMANDOPTIONS_H
#define LLVM_CLANG_COMMANDOPTIONS_H

#include "clang/Tooling/CommonOptionsParser.h"

static cl::OptionCategory MainCat("Perform static analysis");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...");

enum DLevel {
    none,l1,l2
};
enum AType {
    ALL, WCC,SCC,WCCfast,WCCdan,SCCfast,SCCdan
};
#define DEBUG_MAX 3
cl::opt<DLevel> DL("dl", cl::desc("Choose Debug level:"),
                           cl::values(
                                      clEnumValN(none, "l0", "Print no debugging info"),
                                      clEnumValN(l1, "l1", "Print common debug info"),
                                      clEnumValN(l2, "l2", "Print extended debug info")), cl::cat(MainCat));

cl::opt<AType> AlgType("alg", cl::desc("Choose Algorithms:"),
                       cl::values(
                                  clEnumVal(ALL, "Compute both WCC and SCC using the fast and the Danicic's methods and compare the results"),
                                  clEnumVal(WCC, "Compute WCC using the fast and the Danicic's methods and compare the results"),
                                  clEnumVal(SCC, "Compute SCC using the fast and the Danicic's methods and compare the results"),
                                  clEnumVal(WCCfast, "Compute WCC using the fast method"),
                                  clEnumVal(WCCdan, "Compute WCC using the Danicic's method"),
                                  clEnumVal(SCCfast, "Compute SCC using the fast method"),
                                  clEnumVal(SCCdan, "Compute SCC using the Danicic's method")), cl::cat(MainCat));

cl::opt<string> ProcName("proc", cl::desc("Specify function name to be analyzed"), cl::value_desc("function name"), cl::init("none"));

#endif
