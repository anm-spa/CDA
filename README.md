<h1> Computing Weak and Strong Control Closure </h1>

This tool is developed based on clang/LLVM using the Libtooling infrastructure to analyse the control dependency of intraprocedural programs. This directory should be placed inside the clang/tools directory of the llvm project. See the documentation of the Libtooling  infrastructure (https://clang.llvm.org/docs/LibTooling.html). Building the llvm will also build this tool automatically and will produce the "clang-cda" executables.


Given any source code file, this tool will compute the weak and strong control closure of a set of CFG nodes. For each of them, two different algorithms are implemented:
1. Danicic et al's method: Sebastian Danicic, Richard W. Barraclough, Mark Harman, John D. Howroyd, Ákos Kiss, Michael R. Laurence,
A unifying theory of control dependence and its application to arbitrary program structures,Theoretical Computer Science, Volume 412, Issue 49,2011,Pages 6809-6842,
ISSN 0304-3975
2. Fast or Efficient method: Masud A.N. (2020) Simple and Efficient Computation of Minimal Weak Control Closure. In: Pichardie D., Sighireanu M. (eds) Static Analysis. SAS 2020. Lecture Notes in Computer Science, vol 12389. Springer, Cham. https://doi.org/10.1007/978-3-030-65474-0_10
  
A journal version of (2) is under review.


There are several options that can be given in the command line.
* Choose algorithms:
  * -alg=WCCfast (Compute WCC using method 2)
  * -alg=WCCdan (Compute WCC using method 1)
  * -alg=SCCfast (Compute SCC using method 2)
  * -alg=SCCdan (Compute SCC using method 1)
  * -alg=WCC (Compute WCC using method 1 and 2 and compare the execution time and results)
  * -alg=SCC (Compute SCC using method 1 and 2 and compare the execution time and results)
  * -alg=ALL (Compute WCC and SCC using method 1 and 2 and compare the execution time and results)

* Choose debug lebels:
  * -dl=l0 (only print the final results)
  * -dl=l1 (print results for each procedure individually)
  * -dl=l2 (print extended debug info)

* It is possible to analyze a single procedure in a source code. In that case, give the option -proc=<procedure name>
  
All methods compute the control closure of Np which is a subset of all CFG nodes N. Np is selected randomly in all algorithms. Np can be given only if -proc=<procedure name> option is used along with -dl=l2
  
* Example input and output 
  > clang-cda <source-dir>/mg.c -dl=l1 -alg=ALL -proc=S_magic_methcall1
  * --------------------
  * Nodes in Nprime: : 1, 2, 3, 5, 6, 7, 10
  * WCC (fast algorithm): : 1, 2, 3, 5, 6, 7, 9, 10
  * SCC (fast algorithm): : 1, 2, 3, 5, 6, 7, 9, 10
  * WCC (Danicic's algorithm): : 1, 2, 3, 5, 6, 7, 9, 10
  * SCC (Danicic's algorithm): : 1, 2, 3, 5, 6, 7, 9, 10
  * Func: S_magic_methcall1, nCFG: 12, NPsize: 7, WCCFast exec time: 109, WCCDanicic exec time: 352, SCCFast exec time: 207, SCCDanicic exec time: 1815 
  * WCC res in both methods are same 
  * SCC res in both methods are same
  
  * SourceFile: mg.c CFG size: 1303, time (WCC fast): 109, time (WCC Danicic): 352, time (SCC fast): 207, time (SCC Danicic): 1815
  * WCC Result matchs
  * SCC Result matchs

*****ALL TIMES ARE MEASURED IN MICROSECONDS
