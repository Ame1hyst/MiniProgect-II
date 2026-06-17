#include <utility>
#include <algorithm>
#include "state.hpp"
#include "quiescence.hpp"


/*============================================================
 * Quiescence — quiesce
 *============================================================*/
int Quiescence::quiesce(
    State *state,
    GameHistory& history,
    int ply,
    int alpha,
    int beta,
    SearchContext& ctx,
    const QSParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;

    if(state->legal_actions.empty() && state->game_state == UNKNOWN)
        state->get_legal_actions();

    if(state->game_state == WIN)  return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    // eval stand pat
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    if(stand_pat >= beta) return beta; // prune -> return val in [alpha, beta] 
    if(stand_pat > alpha) alpha = stand_pat;

    // Collect captures and sort by MVV-LVA
    std::vector<Move> captures;
    for(auto& action : state->legal_actions){
        if(action.first.first < BOARD_H &&
           state->piece_at(1 - state->player,
               action.second.first, action.second.second) > 0)
            captures.push_back(action);
    }

    std::sort(captures.begin(), captures.end(), [&](const Move& a, const Move& b){
        int victim_a = state->piece_at(1 - state->player, a.second.first, a.second.second);
        int victim_b = state->piece_at(1 - state->player, b.second.first, b.second.second);
        int attk_a = state->piece_at(state->player, a.first.first, a.first.second);
        int attk_b = state->piece_at(state->player, b.first.first, b.first.second);
        return (PIECE_VALUES[victim_a] - PIECE_VALUES[attk_a]) >
               (PIECE_VALUES[victim_b] - PIECE_VALUES[attk_b]);
    });

    for(auto& action : captures){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();

        int raw = quiesce(next, history, ply + 1,
            same ?  alpha : -beta,
            same ?  beta  : -alpha,
            ctx, p);
        int score = same ? raw : -raw;

        delete next;

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}


/*============================================================
 * Quiescence — eval_ctx
 *============================================================*/
int Quiescence::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const QSParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;

    if(state->legal_actions.empty() && state->game_state == UNKNOWN)
        state->get_legal_actions();

    if(state->game_state == WIN)  return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    int rep_score;
    if(state->check_repetition(history, rep_score)) return rep_score;
    history.push(state->hash());

    if(depth <= 0){
        history.pop(state->hash());
        return quiesce(state, history, ply, alpha, beta, ctx, p);
    }

    // Sort all moves — captures first by MVV-LVA
    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            int victim_a = state->piece_at(1 - state->player, a.second.first, a.second.second);
            int vb = state->piece_at(1 - state->player, b.second.first, b.second.second);
            return PIECE_VALUES[victim_a] > PIECE_VALUES[vb];
        });

    for(auto& action : state->legal_actions){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();

        int raw = eval_ctx(next, depth - 1,
            same ? alpha : -beta,
            same ? beta  : -alpha,
            history, ply + 1, ctx, p);
        int score = same ? raw : -raw;

        delete next;

        if(score >= beta) { history.pop(state->hash()); return beta; }
        if(score > alpha) alpha = score;
    }

    history.pop(state->hash());
    return alpha;
}


/*============================================================
 * Quiescence — search
 *============================================================*/
SearchResult Quiescence::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    QSParams p = QSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size())
        state->get_legal_actions();

    int alpha = M_MAX;
    int beta  = P_MAX;

    int move_index  = 0;
    int total_moves = (int)state->legal_actions.size();

    // Sort root moves — captures first
    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            int victim_a = state->piece_at(1 - state->player, a.second.first, a.second.second);
            int vb = state->piece_at(1 - state->player, b.second.first, b.second.second);
            return PIECE_VALUES[victim_a] > PIECE_VALUES[vb];
        });

    for(auto& action : state->legal_actions){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();

        int raw = eval_ctx(next, depth - 1,
            same ? alpha : -beta,
            same ? beta  : -alpha,
            history, 1, ctx, p);
        int score = same ? raw : -raw;

        delete next;

        if(score > alpha){
            alpha = score;
            result.best_move = action;
            result.score = alpha;

            if(p.report_partial && ctx.on_root_update)
                ctx.on_root_update({result.best_move, alpha, depth, move_index + 1, total_moves});
        }
        move_index++;
    }

    result.nodes    = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv       = {result.best_move};
    result.score    = alpha;
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