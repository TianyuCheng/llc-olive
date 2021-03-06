#include "RegisterAllocator.h"
#ifndef CLIMITS_H
#include <climits>
#endif
#include "FunctionState.h"

void SimpleRegisterAllocator::Allocate(FunctionState *fstate, std::ostream &out, int reg, int line) {
#if DEBUG
    std::cerr << "=========== ALLOCATE for virtual reg: " << reg << std::endl;
    std::cerr << "BEFORE ALLOCATION" << std::endl;
    DumpVirtualRegs();
#endif

    // add the current target virtual register to nospill, because we cannot spill it in current instruction
    nospills.push_back(reg);

    int reg_idx = virtual2machine[reg];
    if (reg_idx == -1) {
        // this register has never been assigned,
        int free_reg_idx = GetFreeReg(out, fstate, line);    // index of a free physical register
        // double mapping
        virtual2machine[reg] = free_reg_idx;
        register_status[free_reg_idx] = reg;
    }
    else if (reg_idx < -1) {
        int offset = reg_idx;                           // negative numbers used as offset to RBP
        int free_reg_idx = GetFreeReg(out, fstate, line);    // index of a free physical register
#if DEBUG
        std::cerr << "vir_reg_idx: " << reg << std::endl;
        std::cerr << "phy_reg_idx: " << free_reg_idx << std::endl;
#endif
        fstate->RestoreSpill(out, free_reg_idx, offset);
        // double mapping
        virtual2machine[reg] = free_reg_idx;
        register_status[free_reg_idx] = reg;
    }
#if DEBUG
    std::cerr << "AFTER ALLOCATION" << std::endl;
    DumpVirtualRegs();
#endif

    // else do nothing because this register is live, and have a valid mapping
}

int SimpleRegisterAllocator::GetRegToSpill(std::ostream &out, FunctionState *fstate, int line) {
    // assert(spillable % 2 == 0 && "Not spillable while preparing function call. You must have exhaust your registers!");

    int distance = -1;
    int phy_reg_to_spill = -1;
    for (auto it = register_status.begin(); it != register_status.end(); it++) {
        int phy_reg = it->first;
        int vir_reg = it->second;
        if (!CanSpill(vir_reg)) continue;

        LiveRange *range = liveness[vir_reg];
        if (range) {
            assert(range && "range must be not null");
            std::vector<int> &innerpoints = range->innerpoints;
            int num_points = innerpoints.size();

            // find out the virtual register that has the furtherest use in the future
            for (int i = 0; i < num_points - 1; i++) {
                if (innerpoints[i] <= line && innerpoints[i+1] >= line) {
                    int dist = innerpoints[i+1] - line;
                    if (dist >= distance) {
                        distance = dist;
                        phy_reg_to_spill = phy_reg;
                        break;
                    }
                }
            }
        }
    }

    assert(phy_reg_to_spill != -1 && "Not enough registers!");

    // generate spill code
    int vir_reg = register_status[phy_reg_to_spill];
    virtual2machine[vir_reg] = fstate->CreateSpill(out, phy_reg_to_spill);

    return phy_reg_to_spill;
}

int SimpleRegisterAllocator::GetFreeReg(std::ostream &out, FunctionState *fstate, int line) {
    // first search through all registers to find free physical registers
    for (auto it = register_status.begin(); it != register_status.end(); it++) {
        int phy_reg = it->first;
        int vir_reg = it->second;

        // if this physical register is not available for function call
        if (spillable % 2 == 1 && std::find(disabled_regs.begin(), 
                    disabled_regs.end(), phy_reg) != disabled_regs.end())
            continue;

        // registers used in current instruction is not spillable
        if (!CanSpill(vir_reg)) continue;

        // if this physical register is not assigned to some virtual register
        if (vir_reg == -1) return phy_reg;

#if 1
        // if the virtual register that is assigned is not live anymore
        LiveRange *range = liveness[vir_reg];
        if (!range) {
            std::cerr << "virtual reg:  " << vir_reg << std::endl;
            std::cerr << "current line: " << line << std::endl;
            for (auto it = liveness.begin(); it != liveness.end(); ++it)
                std::cerr << it->first << std::endl;
            assert(false);
        }
        assert(range && "range must not be null");
        if (line > range->endpoint) {
            virtual2machine[vir_reg] = -1;      // reset register assignment because the virtual register is expired
            // virtual2machine[vir_reg] = fstate->CreateSpill(out, phy_reg);
            return phy_reg;
        }
#endif
    }

    // if we cannot find a free physical register, then we have to spill
    int reg_to_spill = GetRegToSpill(out, fstate, line);
    return reg_to_spill;
}

void SimpleRegisterAllocator::EnableSpill(FunctionState *fstate, std::ostream &out) { 
    spillable--;
}
void SimpleRegisterAllocator::DisableSpill(FunctionState *fstate, std::ostream &out) { 
    spillable++;
}

int maxIndexVector (std::vector<int>& vec) {
    int max_elem = -1, max_index = 0;
    for (unsigned i = 0; i < vec.size(); i ++) {
        // std::cout << i << ":" << vec[i] << std::endl;
        if (vec[i] == INT_MAX) {
            return i;
        }
        if (vec[i] > max_elem) {
            max_index = i;
            max_elem = vec[i];
        }
    }
    return max_index;
}

int RegisterAllocator::findNextIntersect(int pos, Interval* cur_itv, Interval* itv) {
    int i = 0, j = 0;
    // move iterator after pos
    int size_cur_itv = cur_itv->liveranges.size();
    int size_itv = itv->liveranges.size();
    for (; i < size_cur_itv; i ++) 
        if (cur_itv->liveranges[i].endpoint >= pos) break;
    for (; j < size_itv; j ++) 
        if (itv->liveranges[j].startpoint >= pos) break;
    int cur_itv_st_idx = i, itv_st_idx = j;
    // non-linear next intersection search
    int visit1_min = INT_MAX, visit2_min = INT_MAX;
    bool found = false;
    for (i = cur_itv_st_idx + 1; !found && i < size_cur_itv; i++) { 
        int startpoint = cur_itv->liveranges[i].startpoint;
        for (j = itv_st_idx; j < size_itv; j ++) {
#if 0
            std::cout << "check:" << j << "]" << startpoint << " :: " 
                << itv->liveranges[j].startpoint << ", " 
                << itv->liveranges[j].endpoint
                << std::endl; 
#endif
            if (itv->liveranges[j].startpoint <= startpoint && 
                    startpoint <= itv->liveranges[j].endpoint) {
                visit1_min = startpoint;
                found = true;
                break;
            } else if (startpoint < itv->liveranges[j].startpoint) break;
        }
    }
    found = false;
    for (j = itv_st_idx; !found && j < size_itv; j ++) {
        int startpoint = itv->liveranges[j].startpoint;
        for (i = cur_itv_st_idx; i < size_cur_itv; i ++) {
#if 0
            std::cout << "check:" << i << "]" << startpoint << " :: " 
                << cur_itv->liveranges[i].startpoint << ", " 
                << cur_itv->liveranges[i].endpoint
                << std::endl; 
#endif
            if (cur_itv->liveranges[i].startpoint <= startpoint && 
                    startpoint <= cur_itv->liveranges[i].endpoint) {
                visit2_min = startpoint;
                found = true;
                break;
            } else if (startpoint < cur_itv->liveranges[i].startpoint) break;
        }
    }
    // std::cout << "visti_min:" << visit1_min << ", visit2_min: " << visit2_min << std::endl; 
    return std::min(visit1_min, visit2_min);
}

bool RegisterAllocator::isIntersect(Interval* ia, Interval* ib) {
    unsigned size_ia = ia->liveranges.size();
    unsigned size_ib = ib->liveranges.size();
    for (unsigned i = 0; i < size_ia; i++) { 
        int startpoint = ia->liveranges[i].startpoint;
        int endpoint = ia->liveranges[i].endpoint;
        for (unsigned j = 0; j < size_ib; j ++) {
            if ((ib->liveranges[j].startpoint <= startpoint && startpoint <= ib->liveranges[j].endpoint)
                 || (ib->liveranges[j].startpoint <= endpoint && endpoint <= ib->liveranges[j].endpoint)) {
                return true;
            } else if (endpoint < ib->liveranges[j].startpoint) break;
        }
    }
    for (unsigned j = 0; j < size_ib; j ++) {
        int startpoint = ib->liveranges[j].startpoint;
        int endpoint = ib->liveranges[j].endpoint;
        for (unsigned i = 0; i < size_ia; i++) { 
            if ((ia->liveranges[i].startpoint <= startpoint && startpoint <= ia->liveranges[i].endpoint)
                 || (ia->liveranges[i].startpoint <= endpoint && endpoint <= ia->liveranges[i].endpoint)) {
                return true;
            } else if (endpoint < ia->liveranges[i].startpoint) break;
        }
    }
    return false;
}

int RegisterAllocator::tryAllocateFreeReg(int i) {
    std::vector<int> freeUntilPos (NUM_REGS, INT_MAX);
   // std::cout << "active_size: " << active.size() << std::endl;
   // std::cout << "inactive_size: " << inactive.size() << std::endl;
    // FOR EACH active interval
    for (int iid : active) {
       std::cout << "[active] set_zero: iid=" << iid 
           << ", reg=" << virtual2machine[iid] << std::endl;
        freeUntilPos[virtual2machine[iid]] = 0; // TODO: interval's physical register will change
    }
    // FOR EACH inactive interval
    int pos = iid_start_pairs[i].second; 
    int cur_iid = iid_start_pairs[i].first;
    for (int iid : inactive) {
        Interval* cur_itv = all_intervals[cur_iid]; 
        Interval* itv = all_intervals[iid]; 
        int potential = findNextIntersect(pos, cur_itv, itv);
        std::cout << "[inactive] set_th: iid=" << iid 
            << ", reg=" << virtual2machine[iid] << ", next_intersect=" << potential << std::endl;
        freeUntilPos[virtual2machine[iid]] = std::min(freeUntilPos[virtual2machine[iid]], potential);
    }
    std::cout <<  "freeUntilPos: ";
    for (int x : freeUntilPos) 
        std::cout << x << " "  ;
    std::cout << std::endl;
    return maxIndexVector(freeUntilPos);
}

int RegisterAllocator::findNextUseHelper(std::vector<int>& use_vec, int after) {
  //      std::cout << "helper start: ";
    for (int use_pos : use_vec) {
        std::cout << use_pos << " ";
        if (use_pos > after) 
            return use_pos;
    }
    std::cout << std::endl;
    return INT_MAX;
}

int RegisterAllocator::findNextUse(int cur_iid, int iid, int cur_start) {

    int next_use = -1;
    if (use_contexts.find(iid) == use_contexts.end()) {
    // std::cout << "aaaaaaaaaa: " << next_use << std::endl;
        next_use = INT_MAX;
    } else { 
    // std::cout << "bbbbbbbb: " << next_use << std::endl;
        next_use = findNextUseHelper(*(use_contexts[iid]), cur_start);
    }
    std::cout << "next_use: " << next_use << std::endl;
    if (next_use < 0)
        assert(false && "No next use! There should be one for the inactive interval");
    return next_use;
}

int RegisterAllocator::allocateBlockedReg(int i) {
    std::vector<int> nextUsePos (NUM_REGS, INT_MAX);
    // FOR EACH active interval
    int cur_iid = iid_start_pairs[i].first;
    int cur_start = iid_start_pairs[i].second;
    for (int iid : active) {
        std::cout << "[active] findNextUse: iid=" << iid 
            << ", cur_start: " << cur_start << std::endl;
        int potential = findNextUse(cur_iid, iid, cur_start);
        nextUsePos[virtual2machine[iid]] = std::min(nextUsePos[virtual2machine[iid]], potential);
    }
    // FOR EACH inactive interval
    for (int iid : inactive) {
        std::cout << "[inactive] findNextUse: iid=" << iid 
            << ", cur_start: " << cur_start << std::endl;
        if (isIntersect(all_intervals[iid], all_intervals[cur_iid])) {
            int potential = findNextUse(cur_iid, iid, cur_start);
            nextUsePos[virtual2machine[iid]] = std::min(nextUsePos[virtual2machine[iid]], potential);
        }
    }
    return maxIndexVector(nextUsePos);
}

void RegisterAllocator::updateRAState(int i) {
    // start_of_current
    int pos = iid_start_pairs[i].second;
    // update active
    std::vector<int> active2handled; 
    // std::set<int> active2inactive;
    for (auto it=active.begin(); it!=active.end();it++) {
        int iid = *it;
        // std::cout << "iid:" << iid << std::endl;
        Interval* iid_it = all_intervals[iid];
        // int iid_lr_size = iid_it->liveranges.size();
        int iid_endpoint = iid_it->liveranges.rbegin()->endpoint;
        std::cout << "active_update: "<< iid << "," << pos << "," << iid_endpoint << std::endl;
        if (iid_endpoint <= pos) {
            active2handled.push_back(iid);
        } 
    }

    // apply all update
    // for (int iid : active2inactive) { active.erase(iid); inactive.insert(iid); }
    for (int iid : active2handled) { 
        std::cout << "active2handled: "<< iid << "," << pos  << std::endl;
        active.erase(iid); 
        handled.insert(iid); 
    }
    return ;
}

void RegisterAllocator::resolveConflicts() {
    // TODO: insert move instruction (from stack back to register)
    
}

// split [a,b] to [a, start] and [next_use, b]
void RegisterAllocator::splitInterval(int iid, int start, int reg) {
    Interval* interval = all_intervals[iid];
    // std::cout << "interval():" << iid << std::endl;
    int next_use;
    if (use_contexts.find(iid) == use_contexts.end()) next_use = INT_MAX;
    else next_use = findNextUseHelper(*(use_contexts[iid]), start);
    // std::cout << "findNextUseHelper():" << std::endl;
    int lr_id = -1;
    for (int i = 0; i < interval->liveranges.size(); i ++) {
        if (interval->liveranges[i].startpoint <= start && 
                start <= interval->liveranges[i].endpoint) {
            lr_id = i; break;
        }
    }
   // std::cout << "find lr_id():" << std::endl;
    if (next_use == INT_MAX) {
        interval->liveranges[lr_id].endpoint = start;
        interval->liveranges[lr_id].pos = reg;
        return ;
    }
    int tmp_endpoint = interval->liveranges[lr_id].endpoint;  // b
    interval->liveranges[lr_id].endpoint = start;
    std::cout << "split_to: " << interval->liveranges[lr_id].startpoint 
              << ", " << interval->liveranges[lr_id].endpoint << std::endl;
    // insert an new holes
    LiveRange hole (start, next_use);
    stack.push_back(iid);
    hole.set_in_stack (stack.size()-1);
    interval->holes.push_back(hole);

    std::cout << "hole: " << hole.startpoint << ", " << hole.endpoint << std::endl;

    // insert an new live range
    LiveRange lr (next_use, tmp_endpoint);
    lr.set_in_register(reg);
    // std::cout << "insert():" << std::endl;
    interval->liveranges.insert(interval->liveranges.begin()+lr_id+1, lr);
    insert_intervals(iid, next_use);
    // system state update: transit iid from active to inactive
    active.erase(iid);
    inactive.insert(iid);
    return ;
}

/* Linear Scan Algorithm for SSA form (support splitting of interval) */
void RegisterAllocator::linearScanSSA () {
    // initialize four sets recording the system state
    active.clear();
    inactive.clear();
    handled.clear();
    unhandled.clear();
    for (int i = 0; i < iid_start_pairs.size(); i ++) 
        unhandled.insert(iid_start_pairs[i].first);
    // start linear scan algorithm
    std::cout << "Total number of intervals:" << all_intervals.size() << std::endl;
    int num_pre_intervals = iid_start_pairs.size();
    for (int i = 0; i < iid_start_pairs.size(); i ++) {
        int cur_iid = iid_start_pairs[i].first;
        int cur_start = iid_start_pairs[i].second;
        bool expire_after_processed = false;
        if (inactive.find(cur_iid) != inactive.end()) {
            // it must be splitted intervals
            inactive.erase(cur_iid);
            if (all_intervals[cur_iid]->liveranges.rbegin()->endpoint == cur_start) {
                // directly turn to handled since it is going to expire
                expire_after_processed = true;
            }
        }
        std::cout << "-------------------------------" << std::endl;
        std::cout << "Interval: " << cur_iid << ", cur: " << cur_start << ", end: " 
                  << all_intervals[cur_iid]->liveranges.rbegin()->endpoint
                  << std::endl;

        std::cout << "active_set_size: "   << active.size() << ", "
                  << "inactive_set_size: " << inactive.size() 
                  << std::endl;

        std::cout << ">> updateRAState(): " << std::endl;
        updateRAState(i); // update Register Allocation State

        std::cout << "active_set_size: "   << active.size() << ", "
                  << "inactive_set_size: " << inactive.size() 
                  << std::endl;
        std::cout << "active_iid: ";
        for (int iid : active) std::cout << iid << " ";
        std::cout <<  std::endl;
        std::cout << "inactive_iid: ";
        for (int iid : inactive) std::cout << iid << " ";
        std::cout <<  std::endl;
        
        int reg;
        if (active.size() == NUM_REGS) {
            // look for one occupied register to allocate
            std::cout << ">> allocateBlockedReg():" << std::endl;
            reg = allocateBlockedReg(i);
            int spill = register_map[reg];
            active.erase(spill);
            inactive.insert(spill);
            std::cout << "active_erase: " << spill 
                      << ", active_after: " << active.size()
                      << ", inactive_after: " << inactive.size()
                      << std::endl;
            splitInterval(spill, cur_start, reg);
        } else {
            // look for one register to allocate
            std::cout << ">> tryAllocateFreeReg():"  << std::endl;
            reg = tryAllocateFreeReg(i); 
        }
        std::cout << "register assigned = " << reg << std::endl;
        assigned_registers.push_back(reg);
        register_map[reg] = cur_iid;
        virtual2machine[cur_iid] = reg;
        if (!expire_after_processed && active.find(cur_iid)==active.end()) 
            active.insert(cur_iid);
        if (unhandled.find(cur_iid) != unhandled.end()) unhandled.erase(cur_iid);
        std::cout << "REGISTER_MAP: ";
        for (auto it=register_map.begin(); it!=register_map.end(); it++) 
            std::cout << it->first << ":" << it->second << " ";
        std::cout << std::endl;
        transitions.push_back(register_map);
    }

    //###########################################################
    // Assign register to live ranges
    //###########################################################
    assert(assigned_registers.size() == iid_start_pairs.size() 
            && "size equality constraint fails");
    for (int i = 0; i < iid_start_pairs.size(); i ++) {
        int cur_iid = iid_start_pairs[i].first;
        int cur_start = iid_start_pairs[i].second;
        Interval* itv = all_intervals[cur_iid];
        for (int j = 0; j < itv->liveranges.size(); j ++) {
            if (itv->liveranges[j].isInRange(cur_start)) {
                itv->liveranges[j].pos = assigned_registers[i];
                break;
            }
        }
    }

    //###########################################################
    // Print Physical Register Status Transition
    //###########################################################
    std::cout << "------------RegisterTransition---BEGIN-------" << std::endl;
    for (int i = 0; i < transitions.size(); i ++) {
        for (auto it=transitions[i].begin(); it!=transitions[i].end(); it++)  {
            std::cout << "r" << it->first << ":" << it->second << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "------------RegisterTransition----END------" << std::endl;
}

