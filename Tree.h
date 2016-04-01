#ifndef TREE_H
#define TREE_H

#include <cmath>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <vector>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#include "Insts.h"
#include "Value.h"

class Tree
{
public:
    Tree(int opcode)
        : op(opcode), val(0), refcnt(0), level(1) 
    {
    }
    Tree(int opcode, VALUE v)
        : op(opcode), val(v), refcnt(0), level(1) 
    {
    }
    Tree(int opcode, Tree *l, Tree *r)
        : op(opcode), val(0), refcnt(0), level(1) 
    {
        AddChild(l);
        AddChild(r);
    }

    virtual ~Tree();

    int GetOpCode() const { 
        // std::cerr << "GET OPCODE: " << op << std::endl;
        return op; 
    }
    
    void SetValue(VALUE v) { val = v; }
    VALUE& GetValue() { return val; }

    void UseAsRegister() { isReg = true; }
    bool IsVirtualReg() const { return isReg; }
    int  GetVirtualReg() const { return val.AsVirtualReg(); }

    void SetComputed(bool c) { computed = c; }
    bool IsComputed() const { return computed; }

    void SetInst(llvm::Instruction *inst) { this->inst = inst; }

    void SetLevel(int level) { this->level = level; }
    int GetLevel() const { return this->level; }

    Tree** GetKids() { return &kids[0]; }
    void AddChild(Tree *ct);
    Tree* GetChild(int n);
    void KidsAsArguments();

    Tree* GetTreeRef() { refcnt++; return this; }
    void RemoveRef() { refcnt--; }

    int GetRefCount() const { return refcnt; }
    int GetNumKids() const { return kids.size(); }

    void CastInt(llvm::ConstantInt *cnst);
    void CastFP(llvm::ConstantFP *cnst);

    void DisplayTree() { DisplayTree(0); std::cerr << "\n"; }

    // leave these attributes public for simplicity
	struct { struct burm_state *state; } x;
	VALUE val;

private:
    void DisplayTree(int indent);

private:
	int op;

    int level;
    int refcnt;

    bool isReg;
    bool computed;

    X86OperandType operandType;
    llvm::Instruction *inst;
    std::vector<Tree*> kids;
    std::vector<Tree*> freeList;
};

#define GET_KIDS(p)	((p)->GetKids())
#define PANIC printf
#define STATE_LABEL(p)  ((p)->x.state)
#define SET_STATE(p,s)  (p)->x.state=(s)
#define DEFAULT_COST	break
#define OP_LABEL(p)     ((p)->GetOpCode())
#define NO_ACTION(x)

typedef Tree* NODEPTR;

#endif /* end of include guard: TREE_H */