# Test volume fixtures

Synthetic VeraCrypt-compatible `.hc` files used by native smoke tests, fixture verification,
and instrumented Android tests. **Passwords are never stored in the repo** — only in
`TEST_VOLUME_PASSWORD` (environment / CI secret).

## Layout

| Path | Role |
|------|------|
| `testdata/manifest.json` | Declares each fixture (algorithm, size, filesystem). |
| `testdata/fixtures/*.hc` | Pre-generated encrypted containers (git-tracked binaries). |

## Verify (CI: `fixtures-verify` stage)

```bash
export TEST_VOLUME_PASSWORD='your-synthetic-password'
scripts/fixtures/generate-fixtures.sh --verify
```

Checks manifest JSON, file presence, size, and (when `TEST_VOLUME_PASSWORD` is set) runs the
native smoke binary against each listed fixture.

## Regenerate

Full regeneration requires the VeraCrypt `vcencrypt` / format tools on a trusted workstation.
`generate-fixtures.sh` documents the exact parameters and can re-run smoke verification after
manual replacement of blobs under `testdata/fixtures/`.
