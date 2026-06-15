#include <utility>
#include "state.hpp"
#include "alphabeta.hpp"


/*============================================================
 * AlphaBeta — eval_ctx
 *
 * Negamax with alpha-beta pruning.
 * Call structure mirrors minimax.cpp but carries alpha & beta.
 *============================================================*/
int AlphaBeta::eval_ctx(
    [[maybe_unused]] State*          state,
    [[maybe_unused]] int             depth,
    [[maybe_unused]] int             alpha,
    [[maybe_unused]] int             beta,
    [[maybe_unused]] GameHistory&    history,
    [[maybe_unused]] int             ply,
    [[maybe_unused]] SearchContext&  ctx,
    [[maybe_unused]] const ABParams& p
){
    // [TODO A-1]
    // Increment ctx.nodes by 1 — this counts every node visited.
    // Update ctx.seldepth if ply is deeper than the current seldepth.
    // If ctx.stop is true, return 0 immediately.
    //
    // Hint: copy the same three lines from MiniMax::eval_ctx().


    // [TODO A-2]
    // Generate legal moves if they have not been generated yet.
    // Condition: state->legal_actions is empty AND state->game_state == UNKNOWN
    // Call: state->get_legal_actions()
    //
    // Hint: same lazy-generation guard as in minimax.cpp.


    // [TODO A-3]
    // Handle terminal states:
    //   - If state->game_state == WIN  → return P_MAX - ply   (prefer faster wins)
    //   - If state->game_state == DRAW → return 0
    //
    // Hint: identical to minimax.cpp.


    // [TODO A-4]
    // Handle repetition:
    //   int rep_score;
    //   if (state->check_repetition(history, rep_score)) return rep_score;
    // Then push the current position hash:
    //   history.push(state->hash());
    //
    // Hint: identical to minimax.cpp — the game handles the rule.


    // [TODO A-5]
    // Handle the leaf node (depth <= 0):
    //   Evaluate the position with state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history)
    //   Pop the hash from history before returning.
    //
    // Hint: identical to minimax.cpp.


    // [TODO A-6]  ← THIS IS THE KEY ALPHA-BETA STEP
    // Negamax loop with pruning:
    //
    //   int best_score = M_MAX;
    //
    //   for (auto& action : state->legal_actions) {
    //
    //       State* next = static_cast<State*>(state->next_state(action));
    //       bool same   = next->same_player_as_parent();
    //
    //       // Recurse one level deeper.
    //       // Pass   (-beta, -alpha)  as the new window for the child.
    //       // (Negamax: child's alpha = -(parent's beta),
    //       //           child's beta  = -(parent's alpha))
    //       int raw   = eval_ctx(next, depth - 1, -beta, -alpha,
    //                            history, ply + 1, ctx, p);
    //
    //       // Convert to the current player's perspective.
    //       int score = same ? raw : -raw;
    //
    //       delete next;
    //
    //       if (score > best_score) best_score = score;
    //       if (score > alpha)      alpha = score;
    //
    //       // Beta cut-off: the opponent already has a better option,
    //       // so we can stop searching the remaining moves.
    //       if (alpha >= beta) break;   // ← prune!
    //   }
    //
    //   history.pop(state->hash());
    //   return best_score;

    // ── placeholder: returns 0 until you implement the TODOs above ──
    return 0;
}


/*============================================================
 * AlphaBeta — search
 *
 * Root-level search: iterates legal moves, calls eval_ctx for
 * each, tracks the best move, and returns a SearchResult.
 *============================================================*/
SearchResult AlphaBeta::search(
    State*         state,
    [[maybe_unused]] int            depth,
    [[maybe_unused]] GameHistory&   history,
    SearchContext& ctx
){
    ctx.reset();
    ABParams p = ABParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if (!state->legal_actions.size())
        state->get_legal_actions();

    // [TODO B-1]
    // Set up the root alpha-beta window:
    //   int alpha = M_MAX - 10;   // best score found so far (start very low)
    //   int beta  = P_MAX + 10;   // opponent's upper bound (start very high)
    //
    // Hint: alpha/beta at the root span the full range so nothing is pruned
    // before we have evaluated at least one move.


    // [TODO B-2]
    // Loop over state->legal_actions (use move_index and total_moves like
    // minimax.cpp does for the on_root_update callback).
    //
    // For each action:
    //   a. Call state->next_state(action) to get the child state.
    //   b. Call eval_ctx(next, depth-1, -beta, -alpha, history, 1, ctx, p)
    //   c. Convert the raw score: score = same ? raw : -raw
    //   d. delete next
    //   e. If score > alpha:
    //        - alpha          = score
    //        - result.best_move = action
    //        - result.score   = score
    //        - call ctx.on_root_update if p.report_partial && ctx.on_root_update
    //   f. Increment move_index.
    //
    // NOTE: Do NOT prune at the root — we want to report all root moves.


    // [TODO B-3]
    // Fill in result fields and return:
    //   result.nodes    = ctx.nodes;
    //   result.seldepth = ctx.seldepth;
    //   result.pv       = { result.best_move };
    //   return result;

    return result; // ← remove this line once you implement B-3
}


/*============================================================
 * AlphaBeta — default_params / param_defs
 *
 * No changes needed here — these are already complete.
 * They tell the GUI which checkboxes to show for this algorithm.
 *============================================================*/
ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval",       "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial",   "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval",       ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial",   ParamDef::CHECK, "true"},
    };
}
