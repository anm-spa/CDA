/****************************************************************/
/*          All rights reserved (it will be changed)            */
/*          masud.abunaser@mdh.se                               */
/****************************************************************/

#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Sema/Sema.h"
#include "clang/Basic/LangOptions.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <ctime>
#include <sstream>
#include <string>
#include <algorithm>
#include <chrono>

using namespace std;
using namespace std::chrono;

using namespace llvm;
using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;

#include "commandOptions.h"
#include "util.h"
#include "cdAnalysis.h"



CDAnalysisInfoType analResults;


class CFGVisitor : public RecursiveASTVisitor<CFGVisitor> {
public:
    CFGVisitor(ASTContext &C,unsigned dl,unsigned alg,std::string proc) : context(C) {_dl=dl;_algt=alg;_proc=proc;}
    
    void TraverseCFG(std::unique_ptr<CFG> &cfg)
    {
    }
    
    void ComputeandPrintCDG(FunctionDecl *f)
    {
         // a brute force method to compute the CDG using WCC. This method is not properly tested
          Stmt *funcBody = f->getBody();
          std::unique_ptr<CFG> sourceCFG = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
          unsigned size=sourceCFG->size();
          unsigned exit=sourceCFG->getExit().getBlockID();
          for(int i=0;i<size;i++)
          {
              std::unique_ptr<CFG> sourceCFG= CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
              std::set<unsigned> N;
              N.insert(i);
              N.insert(exit);
              CDAnalysis *cdanal=new CDAnalysis(std::move(sourceCFG),context,N);
              std::set<unsigned> W=cdanal->computeWCCExt(N);
              if(!W.empty()){
              llvm::errs()<<" node: "<< i<<" CD: ";
              for(auto v:W) llvm::errs()<<v<<" ";
                 llvm::errs()<<"\n";
              }
          }
      }
    
    
   void computeWCCfast(std::unique_ptr<CFG> cfg, std::set<unsigned> np, std::string proc)
    {
        // Compute WCC Efficiently (SAS and JSS algorithms)
        ExecTime<std::chrono::microseconds> timerWCCFast;
        CDAnalysis *cdanal=new CDAnalysis(std::move(cfg),context,np);
        std::set<unsigned> wccFast=cdanal->computeWCCExt(np);
        unsigned long etimeWCCFast=timerWCCFast.stop();
        analResults.storeWCCFastInfo(proc,etimeWCCFast,wccFast);
    }
    
    void computeSCCfast(std::unique_ptr<CFG> cfg, std::set<unsigned> np, std::string proc)
     {
       
         // Compute SCC Efficiently (SAS and JSS algorithms)
         ExecTime<std::chrono::microseconds> timerSCCFast;
         CDAnalysis *cdanal=new CDAnalysis(std::move(cfg),context,np);
         std::set<unsigned> sccFast=cdanal->computeSCC(np);
         unsigned long etimeSCCFast=timerSCCFast.stop();
         analResults.storeSCCFastInfo(proc,etimeSCCFast,sccFast);

     }
    void computeWCCDanicic(std::unique_ptr<CFG> cfg, std::set<unsigned> np, std::string proc)
    {
        ExecTime<std::chrono::microseconds> timerWCCDanicic;
        CDAnalysisDanicic *cDanicic=new CDAnalysisDanicic(std::move(cfg),context,np);
        std::set<unsigned> wccDanicic=cDanicic->computeWCCDanicic(np);
        unsigned long etimeWCCDanicic=timerWCCDanicic.stop();
        analResults.storeWCCDanicicInfo(proc,etimeWCCDanicic,wccDanicic);
    }

    void computeSCCDanicic(std::unique_ptr<CFG> cfg, std::set<unsigned> np, std::string proc)
    {
        ExecTime<std::chrono::microseconds> timerSCCDanicic;
        CDAnalysisDanicic *cDanicic=new CDAnalysisDanicic(std::move(cfg),context,np);
        std::set<unsigned> sccDanicic=cDanicic->computeSCCDanicic(np);
        unsigned long etimeSCCDanicic=timerSCCDanicic.stop();
        analResults.storeSCCDanicicInfo(proc,etimeSCCDanicic,sccDanicic);
    }
    
    void ComputeAndCompareCDComputation(FunctionDecl *f)
    {
        std::srand (std::time (0));
        Stmt *funcBody = f->getBody();
        std::unique_ptr<CFG> sourceCFG = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
        unsigned size=sourceCFG->size();
        std::set<unsigned> vDefs;
        unsigned npsize=std::min(rand()%size+2,size-1);
        vDefs=getSetOfRandNumber(0,size-1,npsize);
        std::string proc_name=f->getNameInfo().getAsString();
        analResults.storeProcInfo(proc_name,size, vDefs);
      
        // Compute WCC and SCC Efficiently (SAS and JSS algorithms)
        ExecTime<std::chrono::microseconds> timerWCCFast;
        CDAnalysis *cdanal=new CDAnalysis(std::move(sourceCFG),context,vDefs);
        std::set<unsigned> wccFast=cdanal->computeWCCExt(vDefs);
        unsigned long etimeWCCFast=timerWCCFast.stop();
        analResults.storeWCCFastInfo(proc_name,etimeWCCFast,wccFast);
        
        if(_dl==DEBUG_MAX)
            cdanal->printGraph();
        
        if(_dl==DEBUG_MAX)
        {
            //The following function was used in the SAS conference version
            std::set<unsigned> wccFastSAS=cdanal->computeWCCFixpoint(vDefs);
            std::set<unsigned> diff1, diff2;
            std::set_difference(wccFast.begin(), wccFast.end(), wccFastSAS.begin(), wccFastSAS.end(),
            std::inserter(diff1, diff1.begin()));
            std::set_difference(wccFastSAS.begin(), wccFastSAS.end(), wccFast.begin(), wccFast.end(),
            std::inserter(diff2, diff2.begin()));
            if(diff1.size()>0 || diff2.size()>0)
                debug_print("Error: WCC computations in SAS and JSS are not producing equal results\n",_dl,DEBUG_MAX);
            else debug_print("WCC computations in SAS and JSS are producing EQUAL RESULTS\n",_dl,DEBUG_MAX);
        }
        
        ExecTime<std::chrono::microseconds> timerSCCFast;
        std::set<unsigned> sccFast=cdanal->computeSCC(vDefs);
        unsigned long etimeSCCFast=timerSCCFast.stop();
        analResults.storeSCCFastInfo(proc_name,etimeSCCFast,sccFast);

        
        // Compute WCC and SCC using Danicic's algorithms)

        std::unique_ptr<CFG> sCFG = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
        ExecTime<std::chrono::microseconds> timerWCCDanicic;
        CDAnalysisDanicic *cDanicic=new CDAnalysisDanicic(std::move(sCFG),context,vDefs);
        std::set<unsigned> wccDanicic=cDanicic->computeWCCDanicic(vDefs);
        unsigned long etimeWCCDanicic=timerWCCDanicic.stop();
        analResults.storeWCCDanicicInfo(proc_name,etimeWCCDanicic,wccDanicic);

        
        ExecTime<std::chrono::microseconds> timerSCCDanicic;
        std::set<unsigned> sccDanicic=cDanicic->computeSCCDanicic(vDefs);
        unsigned long etimeSCCDanicic=timerSCCDanicic.stop();
        analResults.storeSCCDanicicInfo(proc_name,etimeSCCDanicic,sccDanicic);
    }
    
    bool VisitFunctionDecl(FunctionDecl *f) {
        clang::SourceManager &sm(context.getSourceManager());
        if(!sm.isInMainFile(sm.getExpansionLoc(f->getBeginLoc())))
            return true;
        if(_proc!="none" && f->getNameInfo().getAsString()!=_proc) return true;
        
        if (f->hasBody()) {
            Stmt *funcBody = f->getBody();
            std::unique_ptr<CFG> cfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
            std::unique_ptr<CFG> wcfg;
            std::srand (std::time (0));
            unsigned size=cfg->size();
            std::set<unsigned> np;
            unsigned npsize=std::min(rand()%size+2,size-1);
            np=getSetOfRandNumber(0,size-1,npsize);
            std::string proc_name=f->getNameInfo().getAsString();
            
            if(_proc!="none" && f->getNameInfo().getAsString()==_proc && _dl==2)
            {
                cfg->viewCFG(LangOptions());
                std::cout<<"CFG has nodes numbered from 0 to "<<cfg->size()<<". Enter the Np set (ended by a negative number) or press enter:\n";
                std::set<unsigned> data;
                std::string text_line;
                std::getline(std::cin, text_line);
                std::istringstream input_stream(text_line);
                int value;
                while (input_stream >> value)
                {
                    if(value>=0 && value < size)
                       data.insert(value);
                    else break;
                }
                if(!data.empty())
                    np=data;
                ComputeandPrintCDG(f);
            }
            analResults.storeProcInfo(proc_name,size, np);
            switch(_algt){
                case 0://ComputeAndCompareCDComputation(f);
                        computeWCCfast(std::move(cfg),np,proc_name);
                        wcfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
                        computeWCCDanicic(std::move(wcfg),np,proc_name);
                        wcfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
                        computeSCCfast(std::move(wcfg),np,proc_name);
                        wcfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
                        computeSCCDanicic(std::move(wcfg),np,proc_name);
                        break;
                case 1:computeWCCfast(std::move(cfg),np,proc_name);
                       wcfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
                       computeWCCDanicic(std::move(wcfg),np,proc_name);
                       break;
                case 2:computeSCCfast(std::move(cfg),np,proc_name);
                       wcfg = CFG::buildCFG(f, funcBody, &context, CFG::BuildOptions());
                       computeSCCDanicic(std::move(wcfg),np,proc_name);
                       break;
                case 3:computeWCCfast(std::move(cfg),np,proc_name);
                    break;
                case 4:computeWCCDanicic(std::move(cfg),np,proc_name);
                    break;
                case 5:computeSCCfast(std::move(cfg),np,proc_name);
                    break;
                case 6:computeSCCDanicic(std::move(cfg),np,proc_name);
                    break;
            }
        }
        return true;
    }

    
private:
    ASTContext &context;
    std::map<unsigned, CFGBlock*> idToBlock;
    unsigned _dl;
    unsigned _algt;
    std::string _proc;
};


class CFGAstConsumer : public ASTConsumer {
public:
    CFGAstConsumer(ASTContext &C,unsigned dl,unsigned alg,std::string proc) : Visitor(C,dl,alg,proc) {}
    
    // Override the method that gets called for each parsed top-level declaration.
    bool HandleTopLevelDecl(DeclGroupRef DR) override {
        for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
            // Traverse the declaration using our AST visitor.
            Visitor.TraverseDecl(*b);
        }
        return true;
    }
    
private:
    CFGVisitor Visitor;
};


class CFGFrontendAction : public ASTFrontendAction {
    unsigned _dl;
    unsigned _algt;
    std::string _proc;
 public:
    CFGFrontendAction(unsigned dl,unsigned alg,std::string proc) {_dl=dl;_algt=alg;_proc=proc;}
    
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) {
        return make_unique<CFGAstConsumer>(CI.getASTContext(),_dl,_algt,_proc);
    }
};

class CFGFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    unsigned _dl;
    unsigned _algt;
    std::string _proc;
public:
    CFGFrontendActionFactory (unsigned dl,unsigned alg,std::string proc)
    : _dl (dl), _algt(alg), _proc(proc) { }
    
    virtual std::unique_ptr<clang::FrontendAction> create () {
        return std::make_unique<CFGFrontendAction> (_dl,_algt,_proc);
    }
};


int main(int argc,  const char **argv) {
    CommonOptionsParser op(argc, argv, MainCat);
    std::string Path,AbsoluteFN;
    SplitFilename(argv[1],Path,AbsoluteFN);
    analResults.setFileName(AbsoluteFN);
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    CFGFrontendActionFactory cfgFact(DL,AlgType,ProcName);
    Tool.run(&cfgFact);
    analResults.showResults(DL);
    return 1;
}
