#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"


//namespace llvm {
//static Attribute::AttrKind IsFragileAttrKind = Attribute::AttrKind::None;
//}


#include <vector>

// Define the FragileCluster struct
struct FragileCluster {
  llvm::DebugLoc start;
  llvm::DebugLoc end;
};

#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

namespace {
struct FragileFunctionMarkerPass : public llvm::FunctionPass {

  static char ID;
  std::vector<FragileCluster> fragileClusters;
  std::unordered_set<llvm::Function *> fragileFunctions;


  FragileFunctionMarkerPass() : llvm::FunctionPass(ID) {}

  void setFragileClusters(const std::vector<FragileCluster> &clusters) {
    fragileClusters = clusters;
  }

  bool runOnFunction(llvm::Function &F) override {
    bool isFragile = false;
    for (const auto &cluster : fragileClusters) {
      
      for (llvm::BasicBlock &BB : F) {
        // Get the source location for the basic block.
        const llvm::DebugLoc &BBLoc = BB.front().getDebugLoc();

        // Check if the basic block has a valid source location.
        if (BBLoc) {
          // Check if the basic block's source location is within the
          // FragileCluster.        
            if (BBLoc.getLine() >= cluster.start.getLine() &&
              BBLoc.getLine() <= cluster.end.getLine() &&
              BBLoc.getCol() >= cluster.start.getCol() &&
              BBLoc.getCol() <= cluster.end.getCol()) {
            // If there's an overlap, mark the function as "IsFragile".
            isFragile = true;
            break; // No need to check other basic blocks for this function.
          }
        }
      }
    }

    if (isFragile) {
      fragileFunctions.insert(&F);
    }

    return false; // Return false to indicate that the module is not modified.
  }

  void print(llvm::raw_ostream &OS, const llvm::Module *M) const override {

    OS << "Fragile Functions:\n";
    OS << "-----------------------------------------------------------------\n";
    OS << "Function Name\t\t| Function Signature\t| Size\t| Source File\n";
    OS << "-----------------------------------------------------------------\n";

    for (llvm::Function *F : fragileFunctions) {
      OS << F->getName() << '\t';
      // OS << "| " << F->getReturnType()->getAsString() << ' ' << F->getName()
      // << '(';


      llvm::FunctionType *FTy = F->getFunctionType();
      for (unsigned i = 0, e = FTy->getNumParams(); i != e; ++i) {
        //    OS << FTy->getParamType(i)->getAsString();
        if (i + 1 != e) {
          OS << ", ";
        }
      }

      OS << ')' << '\t';
      OS << "| " << F->size() << '\t';
      OS << "| " << F->getParent()->getSourceFileName() << '\n';
    }
  }
};
} // namespace


char FragileFunctionMarkerPass::ID = 0;

// The LLVM pass should be initialized like this:
static llvm::RegisterPass<FragileFunctionMarkerPass>
    X("mark-fragile", "Mark functions with the IsFragile attribute");

void markFragileFunctions(llvm::Module &M,
                          const std::vector<FragileCluster> &fragileClusters) {
  FragileFunctionMarkerPass *FragilePass = new FragileFunctionMarkerPass();
  FragilePass->setFragileClusters(fragileClusters);
  
  // ITerate through the list of functions and invoke the runOfFunction to identify if its fragile or not
  //Issue number 5 to identify
  for (llvm::Function &F : M) {
    FragilePass->runOnFunction(F);
  }

 // llvm::outs() << FragilePass; | Issue number 6 to display
  FragilePass->print(llvm::outs(),&M);
}

int main(int argc, char** argv) {

    //Get the reference of LLVM COntext so that your code can interact with the LLVM Utlities
    llvm::LLVMContext Context;

    //Create a context owner to get the current module from the Context
    std::unique_ptr<llvm::Module> Owner =
        std::make_unique<llvm::Module>("FragileFunction", Context);

    llvm::Module* M = Owner.get();


    // create a function foo for testing the code (positive case)
    llvm::Function* foo = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(Context), false),
        llvm::Function::ExternalLinkage, "foo", M);

    //Add basic block in the funtion foo for the runOnFunction to iterate
    llvm::BasicBlock* BBfoo = llvm::BasicBlock::Create(Context, "EntryBlock", foo);
    
    llvm::IRBuilder<> MyFooBuilder(BBfoo);

    //Set the debug location with overlapping. This test case will test the program for a Fragile Function
    llvm::DebugLoc debugLocFoo = llvm::DILocation::get(Context, 1, 1, foo->getSubprogram());
    MyFooBuilder.SetInsertPoint(BBfoo);
    MyFooBuilder.SetCurrentDebugLocation(debugLocFoo);
    MyFooBuilder.CreateRetVoid();

    // create a function bar for testing the code (negative case)
    llvm::Function* bar = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(Context), false),
        llvm::Function::ExternalLinkage, "bar", M);

    //Add basic block in the funtion bar for the runOnFunction to iterate
    llvm::BasicBlock* BBbar = llvm::BasicBlock::Create(Context, "EntryBlock", bar);
    llvm::IRBuilder<> MyBarBuilder(BBbar);

    //Set the debug location with non overlapping. This test case will test the program for a Fragile Function

    llvm::DebugLoc debugLocBar = llvm::DILocation::get(Context, 1, 0, bar->getSubprogram());

    //to test both functions as fragile 
    //lvm::DebugLoc debugLocBar = llvm::DILocation::get(Context, 1, 1, bar->getSubprogram());

    MyBarBuilder.SetInsertPoint(BBbar);
    MyBarBuilder.SetCurrentDebugLocation(debugLocBar);
    MyBarBuilder.CreateRetVoid();


    // Create a fragile cluster object with debug location. This will an input to the function which identifies the fragile function
    std::vector<FragileCluster> fragileClusters;
    llvm::DebugLoc debugLocFooStart = llvm::DILocation::get(Context, 1, 1, foo->getSubprogram());
    llvm::DebugLoc debugLocFooEnd = llvm::DILocation::get(Context, 2, 1, foo->getSubprogram());
    llvm::DebugLoc debugLocBarStart = llvm::DILocation::get(Context, 1, 1, bar->getSubprogram());
    llvm::DebugLoc debugLocBarEnd = llvm::DILocation::get(Context, 2, 1, bar->getSubprogram());
    fragileClusters.push_back(FragileCluster{ debugLocFooStart, debugLocFooEnd });
    fragileClusters.push_back(FragileCluster{ debugLocBarStart, debugLocBarEnd });


  
    //invoke the function that marks the fragile function
    //Module M contains the list of functions you have added to test the code 
    markFragileFunctions(*M, fragileClusters);


  // print the module
  llvm::outs() << "We just constructed this LLVM module:\n\n" << *M;
  llvm::outs().flush();

  return 0;
}
