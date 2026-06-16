#include <utility>
#include "state.hpp"
#include "quiescence.hpp"


/*============================================================
 * Quiescence — qsearch
 *
 * Quiescence Search to resolve tactical capture sequences
 * at the leaves (depth <= 0).
 *============================================================*/
int Quiescence::qsearch(
    State*                  state,
    int                     alpha,
    int                     beta,
    GameHistory&            history,
    int                     ply,
    SearchContext&          ctx,
    const QuiescenceParams& p
){
    // [TODO Q-1]
    // 1. Increment ctx.nodes.
    // 2. Update ctx.seldepth if ply > ctx.seldepth.
    // 3. If ctx.stop is true, return 0.
    
    // [TODO Q-2]
    // 4. Get static evaluation (stand-pat score):
    //    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    // 5. If stand_pat >= beta, return beta (beta cutoff).
    // 6. If stand_pat > alpha, update alpha = stand_pat.

    // [TODO Q-3]
    // 7. Generate legal moves if they have not been generated yet.
    
    // [TODO Q-4]
    // 8. Loop through all legal_actions:
    //    a. Check if the action is a capture. In MiniChess, a move is a capture if:
    //         action.first.first < BOARD_H && state->piece_at(1 - state->player, action.second.first, action.second.second) > 0
    //    b. If it is NOT a capture, skip it (continue).
    //    c. If it IS a capture:
    //         - Create child: State* next = static_cast<State*>(state->next_state(action));
    //         - Check if player is same: bool same = next->same_player_as_parent();
    //         - Recursive call to qsearch:
    //             int raw = qsearch(next, -beta, -alpha, history, ply + 1, ctx, p);
    //             int score = same ? raw : -raw;
    //         - delete next;
    //         - Update alpha and check beta cutoff:
    //             if (score >= beta) return beta;
    //             if (score > alpha) alpha = score;
    // 9. Return alpha.

    return 0; // Placeholder
}


/*============================================================
 * Quiescence — eval_ctx
 *
 * Negamax with Alpha-Beta pruning, calling qsearch at the leaves.
 *============================================================*/
int Quiescence::eval_ctx(
    State*                  state,
    int                     depth,
    int                     alpha,
    int                     beta,
    GameHistory&            history,
    int                     ply,
    SearchContext&          ctx,
    const QuiescenceParams& p
){
    // [TODO Q-5]
    // 1. Increment nodes, update seldepth, check stop flag.
    // 2. Lazy generation of legal actions.
    // 3. Handle terminal states (WIN/DRAW).
    // 4. Handle repetition check.
    //
    // 5. Handle the leaf node (depth <= 0):
    //    Instead of calling state->evaluate() like in AlphaBeta,
    //    call qsearch:
    //        int score = qsearch(state, alpha, beta, history, ply, ctx, p);
    //    Pop the hash from history before returning!
    //
    // 6. Alpha-Beta Negamax loop:
    //    Loop through legal_actions, recurse eval_ctx with (-beta, -alpha),
    //    convert to current perspective, update best_score/alpha, and
    //    break if alpha >= beta.
    
    return 0; // Placeholder
}


/*============================================================
 * Quiescence — search
 *
 * Root-level search: iterates legal moves, tracks best move.
 *============================================================*/
SearchResult Quiescence::search(
    State*                  state,
    int                     depth,
    GameHistory&            history,
    SearchContext&          ctx
){
    ctx.reset();
    QuiescenceParams p = QuiescenceParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if (!state->legal_actions.size())
        state->get_legal_actions();

    // [TODO Q-6]
    // 1. Set up the root alpha-beta window:
    //    int alpha = M_MAX - 10;
    //    int beta  = P_MAX + 10;
    // 2. Loop over state->legal_actions. For each action:
    //    a. Get child state.
    //    b. Call eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
    //    c. Convert score.
    //    d. Update best_score and best_move.
    //    e. Call on_root_update if p.report_partial.
    //    f. Update alpha if score > alpha.
    // 3. Return the SearchResult with nodes, seldepth, and pv updated.

    return result;
}


/*============================================================
 * Quiescence — default_params / param_defs
 *============================================================*/
ParamMap Quiescence::default_params(){
    return {
        {"UseKPEval",       "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial",   "true"},
    };
}

std::vector<ParamDef> Quiescence::param_defs(){
    return {
        {"UseKPEval",       ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial",   ParamDef::CHECK, "true"},
    };
}
