# Repository-scoped commit attribution

These instructions apply only to this AirportItlwm-Tahoe repository.

- Keep the configured human Git author and committer identities.
- For an otherwise required, non-empty new commit substantially prepared with Codex CLI, append exactly once: `Co-authored-by: Codex <noreply@openai.com>`.
- Attribute only tools that actually contributed. Do not add ChatGPT or a cloud connector bot by default; no applicable official ChatGPT co-author identity has been established. Preserve the documented ChatGPT credit.
- Preserve upstream authorship and existing co-author trailers, including gsfront's original Claude attribution. Do not rewrite published commits, move existing release tags, rebuild release binaries or create empty commits for attribution.
- Do not install attribution hooks or change global Git/Codex configuration. Evidence and scope are in [docs/AI_ATTRIBUTION.md](docs/AI_ATTRIBUTION.md).
