#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

//Quiescence parameter
struct QSParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;

    static QSParams from_map(const ParamMap& m){
        QSParams p;
        p.use_kp_eval = param_bool(m, "UseKPEval",       true);
        p.use_eval_mobility = param_bool(m, "UseEvalMobility", true);
        p.report_partial = param_bool(m, "ReportPartial",   true);
        return p;
    }
};

class Quiescence {
public:
    static int quiesce(
        State *state,
        GameHistory& history,
        int ply,
        int alpha,
        int beta,
        SearchContext& ctx,
        const QSParams& p
    );
    static int eval_ctx(
        State *state,
        int depth,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const QSParams& p
    );
    static SearchResult search(
        State *state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );
    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};