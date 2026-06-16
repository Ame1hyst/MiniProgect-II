#include <utility>
#include "state.hpp"
#include "alphabeta.hpp"


// AlphaBeta — eval_ctx
int AlphaBeta::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const ABParams& p
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

    // Alpha Beta Negamax    
      for (auto& action : state->legal_actions) {
    
          State* next = static_cast<State*>(state->next_state(action));
          bool same = next->same_player_as_parent(); // for extra turn
    

          int raw = eval_ctx(
            next, depth - 1, 
            same ? alpha : -beta,
            same ? beta  : -alpha, 
            history, ply + 1, ctx, p
            );
    
          // Convert to the current player's perspective.
          int score = same ? raw : -raw;
    
          delete next;
    
          if (score > alpha) alpha = score;

          // Beta cut-off -opponent already has a better option,
          if (alpha >= beta) break; // prune
      }
    
      history.pop(state->hash());
      return alpha;
}


// AlphaBeta search
SearchResult AlphaBeta::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    ABParams p = ABParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if (!state->legal_actions.size())
        state->get_legal_actions();

    // Set up alpha-beta window:
    int alpha = M_MAX;  
    int beta  = P_MAX;   
    
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for (auto& action : state->legal_actions) {
    
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent(); // for extra turn
    

        int raw = eval_ctx(
            next, depth - 1, 
            same ? alpha : -beta,
            same ? beta  : -alpha, 
            history, 1, ctx, p
            );
    
        // Convert to the current player's perspective.
        int score = same ? raw : -raw;
        delete next;
    
        if (score > alpha){
            alpha = score;
            result.best_move = action;
            result.score = alpha;
        
            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, alpha, depth, move_index + 1, total_moves});
            }

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
 * AlphaBeta — default_params / param_defs
 *============================================================*/
ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
