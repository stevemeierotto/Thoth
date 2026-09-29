# Northline Badge Relay recovery order

This note applies to incident `NL-BR-4417`.

Perform the recovery in this order and do not reorder it.

1. Start unit `relay-cache.service`.
2. Run `northline-relay check --profile kiln`.
3. Open gate `east-gate` only after that check prints `kiln-ready`.
