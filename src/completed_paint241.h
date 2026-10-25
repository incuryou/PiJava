#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Host-turn budget, NOT a paint continuation or a game-clock/FPS cap. Only
// already completed paint callbacks count. Pending work is retained, with
// explicit alternating service for serial callbacks and Java worker turns.
struct CompletedPaintBudget241 {
    bool enabled=true,active=false,serial_owed=false,worker_owed=false;
    int owed_canvas=0;
    int64_t completed_us=0;
    uint64_t turns=0,handoffs=0,serial_deferrals=0,worker_priorities=0;
    static constexpr int64_t BudgetUs=8000;
    void reset(){bool e=enabled;*this=CompletedPaintBudget241{};enabled=e;}
    void begin(bool native){active=enabled&&native;completed_us=0;if(active)++turns;}
    void painted(int64_t us){if(active)completed_us+=std::max<int64_t>(0,us);}
    bool exhausted() const {return active&&completed_us>=BudgetUs;}
    bool skip_prepaint(int canvas) const {return active&&serial_owed&&owed_canvas==canvas;}
    void serviced_serial(){serial_owed=false;owed_canvas=0;}
    void end(int canvas,bool serial_pending,bool worker_pending){
        if(exhausted()){
            ++handoffs;serial_owed=serial_pending;owed_canvas=canvas;
            if(serial_pending)++serial_deferrals;
            worker_owed=worker_pending;
        }else if(!serial_pending)serviced_serial();
        active=false;
    }
};
 VMS_EXEC_ACCESSOR(g_completed_paint241, CompletedPaintBudget241)
