#pragma once
/*============================================================
 * Algorithm Registry
 *
 * Each algorithm defines:
 *   - search() function
 *   - default_params() returning ParamMap
 *   - param_defs() for UCI option advertisement
 *============================================================*/

#include <string>
#include <functional>
#include <vector>
#include "search_types.hpp"
#include "game_history.hpp"
#include "minimax.hpp"
#include "random.hpp"
#include "alphabeta.hpp"   // [TODO REGISTRY] — added for the new Alpha-Beta AI
#include "pvs.hpp"


struct AlgoEntry {
    std::string name;
    ParamMap default_params;
    std::vector<ParamDef> param_defs;
    std::function<SearchResult(State*, int, GameHistory&, SearchContext&)> search;
};

inline const std::vector<AlgoEntry>& get_algo_table(){
    static const std::vector<AlgoEntry> table = {
        {
            "minimax",
            MiniMax::default_params(),
            MiniMax::param_defs(),
            [](State* s, int d, GameHistory& h, SearchContext& c){
                return MiniMax::search(s, d, h, c);
            }
        },
        {
            "random",
            Random::default_params(),
            Random::param_defs(),
            [](State* s, int d, GameHistory& h, SearchContext& c){
                return Random::search(s, d, h, c);
            }
        },
        {
            // [TODO REGISTRY] — nothing to implement here, this entry is complete.
            // Once you implement eval_ctx and search in alphabeta.cpp,
            // this entry makes "alphabeta" appear in the GUI dropdown
            // and the UBGI Algorithm option automatically.
            "alphabeta",
            AlphaBeta::default_params(),
            AlphaBeta::param_defs(),
            [](State* s, int d, GameHistory& h, SearchContext& c){
                return AlphaBeta::search(s, d, h, c);
            }
        },
        {
            "pvs",
            PVS::default_params(),
            PVS::param_defs(),
            [](State* s, int d, GameHistory& h, SearchContext& c){
                return PVS::search(s, d, h, c);
            }
        },
    };
    return table;
}

inline const AlgoEntry* find_algo(const std::string& name){
    for(auto& entry : get_algo_table()){
        if(entry.name == name){
            return &entry;
        }
    }
    return nullptr;
}

inline std::string default_algo_name(){ return "minimax"; }
