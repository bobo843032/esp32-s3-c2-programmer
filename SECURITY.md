# Security Policy

## Scope

This repository contains an experimental C2 programmer and offline research tools. Use them on owned or explicitly authorized hardware. No vendor affiliation or production-ready safety guarantee is implied.

## Reporting

Report reproducible source or protocol defects with sanitized commands and output. Do not include credentials, private keys, firmware dumps, device identifiers, or personal information in public issues. For sensitive findings, use an available private maintainer contact or GitHub private vulnerability reporting if enabled. No private reporting channel is assumed to exist.

## Destructive operations

Write and erase can permanently destroy firmware and configuration. Software confirmation, ARM, and backup attempts are not electrical safeguards or proof of recoverability. Erasing a page can affect a lock byte even when direct writes to that address are blocked.

See [validation and limitations](docs/validation.md) for the actual checks and unresolved behavior. Test on an isolated recoverable target; target reset or halt can interrupt cooling and other controlled loads.

## Sensitive files

Firmware dumps, Ghidra projects, generated exports, local backups, credentials, and environment files are excluded through `.gitignore`. Ignore rules do not protect files already tracked or uploaded manually. Review every upload, including generated files.

No target firmware or access credentials are included in the repository package. The programmer does not implement a read-protection bypass.
