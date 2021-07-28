//
//  cdAnalysis.h
//  LLVM
//
//  Created by Abu Naser Masud on 2020-04-17.
//

#ifndef cdAnalysis_h
#define cdAnalysis_h

#include <algorithm>
#include <ctime>
#include <chrono>
#include "util.h"
#include <iostream>
#include <fstream>

using namespace std::chrono;
using namespace llvm;
using namespace clang;


class CDAnalysis
{
private:
    std::unique_ptr<clang::CFG> cfg;
    clang::ASTContext &context;
    typedef std::pair<std::set<unsigned>,std::set<unsigned>> edgeType;
    std::map<unsigned, edgeType> graphCDNet;
    std::map<unsigned,unsigned> graphRank;
    unsigned nodeSplitted;
    std::map<unsigned,unsigned> splitMap,splitMapRev;
    std::map<unsigned, CFGBlock*> CFGIdMap;
    std::set<unsigned> defs;
    llvm::BitVector DefVect, DefVectExt,LeaveVect;
    std::map<unsigned,std::set<unsigned>> varToPhiNodes;
    unsigned nextRank;
    std::set<unsigned> domTreeLeaves;
    
public:
    CDAnalysis(ASTContext &C):context(C){}
    CDAnalysis(std::unique_ptr<clang::CFG> &&CFG, ASTContext &C, std::set<unsigned> vDefs);
    void printGraph();
    std::set<unsigned> checkReachability(llvm::BitVector wccPre,std::set<unsigned> Defs);
    std::set<unsigned> computeWCC(std::set<unsigned> Defs, unsigned long &etime, unsigned long &etime1);
    std::set<unsigned> computeWCCExt(std::set<unsigned> Defs);
    std::set<unsigned> computeWCCFixpoint(std::set<unsigned> Defs);
    std::set<unsigned> computeIncrementalWD(std::set<unsigned> Defs, std::map<unsigned,unsigned > &Pre,std::set<unsigned> AllX);
    std::set<unsigned> computeGamma(std::set<unsigned> Np, llvm::BitVector &V);
    std::set<unsigned> computeSCC(std::set<unsigned> Np);
    std::set<unsigned> computeTheta(std::set<unsigned> Vp, unsigned u);
    std::set<std::pair<unsigned,unsigned> > findAllReachableEdges(std::set<unsigned> Vp);
    std::set<unsigned> filterOutUnreachablenodes(std::set<unsigned> X,std::set<unsigned> Gamma);
    void printGraph(std::map<unsigned, edgeType> graph);
    bool existInitWitness(std::map<unsigned, edgeType> solnGraph,std::set<unsigned> startNodes, unsigned diffFrom, llvm::BitVector Res);
    bool existInitWitnessEff(std::map<unsigned, edgeType> solnGraph,std::set<unsigned> startNodes, unsigned diffFrom, llvm::BitVector Res);
    llvm::BitVector excludeImpreciseResults(std::set<unsigned> wcc,std::set<unsigned> Defs, std::map<unsigned,unsigned > &Pre);
};


CDAnalysis::CDAnalysis(std::unique_ptr<clang::CFG> &&CFG, ASTContext &C, std::set<unsigned> vDefs):context(C)
{
    cfg=std::move(CFG);
    for (auto *Block : *cfg){
        CFGIdMap[Block->getBlockID()]=Block;
    }
    DefVect=llvm::BitVector(cfg->size());
    for(auto v:vDefs)
        DefVect[v]=true;
}


std::set<unsigned> CDAnalysis::checkReachability(llvm::BitVector wccPre, std::set<unsigned> Defs)
{
    std::queue<unsigned> Q;
    for(auto n:Defs){
        Q.push(n);
    }
    
    std::set<unsigned> wcc;
    llvm::BitVector V(cfg->size());
    while(!Q.empty())
    {
        unsigned n=Q.front();
        Q.pop();
        V[n]=true;
        if(wccPre[n]) wcc.insert(n);
        CFGBlock* B=CFGIdMap[n];
        if(!B) continue;
        CFGBlock::succ_iterator ib=B->succ_begin();
        CFGBlock::succ_iterator ie=B->succ_end();
        for(;ib!=ie;ib++){
            if(!(*ib)) continue;
            if(!V[(*ib)->getBlockID()])
                Q.push((*ib)->getBlockID());
        }
    }
    return wcc;
}

std::set<unsigned>  CDAnalysis::computeWCC(std::set<unsigned> Defs, unsigned long &etime, unsigned long &etime1)
{
    ExecTime<std::chrono::microseconds> timer;
    
    std::queue<unsigned> Q;
    std::map<unsigned,unsigned > Pre;
    unsigned UNINITVAL=cfg->size()+10;
    std::set<unsigned> wcc;
    llvm::BitVector V(cfg->size());
    
    for(auto *B:*cfg)
    {
        if(!B) continue;
        Pre[B->getBlockID()]=UNINITVAL;
    }
    
    for(auto n:Defs)
    {
        Pre[n]=n;
        Q.push(n);
        V[n]=true;
    }
    
    while(!Q.empty())
    {
        unsigned currNode=Q.front();
        Q.pop();
        V[currNode]=false;
        
        CFGBlock *B=CFGIdMap[currNode];
        CFGBlock::pred_iterator pb=B->pred_begin();
        CFGBlock::pred_iterator pe=B->pred_end();
       
        for(;pb!=pe;pb++){
            if(!(*pb)) continue;
            unsigned n=(*pb)->getBlockID();
            std::set<unsigned> S;
            CFGBlock::succ_iterator ib=(*pb)->succ_begin();
            CFGBlock::succ_iterator ie=(*pb)->succ_end();
            for(;ib!=ie;ib++){
                if(!(*ib)) continue;
                unsigned m=(*ib)->getBlockID();
                if(Pre[m]==UNINITVAL)
                    continue;
                else S.insert(Pre[m]);
            }
            if(S.size()>1)
            {
                if(!V[n] && Pre[n]!=n)
                {
                    Q.push(n);
                    V[n]=true;
                }
                Pre[n]=n;
                wcc.insert(n);
            }
            else if(S.size()<2)
            {
                unsigned oldVal=Pre[n];
                if(DefVect[n] || Pre[n]==n) continue;
                else Pre[n]=*S.begin();
                if(!V[n] && oldVal!=Pre[n]){
                    Q.push(n);
                    V[n]=true;
                }
            }
        }
    }
    
    etime=timer.stop();
    ExecTime<std::chrono::microseconds> timer1;
    llvm::BitVector wccNode=excludeImpreciseResults(wcc,Defs,Pre);
    etime1=timer1.stop();
    std::set<unsigned> wccFinal=checkReachability(wccNode,Defs);
    return wccFinal;
}


std::set<unsigned>  CDAnalysis::computeWCCFixpoint(std::set<unsigned> Defs)
{
    std::queue<unsigned> Q;
    std::map<unsigned,std::set<std::pair<unsigned,unsigned> >> Pre;
    unsigned UNINITVAL=cfg->size()+10;
    std::set<unsigned> wcc;
    llvm::BitVector V(cfg->size());
    for(auto *B:*cfg)
    {
        if(!B) continue;
        Pre[B->getBlockID()]=std::set<std::pair<unsigned,unsigned>>();
    }
    for(auto n:Defs)
    {
        Pre[n].insert({n,0});
        Q.push(n);
        V[n]=true;
    }
    while(!Q.empty())
    {
        unsigned currNode=Q.front();
        Q.pop();
        V[currNode]=false;
        
        CFGBlock *B=CFGIdMap[currNode];
        CFGBlock::pred_iterator pb=B->pred_begin();
        CFGBlock::pred_iterator pe=B->pred_end();
       
        for(;pb!=pe;pb++){
            if(!(*pb)) continue;
            unsigned n=(*pb)->getBlockID();
            std::set<std::pair<unsigned,unsigned>> S;
            std::set<unsigned> Sx;
            CFGBlock::succ_iterator ib=(*pb)->succ_begin();
            CFGBlock::succ_iterator ie=(*pb)->succ_end();
            for(;ib!=ie;ib++){
                if(!(*ib)) continue;
                unsigned m=(*ib)->getBlockID();
                if(!Pre[m].empty()){
                   S.insert(*Pre[m].begin());
                   Sx.insert((*Pre[m].begin()).first);
                }
            }
            std::set<std::pair<unsigned,unsigned>> Y;
            if(Defs.find(n)!=Defs.end())
               Y.insert({n,0});
            else if(Sx.size()>1)
               Y.insert({n,0});
            else if(Sx.size()==1){
                unsigned min=UNINITVAL;
                for(auto x:S)
                {
                    if(x.first==*Sx.begin() && min>x.second)
                        min=x.second;
                }
                Y.insert({*Sx.begin(),min+1});
            }
            else Y=std::set<std::pair<unsigned,unsigned>>();
            
            if(Sx.size()>1)
            {
                wcc.insert(n);
            }
            if(Y.empty()) continue;
            else if(Pre[n].empty() && !Y.empty()) {
                Pre[n]=Y;
                if(!V[n]){
                   Q.push(n);
                   V[n]=true;
                }
            }
            else{
                   std::pair<unsigned,unsigned> x=*Y.begin(), y=*Pre[n].begin();
                   bool F1=(y.first==n);
                   bool F2=(y.first!=n), F3=(x.first!=n), F4=(y.second <= x.second);
                   if(!(F1 || ( F2 && F3 && F4))){
                      Pre[n]=Y;
                      if(!V[n]){
                       Q.push(n);
                       V[n]=true;
                      }
                    }
              }
       }
    }
    
    std::map<unsigned,unsigned > Prem;
    for(auto *B:*cfg)
    {
        if(!B) continue;
        unsigned n=B->getBlockID();
        if((Pre[n]).size()==0) Prem[n]=UNINITVAL;
        else Prem[n]=(*Pre[n].begin()).first;
    }
    llvm::BitVector wccNode=excludeImpreciseResults(wcc,Defs,Prem);
    std::set<unsigned> wccFinal=checkReachability(wccNode,Defs);
    return wccFinal;
}


std::set<unsigned>  CDAnalysis::computeWCCExt(std::set<unsigned> Defs)
{
    std::queue<unsigned> Q;
    std::map<unsigned,unsigned > Pre;
    unsigned UNINITVAL=cfg->size()+10;
    std::set<unsigned> wcc;
    llvm::BitVector V(cfg->size());
    for(auto *B:*cfg)
    {
        if(!B) continue;
        Pre[B->getBlockID()]=UNINITVAL;
    }
    
    for(auto n:Defs)
    {
        Pre[n]=n;
        Q.push(n);
        V[n]=true;
    }
    
    while(!Q.empty())
    {
        unsigned currNode=Q.front();
        Q.pop();
        V[currNode]=false;
        
        CFGBlock *B=CFGIdMap[currNode];
        CFGBlock::pred_iterator pb=B->pred_begin();
        CFGBlock::pred_iterator pe=B->pred_end();
       
        for(;pb!=pe;pb++){
            if(!(*pb)) continue;
            unsigned n=(*pb)->getBlockID();
            std::set<unsigned> S;
            CFGBlock::succ_iterator ib=(*pb)->succ_begin();
            CFGBlock::succ_iterator ie=(*pb)->succ_end();
            for(;ib!=ie;ib++){
                if(!(*ib)) continue;
                unsigned m=(*ib)->getBlockID();
                if(Pre[m]==UNINITVAL)
                    continue;
                else S.insert(Pre[m]);
            }
            
            if(S.size()>1)
            {
                if(!V[n] && Pre[n]!=n)
                {
                    Q.push(n);
                    V[n]=true;
                }
                Pre[n]=n;
                wcc.insert(n);
            }
            else if(S.size()<2)
            {
                unsigned oldVal=Pre[n];
                if(DefVect[n] || Pre[n]==n) continue;
                else Pre[n]=*S.begin();
                if(!V[n] && oldVal!=Pre[n]){
                    Q.push(n);
                    V[n]=true;
                }
            }
        }
    }
    llvm::BitVector wccNode=excludeImpreciseResults(wcc,Defs,Pre);
    std::set<unsigned> wccFinal=checkReachability(wccNode,Defs);
    return wccFinal;
}

std::set<unsigned>  CDAnalysis::computeIncrementalWD(std::set<unsigned> Defs, std::map<unsigned,unsigned > &Pre, std::set<unsigned> AllX)
{
    std::queue<unsigned> Q;
    unsigned UNINITVAL=cfg->size()+10;
    std::set<unsigned> wcc;
    llvm::BitVector V(cfg->size());
    for(auto n:Defs)
    {
        Pre[n]=n;
        Q.push(n);
        V[n]=true;
    }
    
    while(!Q.empty())
    {
        unsigned currNode=Q.front();
        Q.pop();
        V[currNode]=false;
        
        CFGBlock *B=CFGIdMap[currNode];
        CFGBlock::pred_iterator pb=B->pred_begin();
        CFGBlock::pred_iterator pe=B->pred_end();
        
        for(;pb!=pe;pb++){
            if(!(*pb)) continue;
            unsigned n=(*pb)->getBlockID();
            std::set<unsigned> S;
            CFGBlock::succ_iterator ib=(*pb)->succ_begin();
            CFGBlock::succ_iterator ie=(*pb)->succ_end();
            for(;ib!=ie;ib++){
                if(!(*ib)) continue;
                unsigned m=(*ib)->getBlockID();
                if(Pre[m]==UNINITVAL)
                    continue;
                else S.insert(Pre[m]);
            }
            
            if(S.size()>1)
            {
                if(!V[n] && Pre[n]!=n)
                {
                    Q.push(n);
                    V[n]=true;
                }
                Pre[n]=n;
                wcc.insert(n);
            }
            else if(S.size()<2)
            {
                unsigned oldVal=Pre[n];
                if(Pre[n]==n) continue;
                else Pre[n]=*S.begin();
                if(!V[n] && oldVal!=Pre[n]){
                    Q.push(n);
                    V[n]=true;
                }
            }
        }
    }
    llvm::BitVector wccNode=excludeImpreciseResults(wcc,Defs,Pre);
    std::set<unsigned> wccFinal=checkReachability(wccNode,AllX);
    return wccFinal;
}
    
llvm::BitVector CDAnalysis::excludeImpreciseResults(std::set<unsigned> wcc,std::set<unsigned> Defs, std::map<unsigned,unsigned > &Pre)
{
    std::map<unsigned, unsigned> Acc;
    std::map<unsigned,bool> isFinal;
    std::map<unsigned, edgeType> solnGraph;
    unsigned UNINITVAL=cfg->size()+10;
    llvm::BitVector V1(cfg->size());
    llvm::BitVector wccNode(cfg->size());
    std::queue<unsigned> Q;
    
    for(auto n:wcc)
    {
        isFinal[n]=false;
        wccNode[n]=false;
        
        CFGBlock *B=CFGIdMap[n];
        CFGBlock::succ_iterator ib=B->succ_begin();
        CFGBlock::succ_iterator ie=B->succ_end();
        for(;ib!=ie;ib++)
        {
            if(!(*ib)) continue;
            unsigned m=(*ib)->getBlockID();
            if(Pre[m]!=UNINITVAL)
            {
                solnGraph[n].first.insert(Pre[m]);
                solnGraph[Pre[m]].second.insert(n);
            }
        }
        Acc[n]=UNINITVAL;
    }
    
    for(auto n:Defs){
        Acc[n]=n;
        isFinal[n]=true;
        wccNode[n]=false;
        for(auto m:solnGraph[n].second){
            if(!V1[m]) {
                Q.push(m);
                V1[m]=true;
            }
        }
    }
   // printGraph(solnGraph);
    while(!Q.empty())
    {
        unsigned n=Q.front();
        Q.pop();
        V1[n]=false;
        std::set<unsigned> S;
        unsigned solvedNode;
        std::set<unsigned> unsolvedNode;
        for(auto m:solnGraph[n].first){
            if(Acc[m]!=UNINITVAL) {S.insert(Acc[m]);solvedNode=Acc[m];}
            else unsolvedNode.insert(m);
        }
        
        if(S.size()>1)
        {
            Acc[n]=n;
            isFinal[n]=true;
            if(!DefVect[n])
                wccNode[n]=true;
        }
        else
        {
            if(existInitWitness(solnGraph,unsolvedNode,solvedNode,wccNode))
            {
                Acc[n]=n;
                isFinal[n]=true;
                if(!DefVect[n])
                    wccNode[n]=true;
            }
            else
            {
                isFinal[n]=true;
                Acc[n]=Acc[solvedNode];
                Pre[n]=Acc[solvedNode];
            }
        }
        for(auto m:solnGraph[n].second){
            if(!V1[m] && !isFinal[m]) {
                Q.push(m);
                V1[m]=true;
            }
        }
    }

    return wccNode;
}
    

bool CDAnalysis::existInitWitness(std::map<unsigned, edgeType> solnGraph,std::set<unsigned> startNodes, unsigned diffFrom, llvm::BitVector Res){
     llvm::BitVector V(cfg->size());
     std::stack<unsigned> W;
     for(auto n:startNodes)
       W.push(n);
     while(!W.empty())
     {
         unsigned n=W.top();
         W.pop();
         V[n]=true;
         if((DefVect[n] || Res[n]) && n!=diffFrom) {return true;}
         if(DefVect[n] || Res[n]) continue;
         for(auto m:solnGraph[n].first){
             if(!V[m]) W.push(m);
         }
     }
    return false;
}

bool CDAnalysis::existInitWitnessEff(std::map<unsigned, edgeType> solnGraph,std::set<unsigned> startNodes, unsigned diffFrom, llvm::BitVector Res){
     llvm::BitVector V(cfg->size());
     std::stack<unsigned> W;
     for(auto n:startNodes)
       W.push(n);
     while(!W.empty())
     {
         unsigned n=W.top();
         W.pop();
         V[n]=true;
         if((DefVect[n] || Res[n]) && n!=diffFrom) {return true;}
         if(DefVect[n] || Res[n]) continue;
         for(auto m:solnGraph[n].first){
             if(!V[m]) W.push(m);
         }
     }
    return false;
}



std::set<std::pair<unsigned,unsigned> > CDAnalysis::findAllReachableEdges(std::set<unsigned> Vp)
{
    std::queue<CFGBlock *> Q;
    llvm::BitVector inQ(cfg->size());
    for(auto u:Vp){
        CFGBlock* uB= CFGIdMap[u];
        if(!uB) continue;
        CFGBlock::succ_iterator ib=uB->succ_begin();
        CFGBlock::succ_iterator ie=uB->succ_end();
        for(;ib!=ie;ib++){
            if(*ib){
                Q.push(CFGIdMap[(*ib)->getBlockID()]);
                inQ[(*ib)->getBlockID()]=true;
            }
        }
    }
    
    llvm::BitVector V(cfg->size());
    std::set<std::pair<unsigned,unsigned> > Res;
    
    while(!Q.empty())
    {
        CFGBlock *n=Q.front();
        Q.pop();
        inQ[n->getBlockID()]=false;
        V[n->getBlockID()]=true;
        CFGBlock::succ_iterator ib=n->succ_begin();
        CFGBlock::succ_iterator ie=n->succ_end();
        for(;ib!=ie;ib++){
            if(*ib)  {
                Res.insert(std::pair<unsigned,unsigned>(n->getBlockID(),(*ib)->getBlockID()));
                if(!V[(*ib)->getBlockID()]){
                    Q.push(*ib);
                    inQ[(*ib)->getBlockID()]=true;
                }
            }
        }
    }
    return Res;
}

std::set<unsigned> CDAnalysis::filterOutUnreachablenodes(std::set<unsigned> X,std::set<unsigned> Gamma)
{
    llvm::BitVector inGamma(cfg->size());
    llvm::BitVector visit(cfg->size());
    for(auto n:Gamma)
        inGamma[n]=true;
    std::set<unsigned> Y(X),GammaR;
    while(!Y.empty())
    {
        unsigned n=*Y.begin();
        Y.erase(Y.begin());
        visit[n]=true;
        if(inGamma[n]) GammaR.insert(n);
        CFGBlock *B=CFGIdMap[n];
        if(!B) continue;
        for(auto pB:B->succs())
        {
            if(!(pB)) continue;
            if(!visit[pB->getBlockID()]) Y.insert(pB->getBlockID());
        }
    }
    return GammaR;
}

std::set<unsigned> CDAnalysis::computeGamma(std::set<unsigned> Np, llvm::BitVector &inX)
{
    std::set<unsigned> X(Np),Gamma;
    while(!X.empty())
    {
        unsigned n=*X.begin();
        X.erase(X.begin());
        inX[n]=true;
        CFGBlock *B=CFGIdMap[n];
        for(auto pB:B->preds()){
            if(!(pB)) continue;
            bool succ=true;
            for(auto sPB:(*pB).succs())
            {
                if(!(sPB)) continue;
                if(!inX[(*sPB).getBlockID()]) {succ=false;break;}
            }
            if(succ && !inX[(*pB).getBlockID()]) X.insert((*pB).getBlockID());
        }
    }
    for(auto *B:*cfg)
    {
        if(!inX[B->getBlockID()]) Gamma.insert(B->getBlockID());
    }
    return Gamma;
}

std::set<unsigned> CDAnalysis::computeSCC(std::set<unsigned> Np)
{
    std::set<unsigned> scc,X={};
    llvm::BitVector inX(cfg->size());
    bool changed=true;
    std::set<unsigned> Xp(Np),WD={};
    std::map<unsigned,unsigned > Pre;
    unsigned UNINITVAL=cfg->size()+10;
    for(auto *B:*cfg)
    {
        if(!B) continue;
        Pre[B->getBlockID()]=UNINITVAL;
    }
    while(changed){
        changed=false;
        std::set<unsigned> GammaPre=computeGamma(Xp,inX);
        X.insert(Xp.begin(),Xp.end());
        std::set<unsigned> WDAux=computeIncrementalWD(Xp,Pre,X);
        WD.insert(WDAux.begin(),WDAux.end());
        Xp={};
        std::set<unsigned> Gamma=filterOutUnreachablenodes(X,GammaPre);
        for(auto p:WD)
        {
            CFGBlock *B=CFGIdMap[p];
            for(auto rB:B->succs())
            {
                if(!(rB)) continue;
                if(inX[rB->getBlockID()]) {
                    scc.insert(p);
                    Xp.insert(p);
                    changed=true;
                    break;
                }
            }
        }
        for(auto x:Xp)
           WD.erase(x);
        for(auto p:Gamma)
        {
            CFGBlock *B=CFGIdMap[p];
            for(auto rB:B->succs())
            {
                if(!(rB)) continue;
                if(inX[rB->getBlockID()]) {
                    scc.insert(p);
                    Xp.insert(p);
                    changed=true;
                    break;
                }
            }
        }
        for(auto n:Xp) DefVect[n]=true;
    }
    return scc;
}


void CDAnalysis::printGraph()
{
    CFGBlock* src=&(cfg->getEntry());
    std::queue<CFGBlock*> W;
    W.push(src);
    llvm::BitVector V(cfg->size());
    V[src->getBlockID()]=true;
    std::ofstream fout ("/tmp/temp.dot");
    fout<< "digraph G {" << endl;
    map<unsigned,string> nodes;
    for(auto *B:*cfg)
    {
        if(DefVect[B->getBlockID()])
            fout <<B->getBlockID()<< "[fontcolor=red label ="<<"\"" << B->getBlockID()<<" \""<<"]"<<endl;
        else
            fout <<B->getBlockID()<< "[label ="<<"\"" << B->getBlockID()<<"  \""<<"]"<<endl;
    }
    while(!W.empty())
    {
        CFGBlock* x=W.front();
        W.pop();
        CFGBlock::succ_iterator ib=x->succ_begin();
        CFGBlock::succ_iterator ie=x->succ_end();
        //for(auto m:graphCDNet[n].first)
        for(;ib!=ie;ib++){
            if(!(*ib)) continue;
                fout << x->getBlockID() <<" -> " << (*ib)->getBlockID() <<endl;
            if(!V[(*ib)->getBlockID() ]) {V[(*ib)->getBlockID() ]=true; W.push(*ib);}
        }
    }
    fout << "}" << endl;
   system("open /tmp/temp.dot");
}

void CDAnalysis::printGraph(std::map<unsigned, edgeType> graph)
{
    unsigned src=(cfg->getEntry()).getBlockID();
    std::queue<unsigned> W;
    W.push(src);
    llvm::BitVector V(cfg->size());
    V[src]=true;
    std::ofstream fout ("/tmp/soln.dot");
    fout<< "digraph G {" << endl;
    // fout<<"  node [shape=circle];"<<endl;
    map<unsigned,string> nodes;
    std::map<unsigned, edgeType>::iterator itb=graph.begin();
    while(itb!=graph.end())
    {
        if(DefVect[itb->first])
            fout <<itb->first<< "[fontcolor=red label ="<<"\"" << itb->first<<" \""<<"]"<<endl;
        else
            fout <<itb->first<< "[label ="<<"\"" << itb->first<<"  \""<<"]"<<endl;
        itb++;
    }
    itb=graph.begin();
    while(itb!=graph.end())
    {
        for(auto n:itb->second.first)
            fout << itb->first <<" -> " << n<<endl;
        itb++;
    }
    fout << "}" << endl;
    system("open /tmp/soln.dot");
}


class CDAnalysisDanicic
{
private:
    std::unique_ptr<clang::CFG> cfg;
    clang::ASTContext &context;
    typedef std::pair<std::set<unsigned>,std::set<unsigned>> edgeType;
    std::map<unsigned, edgeType> graphCDNet;
    std::map<unsigned, CFGBlock*> CFGIdMap;
   
    
public:
    CDAnalysisDanicic(ASTContext &C):context(C){}
    CDAnalysisDanicic(std::unique_ptr<clang::CFG> &&CFG, ASTContext &C, std::set<unsigned> vDefs);
    //void printGraph(std::set<unsigned>);
    std::set<unsigned> computeTheta(std::set<unsigned> Vp, unsigned u);
    std::set<std::pair<unsigned,unsigned> > findAllReachableEdges(std::set<unsigned> Vp);
    bool isReachable(unsigned p, std::set<unsigned> X);
    std::set<unsigned> computeWCCDanicic(std::set<unsigned> Vp);
    std::set<unsigned> computeSCCDanicic(std::set<unsigned> Vp);
    std::set<unsigned> computeGamma(std::set<unsigned> Vp, llvm::BitVector &inX);
    std::set<unsigned> computeGammaD(std::set<unsigned> Vp, llvm::BitVector &inX);
};


CDAnalysisDanicic::CDAnalysisDanicic(std::unique_ptr<clang::CFG> &&CFG, ASTContext &C, std::set<unsigned> vDefs):context(C)
{
    cfg=std::move(CFG);
    for (auto *Block : *cfg){
        CFGIdMap[Block->getBlockID()]=Block;
    }
}

std::set<unsigned> CDAnalysisDanicic::computeTheta(std::set<unsigned> Vp, unsigned u)
{
    CFGBlock *ublock;
    if(CFGIdMap.find(u)!=CFGIdMap.end())
        ublock=CFGIdMap[u];
    else assert("Node not found" && false);
    llvm::BitVector Vect(cfg->size());
    llvm::BitVector V(cfg->size());
    llvm::BitVector inQ(cfg->size());
    for(auto n:Vp)
        Vect[n]=true;
    std::queue<CFGBlock *> Q;
    std::set<unsigned> Res;
    Q.push(ublock);
    inQ[ublock->getBlockID()]=true;
    while(!Q.empty())
    {
        CFGBlock *n=Q.front();
        Q.pop();
        inQ[n->getBlockID()]=false;
        if(Vect[n->getBlockID()])  Res.insert(n->getBlockID());
        V[n->getBlockID()]=true;
        CFGBlock::succ_iterator ib=n->succ_begin();
        CFGBlock::succ_iterator ie=n->succ_end();
        for(;ib!=ie;ib++){
            if(!(*ib)) continue;
            if(Vect[n->getBlockID()]) continue;
            if(!V[(*ib)->getBlockID()])  {Q.push(*ib);inQ[(*ib)->getBlockID()]=true;}
        }
    }
    return Res;
}

std::set<std::pair<unsigned,unsigned> > CDAnalysisDanicic::findAllReachableEdges(std::set<unsigned> Vp)
{
    std::queue<CFGBlock *> Q;
    llvm::BitVector inQ(cfg->size());
    for(auto u:Vp){
        CFGBlock* uB= CFGIdMap[u];
        if(!uB) continue;
        CFGBlock::succ_iterator ib=uB->succ_begin();
        CFGBlock::succ_iterator ie=uB->succ_end();
        for(;ib!=ie;ib++){
            if(*ib){
                Q.push(CFGIdMap[(*ib)->getBlockID()]);
                inQ[(*ib)->getBlockID()]=true;
               }
            }
    }
    
    llvm::BitVector V(cfg->size());
    std::set<std::pair<unsigned,unsigned> > Res;
    
    while(!Q.empty())
    {
        CFGBlock *n=Q.front();
        Q.pop();
        inQ[n->getBlockID()]=false;
        V[n->getBlockID()]=true;
        CFGBlock::succ_iterator ib=n->succ_begin();
        CFGBlock::succ_iterator ie=n->succ_end();
        for(;ib!=ie;ib++){
            if(*ib)  {
                Res.insert(std::pair<unsigned,unsigned>(n->getBlockID(),(*ib)->getBlockID()));
                if(!V[(*ib)->getBlockID()]){
                    Q.push(*ib);
                    inQ[(*ib)->getBlockID()]=true;
                }
            }
        }
    }
    return Res;
}

bool CDAnalysisDanicic::isReachable(unsigned p, std::set<unsigned> X)
{
    std::queue<CFGBlock *> Q;
    llvm::BitVector inQ(cfg->size());
    llvm::BitVector inX(cfg->size());
    llvm::BitVector visit(cfg->size());
    for(auto u:X){
        inX[u]=true;
    }
    
    Q.push(CFGIdMap[p]);
    inQ[p]=true;
    
    while(!Q.empty())
    {
        CFGBlock *n=Q.front();
        Q.pop();
        inQ[n->getBlockID()]=false;
        visit[n->getBlockID()]=true;
        if(inX[n->getBlockID()]) return true;
        CFGBlock::pred_iterator ib=n->pred_begin();
        CFGBlock::pred_iterator ie=n->pred_end();
        for(;ib!=ie;ib++){
            if(!(*ib)) continue;
            if(!visit[(*ib)->getBlockID()] && !inQ[(*ib)->getBlockID()]){
                Q.push(*ib);
                inQ[(*ib)->getBlockID()]=true;
            }
        }
    }
    return false;
}



std::set<unsigned> CDAnalysisDanicic::computeWCCDanicic(std::set<unsigned> Vp)
{
    std::set<unsigned> X(Vp), Y;
    bool hasFound;
    do{
        hasFound=false;
        std::set<std::pair<unsigned,unsigned> > edges=findAllReachableEdges(X);
        for(auto e:edges)
        {
            std::set<unsigned> S1,S2;
            S1=computeTheta(X,e.second);
            S2=computeTheta(X,e.first);
            if(S1.size()==1 && S2.size()>=2){
                X.insert(e.first);
                Y.insert(e.first);
                hasFound=true;
            }
        }
    }while(hasFound);
    return Y;
}

std::set<unsigned> CDAnalysisDanicic::computeGamma(std::set<unsigned> Np, llvm::BitVector &inX)
{
    std::set<unsigned> X=Np;
    std::queue<CFGBlock *> Q;
    llvm::BitVector inQ(cfg->size());
    llvm::BitVector visit(cfg->size());
    
    for(auto u:Np){
        CFGBlock* uB= CFGIdMap[u];
        if(!uB) continue;
        Q.push(uB);
        inQ[u]=true;
        inX[u]=true;
    }
    
    while(!Q.empty())
    {
        CFGBlock* B=Q.front();
        Q.pop();
        inQ[B->getBlockID()]=false;
        visit[B->getBlockID()]=true;
        CFGBlock::pred_iterator pb=B->pred_begin();
        CFGBlock::pred_iterator pe=B->pred_end();
        for(;pb!=pe;pb++)
        {
            if(!(*pb)) continue;
            bool succ=true;
            for(auto nB:(*pb)->succs())
            {
                if(!nB) continue;
                if(!inX[nB->getBlockID()]) {succ=false;break;}
            }
            if(succ){
                inX[(*pb)->getBlockID()]=true;
                if(!inQ[(*pb)->getBlockID()] && !visit[(*pb)->getBlockID()]) Q.push(*pb);
            }
        } //endfor
    } // end while
    std::set<unsigned> Gamma;
    for(auto *B:*cfg)
    {
        if(!inX[B->getBlockID()]) Gamma.insert(B->getBlockID());
    }
    return Gamma;
}

std::set<unsigned> CDAnalysisDanicic::computeGammaD(std::set<unsigned> Np, llvm::BitVector &inX)
{
    
    std::map<unsigned, std::set<unsigned>> G;
     std::set<unsigned> Gamma;
    for(auto *B:*cfg)
    {
        if(!B) continue;
        for(auto sB:B->succs())
        {
            if(!sB) continue;
            G[B->getBlockID()].insert((*sB).getBlockID());
        }
    }
    
    for(auto u:Np){
        inX[u]=true;
    }
    std::set<unsigned> X=Np;
    bool changed=true;
    while(changed){
        changed=false;
        for(auto it:G)
        {
            if(inX[it.first]) continue;
            //if(!it.second.empty())
            std::set<unsigned> temp;
            for(auto x:it.second)
            {
                if(inX[x]) temp.insert(x);
            }
            for(auto x:temp)
               it.second.erase(x);
            
            if(it.second.size()==0)
            {
                  changed=true;
                  inX[it.first]=true;
            }
        }
    }
    for(auto *B:*cfg)
    {
        if(!inX[B->getBlockID()]) Gamma.insert(B->getBlockID());
    }
    return Gamma;
}

std::set<unsigned> CDAnalysisDanicic::computeSCCDanicic(std::set<unsigned> Vp)
{
    std::set<unsigned> X(Vp), Y,Gamma;
    bool hasFound;
    do{
        hasFound=false;
        std::set<std::pair<unsigned,unsigned> > edges=findAllReachableEdges(X);
        for(auto e:edges)
        {
            llvm::BitVector inX(cfg->size());
            Gamma=computeGammaD(X,inX);
            std::set<unsigned> S1,S2;
            S1=computeTheta(X,e.second);
            S2=computeTheta(X,e.first);
            if(inX[e.second] && S1.size()==1)
            if(S2.size()>=2 || !inX[e.first]){
                X.insert(e.first);
                Y.insert(e.first);
                hasFound=true;
            }
        }
    }while(hasFound);
    
    return Y;
}



#endif /* cdAnalysis_h */
