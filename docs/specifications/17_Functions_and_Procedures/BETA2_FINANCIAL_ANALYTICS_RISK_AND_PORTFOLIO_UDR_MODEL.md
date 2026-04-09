# Beta 2 Financial Analytics Risk And Portfolio UDR Model

## Purpose

This document defines the finance-oriented UDR family for pricing, term
structures, cashflow schedules, returns analysis, volatility estimation, and
portfolio optimization.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `QuantLib`, `arch`, `rugarch`, `PyPortfolioOpt`, and
`PerformanceAnalytics`.

## Owning package

- `sb_pkg_fin_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_stats_udr`
- `sb_pkg_opt_udr`
- `sb_pkg_prob_udr`
- `sb_pkg_exact_math_udr`

## Mandatory surfaces

The package shall provide:

- discount curve and yield curve construction
- schedule and coupon generation
- net present value, duration, convexity, and sensitivity helpers
- bond, swap, option, and vanilla derivative pricing for admitted instrument
  classes
- return series construction
- portfolio risk and drawdown metrics
- volatility and GARCH-family estimation for the admitted model set
- mean-variance and related bounded portfolio optimization
- stress, scenario, and backtest helper routines

## Required routine families

At minimum the following families shall exist:

- `sb_fin.curve_build_*`
- `sb_fin.schedule_*`
- `sb_fin.price_*`
- `sb_fin.greeks_*`
- `sb_fin.returns_*`
- `sb_fin.risk_*`
- `sb_fin.volatility_*`
- `sb_fin.portfolio_opt_*`
- `sb_fin.backtest_*`

## Example contract

```sql
select *
from sb_fin.portfolio_opt_mean_variance(
    returns_query => 'select trade_date, asset_id, return_pct from analytics.asset_returns',
    risk_free_rate => 0.03,
    target_return => 0.08
);
```

## Result rules

1. Pricing routines shall return structured outputs including price, inputs
   digest, curve/version ids, and warning state.
2. Risk routines shall return explicit metric vectors, not human-formatted
   text.
3. Portfolio routines shall return weights, constraints applied, solver status,
   and realized objective values.

## Operational rules

1. Time and calendar handling shall be explicit and may not depend on host
   locale defaults.
2. All stochastic pricing and simulation routines shall require explicit seed
   and simulation-budget inputs.
3. Finance routines shall preserve decimal fidelity where a decimal surface is
   required and may not silently demote to binary floating point.
4. Production pricing routines shall expose audit metadata sufficient to
   reproduce a result.

## Explicit exclusions

- unrestricted exotic derivative engines
- real-time market-data network ingestion
- unmanaged pricing plugins outside ScratchBird admission control
