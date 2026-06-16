#include <utility>
#include "state.hpp"
#include "pvs.hpp"



int PVS::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSParams& p
){

    ctx.nodes++; //count node visited
    if(ply > ctx.seldepth) ctx.seldepth = ply; //update depth
    if(ctx.stop) return 0;


    // lazy-generation
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }
    
    //terminal check
    if(state->game_state == WIN)  return P_MAX - ply;
    if(state->game_state == DRAW) return 0;


    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        ); 
        history.pop(state->hash());
        return score;
    }

    // PVS negamax
    bool first_move = true;
    for(auto& action : state->legal_actions){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();
        int score;
        
        if(first_move){
            // Move 1 - same as Alpha-Beta
            int raw = eval_ctx(next, depth-1,
                same ? alpha : -beta,
                same ? beta  : -alpha,
                history, ply+1, ctx, p);
            score = same ? raw : -raw;
            first_move = false;
        }
        else {
            // Moves 2+: null window first
            int raw = eval_ctx(next, depth-1,
                same ?  alpha    : -(alpha+1),  // null window
                same ? (alpha+1) :  -alpha,
                history, ply+1, ctx, p);
            score = same ? raw : -raw;

            if(score > alpha && score < beta){
                int raw2 = eval_ctx(next, depth-1,
                    same ? score : -beta,
                    same ? beta  : -score,
                    history, ply+1, ctx, p);
                score = same ? raw2 : -raw2;
            }
        }
        delete next;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;  
    }
    history.pop(state->hash());
    return alpha;
}


// PVS search
SearchResult PVS::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    PVSParams p = PVSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if (!state->legal_actions.size())
        state->get_legal_actions();

    // setup
    int alpha = M_MAX;
    int beta  = P_MAX;
    bool first_move = true;


    int move_index  = 0;
    int total_moves = (int)state->legal_actions.size();


      for (auto& action : state->legal_actions) {
            State* next = static_cast<State*>(state->next_state(action));
            bool same = next->same_player_as_parent();
            int score;
    
        if (first_move) {
              int raw = eval_ctx(next, depth - 1, 
                same ? alpha : -beta, 
                same ? beta  : -alpha, 
                history, 1, ctx, p);
              score = same ? raw : -raw;
              first_move = false;
          } 
        else {
            // Null window search
            int raw = eval_ctx(next, depth - 1, 
                same ? alpha : -(alpha+1), 
                same ? (alpha+1) : -alpha, 
                history, 1, ctx, p);
              score = same ? raw : -raw;
            if (score > alpha && score < beta) {
                int raw2 = eval_ctx(next, depth-1,
                    same ? score : -beta,
                    same ? beta  : -score,
                    history, 1, ctx, p);
                score = same ? raw2 : -raw2;
              }
          }
    
        delete next;
        
        if(score > alpha){
            alpha = score;
            result.best_move = action;
            result.score = alpha;

            if(p.report_partial && ctx.on_root_update)
                ctx.on_root_update({result.best_move, alpha, depth, move_index+1, total_moves});
        }
        move_index++;
    }

    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    result.score = alpha;
    return result;
}
    

/*============================================================
 * PVS — default_params / param_defs
 *============================================================*/
ParamMap PVS::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
