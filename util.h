//
//  ssaTime.h
//  LLVM
//
//  Created by Abu Naser Masud on 2019-06-07.
//

#ifndef UTIL_H
#define UTIL_H

#include <type_traits>
#include <typeinfo>
#include <chrono>

using namespace std;

typedef high_resolution_clock Clock;
typedef Clock::time_point ClockTime;

std::set<unsigned> getSetOfRandNumber(int min,int max,int num)
{
    int rnd;
    vector<int> diff;
    vector<int> tmp;
    std::set<unsigned> rset;
    for(int i = min;i < max+1 ; i++ )
    {
        tmp.push_back(i);
    }
    srand((unsigned)time(0));
    for(int i = 0 ; i < num ; i++)
    {
        do{
            rnd = min+rand()%(max-min+1);
     
        }while(tmp.at(rnd-min)==-1);
        diff.push_back(rnd);
        tmp.at(rnd-min) = -1;
    }
    for(auto n:diff) rset.insert(n);
    return rset;
}

void debug_print(std::string msg, unsigned dl, unsigned d)
{
    if(dl>=d)
      llvm::errs()<<msg;
    return;
}

void SplitFilename (const string& str, string &path, string &filename)
{
    size_t found;
    found=str.find_last_of("/\\");
    path=str.substr(0,found);
    filename=str.substr(found+1);
}

void debug_print(std::set<unsigned> S,std::string msg, unsigned dl, unsigned d)
{
    if(dl>=d){
        llvm::errs()<<msg<<": ";
        unsigned size=S.size(),i=0;
        for(auto v:S)
        {
            if(++i==size)
                llvm::errs()<<v;
            else
                llvm::errs()<<v<<", ";
        }
        llvm::errs()<<"\n";
    }
    return;
}

class CDAnalysisInfoType{
private:
    struct BInfo{
        unsigned long NCFG, Npsize;
        std::set<unsigned> Np;
    };
    struct CCInfo{
        unsigned long timeCC;
        std::set<unsigned> CC;
    };
    
    struct allInfo{
        struct BInfo procInfo;
        struct CCInfo WCCFast,SCCFast,WCCDanicic,SCCDanicic;
        bool iswccfast, issccfast, iswccdanicic, issccdanicic;
        bool isComWCC, diffWCC, isComSCC, diffSCC;
    };
    
    std::string FileName;
    
    std::map<std::string,struct allInfo> analInfo;
public:
    CDAnalysisInfoType()
    {
    }
    
    void setFileName(std::string f) {FileName=f;}
    
    std::pair<bool,bool> diffCC(std::string proc,int dl,bool diffwcc=true)
    {
        if(dl==0) return std::pair<bool,bool>(false,false);
        bool nodiff=true;
        if(analInfo.find(proc)!=analInfo.end()){
            std::set<unsigned> CCFast,CCDanicic;
            if(analInfo[proc].iswccfast && analInfo[proc].iswccdanicic && diffwcc)
            {
                CCFast=analInfo[proc].WCCFast.CC;
                CCDanicic=analInfo[proc].WCCDanicic.CC;
            }
            else if(analInfo[proc].issccfast && analInfo[proc].issccdanicic && !diffwcc)
            {
                CCFast=analInfo[proc].SCCFast.CC;
                CCDanicic=analInfo[proc].SCCDanicic.CC;
            }
            else return std::pair<bool,bool>(false,false);
            for(auto n:CCFast)
                if(CCDanicic.find(n)==CCDanicic.end())
                {
                    nodiff=false;
                    break;
                }
            for(auto n:CCDanicic)
                if(CCFast.find(n)==CCFast.end())
                {
                    nodiff=false;
                    break;
                }
        }
        return std::pair<bool,bool>(true,!nodiff);
    }
    
    bool storeProcInfo(std::string proc,unsigned long n, std::set<unsigned> np)
    {
        struct BInfo pinfo={n,np.size(),np};
        struct CCInfo wf={},wd={},sf={},sd={};
        struct allInfo i={pinfo,wf,sf,wd,sd,false,false,false,false,false,false,false,false};
        analInfo[proc]=i;
        return 0;
    }
    
    bool storeWCCFastInfo(std::string proc,unsigned long time, std::set<unsigned> res)
    {
        if(analInfo.find(proc)!=analInfo.end())
        {
            analInfo[proc].WCCFast.timeCC=time;
            analInfo[proc].WCCFast.CC=res;
            analInfo[proc].iswccfast=true;
            return 0;
        }
        return 1;
    }
    
    bool storeSCCFastInfo(std::string proc,unsigned long time, std::set<unsigned> res)
    {
        if(analInfo.find(proc)!=analInfo.end())
        {
            analInfo[proc].SCCFast.timeCC=time;
            analInfo[proc].SCCFast.CC=res;
            analInfo[proc].issccfast=true;
            return 0;
        }
        return 1;
    }
    
    bool storeWCCDanicicInfo(std::string proc,unsigned long time, std::set<unsigned> res)
    {
        if(analInfo.find(proc)!=analInfo.end())
        {
            analInfo[proc].WCCDanicic.timeCC=time;
            analInfo[proc].WCCDanicic.CC=res;
            analInfo[proc].iswccdanicic=true;
            return 0;
        }
        return 1;
    }
    
    bool storeSCCDanicicInfo(std::string proc,unsigned long time, std::set<unsigned> res)
    {
        if(analInfo.find(proc)!=analInfo.end())
        {
            analInfo[proc].SCCDanicic.timeCC=time;
            analInfo[proc].SCCDanicic.CC=res;
            analInfo[proc].issccdanicic=true;
            return 0;
        }
        return 1;
    }
    
    void showResults(int dl)
    {
        unsigned long N=0, tWccfast=0,tWccDanicic=0,tSccfast=0,tSccDanicic=0;
        bool wf=false,wd=false,sf=false,sd=false;
        bool Res=true;
        bool diffWCC=true,diffSCC=true;
        for(auto ib=analInfo.begin(),ie=analInfo.end();ib!=ie;ib++)
        {
            N+=(*ib).second.procInfo.NCFG;
            if((*ib).second.iswccfast){
                tWccfast=tWccfast+(*ib).second.WCCFast.timeCC;
                (*ib).second.WCCFast.CC.insert((*ib).second.procInfo.Np.begin(),(*ib).second.procInfo.Np.end());
                wf=true;
            }
            
            if((*ib).second.issccfast){
                tSccfast=tSccfast+(*ib).second.SCCFast.timeCC;
                (*ib).second.SCCFast.CC.insert((*ib).second.procInfo.Np.begin(),(*ib).second.procInfo.Np.end());
                sf=true;
            }
            
            if((*ib).second.iswccdanicic){
                tWccDanicic=tWccDanicic+(*ib).second.WCCDanicic.timeCC;
                (*ib).second.WCCDanicic.CC.insert((*ib).second.procInfo.Np.begin(),(*ib).second.procInfo.Np.end());
                wd=true;
            }
            
            if((*ib).second.issccdanicic){
                tSccDanicic=tSccDanicic+(*ib).second.SCCDanicic.timeCC;
                (*ib).second.SCCDanicic.CC.insert((*ib).second.procInfo.Np.begin(),(*ib).second.procInfo.Np.end());
                sd=true;
            }
            
            std::pair<bool,bool> diff=diffCC((*ib).first,dl);
            if(diff.first){
                (*ib).second.isComWCC=diff.first;
                (*ib).second.diffWCC=diff.second;
                diffWCC= diffWCC & (*ib).second.diffWCC;
                Res= Res & (*ib).second.diffWCC;
            }
            diff=diffCC((*ib).first,dl,false);
            if(diff.first){
                (*ib).second.isComSCC=diff.first;
                (*ib).second.diffSCC=diff.second;
                diffSCC= diffSCC & (*ib).second.diffSCC;
                Res= Res & (*ib).second.diffSCC;
            }
                        
            std::string ostr="Func: " + (*ib).first + ", nCFG: " + to_string((*ib).second.procInfo.NCFG)+", NPsize: "+to_string((*ib).second.procInfo.Npsize);
            
            if((*ib).second.iswccfast)
                ostr=ostr+ ", WCCFast: " +to_string((*ib).second.WCCFast.timeCC);
            
            if((*ib).second.iswccdanicic)
                ostr=ostr+ ", WCCDanicic: " +to_string((*ib).second.WCCDanicic.timeCC);
            
            if((*ib).second.issccfast)
                ostr=ostr+ ", SCCFast: " +to_string((*ib).second.SCCFast.timeCC);
            
            if((*ib).second.issccdanicic)
                ostr=ostr+ ", SCCDanicic: " +to_string((*ib).second.SCCDanicic.timeCC);
                
            if((*ib).second.isComWCC && (*ib).second.diffWCC) ostr=ostr+ " WCC res differes ";
            else if((*ib).second.isComWCC && !(*ib).second.diffWCC) ostr=ostr+ " WCC res OK";
            if((*ib).second.isComSCC && (*ib).second.diffSCC) ostr=ostr+ " SCC res differes ";
            else if((*ib).second.isComSCC && !(*ib).second.diffSCC)ostr=ostr+ " SCC res OK";
            ostr=ostr+"\n";
            debug_print("--------------------\n", dl, 1);
            debug_print((*ib).second.procInfo.Np, "Nodes in Nprime: ", dl, 1);
            if((*ib).second.iswccfast)
                debug_print((*ib).second.WCCFast.CC, "WCC (fast algorithm): ", dl, 1);
            if((*ib).second.issccfast)
                debug_print((*ib).second.SCCFast.CC, "SCC (fast algorithm): ", dl, 1);
            if((*ib).second.iswccdanicic)
                debug_print((*ib).second.WCCDanicic.CC, "WCC (Danicic's algorithm): ", dl, 1);
            if((*ib).second.issccdanicic)
                debug_print((*ib).second.SCCDanicic.CC, "SCC (Danicic's algorithm): ", dl, 1);
            debug_print(ostr,dl,1);

        }
        
        std::cout<<"\n\nSourceFile: "<<FileName<<" CFG size: "<<N;
        if(wf) std::cout<<", time (WCC fast): "<<tWccfast;
        if(wd) std::cout<<", time (WCC Danicic): "<<tWccDanicic;
        if(sf) std::cout<<", time (SCC fast): "<<tSccfast;
        if(sd) std::cout<<", time (SCC Danicic): "<<tSccDanicic;
        if(!diffWCC && wf && wd) std::cout<<", WCC Result matchs\n";
        if(diffWCC && wf && wd) std::cout<<", WCC Result differs\n";

        if(!diffSCC && sf && sd) std::cout<<", SCC Result matchs\n";
        if(diffSCC && sf && sd) std::cout<<", SCC Result differs\n";
        std::cout<<"\n";
    }
    
};



template<class Resolution = std::chrono::milliseconds>
class ExecTime {
public:
    typedef typename std::conditional<std::chrono::high_resolution_clock::is_steady,
    std::chrono::high_resolution_clock,
    std::chrono::steady_clock>::type Clock;
private:
    const Clock::time_point startClock=Clock::now();
    
public:
    ExecTime() = default;
    ~ExecTime() {
    }
    
    unsigned long stop() {
        const auto end = Clock::now();
        return std::chrono::duration_cast<Resolution>( end - startClock).count();
    }
    
}; // ExecutionTimer
#endif // EXECUTION_TIMER_H
