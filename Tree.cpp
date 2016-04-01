#include "Tree.h"

Tree::~Tree() {
    for (Tree *ct : freeList) delete ct;
}

/**
 * Turn n-ary tree into binary tree
 * for argument matching
 * */
void Tree::KidsAsArguments() {
    int n = GetNumKids();
    if (n == 0) {
        // if no arg is provided, then terminate it
        AddChild(new Tree(NOARGS));
    } else {
        // if args are provided, then transform it into binary tree
        std::vector<Tree*> copy = kids;
        kids = std::vector<Tree*>();
        
        Tree *current = new Tree(ARGS);
        this->AddChild(current);

        for (int i = 0; i < n; i++) {
            Tree *args;
            if (i != n - 1) args = new Tree(ARGS);
            else            args = new Tree(NOARGS);
            current->AddChild(copy[i]);
            current->AddChild(args);
            current = args;
        }
    }
}

void Tree::AddChild(Tree *ct) {
    kids.push_back(ct);
    level = std::max(level, ct->GetLevel() + 1);
}

Tree* Tree::GetChild(int n) {
    assert (n < kids.size() && "GetChild index must be smaller than kids size");
    return kids[n];
}

void Tree::CastInt(llvm::ConstantInt *cnst) {
    freeList.push_back(new Tree(IMM));
    Tree *ct = freeList.back();
    const llvm::APInt integer = cnst->getValue();
    unsigned bitWidth = cnst->getBitWidth();
    if (integer.isSignedIntN(bitWidth)) {
        switch (bitWidth) {
            case 1:
                ct->SetValue((bool) cnst->getSExtValue());
                break;
            case 8:
                ct->SetValue((int8_t)cnst->getSExtValue());
                break;
            case 16:
                ct->SetValue((int16_t)cnst->getSExtValue());
                break;
            case 32:
                ct->SetValue((int32_t)cnst->getSExtValue());
                break;
            case 64:
                ct->SetValue((int64_t)cnst->getSExtValue());
                break;
            default:
                std::cerr << "CAST CONSTANT INT FAILURE" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
    else {
        switch (bitWidth) {
            case 1:
                ct->SetValue((bool) cnst->getZExtValue());
                break;
            case 8:
                ct->SetValue((int8_t)cnst->getZExtValue());
                break;
            case 16:
                ct->SetValue((int16_t)cnst->getZExtValue());
                break;
            case 32:
                ct->SetValue((int32_t)cnst->getZExtValue());
                break;
            case 64:
                ct->SetValue((int64_t)cnst->getZExtValue());
                break;
            default:
                std::cerr << "CAST CONSTANT INT FAILURE" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
    this->AddChild(ct);
}

void Tree::CastFP(llvm::ConstantFP *cnst) {
    freeList.push_back(new Tree(IMM));
    Tree *ct = freeList.back();
    // for now, assume all floating point uses 64 bit double
    ct->SetValue((double) cnst->getValueAPF().convertToDouble());
    this->AddChild(ct);
}

void Tree::DisplayTree(int indent) {
    for (int i = 0; i < 2 * indent; i++)
        std::cerr << " ";
    switch (op) {
        case DUMMY:
            std::cerr << "op: " << "dummy" << std::endl;
            break;
        case ARGS:
            std::cerr << "op: " << "args" << std::endl;
            break;
        case NOARGS:
            std::cerr << "op: " << "noargs" << std::endl;
            break;
        case LABEL:
            std::cerr << "op: " << "label" << std::endl;
            break;
        case REG:
            std::cerr << "op: " << "reg" << std::endl;
            break;
        case IMM:
            std::cerr << "op: " << "imm" << std::endl;
            break;
        case MEM:
            std::cerr << "op: " << "mem" << std::endl;
            break;
        default:
            std::cerr << "op: " << llvm::Instruction::getOpcodeName(op) << "(" << op << ")" << std::endl;
    }
    for (int i = 0; i < kids.size(); i++) {
        kids[i]->DisplayTree(indent + 1);
    }
}
