# Beta 2 Machine-Learning Utility And Inference UDR Model

## Purpose

This document defines the bounded machine-learning utility and inference family
for ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `scikit-learn`.

## Owning package

- `sb_pkg_ml_udr`

## Stage dependency

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_arrow_udr`

It may consume:

- `sb_pkg_stats_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_symbolic_udr`

## Mandatory surfaces

The package shall provide:

- preprocessing
- normalization and scaling
- imputation
- encoding
- train/test split helpers
- feature extraction and feature selection helpers
- metrics
- bounded model fitting for admitted classical models
- model serialization
- model loading
- batch inference
- row-at-a-time inference

## Required routine families

At minimum the following routine families shall exist:

- `sb_ml.fit_*`
- `sb_ml.predict_*`
- `sb_ml.transform_*`
- `sb_ml.score_*`
- `sb_ml.model_save(...)`
- `sb_ml.model_load(...)`
- `sb_ml.metrics_*`

## Admitted model classes

Beta 2 shall admit only classical bounded models that fit the ScratchBird
operational model well, including:

- linear regression
- logistic regression
- naive Bayes
- k-nearest-neighbor for bounded inference surfaces
- decision-tree and forest surfaces where bounded and explicitly admitted
- clustering helpers for bounded workloads

Deep-learning frameworks and unrestricted training runtimes are excluded.

## Example contract

```sql
select *
from sb_ml.predict_logistic(
    model_artifact => :model_blob,
    source_query => 'select income, age, score from analytics.candidates'
);
```

## Artifact rules

1. Every model shall have:
   - algorithm id
   - feature schema
   - training metadata
   - hyperparameter record
   - model payload
   - version id
2. Inference must reject a model when feature schema or type mapping does not
   match the invocation contract.
3. Saved models must be catalog-addressable and auditable.

## Operational rules

1. Fitting uses `stateful_fit`.
2. Inference uses `stateful_inference` or `vectorized_deterministic`.
3. Training routines shall be bounded by explicit runtime and memory policy.
4. Inference routines shall expose per-batch timing, rows scored, and model
   cache-hit metrics.

## Explicit exclusions

- unrestricted neural-network training stacks
- GPU-first deep-learning frameworks
- notebook-native experimentation surfaces
- remote model registries as a baseline requirement
