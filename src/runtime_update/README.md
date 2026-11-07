# Wine 11 runtime update integration

The coordinator in `src/runtime_update` is an integration boundary, not an
installer. No release feed, signing key, downloader, PS5 replacement adapter, or
startup recovery code is configured. A null backend reports unsupported and
never offers or installs anything. The game must not present this component as
an operational updater until the following requirements are implemented and
verified on the target installation type.

## Trust and compatibility

Provision an owner-controlled publisher key and an authenticated release feed
out of band. Verify a signed manifest before presenting an offer, with an
immutable release ID, exact artifact hashes and sizes, target title identity,
architecture, minimum compatible platform and runtime layout, and version or
monotonic anti-rollback policy. Reject unknown keys, malformed metadata,
incompatible titles, downgrade/replay and size violations. Use authenticated
transport as well as publisher signatures; TLS alone does not establish package
integrity. The user must see the version and explicitly approve that exact
release. Revalidate metadata and bytes immediately before installation. No
Windows Update, Steam game update, or arbitrary Wine upstream package is a
substitute for a build packaged for this title and platform.

## Target-side installation and recovery

The PS5 adapter must establish that the supported installation mechanism can
replace this specific title's runtime, that sufficient space exists for staging
and recovery, and that no runtime or game process is using the affected files.
It must never overwrite game saves or user data. If these conditions cannot be
proved, refuse installation. Stage verified files separately on the same volume,
then use a platform-supported, durable transaction with a recorded recovery
journal. Preserve the previous working version until a post-restart health check
succeeds. On startup, before launching the runtime, recover any interrupted
transaction to a known working version; if neither version can be validated,
stop and report recovery required. Flush data and journal in the required order
and account for partial writes and power loss. Atomic rename alone does not
establish crash durability, and a running or signed console title may not be
replaceable this way. The exact mechanism must be validated against the actual
PS5 packaging and privileges rather than assumed.

The backend returns `installed` only once the transaction is committed and the
next startup can find the new runtime, `failed_unchanged` or `failed_restored`
only when the previous runtime is verified usable, and `recovery_required`
when that cannot be established. `restart_required` means the UI should tell
the user to close and restart Wine 11; it does not restart automatically.
An unsuccessful check must never be displayed as up to date.

## Integration and validation

Call `tick` from a nonblocking worker or arrange for backend work to run there;
the synchronous coordinator itself does not create threads. Persist check
schedule and transaction state in an appropriate durable store if checks must
survive restarts; the present scheduler is process-local. Bind UI confirmation
to the exact offered release ID and invalidate it when a new check changes the
offer. Before enabling the adapter, test signature failures, manifest replay,
wrong target, interrupted downloads, insufficient space, busy runtime, write and
flush failures, interruptions at every transaction boundary, startup recovery,
restart health checks, and preservation of saves on the actual supported PS5
installation. Test with a real signed release and publisher key controlled by
the project. No release source or key is included here.
