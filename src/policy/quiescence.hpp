#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

/*============================================================
 * Quiescence Search Parameters
 *============================================================*/
struct QuiescenceParams {
    bool use_kp_eval       = true;
    bool use_eval_mobility = true;
    bool report_partial    = true;

    static QuiescenceParams from_map(const ParamMap& m){
        QuiescenceParams p;
        p.use_kp_eval       = param_bool(m, "UseKPEval",       true);
        p.use_eval_mobility = param_bool(m, "UseEvalMobility", true);
        p.report_partial    = param_bool(m, "ReportPartial",   true);
        return p;
    }
};

/*============================================================
 * Quiescence class — declarations only
 * Implementations are in quiescence.cpp
 *============================================================*/
class Quiescence {
public:
    static int qsearch(
        State*                  state,
        int                     alpha,
        int                     beta,
        GameHistory&            history,
        int                     ply,
        SearchContext&          ctx,
        const QuiescenceParams& p
    );

    static int eval_ctx(
        State*                  state,
        int                     depth,
        int                     alpha,
        int                     beta,
        GameHistory&            history,
        int                     ply,
        SearchContext&          ctx,
        const QuiescenceParams& p
    );

    static SearchResult search(
        State*                  state,
        int                     depth,
        GameHistory&            history,
        SearchContext&          ctx
    );

    static ParamMap              default_params();
    static std::vector<ParamDef> param_defs();
};
