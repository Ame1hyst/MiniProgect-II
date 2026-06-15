#include <utility>
#include "state.hpp"
#include "pvs.hpp"


/*============================================================
 * PVS — qsearch
 *
 * Quiescence Search (qsearch) to resolve tactical capture sequences
 * at the leaves (depth <= 0). This prevents the "horizon effect"
 * where the engine makes poor moves because a capture was just beyond
 * its search depth.
 *============================================================*/
int PVS::qsearch(
    State*           state,
    int              alpha,
    int              beta,
    GameHistory&     history,
    int              ply,
    SearchContext&   ctx,
    const PVSParams& p
){
    // [TODO PVS-Q-1]
    // 1. Increment ctx.nodes.
    ctx.nodes++;
    if (ply > ctx.seldepth) ctx.seldepth = ply;
    if (ctx.stop) return 0;

    // 2. Get static evaluation (stand-pat score):
    //      int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);

    // 3. If stand_pat >= beta, return beta (beta cutoff).
    if (stand_pat >= beta) {
        return beta;
    }

    // 4. If stand_pat > alpha, update alpha = stand_pat.
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    // 5. Generate legal moves if they have not been generated yet:
    //      if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
    //          state->get_legal_actions();
    //      }
    if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
        state->get_legal_actions();
    }

    // 6. Loop through all legal_actions:
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
    // 7. Return alpha.
    for (auto& action : state->legal_actions) {
        bool is_capture = false;
        if (action.first.first < BOARD_H) {
            if (state->piece_at(1 - state->player, action.second.first, action.second.second) > 0) {
                is_capture = true;
            }
        }

        if (!is_capture) continue;

        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();

        int raw = qsearch(next, -beta, -alpha, history, ply + 1, ctx, p);
        int score = same ? raw : -raw;

        delete next;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    
    return alpha;
}


/*============================================================
 * PVS — eval_ctx
 *
 * Negamax with Principal Variation Search (PVS).
 *============================================================*/
int PVS::eval_ctx(
    State*           state,
    int              depth,
    int              alpha,
    int              beta,
    GameHistory&     history,
    int              ply,
    SearchContext&   ctx,
    const PVSParams& p
){
    // [TODO PVS-A-1]
    // Increment ctx.nodes by 1 — this counts every node visited.
    // Update ctx.seldepth if ply is deeper than the current seldepth.
    // If ctx.stop is true, return 0 immediately.
    //
    // Hint: copy this from alphabeta.cpp.
    ctx.nodes++;
    if (ply > ctx.seldepth) ctx.seldepth = ply;
    if (ctx.stop) return 0;


    // [TODO PVS-A-2]
    // Generate legal moves if they have not been generated yet.
    // Condition: state->legal_actions is empty AND state->game_state == UNKNOWN
    // Call: state->get_legal_actions()
    if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
        state->get_legal_actions();
    }


    // [TODO PVS-A-3]
    // Handle terminal states:
    //   - If state->game_state == WIN  → return P_MAX - ply   (prefer faster wins)
    //   - If state->game_state == DRAW → return 0
    if (state->game_state == WIN)  return P_MAX - ply;
    if (state->game_state == DRAW) return 0;


    // [TODO PVS-A-4]
    // Handle repetition:
    //   int rep_score;
    //   if (state->check_repetition(history, rep_score)) return rep_score;
    // Then push the current position hash:
    //   history.push(state->hash());
    int rep_score;
    if (state->check_repetition(history, rep_score)) return rep_score;
    history.push(state->hash());


    // [TODO PVS-A-5]
    // Handle the leaf node (depth <= 0):
    //   If p.use_quiescence is true:
    //       int score = qsearch(state, alpha, beta, history, ply, ctx, p);
    //   Else:
    //       int score = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    //   Pop the hash from history before returning.
    if (depth <= 0) {
        int score;
        if (p.use_quiescence) {
            score = qsearch(state, alpha, beta, history, ply, ctx, p);
        } else {
            score = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
        }
        history.pop(state->hash());
        return score;
    }


    // [TODO PVS-A-6]  ← THIS IS THE PVS NEGABASE SEARCH LOOP
    // Principal Variation Search loop with null-window search:
    //
    //   int best_score = M_MAX;
    //   bool is_first = true;
    //
    //   for (auto& action : state->legal_actions) {
    //       State* next = static_cast<State*>(state->next_state(action));
    //       bool same   = next->same_player_as_parent();
    //       int score;
    //
    //       if (is_first) {
    //           // Search the first move with full window (alpha, beta)
    //           int raw = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
    //           score = same ? raw : -raw;
    //           is_first = false;
    //       } else {
    //           // Search subsequent moves with a null/zero window (alpha, alpha+1)
    //           // Pass -alpha-1 as beta, and -alpha as alpha
    //           int raw = eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);
    //           score = same ? raw : -raw;
    //
    //           // If the null-window search fails high (i.e. score > alpha),
    //           // we must re-search with the full window (-beta, -alpha)
    //           if (score > alpha && score < beta) {
    //               raw = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
    //               score = same ? raw : -raw;
    //           }
    //       }
    //
    //       delete next;
    //
    //       if (score > best_score) best_score = score;
    //       if (score > alpha)      alpha = score;
    //
    //       // Beta cut-off
    //       if (alpha >= beta) break;
    //   }
    //
    //   history.pop(state->hash());
    //   return best_score;
    int best_score = M_MAX;
    bool is_first = true;

    for (auto& action : state->legal_actions) {
        State* next = static_cast<State*>(state->next_state(action));
        bool same   = next->same_player_as_parent();
        int score;

        if (is_first) {
            int raw = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            score = same ? raw : -raw;
            is_first = false;
        } else {
            int raw = eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);
            score = same ? raw : -raw;

            if (score > alpha && score < beta) {
                raw = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
                score = same ? raw : -raw;
            }
        }

        delete next;

        if (score > best_score) best_score = score;
        if (score > alpha)      alpha = score;

        if (alpha >= beta) break;
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * PVS — search
 *
 * Root-level search: iterates legal moves, calls eval_ctx for
 * each, tracks the best move, and returns a SearchResult.
 *============================================================*/
SearchResult PVS::search(
    State*         state,
    int            depth,
    GameHistory&   history,
    SearchContext& ctx
){
    ctx.reset();
    PVSParams p = PVSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if (!state->legal_actions.size())
        state->get_legal_actions();

    // [TODO PVS-B-1]
    // Set up the root alpha-beta window:
    //   int alpha = M_MAX - 10;
    //   int beta  = P_MAX + 10;
    //
    // Hint: similar to root search in alphabeta.cpp.
    int alpha = M_MAX - 10;
    int beta  = P_MAX + 10;


    // [TODO PVS-B-2]
    // Loop over state->legal_actions and implement root PVS search:
    //
    //   int best_score = M_MAX - 10;
    //   int move_index = 0;
    //   int total_moves = (int)state->legal_actions.size();
    //   bool is_first = true;
    //
    //   for (auto& action : state->legal_actions) {
    //       State* next = static_cast<State*>(state->next_state(action));
    //       bool same   = next->same_player_as_parent();
    //       int score;
    //
    //       if (is_first) {
    //           int raw = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
    //           score = same ? raw : -raw;
    //           is_first = false;
    //       } else {
    //           // Null-window search
    //           int raw = eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p);
    //           score = same ? raw : -raw;
    //
    //           // Re-search with full window if needed
    //           if (score > alpha && score < beta) {
    //               raw = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
    //               score = same ? raw : -raw;
    //           }
    //       }
    //
    //       delete next;
    //
    //       if (score > best_score) {
    //           best_score = score;
    //           result.best_move = action;
    //           result.score = best_score;
    //
    //           if (p.report_partial && ctx.on_root_update) {
    //               ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
    //           }
    //       }
    //
    //       if (score > alpha) alpha = score;
    //       move_index++;
    //   }
    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    bool is_first = true;

    for (auto& action : state->legal_actions) {
        State* next = static_cast<State*>(state->next_state(action));
        bool same   = next->same_player_as_parent();
        int score;

        if (is_first) {
            int raw = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            score = same ? raw : -raw;
            is_first = false;
        } else {
            // Null-window search
            int raw = eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p);
            score = same ? raw : -raw;

            // Re-search with full window if needed
            if (score > alpha && score < beta) {
                raw = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
                score = same ? raw : -raw;
            }
        }

        delete next;

        if (score > best_score) {
            best_score = score;
            result.best_move = action;
            result.score = best_score;

            if (p.report_partial && ctx.on_root_update) {
                ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }

        if (score > alpha) alpha = score;
        move_index++;
    }


    // [TODO PVS-B-3]
    // Populate the final fields of the result:
    //   result.nodes = ctx.nodes;
    //   result.seldepth = ctx.seldepth;
    //   result.pv = { result.best_move };
    // And return result.
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = { result.best_move };

    return result;
}


/*============================================================
 * PVS — default_params / param_defs
 *============================================================*/
ParamMap PVS::default_params(){
    return {
        {"UseKPEval",       "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial",   "true"},
        {"UseQuiescence",   "true"},
    };
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval",       ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial",   ParamDef::CHECK, "true"},
        {"UseQuiescence",   ParamDef::CHECK, "true"},
    };
}
