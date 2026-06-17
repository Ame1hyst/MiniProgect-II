#include <utility>
#include <algorithm>
#include "state.hpp"
#include "pvs_q.hpp"


int PVSQ::quiesce(
    State *state,
    GameHistory& history,
    int ply,
    int alpha,
    int beta,
    SearchContext& ctx,
    const PVSQParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;

    if(state->legal_actions.empty() && state->game_state == UNKNOWN)
        state->get_legal_actions();

    if(state->game_state == WIN)  return -(P_MAX - ply);
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

    std::stable_sort(captures.begin(), captures.end(), [&](const Move& a, const Move& b){
        int victim_a = state->piece_at(1 - state->player, a.second.first, a.second.second);
        int victim_b = state->piece_at(1 - state->player, b.second.first, b.second.second);
        int attk_a = state->piece_at(state->player, a.first.first, a.first.second);
        int attk_b = state->piece_at(state->player, b.first.first, b.first.second);
        return (100 * PIECE_VALUES[victim_a] - PIECE_VALUES[attk_a]) >
        (100 * PIECE_VALUES[victim_b] - PIECE_VALUES[attk_b]);
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

// pvs for ordering, Q search at leaf
int PVSQ::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSQParams& p
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

    //leaf
    if(depth <= 0){
        history.pop(state->hash());
        return quiesce(state, history, ply, alpha, beta, ctx, p);
    }

    // Move ordering: captures first (MVV-LVA), then quiet moves
    std::stable_sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            int va = state->piece_at(1 - state->player, a.second.first, a.second.second);
            int vb = state->piece_at(1 - state->player, b.second.first, b.second.second);
            return PIECE_VALUES[va] > PIECE_VALUES[vb];
        });

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
            // Moves 2++: null window 
            int raw = eval_ctx(next, depth-1,
                same ?  alpha    : -(alpha+1),  // null window (almost zero)
                same ? (alpha+1) :  -alpha,
                history, ply+1, ctx, p);
            score = same ? raw : -raw;
            
            // re-search - better than assume but not beta cut-off
            if(score > alpha && score < beta){ 
                int raw2 = eval_ctx(next, depth-1,
                    same ? score : -beta,
                    same ? beta  : -score,
                    history, ply+1, ctx, p);
                score = same ? raw2 : -raw2;
            }
        }
        delete next;
        if (score >= beta) { history.pop(state->hash()); return beta; } // prune - beta cut-off
        if (score > alpha) alpha = score;
 
    }
    history.pop(state->hash());
    return alpha;
}

// PVSQ serach
SearchResult PVSQ::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    PVSQParams p = PVSQParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size())
        state->get_legal_actions();

    // Initial MVV-LVA sort
    std::stable_sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            int va = state->piece_at(1 - state->player, a.second.first, a.second.second);
            int vb = state->piece_at(1 - state->player, b.second.first, b.second.second);
            return PIECE_VALUES[va] > PIECE_VALUES[vb];
        });

    // Move hint from previous iteration to front (set by us after each depth)
    auto hint_it = ctx.params.find("BestMoveHint");
    if(hint_it != ctx.params.end() && hint_it->second.size() == 4){
        const std::string& h = hint_it->second;
        int fr = h[0]-'0', fc = h[1]-'0', tr = h[2]-'0', tc = h[3]-'0';
        auto it = std::find_if(state->legal_actions.begin(), state->legal_actions.end(),
            [&](const Move& m){
                return (int)m.first.first==fr  && (int)m.first.second==fc &&
                       (int)m.second.first==tr && (int)m.second.second==tc;
            });
        if(it != state->legal_actions.end())
            std::rotate(state->legal_actions.begin(), it, it + 1);
    }

    int alpha = M_MAX;
    int beta  = P_MAX;
    bool first_move = true;
    int move_index  = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        if(ctx.stop) break;

        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();
        int score;

        if(first_move){
            int raw = eval_ctx(next, depth - 1,
                same ? alpha : -beta,
                same ? beta  : -alpha,
                history, 1, ctx, p);
            score = same ? raw : -raw;
            first_move = false;
        } else {
            int raw = eval_ctx(next, depth - 1,
                same ?  alpha    : -(alpha+1),
                same ? (alpha+1) :  -alpha,
                history, 1, ctx, p);
            score = same ? raw : -raw;

            if(score > alpha && score < beta){
                int raw2 = eval_ctx(next, depth - 1,
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

            // Save hint for next iteration
            ctx.params["BestMoveHint"] =
                std::to_string(action.first.first)  + std::to_string(action.first.second) +
                std::to_string(action.second.first) + std::to_string(action.second.second);

            if(p.report_partial && ctx.on_root_update)
                ctx.on_root_update({result.best_move, alpha, depth, move_index+1, total_moves});
        }
        move_index++;
    }

    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    return result;
}
/*============================================================
 * PVSQ — default_params / param_defs
 *============================================================*/
ParamMap PVSQ::default_params(){
    return {
        {"UseKPEval",       "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial",   "true"},
    };
}

std::vector<ParamDef> PVSQ::param_defs(){
    return {
        {"UseKPEval",       ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial",   ParamDef::CHECK, "true"},
    };
}