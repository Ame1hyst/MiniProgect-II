#include <utility>
#include <algorithm>
#include "state.hpp"
#include "pvs_q.hpp"

/*============================================================
 * Transposition Table
 *============================================================*/
static const int TT_BITS = 20;          // 1M entries
static const int TT_SIZE = 1 << TT_BITS;

static const uint8_t TT_EXACT = 1;
static const uint8_t TT_LOWER = 2;     // fail-high (score >= beta)
static const uint8_t TT_UPPER = 3;     // fail-low  (score <= orig_alpha)

static inline uint16_t pack_move(const Move& m){
    return (uint16_t)(
        ((uint16_t)m.first.first  << 9) |
        ((uint16_t)m.first.second << 6) |
        ((uint16_t)m.second.first << 3) |
         (uint16_t)m.second.second
    );
}
static inline Move unpack_move(uint16_t pm){
    return Move(
        Point((size_t)((pm >> 9) & 7), (size_t)((pm >> 6) & 7)),
        Point((size_t)((pm >> 3) & 7), (size_t)(pm        & 7))
    );
}

struct TTEntry {
    uint64_t key         = 0;
    int32_t  score       = 0;
    uint16_t packed_move = 0;
    uint8_t  depth       = 0;
    uint8_t  flag        = 0;
};

static TTEntry tt[TT_SIZE];

static const Move INVALID_MOVE = Move(
    Point((size_t)-1, (size_t)-1),
    Point((size_t)-1, (size_t)-1)
);


/*============================================================
 * PVSQ — quiesce
 *============================================================*/
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

    if(state->game_state == WIN)  return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    // Stand pat
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    if(stand_pat >= beta) return beta;
    if(stand_pat > alpha) alpha = stand_pat;

    // Collect and sort captures by MVV-LVA
    std::vector<Move> captures;
    for(auto& action : state->legal_actions){
        if(state->piece_at(1 - state->player,
               action.second.first, action.second.second) > 0)
            captures.push_back(action);
    }

    std::sort(captures.begin(), captures.end(), [&](const Move& a, const Move& b){
        int va   = PIECE_VALUES[state->piece_at(1-state->player, a.second.first, a.second.second)];
        int atk_a = PIECE_VALUES[state->piece_at(state->player,  a.first.first,  a.first.second)];
        int vb   = PIECE_VALUES[state->piece_at(1-state->player, b.second.first, b.second.second)];
        int atk_b = PIECE_VALUES[state->piece_at(state->player,  b.first.first,  b.first.second)];
        return (va - atk_a) > (vb - atk_b);
    });

    for(auto& action : captures){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();

        int raw = quiesce(next, history, ply+1,
            same ? alpha : -beta,
            same ? beta  : -alpha,
            ctx, p);
        int score = same ? raw : -raw;
        delete next;

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}


/*============================================================
 * PVSQ — eval_ctx
 *============================================================*/
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

    uint64_t h = state->hash();
    history.push(h);

    // Transposition table lookup
    int      tt_idx    = (int)(h & (uint64_t)(TT_SIZE - 1));
    TTEntry& tte       = tt[tt_idx];
    int      orig_alpha = alpha;
    Move     tt_move   = INVALID_MOVE;

    if(tte.key == h){
        if(tte.packed_move != 0)
            tt_move = unpack_move(tte.packed_move);
        if((int)tte.depth >= depth){
            if(tte.flag == TT_EXACT){ history.pop(h); return tte.score; }
            if(tte.flag == TT_LOWER && tte.score >= beta ){ history.pop(h); return tte.score; }
            if(tte.flag == TT_UPPER && tte.score <= alpha){ history.pop(h); return tte.score; }
        }
    }

    // Quiescence at leaf
    if(depth <= 0){
        history.pop(h);
        return quiesce(state, history, ply, alpha, beta, ctx, p);
    }

    // Move ordering: TT move first, then MVV-LVA
    auto& actions = state->legal_actions;
    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b){
        bool a_tt  = (a == tt_move);
        bool b_tt  = (b == tt_move);
        if(a_tt != b_tt) return a_tt;
        int va    = PIECE_VALUES[state->piece_at(1-state->player, a.second.first,  a.second.second)];
        int atk_a = PIECE_VALUES[state->piece_at(  state->player, a.first.first,   a.first.second)];
        int vb    = PIECE_VALUES[state->piece_at(1-state->player, b.second.first,  b.second.second)];
        int atk_b = PIECE_VALUES[state->piece_at(  state->player, b.first.first,   b.first.second)];
        return (va - atk_a) > (vb - atk_b);
    });

    // PVS negamax
    bool first_move      = true;
    Move best_move_found = INVALID_MOVE;

    for(auto& action : actions){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();
        int score;

        if(first_move){
            int raw = eval_ctx(next, depth-1,
                same ? alpha : -beta,
                same ? beta  : -alpha,
                history, ply+1, ctx, p);
            score = same ? raw : -raw;
            first_move = false;
        } else {
            // Null-window probe
            int raw = eval_ctx(next, depth-1,
                same ?  alpha    : -(alpha+1),
                same ? (alpha+1) :  -alpha,
                history, ply+1, ctx, p);
            score = same ? raw : -raw;

            // Full re-search on fail-high
            if(score > alpha && score < beta){
                int raw2 = eval_ctx(next, depth-1,
                    same ? alpha : -beta,
                    same ? beta  : -alpha,
                    history, ply+1, ctx, p);
                score = same ? raw2 : -raw2;
            }
        }
        delete next;

        if(score >= beta){
            tt[tt_idx] = {h, beta, pack_move(action),
                          (uint8_t)std::min(depth, 255), TT_LOWER};
            history.pop(h);
            return beta;
        }
        if(score > alpha){
            alpha           = score;
            best_move_found = action;
        }
    }

    // Store in TT
    uint8_t  flag = (alpha <= orig_alpha) ? TT_UPPER : TT_EXACT;
    uint16_t pm   = (best_move_found != INVALID_MOVE) ? pack_move(best_move_found) : 0;
    tt[tt_idx] = {h, alpha, pm, (uint8_t)std::min(depth, 255), flag};

    history.pop(h);
    return alpha;
}


/*============================================================
 * PVSQ — search (root)
 *============================================================*/
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

    int alpha = M_MAX;
    int beta  = P_MAX;
    bool first_move = true;

    int move_index  = 0;
    int total_moves = (int)state->legal_actions.size();

    // Root move ordering: MVV-LVA
    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            int va    = PIECE_VALUES[state->piece_at(1-state->player, a.second.first,  a.second.second)];
            int atk_a = PIECE_VALUES[state->piece_at(  state->player, a.first.first,   a.first.second)];
            int vb    = PIECE_VALUES[state->piece_at(1-state->player, b.second.first,  b.second.second)];
            int atk_b = PIECE_VALUES[state->piece_at(  state->player, b.first.first,   b.first.second)];
            return (va - atk_a) > (vb - atk_b);
        });

    for(auto& action : state->legal_actions){
        State* next = static_cast<State*>(state->next_state(action));
        bool same = next->same_player_as_parent();
        int score;

        if(first_move){
            int raw = eval_ctx(next, depth-1,
                same ? alpha : -beta,
                same ? beta  : -alpha,
                history, 1, ctx, p);
            score = same ? raw : -raw;
            first_move = false;
        } else {
            // Null-window probe
            int raw = eval_ctx(next, depth-1,
                same ?  alpha    : -(alpha+1),
                same ? (alpha+1) :  -alpha,
                history, 1, ctx, p);
            score = same ? raw : -raw;

            // Full re-search on fail-high
            if(score > alpha && score < beta){
                int raw2 = eval_ctx(next, depth-1,
                    same ? alpha : -beta,
                    same ? beta  : -alpha,
                    history, 1, ctx, p);
                score = same ? raw2 : -raw2;
            }
        }
        delete next;

        if(score > alpha){
            alpha            = score;
            result.best_move = action;
            result.score     = alpha;

            if(p.report_partial && ctx.on_root_update)
                ctx.on_root_update({result.best_move, alpha, depth,
                                    move_index+1, total_moves});
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