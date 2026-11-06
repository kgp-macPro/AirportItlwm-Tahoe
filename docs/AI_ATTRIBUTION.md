# AI contribution credit and GitHub attribution

The named roles in [CREDITS.md](../CREDITS.md) describe how KGP used ChatGPT and Codex CLI. They are tool acknowledgements under human direction, supervision, review and hardware validation, without a claim of OpenAI endorsement or project support.

## Verified Codex identity

For substantive Codex CLI assistance on an otherwise required new commit, this repository uses:

```text
Co-authored-by: Codex <noreply@openai.com>
```

This is not an invented address or a community-gist convention:

- OpenAI's [pinned Codex attribution implementation](https://github.com/openai/codex/blob/daa69c411c1ed611a357dd97b35edc089bc80399/codex-rs/ext/git-attribution/src/world_state.rs#L9-L25) specifies this exact trailer.
- OpenAI's official [commit 2647075ff355a4e84756417bf0fe94e66dfab50e](https://github.com/openai/codex/commit/2647075ff355a4e84756417bf0fe94e66dfab50e) uses it. GitHub displays **codex** as co-author. The GitHub Commit.authors API was independently checked on 2026-11-06: the `Codex` / `noreply@openai.com` entry maps to [github.com/codex](https://github.com/codex).
- [GitHub's official co-author documentation](https://docs.github.com/en/pull-requests/how-tos/commit-changes/creating-a-commit-with-multiple-authors) explains the account-associated email requirement and trailer syntax. Contributor-page indexing is separate from verifying attribution on the commit itself.

## ChatGPT and other tools

No suitable, officially documented general GitHub co-author identity for **ChatGPT conversations** was established by this review. ChatGPT's collaborative role is credited fully in the documentation. Do not invent an email or substitute a Codex/cloud connector bot identity for ChatGPT. A connector account's existence does not establish its applicability to this workflow.

The new documentation commit was prepared with Codex CLI assistance and is attributed to Codex only. This does not remove ChatGPT's documented historical contribution or assign all AI activity to Codex.

## Repository-only rule

The human author/committer identity stays unchanged. Add the verified Codex trailer exactly once only when Codex CLI actually contributed substantive work to that new commit. Do not add ChatGPT, Claude or other tools automatically. Preserve original upstream commits and all their author/co-author metadata, including gsfront's Claude trailers.

Do not amend published commits, move release tags, rebuild binaries, create empty attribution commits, or change global Git/Codex settings to populate Contributors. This policy is scoped to AirportItlwm-Tahoe through [AGENTS.md](../AGENTS.md); it does not govern unrelated repositories.
