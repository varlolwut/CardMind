# CardMind Phase 4 traceability

This is the only active Phase 4 matrix. Each row is one independently observable
roadmap, interface, recovery, security, or closure boundary. Evidence is retained only
after that row's own acceptance observations pass.

## Scope lock

- Implement only `ROADMAP.md` Phase 4 SSH and remote-workspace behavior.
- The primary P4 task agent may use subagents at its own discretion for read-only review,
  bounded research, inventory, test-result analysis, and an outside opinion. The primary
  agent personally writes and integrates all repository code, tests, and harnesses; subagents
  do not edit the repository unless the user explicitly requests that delegation.
- SSH profiles have a product maximum of five. P4-01 uses a bounded store and must not add an
  SD-paginated profile index or scalable profile/storage framework.
- Extend the Phase 3 tool-policy, catalog, preview, confirmation, audit, and cancellation
  boundaries; do not create a parallel permission, profile, job, or execution framework.
- Extend existing profile CRUD/UI, the single Web PTY/ANSI terminal, manual SFTP/transfer
  streaming and host-key verification/store. Retain existing Device SD scrollback/recall;
  do not replace these boundaries with parallel subsystems.
- Long commands and terminals remain foreground. Background queues, durable jobs, reboot
  resume, running-job reconnect, automatic retry/result handles, generic execution budgets,
  and configurable audit retention remain unphased future work.
- Python, USB, API profiles/PWA/encrypted whole-backup, recovery/HTTPS, and the general
  security audit remain Phases 5 through 9.
- Secret bytes and paths, private-key bytes, passphrases, and SSH authority internals never
  enter model context, tool schemas/results, Web/API read routes, workspace tools, serial,
  logs, diagnostics, exports, or Git.
- Read-only/mutating classification applies only to model-issued `ssh_command` through the
  existing Phase 3 permission/runtime path. It does not change the authority semantics of an
  explicit manual Device or Web terminal session.
- Keep the existing single Web terminal and bounded Device terminal history. Phase 4 adds no
  terminal tabs, terminal-session UI state, configurable history rotation, or new history
  subsystem.
- `main` remains unchanged. Phase 4 may merge only to `develop` after closure passes.

### User scope reduction (2026-08-30)

The user explicitly removed, without implementation: Web terminal tabs, the 8 KiB direct-command
increase, configurable Device history rotation/new viewer, known_hosts pagination/rotation,
separate profile diagnostics, separate P4-18/P4-19 journey subsystems, and broad P4-20
re-certification of unchanged NVS/SD. Existing single Web terminal, 1,024-byte command cap,
terminal.log/old.log and bounded known_hosts remain. Host-key mismatch blocking remains mandatory.
P4-16/P4-17 own the required consolidated Device/Web controls. Safe Actions remain only as a
small fixed built-in reviewed set without presets, editor, macros, persistence or framework.

## Execution matrix

| ID | Atomic boundary | Required observation | Status |
| --- | --- | --- | --- |
| P4-01 | Existing indexed NVS SSH profile store capped at five, stable opaque non-secret uint64 IDs, public summaries, selected-profile JIT secret load, and ID-only legacy compatibility | Five profiles and IDs persist and a sixth is rejected before any write; IDs survive edit/select/delete/reboot and move with profiles; summaries read and return no w/k; selected runtime reads only exact selected w/k; legacy count zero through five adds only missing i0 through i4 and count above five stays unchanged | completed |
| P4-02 | Opaque private-key records and explicit profile-to-key ID binding | No shared mutable key slot; key material remains write-only and unreachable through model/file/read APIs | completed |
| P4-03 | Conservative model SSH classification in the existing Phase 3 policy/runtime path | Every arbitrary shell `ssh_command` remains `SshMutate`; `SshRead` is available only to exact fixed reviewed safe actions or strict non-shell templates; no regex/prefix shell classifier exists; manual Device/Web terminal authority is unchanged | completed |
| P4-04 | Configurable model SSH command timeout/output policy while retaining the existing 1,024-byte direct command cap | Timeout/output policy is explicit; the user-removed 8 KiB increase is not implemented | completed |
| P4-05 | Single-pass streamed SD command log, bounded model summary/reference, cancellation, and downloadable output | Output is written once while streaming to SD; the model receives only a bounded summary/reference; a long foreground command cancels and its log downloads without background execution | completed |
| P4-06 | Paged model SFTP list/read/write/move through existing `SftpReadWrite` and Phase 3 permission/confirmation boundaries | Model listing is bounded/paged; `Ask` confirms every model SFTP call; `Allow` still confirms overwrite/delete/move onto an existing target; overwrite defaults deny and no prior remote target is truncated/deleted before confirmed safe replacement; manual Device/Web SFTP remains direct-user authority | completed |
| P4-07 | Existing streaming CardMind workspace transfer to/from selected remote host | Both directions preserve workspace policy, total foreground deadline and cooperative cancel; overwrite defaults deny and replacement never destroys the prior target before confirmation | pending |
| P4-08 | Small fixed built-in Safe Actions set for logs, service state, containers, disk and processes | Reviewed fixed actions obey existing Off/Ask/ceiling/host-key/timeout/audit boundaries; no presets, editor, macros, persistence schema or action framework | pending |
| P4-09 | Removed by user: Web terminal tabs; existing single terminal remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-10 | Removed by user: configurable Device terminal-history rotation and new viewer; existing terminal.log/old.log remains | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-11 | Existing bounded known_hosts store with unconditional host-key-change block | Every mismatch blocks connection; user-removed pagination/rotation is not implemented | pending |
| P4-12 | Removed by user: separate SSH profile diagnostics feature | Scope closed by explicit user decision; not implemented | removed_by_user |
| P4-13 | Encrypted-at-rest evaluation and physical-access threat-model documentation only | No encrypted vault is implemented in P4; documentation states measured limitations and makes no unsupported encryption claim | pending |
| P4-14 | Project/chat ceilings bound to immutable opaque profile IDs | Authenticated config/project metadata may carry the ID; project/chat selection only narrows hosts and cannot exceed global authority or redirect stale authority | pending |
| P4-15 | Credential/private-key/profile-ID non-addressability across model, file tools, API, logs, serial, and diagnostics | Model SSH never returns credential/private-key bytes, private-key path, or internal profile ID; authenticated config APIs expose only allowed non-secret ID/summary data | pending |
| P4-16 | Consolidated Device Phase 4 journey for profile/security plus required command/SFTP/transfer controls using existing terminal/history | Required Device controls and acceptance are observable without a separate journey subsystem | pending |
| P4-17 | Consolidated Web Phase 4 journey for profile/security plus required command/SFTP/transfer controls using the existing single terminal | Required Web controls and acceptance are observable without terminal tabs or a separate journey subsystem; first real Cardputer private-key auth E2E runs here, with at most one authenticated CSRF-protected forget action for the exact selected profile whose host/port are resolved server-side | pending |
| P4-18 | Removed by user as a separate subsystem: Device command/SFTP/transfer journey | Required controls moved to P4-16; no separate implementation | removed_by_user |
| P4-19 | Removed by user as a separate subsystem: Web command/SFTP/transfer journey | Required controls moved to P4-17; no separate implementation | removed_by_user |
| P4-20 | Changed-boundary-only recovery and security acceptance | Verify only Phase 4 changed boundaries, including mismatch block, deletion of only the exact test-host known_hosts entry and byte-for-byte preservation of unrelated entries; broad re-certification of unchanged NVS/SD was removed by user | pending |
| P4-21 | Phase closure: documentation, focused/full regression, performance/resources, cleanup, independent review, CI/PR/merge | Docs/threat model/licenses/secret scans and exact Device/Web regressions pass without repeating P4-17/P4-20-specific scenarios; idle/general mode retains the 70 KiB floor; active SSH is compared with the existing active-SSH heap/largest-block/stack/latency baseline and must not regress or reset/freeze; SD ownership and exact cleanup are evidenced; final review has no blockers; green CI and reviewed PR merge only to `develop` with `main` and stash unchanged | pending |

## P4-01 design gate

**Status:** completed
**Pivot baseline:** feature/phase-4-ssh-remote-workspace at
99f6de9e0e5d558ba0162c714a14044111d9aab2. The discarded overengineered attempt is retained
locally only at trash/p4-01-overengineered-20260830
(aca0539a29a337e61160d86134d65ef5098a12e1).

### Locked roadmap clauses and acceptance

- Cap saved SSH profiles at five.
- Keep the existing indexed cardmind_ssh NVS representation. Public n/h/p/u/a fields remain
  separate from password/passphrase w/k fields.
- Persist one collision-checked non-zero opaque uint64_t ID for each logical profile index.
  IDs survive edit, selection and reboot, and move with their profile when indexed deletion shifts
  records.
- Expose a bounded public-summary loader that reads no w/k values.
- The selected runtime loader reads only the exact selected profile's w/k values just in time.
- Legacy logical counts zero through five gain only missing ID keys. Existing profile fields,
  selection and secrets are not rewritten by ID assignment. A stored indexed count above five
  fails closed before any ID write.
- A sixth create is rejected after read-only count inspection and before ID assignment or any
  profile NVS write.

### Inventory and ownership

- Existing indexed producers/consumers remain saveSshProfileAt(), saveSshProfile(),
  selectSshProfile(), deleteSshProfile(), Device SshTools.ino, Web Console profile CRUD and
  the bounded compatibility loadSshProfiles() API.
- Selected execution consumers are ssh_tool.cpp, pending_tool_call.cpp, Device terminal and
  Web terminal through loadSshProfile().
- loadSshProfiles() remains the existing compatibility owner for Device/Web editing in P4-01.
  It may materialize full records; P4-16/P4-17 own migration of those UI consumers to public
  summaries. P4-01 claims JIT behavior only for loadSshProfile() and secret-free behavior only
  for loadSshProfileSummaries().
- The older single-profile public keys and password/key_pass remain readable without format
  migration. ID key zero identifies that logical profile until an explicit existing CRUD mutation
  converts it through the existing indexed writer.
- NVS write/crash behavior remains the existing indexed-store behavior. This row adds no new
  recovery owner. SD profile metadata and the legacy SD private-key file are untouched.

### Minimal design

- Add SshProfileSummary, physically containing only opaque ID, name, host, port, username and
  auth mode.
- Store IDs as fixed indexed NVS keys i0 through i4. Zero means missing and is never a legal
  persisted ID.
- Validate all public records, stored selection and all non-zero IDs before assigning a missing
  ID. Generate from ESP32 randomness with a bounded collision check against at most five records,
  persist one missing ID, and read it back.
- loadSshProfileSummaries() rejects stored indexed count above five before writes, reads only
  public keys, assigns only missing IDs for valid zero-to-five legacy state, and returns at most
  five summaries.
- loadSshProfile() first resolves the selected public summary, then opens NVS read-only and reads
  only selected w/k or the one legacy password/key_pass pair.
- Existing CRUD keeps index semantics. Create appends one new ID; edit and selection preserve IDs;
  delete erases the matching ID alongside the profile and writes shifted profile/ID pairs together
  through the existing writer.
- initializeSshStorage() invokes the summary loader after existing known-host recovery so legacy
  IDs are assigned at boot without touching profile fields or secrets.

### Expected write set

- firmware/CardputerAssistant/src/ssh_client.h
- firmware/CardputerAssistant/src/ssh_client.cpp
- .agents/P4_TRACEABILITY.md
- firmware/CardputerAssistant/SshTools.ino only if the existing focused diagnostic cannot prove
  the frozen observations without a smaller direct manual check.
- No pending_tool_call, tool_router, Device/Web UI/assets, policy, project/chat, key storage or
  unrelated production files.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Five plus sixth and reboot | Five profiles, IDs and selection persist; sixth create returns failure with profile fields, secrets, IDs and selection byte-for-byte unchanged |
| ID stability | Edit and select preserve IDs; deleting an index moves every surviving ID with its matching public fields and secret; reboot is unchanged |
| Public summaries | At most five records; no password/passphrase members, reads or returned bytes |
| Selected JIT | Runtime reads only exact selected indexed w/k or exact legacy password/key_pass; switching selection changes the sole secret pair read |
| Legacy zero through five | Only missing i0...i4 keys are added; existing public fields, secrets and selection remain unchanged |
| Legacy above five | All P4-01 loads and mutations fail closed and no ID/profile/secret/selection key changes |
| Focused resources | After review GO, exact-core compile and focused Device run preserve bounded heap/stack/latency and no reset/freeze |

### Forbidden effects

- No public profile blob, binary/string codec, COW slot, binding, generation, orphan GC, marker,
  transaction/recovery layer or new storage framework.
- No pending approval identity, pending_tool_call, tool_router, UI/Web asset, project/chat,
  policy, SFTP, command, known-host or private-key change.
- No profile/secret rewrite during legacy ID assignment and no write on stored indexed count above
  five or sixth create.
- No claim that compatibility Device/Web list consumers are already selected-only JIT.
- No test harness before a read-only code-review GO.

### Gate status

- Independent read-only review returned GO for both the minimal production diff and the retained diagnostic after concrete false-success, bounds, selected-JIT, shifted-identity and restoration defects were corrected.
- The pinned build, COM8 upload, focused real-device diagnostic, reboot persistence observation, resource checks, exact-owned cleanup/restoration and final independent closure review all passed.
- P4-01 is complete. No P4-02 or later production scope was implemented.
### Observed P4-01 evidence after review GO

- Cheap boundary checks passed: only ssh_client.h, ssh_client.cpp and the existing SshTools.ino
  diagnostic changed; diff whitespace and placeholder scans passed.
- Exact pinned build passed with FQBN m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom
  and resolved M5Stack ESP32 core 3.2.1. Flash use was 3,300,002 bytes and global RAM 65,604 bytes.
- Built image: 3,300,192 bytes; SHA-256
  EA16B47B3EE2F6C00921F115FA087260A185F8B285DA00608253D0B700DF03B6.
- COM8 upload completed with verified flash hash and no NVS or microSD erase.
- Focused real-device SSHPROFILETEST passed with exact-owned fixture cleanup and restoration.
  STATUS before/after remained responsive with reset reason 1, free heap 122,644 bytes,
  minimum heap 110,824 bytes and stack margin 7,868 bytes. Largest block was 60,404 before and
  58,356 after, matching the retained P3 largest-block baseline after the changed-boundary run.
- The focused diagnostic directly exercised fill-to-five, sixth-create rejection with unchanged profiles/summaries/selection, ID-preserving edit/select/delete-shift, exact selected-profile JIT load, exact-owned cleanup, restoration and repeated cleanup; it returned `SSHPROFILETEST result=pass`.
- Independent source review verified that public summaries have no password/passphrase members or reads, the selected loader reads only the selected indexed `w/k` pair, valid legacy counts add only missing IDs and stored counts above five fail closed before writes. The reviewer accepted static evidence for the unsafe-to-fixture count-above-five path, so user NVS was not deliberately corrupted.
- A whole-NVS hash comparison changed across reboot because unrelated NVS owners also write there; that result was rejected as a test-method defect and was not used as P4-01 evidence or repeated under the same hypothesis.
- A purpose-built memory-only parser then compared only written `cardmind_ssh/i0...i4` uint64 entries across a hardware reboot. Exact equality passed for one persisted profile; no ID value, raw partition, hash or secret was logged or written to disk.
- Post-reboot focused execution remained responsive with reset reason 11, free heap 120,528 bytes, largest block 56,308 bytes, minimum heap 113,336 bytes and stack margin 7,868 bytes; no freeze or unexpected reset occurred.
- The independent closure reviewer returned `GO` after the exact reboot-ID observation. P4-01 is complete on 2026-08-30.

## P4-02 design gate

**Status:** completed

- Independent pre-edit review returned `GO` for the fixed-five record design, commit ordering, bounded cleanup, selected-key JIT and the migration-only zero-profile owner.
- That design reviewer was closed after this verdict and will receive no P4-02 follow-up or later-row work; final P4-02 code review is reserved for one fresh read-only reviewer after the diff is stable.
**Started:** 2026-08-30 after P4-01 closure and visible-plan transition.

### Locked roadmap clauses and non-goals

- Replace the single shared private-key slot with opaque key records and bind each profile to an
  explicit key ID. Installed key bytes remain write-only and inaccessible to workspace/file tools,
  model context/results, authenticated read APIs, serial, logs, diagnostics and exports.
- Preserve the existing five-profile cap, password/passphrase NVS owner, 16 KiB validation ceiling,
  selected-profile runtime, manual Device/Web authority semantics and one live SSH session.
- Remove the installed legacy SD private-key file only after a complete NVS record and every
  required profile binding have been read back. A failed migration leaves the legacy key and old
  authority intact.
- Do not implement encryption-at-rest, a vault, credential/key manager, repository, codec framework,
  general transaction/recovery engine, profile UI redesign, model policy, project/chat ceilings,
  SFTP changes or P4-03 and later scope.

### Producer, consumer and persisted-state inventory

- Current installed-key authority is the single legacy SD private-key file; libssh2 authenticates
  it through its mounted internal path. The existing exact-owned Web key-upload staging file and
  Device import read an explicitly selected `.pem` or
  `.key` workspace source, then removes that source only after installation succeeds.
- Producers are `installSshPrivateKey()`, Device `installSshKeyFromWorkspace()` and authenticated
  Web `POST /api/ssh/key`. Consumers are `sshProfileIsComplete()`, Device terminal/SFTP,
  Web terminal/SFTP and model `ssh_command` through `loadSshProfile()` and
  `SshClient::authenticateControlled()`.
- Existing profile public fields are indexed `n/h/p/u/a`; password/passphrase secrets remain
  indexed `w/k`; P4-01 profile IDs remain `i0...i4`. P4-02 adds only indexed non-secret profile
  key references, five authoritative key-record slots and one fixed transient copy-on-write slot
  in the same existing SSH NVS owner.
- Device/Web profile CRUD currently materializes full profiles and must preserve the key reference
  during edit and indexed delete/shift. Web state returns only selected-profile installed/not-installed;
  it must not serialize a key ID or key bytes. Model/tool schemas remain unchanged.
- Boot owner is `initializeSshStorage()`. Cleanup runs only at boot and before key/profile mutation,
  scans exactly five profile references and five record slots, reads no unselected key bytes and
  removes only exact unreferenced/incomplete slot entries. There is no SD recovery file or growing
  scan. SD removal during legacy migration/upload fails explicitly and leaves committed authority
  unchanged; upload abort/session close retains existing exact-owned staging cleanup.
- Installed Arduino ESP32 `Preferences` 3.2.1 delegates blobs to `nvs_set_blob`/`nvs_get_blob` and
  commits each write. Espressif documents one data entry per 32 blob bytes plus two overhead entries,
  explicit not-enough-space/value-too-long outcomes and a maximum of the lesser of 508000 bytes or
  97.6% of partition size minus 4000 bytes. The project NVS partition is 0x5000, so P4 must report
  capacity failure and cannot promise five maximum-size keys. Installed libssh2 1.11.1_DEV provides
  `libssh2_userauth_publickey_frommemory()` and removes the need for an installed SD key file.

### Minimal design and state transitions

- Add one internal opaque non-zero `uint64_t` key ID to `SshProfile`; it is never added to
  `SshProfileSummary`, model/tool JSON or Web read state. Persist the per-profile reference as fixed
  indexed keys alongside the existing profile and move it with the profile on delete/shift.
- Store at most five authoritative records plus one fixed transient copy-on-write slot. Each slot has one small ID authority entry and one NVS
  blob containing the same ID followed by 64...16384 PEM bytes. A loader scans only the five small
  profile references and six fixed record IDs, then reads and validates exactly the selected record blob. Authentication zeroizes that one
  heap buffer immediately after libssh2 finishes or fails.
- Install snapshots the stable P4-01 profile ID, validates the staged PEM, finds one free fixed slot,
  writes/readbacks the new blob, writes/readbacks its opaque ID, then writes/readbacks only that
  profile's key reference. Only after the new binding is authoritative may the old unreferenced
  record be removed. NVS capacity failure occurs before binding and preserves the old record/ref.
- The sixth fixed slot exists only so replacement still uses immutable copy-on-write when five
  profiles already own five records; it is not another profile/key capacity or a dynamic registry.
  Crash before record ID commit leaves a fixed-slot blob orphan; crash after ID commit but before
  profile binding leaves a complete unreferenced record; crash after binding leaves the new authority
  and at most the old unreferenced record. The bounded boot/pre-mutation scan handles only these five
  profile references and six record slots. It does not read key values, choose authority, repair a referenced mismatch or expose IDs.
- Legacy migration reads the existing SD key once, creates/readbacks one record, binds existing
  profiles to that exact record without rewriting public fields, passwords or passphrases, verifies
  every binding, and only then removes the legacy SD private-key file. A partial reboot resumes from matching committed
  refs while the legacy file remains. If no profile exists, one migration-only non-secret pointer
  owns the imported record until the first created profile claims it; this is not a general marker
  or transaction API. Conflicting refs, malformed records, missing SD or storage exhaustion fail
  closed and retain the old file.
- Existing Web/Device upload controls are minimally rebound to the selected stable profile ID. The
  existing Web staging and cleanup lifecycle remains; no assets, routes, terminal backend or new UI
  surface are added in this row.

### Expected write set

- `firmware/CardputerAssistant/src/ssh_client.h`
- `firmware/CardputerAssistant/src/ssh_client.cpp`
- `firmware/CardputerAssistant/SshTools.ino`
- `firmware/CardputerAssistant/src/web_console.cpp`
- `.agents/P4_TRACEABILITY.md`
- No `pending_tool_call`, `tool_router`, policy, project/chat, Web asset/state/route, workspace/file,
  SFTP, known-host, partition-table or dependency change.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Record and binding | Two exact-owned profiles can hold distinct opaque records; selected-profile installed state and the reviewed JIT authentication call path resolve only the bound record and zeroize it after use; IDs/bytes are never emitted. First real Cardputer private-key auth E2E is owned by P4-17, not this row |
| Replace/full failure | Replacement switches only the target profile after verified record commit; a deliberately unstoreable bounded key returns an actionable NVS-capacity error and leaves the old ref/record usable |
| Legacy migration/reboot | The existing shared legacy SD private-key file becomes one explicitly bound record, the SD installed-key file disappears only after readback, saved authentication still works and reboot preserves refs |
| Crash/cleanup boundaries | Fixed-slot orphan states are removed at boot/pre-mutation; referenced missing/mismatched records fail closed; cleanup reads no key bytes and never scans beyond five profile references plus six fixed record slots |
| CRUD identity | Edit/select preserve key ref; indexed delete moves refs with profiles and removes only records no remaining profile owns |
| Exposure | Model/file/API/state/serial/log/diagnostic checks contain no key bytes, internal key ID or installed key path; only authenticated selected-profile installed boolean is readable |
| Resources | Exact-core build and focused COM8/Web checks measure NVS capacity, idle heap/stack and active-key SSH against retained baselines with no unexpected reset/freeze |

### First code-review verdict and active-row correction

- The fresh read-only P4-02 code reviewer returned `STOP`; all six findings are classified as
  active-row defects: two stale zero-argument installed-key call sites, ambiguous `cnt` authority
  during cleanup, missing stable-profile recheck at binding commit, an untyped unknown binding
  outcome, no replacement path when five records are authoritative, and installed-state validation
  that did not inspect record identity/PEM.
- The single correction patch must: validate `cnt` type/presence before any deletion; recheck the
  exact stable profile ID immediately before and after `qN` commit; classify binding as committed,
  unchanged or unknown; retain immutable replacement through the one bounded transient slot; and
  make selected installed-state use the same exact record validator as authentication. No build or
  runtime evidence may start until the same reviewer performs its one allowed blocker follow-up.
- The reviewer used its single blocker follow-up and returned `GO`: all six findings are resolved,
  the four-file production scope remains exact, and no key bytes or internal key IDs entered a
  schema, state, log or diagnostic. The reviewer was then closed and will not be reused.

### Evidence log

- Exact-core compilation passed with M5Stack ESP32 `3.2.1` and exact FQBN; the uploaded image is
  3,315,440 bytes with SHA-256 `C613E028604F950FA51AFC5D27F6F0D6D1CC6AED62509D740200322C4AFA52A7`.
- Post-upload idle status passed on COM8 with reset reason 1, free heap 122,600 bytes, largest block
  60,404 bytes, minimum heap 113,128 bytes and stack margin 7,876 bytes.
- The first focused saved-host SSH E2E stopped before authentication with `TCP connection to SSH
  host failed`; Web Console then emitted `result=stopped`. This is classified as an external
  remote-host/connectivity failure, not an active-row firmware defect, and the unchanged experiment
  will not be repeated. P4-02 authentication evidence must use a materially different exact-owned
  reachable local SSH target.
- The exact-owned alternate target used AsyncSSH 2.24.0 with two one-off RSA client keys and passed
  a host loopback public-key smoke. Through the authenticated CardMind WebUI, two collision-checked
  temporary profiles each reported an installed private key, held distinct non-secret references,
  and replacing the first profile's key did not change the second profile's installed state. The
  first profile was then restored to its original one-off key before cleanup. No key bytes, IDs,
  profile values or NVS payloads were printed or retained in evidence.
- A memory-only COM8 inspection used official `esptool` 4.8.1 and Espressif ESP-IDF v5.4.2
  `nvs_parser.py` (SHA-256
  `621BDBF0AC60E34AE190F63BE10F0F1A4DD4D18C25EBAB0CF56FF53A6F2B6C2C`). It wrote no raw flash
  file and returned only the aggregate observation: three profiles, two distinct fixture refs,
  both refs backed by bounded records, and unchanged metadata across reboot.
- Web cleanup selected each fixture only after an exact name check, deleted B then A through the
  existing confirmation, restored the original selection at index zero, and reloaded to observe
  exactly one original profile with both fixture names absent. A post-delete boot/memory inspection
  then observed one profile, no deleted fixture refs, zero orphan records and stable metadata across
  reboot. The first post-cleanup disposable check emitted only an over-redacted generic failure;
  after changing only that harness to expose a fixed assertion code, the materially informative run
  passed. No production correction followed the harness-only failure.
- The final attempt to obtain real device private-key authentication stopped before creating a new
  fixture because WebUI reported the microSD absent. Focused COM8 `status` independently confirmed
  `microsd=unavailable`, `microsd_state=missing`, Wi-Fi connected, reset reason 11, free heap 134,140
  bytes, largest block 67,572 bytes, minimum heap 117,736 bytes and stack margin 9,440 bytes. This is
  classified as an external hardware/lifecycle blocker; P4-02 remains `in_progress`, production is
  unchanged, and authentication will resume only after the card is physically available.
- Exact-owned cleanup stopped the local SSH server and removed both temporary keys, the temporary
  Python environment, official parser copy, stop/ready markers and all disposable server/Web/NVS
  scripts. One key initially resisted deletion because its smoke-test ACL granted read-only access;
  the owning user restored FullControl only on that exact file via DACL, and the final enumerated
  cleanup check reported zero remaining items. Elapsed for this resumed evidence/cleanup pass was
  approximately 45 minutes; the root-cause pivot was from unavailable saved-host transport to a
  local target, followed by an external SD-state stop rather than further firmware edits.
- On resumption, a fresh independent COM8 `status` again reported `microsd=unavailable`,
  `microsd_state=missing` and `micro_sd_required` while Wi-Fi and the rest of the runtime remained
  responsive. Source inspection found the existing `sd-mount` diagnostic performs six `SD.begin`
  attempts, but its success path also invokes `deleteRecordingIfPresent()`. The platform therefore
  rejected that action as potentially destructive; the command did not start and no storage state
  changed. The exact remote observation is persistent card mount/detection failure, not a stale
  Web state or replacement-confirmation state. Physical power-off, microSD reseat and power-on are
  required to distinguish absence/contact failure from a failed card before P4-02 authentication
  evidence can resume; production remains unchanged.
- After the user fully powered off, reseated the same card and powered on, the first read-only COM8
  `status` passed with `microsd=ready`, `microsd_state=ready`, no storage error, chats/files/crash
  journal available, two retained chats and two retained history entries. Reset reason was 1; free
  heap was 122,764 bytes, largest block 60,404 bytes, minimum heap 111,392 bytes and stack margin
  7,876 bytes. No software remount, format or replacement confirmation ran, so recovery used only
  the normal power-on path and preserved the prior card contents.
- The resumed auth-test inventory confirmed that Web unknown-host acceptance persistently calls
  `trustSshHost()`, Web has no exact-host forget route, and profile deletion does not remove a
  known-host entry. The existing automatic primitive `forgetTrustedSshHost(host, port)` is exposed
  only by Device UI and a separate fixed demo diagnostic. Under the user's new rule, a test must
  restore device state automatically after success or failure and may not use a path that can leave
  SD unmounted or touch non-exact-owned data. Therefore no new profile, key or trust entry was
  created while Architect classifies whether the auth E2E belongs to later Web/security integration
  or whether a minimal exact-host cleanup control belongs to a specific current P4 row.
- Architect selected the no-expansion ownership boundary: P4-02 does not add a forget route/control
  or modify known-host behavior. This row owns opaque records, exact profile-to-key binding,
  selected-record JIT load/zeroization in the reviewed authentication call path, immutable
  replacement with preservation on capacity failure, reboot persistence, bounded orphan cleanup
  and non-addressability. The first real Cardputer private-key authentication E2E is explicitly not
  yet proven and belongs to P4-17. P4-17 may add only one authenticated CSRF-protected forget action
  for the exact selected profile, resolving host/port server-side through the existing
  `forgetTrustedSshHost()`; it may not accept a free host, list/clear trust state or create a manager.
  P4-20 owns mismatch blocking and proof that cleanup removes only the exact test-host entry while
  unrelated known-host entries remain unchanged. P4-21 repeats only final regression; a later E2E
  key/JIT defect reopens P4-02 by ownership.
- The resumed capacity scenario used the existing authenticated local Web/API path with one
  collision-checked exact-owned profile. A 1,675-byte RSA PEM first installed successfully. A
  16,384-byte PEM replacement then returned the explicit NVS-capacity rejection while the same
  fixture remained selected, the two-profile public inventory remained unchanged and
  `ssh_key_installed` remained true. The authenticated state exposed neither a private-key record
  ID nor key bytes. This directly proves preservation of the previous binding on capacity failure;
  it does not claim real Cardputer private-key authentication, which remains owned by P4-17.
- Cleanup revalidated the exact fixture name, host, port, user and auth mode before deletion, then
  ran the same absence/restoration check twice. It restored the original one-profile inventory and
  selected index, zeroized both local key buffers, removed only the exact temporary directory, and
  observed SD `ready`, readable and writable with no storage error. The retained two chats/history
  entries were present before the scenario. The same serial holder then received `EXIT` and
  reported `WEB_CONSOLE result=stopped`; no manual recovery or SD lifecycle mutation was needed.
- The functional capacity proof exposed an active-row resource blocker. During the active Web
  scenario free heap was 97,916 bytes, largest block 32,756 bytes, stack margin 5,604 bytes and
  reset reason 1, but minimum heap fell to 57,460 bytes, below the required 70-KiB floor. Source
  ownership is P4-02: `createSshPrivateKeyRecord()` retains the full input record while allocating
  a same-sized readback vector. P4-02 therefore remains `in_progress`; no unchanged experiment will
  be repeated and no later row absorbs this defect.

### Active resource correction gate

- A fresh read-only resource/design reviewer returned `GO` with no blockers. The pinned
  Arduino-ESP32 `Preferences` byte-count contract permits readback into the existing buffer, and
  the installed mbedTLS 3 API provides `mbedtls_sha256()`. Hashing after ID encoding, then checking
  exact length/read count, embedded ID and SHA-256 while wiping digest temporaries preserves the
  existing blob-to-ID-to-binding ordering. The reviewer was closed and will not be reused.
- Frozen minimal change: keep the existing fixed-slot/COW/binding contracts, compute a
  collision-resistant digest of the prepared record, reuse that same secret buffer for NVS
  readback, compare exact length/identity/digest, and preserve all current failure ordering and
  zeroization. Do not change routes, schemas, record layout, slot count, cleanup or authentication.
- Frozen proof after review and one coherent patch: cheap compile/static checks, one exact pinned
  build/upload, then the same single exact-owned small-key plus 16,384-byte capacity-rejection
  scenario with automatic cleanup. It must preserve the old binding, expose no IDs/bytes, keep SD
  ready, avoid reset/freeze and retain at least 70 KiB minimum heap.
- The primary implementation changed only `ssh_client.cpp`: the prepared record is SHA-256 hashed,
  zeroized and reused for complete NVS readback; exact byte count, embedded ID and digest must match.
  `git diff --check` passed. The exact pinned 3.2.1 build passed at 3,315,454 sketch bytes and
  65,612 global bytes; the uploaded image is 3,315,648 bytes with SHA-256
  `BA5853480EE66002ECA10F0945AC024390269528C8E77742C7332F9401ED3240`, and flash verification passed.
- The first post-upload COM8 status reported SD ready with the retained two chats and two history
  entries, reset reason 1, free heap 122,792 bytes, largest block 60,404 bytes, minimum heap 110,980
  bytes and stack margin 7,876 bytes. The focused automatic Web/API rerun then observed explicit
  16,384-byte NVS-capacity rejection, unchanged profile inventory, the previous binding still
  installed and no internal key ID in authenticated state. During that changed boundary free heap
  was 98,804 bytes, largest block 35,828 bytes, minimum heap 74,880 bytes and stack margin 5,588
  bytes; reset reason stayed 1 and SD stayed ready with no error. This raises the measured minimum
  by 17,420 bytes and passes the 70-KiB floor.
- The rerun's `finally` path revalidated exact fixture fields, restored the original one-profile
  inventory and selection, repeated the cleanup as an idempotent no-op, zeroized local key and
  multipart buffers, and left no temporary file. The same serial holder then reported
  `WEB_CONSOLE result=stopped`. Resource-correction elapsed time was about 15 minutes, from the
  20:20 failure classification through the 20:35 completed device cleanup. Final P4-02 closure is
  waiting only for one fresh read-only code review of the stable diff.
- The fresh final code reviewer returned `STOP` with two concrete findings outside the accepted
  SHA-256 buffer-reuse correction. First, indexed profile deletion can reset between separately
  committed public-field, `qN` and `iN` shifts and leave a valid-looking profile paired with the
  previous index's key. Second, indexed removal ignores `Preferences::remove()` failures and can
  report success while an inactive `qN` survives and blocks later initialization. Both findings are
  owned by P4-02 because the unsafe state is the newly added key-reference authority; P4-01 remains
  complete. No production edit or repeated runtime test followed this verdict yet. The reviewer is
  retained only for its one allowed combined follow-up after both blockers are corrected.

### 60-minute closing pivot

**Started:** 2026-08-30 20:52:08 +03:00. **Timebox:** 30 minutes overall, with at most
10 minutes to prove that the existing fields permit fail-closed ordering.

The bounded ordering is possible without reopening P4-01 or adding storage state:

| Reset point in existing indexed delete | Durable observation while old `cnt` remains authoritative |
| --- | --- |
| Before the first write | Original profile, `iN` and `qN` bindings remain valid |
| After destination `iN` takes source `iN`, before/during shifted public and `qN` writes | The source index still owns the same non-zero ID, so duplicate-ID validation fails closed |
| During checked tail removal | The duplicate remains until the tail's required public field is absent; after the tail ID is removed, that missing public field keeps old-`cnt` loading fail closed |
| Last-index deletion with no shift | Remove the tail's required public field first, so old-`cnt` loading fails closed before `qN` or `iN` can become stale |
| After selected-index write, before count write | Old `cnt` still observes the duplicate ID or incomplete tail and fails closed |
| After verified count write | Only the new active prefix is authoritative; each profile, stable ID and key reference is the intended shifted tuple |

- Frozen correction: specialize only the existing indexed-delete write order, check every indexed
  removal, and require every inactive `qN` and `iN` to be absent before reporting success.
- Expected production write set: `firmware/CardputerAssistant/src/ssh_client.cpp` only.
- Forbidden: new schema, public blob, slot indirection, transaction/COW/recovery layer, P4-01
  redesign, adjacent cleanup, helper hunting or another broad inventory pass.
- Frozen proof: cheap static/host checks, the retained reviewer's one combined follow-up, then one
  exact pinned build/upload and the smallest existing delete/shift/cleanup Device boundary. Reuse
  unchanged capacity, resource and reboot evidence; do not rerun the 16-KiB capacity scenario.
- `git diff --check` passed, and the correction added no production file outside
  `ssh_client.cpp`. The retained reviewer used its one combined follow-up and returned `GO`:
  destination-ID duplication remains until required tail metadata is absent, name-first checked
  removal makes last-index deletion fail closed, selection precedes the final count authority
  switch, and final verification rejects any inactive `iN` or `qN`. The reviewer was then closed
  and will not be reused.
- The single pinned closing build passed with exact FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, the unique resolved M5Stack
  core `3.2.1`, 3,318,750 sketch bytes and 65,612 global bytes. The uploaded image is 3,318,944
  bytes with SHA-256 `1D26C08CBDC16889DE116091C66E0F02FFF745CC75B7EB1CEAD97FF0F13FF627`;
  COM8 upload verified the flash hash.
- The existing exact-owned `SSHPROFILETEST` delete/shift/cleanup boundary returned
  `result=pass`. Because each successful delete now includes final inactive-identity verification,
  that runtime pass covers shifted profile/key binding, checked removal and absence of inactive
  `iN/qN`; its retained cleanup/restoration path completed. Post-test status remained responsive
  with SD ready, two retained chats and two history entries, reset reason 1, free heap 122,632
  bytes, largest block 58,356 bytes, minimum heap 111,264 bytes and stack margin 7,812 bytes.
- The disposable serial holder then exited non-zero only because it expected a nonexistent
  `STATUS result=...` suffix after already receiving the complete valid `STATUS` line. This is a
  harness matcher defect, not a firmware failure; no unchanged Device scenario was repeated.
- Unchanged capacity-failure preservation, bounded cleanup, reboot persistence, selected-record
  JIT/zeroization, non-addressability and resource evidence above is reused. The 16-KiB capacity
  scenario was not repeated. Real Cardputer private-key authentication remains explicitly unproven
  here and owned by P4-17. P4-02 is complete; P4-03 is now the sole active row.

### Forbidden effects

- No key record list/editor, user-defined key labels, arbitrary record count, dynamic key registry,
  general cleanup engine, background executor, retry framework or encrypted-at-rest claim.
- No password/passphrase migration, no key bytes on an installed workspace path, no key-ID schema for
  the model, no read/download/export route and no fallback to the legacy SD key after migration commits.
- No deletion of the old key/file before the new record and exact profile bindings are authoritative;
  no ordinary failure result after a binding was confirmed committed.

## P4-03 design gate

**Status:** completed
**Started:** 2026-08-30 21:12:22 +03:00.

### Locked clause and non-goals

- ROADMAP lines 473-475 require every arbitrary model-issued `ssh_command` to remain mutating;
  read authority belongs only to exact fixed reviewed Safe Actions, never inferred from shell text,
  a prefix or a regular expression.
- P4-03 does not implement Safe Actions, SFTP, timeout/output changes, a shell classifier, a command
  parser, another schema, or any Device/Web manual-terminal authority change. P4-08 owns the fixed
  read-authorized actions.

### Existing contract inventory

- `ToolCapability` already persists separate `SshRead` (`sr`) and `SshMutate` (`sm`) policy slots.
  This row changes no policy format, NVS/SD state, migration, reboot, crash or cleanup owner.
- The sole arbitrary shell schema is the canonical `SshCommand`/`ssh_command` catalog row, bound
  directly to `SshMutate`. `buildToolRequestPlan()` takes eligibility only from that catalog
  capability; `resolveChatToolPermissions()` marks `SshRead` unavailable and exposes
  `SshMutate` only when the selected SSH authority is available.
- API schema construction exposes exactly one UTF-8 `command` field. Pending-call normalization
  checks only exact shape, UTF-8 and the retained 1,024-byte bound; it performs no safety parsing.
  Every `SshCommand` is mandatory-confirmation even when policy resolves to Allow, and pending
  execution rechecks selected SSH authority identity.
- `tool_router` dispatches the canonical schema through the existing audited/cancelable
  `executeSshTool()` path. Device `runSshTerminal()` and the existing Web terminal are direct user
  sessions and do not enter model request-plan classification.
- Existing host coverage asserts the catalog maps `SshCommand` to `SshMutate`, mutate eligibility
  exposes the schema, and Allow on `SshRead` with unavailable `SshMutate` exposes no arbitrary SSH
  schema. No vendor API or external standard participates in this application policy decision.

### Minimal design and frozen proof

- Expected repository write set is this traceability entry only. If the existing checks pass, no
  production or test edit is permitted: retaining the exact current mapping is the minimal design.
- After independent design GO, run the existing strict host test once. Required observations are:
  catalog identity remains `SshCommand -> SshMutate`; read-only SSH authority leaves the arbitrary
  schema absent; arbitrary command preview/canonicalization preserves bytes without classifying
  text; mandatory confirmation and audited/cancelable dispatch remain intact.
- A fresh independent read-only closure reviewer then checks the stable no-code boundary and host
  evidence. Device build/upload/Web tests are not useful for an unchanged pure policy/catalog row.
- Independent read-only design review returned `GO` with no blockers. It confirmed that the sole
  catalog schema is bound to `SshMutate`, `SshRead` is unavailable to model requests, argument
  handling performs no safety inference, and mandatory confirmation, authority identity, audit and
  cancellation remain intact. The reviewer accepted the frozen existing host proof and was closed.
- The first WSL host-test invocation was rejected before compilation because the disposable runner
  passed an empty output filename through shell quoting. The changed runner used a fixed exact-owned
  `/tmp` ELF with automatic `trap` cleanup; the strict `-std=c++17 -Wall -Wextra -Werror` suite then
  returned `host_tests: PASS`. This is harness-only evidence and caused no repository change.
- P4-03 has no production or test diff. The passing suite directly covers catalog
  `SshCommand -> SshMutate`, mutate-only schema eligibility, read-only exclusion, mandatory
  confirmation and byte-preserving arbitrary-command preview. A fresh independent read-only
  closure verdict remains before the row can complete.
- A fresh independent read-only closure reviewer returned `GO` with no blockers and was closed.
  P4-03 completed at 2026-08-30 21:24:34 +03:00 after about 12 minutes. No production, test,
  persistence, Device or Web change was made; P4-04 is now the sole active row.

### Forbidden effects

- No regex, prefix, token, AST or shell-content safety inference.
- No assignment of `ssh_command` to `SshRead`, no bypass of Off/Ask/global/project/chat ceilings,
  confirmation, preview, authority identity, audit or cancel.
- No model policy applied to explicit manual Device/Web terminals and no P4-08 implementation.

## P4-04 design gate

**Status:** completed after the approved validator correction
**Started:** 2026-08-30 21:24:34 +03:00.

### Approved reopen gate

- Architect approved reopening only because installed ESP32 core `3.2.1`
  `String::indexOf('\0')` resolves through `strchr` and therefore matches the terminating NUL for
  every non-empty `String`; the observed literal `ls -lR /pub` was rejected before channel open.
- The correction is limited to length-aware embedded-NUL validation on ArduinoJson `7.2.1`
  `JsonString` before constructing `String`, removal of the impossible lower-layer NUL predicate,
  and focused normal-command/embedded-NUL evidence. Timeout, output policy, direct command cap and
  all P4-05 behavior remain unchanged.
- Frozen proof: an ordinary non-empty command parses and reaches execution; a JSON command
  containing `\u0000` fails before connection; existing option defaults/bounds/unknown-field and
  overflow-clear observations still pass. Forbidden effects are any timeout/output-policy change,
  P4-05 implementation change, persistence/UI change or broader validation rewrite.

### Locked clauses and observable contract

- Model-issued `ssh_command` gains only optional `timeout_ms` (`1,000..60,000`, default `60,000`)
  and `max_inline_output_bytes` (`1..16,384`, default `16,384`). The command remains bounded to
  1,024 bytes. Unknown fields, wrong JSON types and out-of-range values fail before connection.
- `timeout_ms` is one total deadline starting immediately before connection and covering connect,
  host-key lookup, authentication and command completion. User cancellation remains a distinct
  canceled outcome; deadline expiry is an explicit failed outcome.
- `max_inline_output_bytes` is the combined stdout/stderr cap returned to the model. P4-04 keeps one
  bounded inline buffer; overflow fails explicitly and returns no partial output. P4-05 alone owns
  one-pass SD logging and bounded summary/reference behavior.
- Values are per call. There is no Settings/storage/migration owner and no Device/Web control.
  Manual Device/Web terminal behavior is unchanged.

### Producer, consumer and failure inventory

- `api_client.cpp` builds the model schema. `pending_tool_call.cpp` validates exact allowed fields,
  normalizes omitted values to explicit defaults and binds the full canonical object to mandatory
  confirmation. Preview continues to show the exact command bytes.
- `ssh_tool.cpp` performs the independent runtime parse, loads only the selected profile, starts the
  total clock immediately before `connectControlled()`, and reuses one latched callback across
  connect/authenticate/execute. One first-observed terminal state is latched as `None`,
  `UserCancelled` or `TimedOut`; user cancellation wins if both are first observed in the same poll.
  The state is polled after every controlled stage returns regardless of that stage's result and
  immediately after host-key lookup before interpreting its result. Only `UserCancelled` maps to a
  canceled outcome, `TimedOut` maps to explicit failure, and a lower-stage error is preserved only
  while the terminal state remains `None`.
- `ssh_client.cpp` already combines stdout and stderr into one buffer and receives explicit timeout
  and output limits. The only lower-boundary correction is to use the row-owned combined-size check,
  clear the partial buffer before returning overflow, and report the actual configured cap.
- There is no persistence, reboot recovery, SD owner, crash replay, cleanup fixture or vendor API
  change. libssh2 and WiFiClient continue through the existing controlled operations.

### Minimal design and expected write set

- Add one small `ssh_command_options.h` contract with constants and pure validation/deadline/output-
  fit helpers. This is not a general execution-budget or output framework.
- Update only `api_client.cpp`, `pending_tool_call.cpp`, `ssh_tool.h/.cpp`, `ssh_client.cpp`, focused
  `tests/host_tests.cpp`, and, only for a small no-network parser/default/bounds Device observation,
  the existing `CardputerAssistant.ino`, `SshTools.ino` and `SerialDiagnostics.ino` diagnostic path.
- The row-owned commit also includes this trace update. It includes no P4-05 behavior or generated
  build/test artifacts.

### Frozen minimal proof

| Observation | Required result |
| --- | --- |
| Defaults and exact shape | Command-only input normalizes to 60,000/16,384; optional fields may appear independently; unknown/mistyped fields fail |
| Bounds | 1,000 and 60,000 ms plus 1 and 16,384 bytes pass; adjacent out-of-range values fail before any connection |
| Total deadline | One wrap-safe elapsed clock is shared across all model SSH stages; deadline expiry is failure, user cancellation remains canceled |
| Inline output | Combined stdout/stderr at the cap succeeds; the first byte beyond it clears partial output and fails with the configured limit |
| Interfaces/non-goals | Model schema documents both fields/defaults/bounds; command preview/mandatory confirmation bind normalized arguments; no Settings/UI/manual-terminal/P4-05 change |

- After design GO: strict host tests first, then one fresh read-only code review. After code-review GO,
  run one exact pinned build/upload and the smallest existing serial diagnostic exercising the same
  parser/default/bounds/deadline/output-fit helpers without network, profile or SD mutation; record
  idle resource/status evidence. No remote fixture or broad SSH regression is required for this
  non-persistent parameter-boundary row.

### Forbidden effects

- No persisted maximum, Device/Web control, generic budget abstraction, dynamic inline allocation
  above 16,384 bytes, truncation-success mode, SD log/reference, background work or retry.
- No change to manual terminal/SFTP authority, timeout or transport behavior; no command-length
  increase; no return of partial stdout/stderr after overflow.

### Design-review blocker and bounded correction

- The independent reviewer returned `STOP` on the initial callback-only wording because blocking
  TCP connect, channel/read timeout or host-key lookup can return after the total deadline without
  polling the callback, allowing a lower-stage error to mask the required timeout outcome.
- The corrected first-observed terminal-state contract above closes that gap without changing lower
  SSH APIs or adding a budget framework. The same reviewer used its one focused follow-up and
  returned `GO`: cancellation/timeout precedence is deterministic, and the terminal state is
  latched during and after every controlled stage and after host-key lookup. The reviewer was then
  closed and will not be reused.

### Implementation and code-review evidence

- The coherent P4-04 diff adds only per-call schema/canonical/runtime options, one SSH-specific
  deadline/output helper, direct combined-output clearing, and a no-network serial diagnostic.
  It adds no persistence, Settings, Device/Web controls, manual-terminal change or P4-05 logging.
- `git diff --check` passed. The strict WSL host suite passed with the exact-owned ELF removed from
  `/tmp`; its first invocation did not compile because the shell expanded an empty output variable,
  and the corrected literal-path invocation passed without a production-code change.
- A fresh read-only code reviewer initially found a missing owning `<cstring>` include and proof
  that exercised only pure helpers. One correction added that include, made the tested append/clear
  primitive the exact `SshClient` path, and added `SSHOPTIONSTEST` over the production parser with
  no profile, network, storage or fixture effects. The strict host suite passed again.
- The reviewer used its single combined follow-up and returned `GO`: both blockers are closed and
  no mandatory defect or scope expansion was introduced. The reviewer was then closed and will
  not be reused. Exact pinned build/upload and Device evidence remain before closure.
- The exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom` and the unique resolved
  M5Stack core `3.2.1`: 3,324,886 sketch bytes and 65,612 global bytes. The uploaded image is
  3,325,072 bytes with SHA-256
  `2AEA99E4683200B10894F331D32A3C4A7BF968B4EC0E6C1F500A234268CC3A66`; every flash segment
  reported hash verification and COM8 returned after reset.
- On the uploaded firmware, `SSHOPTIONSTEST` passed the production parser defaults, exact limits,
  malformed/unknown/type rejection and production combined-output clear path without network,
  profile, NVS or SD mutation. It reported free heap 124,040 bytes, largest block 61,428 bytes and
  stack margin 7,860 bytes.
- Immediate idle `STATUS` reported microSD ready, the preserved two chats/two history entries,
  reset reason 1, free heap 124,304 bytes, largest block 61,428 bytes, minimum heap 113,320 bytes
  and stack margin 7,860 bytes. Against the retained P3 idle baseline, free heap is +1,544 bytes,
  largest block +3,072 bytes and stack margin -8 bytes; the 70-KiB idle floor is preserved with no
  freeze or unexpected reset. No Device fixture existed, and the exact-owned WSL ELF was removed.
- P4-04 completed at 2026-08-30 22:18:57 +03:00 after approximately 54 minutes. P4-05 is now the
  only active row.

### Approved reopen correction evidence

- Static invariants and `git diff --check` passed: exactly one length-aware `JsonString` NUL check
  exists at the parser boundary, the broken lower-layer `String::indexOf('\0')` predicate is absent,
  and the existing diagnostic includes the decoded `\u0000` rejection case.
- A fresh independent read-only code reviewer returned GO with no correctness, security or scope
  blockers and was closed without follow-up or reuse.
- The exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, resolved M5Stack ESP32 core
  `3.2.1`, 3,340,590 sketch bytes and 65,628 global bytes. The uploaded image is 3,340,784 bytes
  with SHA-256 `8082E6D923BB7D8C48C977A73655F802FCEF27AF3F739EEFA26C83AA0B24690F`; every flash segment
  reported hash verification and COM8 returned after reset.
- On that image, `SSHOPTIONSTEST` passed the ordinary command, decoded embedded-NUL rejection,
  unchanged option limits and overflow-clear path: free heap 122,488 bytes, largest block 60,404
  bytes and stack margin 7,860 bytes. Immediate `STATUS` remained responsive with microSD ready,
  two preserved chats/two history entries, reset reason 1, free heap 122,752 bytes, minimum heap
  111,200 bytes and the same largest block/stack margin. No fixture or persisted state was created.
- The approved correction completed at 2026-08-30 23:34:49 +03:00. Standalone commit
  `47d4aaeb3596f9db74741c910de1ad66879bffce` was pushed to the Phase 4 feature branch and GitHub MCP
  verified that exact remote SHA. P4-05 then resumed without a production change.

## P4-05 design gate

### Focused correction after independent design STOP

This correction is authoritative wherever an earlier P4-05 draft is less precise.

- Combined command-output order is the existing SSH callback-delivery order: each stdout chunk,
  then each stderr chunk, exactly as delivered by the single foreground channel loop. Before every
  append, reject a total beyond `UINT32_MAX`; the first spill space preflight covers the complete
  retained inline prefix plus the triggering chunk. While the file is open, the internal persisted
  count means only bytes accepted by exact `File::write` calls. After `flush()` and `close()`, reopen
  the exact file and verify its size. Only that verified size may be reported as `output_bytes` with
  a complete log. A short write, reopen failure, or size mismatch retains the collision-owned
  filename when one was created, but reports storage failure and never claims a verified byte count
  or completeness.
- The output callback returns `OperationResult`; its failure stops channel reads immediately. The
  existing first-observed terminal latch is authoritative, including cancellation precedence when
  cancellation and deadline are first observed together. After reads stop, observe/latch terminal
  state, close channel/session, then perform SD promotion/finalization exactly once. The first
  latched cancellation or timeout remains the tool outcome ahead of later transport, sink, or
  finalization failures, while the error includes relevant storage detail. Cancellation names a log
  only if exact creation succeeded; the API continues to discard canceled tool output, so no
  canceled bytes reach model continuation. This remains one foreground execution with no continuing
  reader after return.
- Promotion is required not only on normal overflow. If cancellation, timeout, connector failure,
  or invalid inline UTF-8 occurs after any below-cap bytes were received, those bytes are promoted
  once before finalization when the original SD remains writable. If promotion cannot start, return
  the primary terminal outcome plus explicit storage failure and no filename. A fixed model summary
  contains metadata only and never samples command bytes.

Frozen focused proof additions:

- Deterministically observe exact-cap inline success and cap-plus-one single-pass spill.
- Observe cancellation below cap and timeout/connector error below cap promoting retained bytes;
  cancellation must not reach model continuation and its error names the log only after creation.
- Spill raw bytes containing NUL/non-UTF-8 and verify exact downloaded bytes without placing those
  bytes in the model result, audit, serial output, or summary.
- Exercise short-write or final-size-mismatch failure at the narrow capture boundary and verify that
  no verified `output_bytes`/completeness claim is emitted.
- Exercise missing/full/removed/replaced SD before creation and an identity/space failure after
  creation; no overwrite, retry, or continued SSH read is allowed.
- Cleanup uses only the returned collision-owned filename, first confirms the original SD volume is
  still active, removes only that file after success or failure, verifies absence, and repeats
  idempotently. Existing non-owned workspace files remain unchanged.

**Focused review status:** `GO` from independent read-only reviewer
`01a05426-cb94-7c51-9a54-1163e18b9a86`; all three concrete blockers were resolved in one focused
follow-up. The reviewer is closed and production evidence may begin.

**Code-review correction status:** `GO` from fresh independent read-only reviewer
`01a05439-74c9-7ae2-b791-1e409205de5e` after its single focused follow-up. The reviewer found four
bounded blockers. The corrected remote diagnostic cancels only after an actual SD spill and accepts
only the exact cancellation outcome; the existing authenticated hardware-Web runner downloads the
exact returned filename and owns two-pass Device cleanup from both its normal path and `finally`;
failed/unverified storage reports a retained filename without claiming it is currently downloadable.
The proportional test-only write set therefore also includes
`tools/hardware_web_e2e.{ps1,mjs}`; it adds one P4-05 suite and no production route or framework.
Static pre-review evidence: `git diff --check`, PowerShell parse and `node --check` all passed.
The reviewer is closed; focused build/device/Web evidence may run.

**Status:** code_review_GO

**Build failure classification:** active-row diagnostic integration defect. The first pinned build
reached Arduino compilation and failed because the preprocessor did not synthesize declarations for
the three new P4-05 `.ino` diagnostic functions before `SerialDiagnostics.ino` consumed them.
Production capture behavior was not implicated. The minimal correction is three explicit declarations
in the existing sketch declaration owner; no contract, route, persistence or test scope changes.

**Device failure classification (2026-08-30 23:14:39 +03:00; elapsed about 45 minutes):**
test/harness defect, not a production failure. The retained diagnostic tried to induce a final-size
mismatch by appending through a second FAT file handle while the capture handle remained open; the
filesystem did not expose that concurrent write to the authoritative reopen, so the synthetic
expectation was invalid. The exact write-count/final-reopen comparison remains independently
code-reviewed, while real missing/full/removed/replaced failures before and after creation stay in
the Device matrix. Remove only the unreliable concurrent-handle injection; do not alter production
capture or repeat the experiment with the same hypothesis. The diagnostic's exact-owned finalizer
ran before the suite returned failure.

**Remote E2E failure classification:** the unchanged existing `SSHDEMOTEST` then passed connect,
authentication, SFTP list/download, PTY and exact host-key cleanup on `test.rebex.net` with
heap 122,092 bytes and stack margin 7,876 bytes. Therefore Wi-Fi, the host, trust and authentication
are not the owner. The new exec/cancel diagnostic returned before any captured chunk, but replaced
its first error with a generic assertion. This is a P4-05 harness-observability defect. One
instrumentation-only correction preserves the underlying bounded libssh2 error in the diagnostic;
production streaming/capture behavior remains unchanged. Do not repeat the remote case until that
new observation is available.

**Foreign regression ownership:** the instrumented remote run observed
`SSH command must contain 1 to 1024 bytes without NUL characters` before channel open for the
literal command `ls -lR /pub`. The P4-04 validator uses Arduino
`String::indexOf('\0')`, which includes the terminating NUL and therefore rejects every non-empty
command. P4-05 capture received no bytes because it was never entered. P4-05 is paused without a
design/code change; P4-04 is explicitly reopened for the single validator correction, focused
command-boundary evidence and its own follow-up commit. After P4-04 closes, resume this unchanged
P4-05 proof from the remote E2E step.

**Focused E2E failure ownership and 60-minute pivot:** after P4-04 closed, `SSHOUTPUTTEST` passed,
but the remote diagnostic retained 157 verified bytes and reported that `ls -lR /pub` completed
before the next cancellation poll. The initial test-harness hypothesis added a bounded post-output
sleep; a materially different second run still returned success with 193 verified bytes. Both
runner finalizers removed their exact-owned files, and two explicit cleanup retries after each run
observed the file already absent. Code inventory then proved an active-row ordering defect:
`executeCommandStreamingControlled` invokes `onOutput`, then checks channel EOF before its next
loop-head cancellation observation. A callback-triggered cancel can therefore lose to same-iteration
EOF. The frozen minimal correction checks the existing callback immediately after stream callbacks
and before EOF success, with the original finite `ls` restored as the regression scenario. No SSH
state, API, timeout, output-storage or channel-management contract changes.
**Started:** 2026-08-30 22:30:00 +03:00.

### Locked clauses and non-goals

- A model-issued foreground `ssh_command` that exceeds its P4-04
  `max_inline_output_bytes` must stop returning partial inline output and instead retain the
  complete combined stdout/stderr stream in one downloadable microSD-backed command log.
- Output through the inline cap remains the existing bounded JSON result. The first byte beyond
  that cap promotes the already collected prefix to the log exactly once, clears the inline
  buffer, and writes every later chunk directly to the same log. The inline buffer never grows
  beyond the requested cap.
- User cancellation remains cooperative and foreground. If any command bytes exist when
  cancellation, timeout or connector failure is observed, those bytes are promoted/finalized as
  a downloadable partial log; cancellation remains `ToolExecutionOutcome::Cancelled`.
- The model receives only a bounded summary, byte count and opaque log filename/download
  reference. It never receives the full spilled bytes, an absolute SD path, credentials or SSH
  authority internals.
- Non-goals: background jobs/queues, durable job manifests, resume/reconnect, automatic retry,
  result handles, retention/rotation settings, a general streaming/storage framework, a larger
  inline buffer, Device/Web controls, manual-terminal changes, SFTP/transfer and P5 execution.

### Existing producer/consumer and persistence inventory

- `executeSshTool()` is the only model-command producer and currently calls the sole live
  `executeCommandControlled()` consumer. Manual Device/Web terminals use separate shell APIs and
  remain unchanged.
- `SshClient` reads libssh2 stdout then stderr in bounded 1,024-byte blocks on the existing
  foreground channel. P4-04 already supplies one total deadline/cancel callback and the combined
  inline cap; P4-05 changes only the output sink after that cap.
- `ToolExecutionResult` and the API continuation accept bounded UTF-8 JSON. Finished failures can
  carry a bounded log reference to the model. Canceled results short-circuit before tool output is
  appended, so the actionable error string must also name the retained log for Device/Web users.
- Existing tool activity records only tool, selected-SSH target, status, duration, bounded result
  bytes and exit status. It never stores command output; P4-05 does not change that journal.
- `createWorkspaceFile()` collision-checks a safe relative name under `/assistant/files`.
  An unlinked generated `.log` is visible/downloadable to the authenticated user through the
  existing `GET /api/file/download?name=...` stream, while project file tools cannot read it
  without an explicit project link. P4-05 creates no link.
- `requireSdWriteAccess()`, `checkSdOperationSpace()` and the retained 1-MiB operational floor
  own missing/full/removed/replaced-card rejection. No write starts before the current card
  identity and capacity permit it.
- ESP32 FS from the pinned M5Stack core `3.2.1` defines `FILE_WRITE` as `w` and
  `FILE_APPEND` as `a`; `File::write` reports accepted bytes, while `flush()` and
  `close()` return no status. Therefore each write count and final file size must be checked.
- A direct append-only `.log` is authoritative user output, not an atomic metadata document.
  Reset while streaming leaves a valid downloadable partial file. No boot scan, transaction
  marker, retry or running-command recovery owns it; ordinary authenticated file deletion owns
  later user cleanup.

### Minimal design and state transitions

- Add one SSH-specific SD output-capture connector with states `inline`, `logged` and
  `finalized`. It owns at most the requested inline bytes, one `File`, a 32-bit persisted byte
  count and one generated name `ssh-command-<16 lowercase hex>.log`.
- Name generation is bounded and collision checked; an existing file is never opened or
  overwritten. Creation reuses `createWorkspaceFile()`, then opens the exact new file with
  `FILE_APPEND`.
- `inline -> logged` occurs on first overflow or explicit promotion after terminal
  cancellation/error. It preflights SD space, writes the prefix once, clears it, then accepts each
  subsequent stdout/stderr chunk only after a per-chunk space check and exact write-count check.
- `finish` flushes, verifies exact file size and closes. Once a filename exists, later
  connector/SD failure retains the honest partial log and returns its reference; failure before
  creation clears partial inline output and leaves no artifact.
- Extend `SshClient` by one SSH-output callback method and implement the existing bounded method
  as a thin callback wrapper, keeping one libssh2 channel loop and unchanged manual callers.
- Successful spill returns bounded JSON with `exit_status`, `output_bytes`, summary and
  `output_log{name,download_path}`. Timeout/failure returns the same bounded reference; cancel
  additionally includes the filename in `error` because the API deliberately discards canceled
  tool output.

### Crash/SD/cleanup table

| Window | Durable state and required behavior |
|---|---|
| Before promotion | No command-log file; inline bytes remain bounded RAM only |
| After collision-checked create, before/during prefix write | Exact new log exists and may be empty/partial; report and retain only that file |
| During later chunk append | Exact log contains the verified persisted prefix; stop on cancel/deadline/short write/SD state change and retain it |
| After flush/size verification | Closed downloadable log with exact persisted byte count |
| Reset in any logged window | No executor resumes; the ordinary workspace file remains an honest partial/final log |
| Test success/failure | Delete only the returned collision-checked fixture, verify absence and repeat cleanup idempotently; preserve all prior files/inventory |

### Frozen proof and forbidden effects

- Cheap gate: strict host regression, callback/output-cap static checks, no new route/settings/assets,
  bounded JSON/reference and no absolute/private-storage path in schemas/results.
- Device gate: one exact-core build/upload; a no-network capture diagnostic proves inline boundary,
  first-byte spill, exact combined bytes, promotion/finalization, SD write/size checks and automatic
  exact-owned cleanup.
- Integration gate: one controlled read-only SSH command produces a spill, cancellation is observed
  after output begins, the authenticated existing Web download route returns the exact logged
  bytes, and a `finally` cleanup deletes only that returned filename and proves absence. Keep Web
  Console active through download/cleanup, then send `EXIT`.
- Record idle and active-SSH free heap, largest block, stack and reset/freeze state against their
  respective retained baselines. The 70-KiB floor applies to idle/general mode; active SSH must not
  regress from the existing active-SSH baseline.
- Forbidden: overwrite/collision, a second output pass, output bytes in audit/serial/model result,
  project linking, model file-tool access to the log, silent truncation, auto retry, background
  execution, log rotation/retention, Web/Device UI work or any P4-06+ implementation.

### Expected write set

- Production: `src/ssh_client.{h,cpp}`, `src/ssh_tool.cpp`, and one narrow
  `src/ssh_command_output.{h,cpp}` SD connector.
- Retained proportional diagnostics only if needed:
  `CardputerAssistant.ino`, `SshTools.ino`, `SerialDiagnostics.ino`, and
  `tests/host_tests.cpp`.
- Evidence only: this traceability row. No `file_workspace`, `sd_storage`, `tool_router`,
  `api_client`, Web route/assets, Settings, profile/key or permission-policy changes.

### Observed P4-05 evidence and closure

- The stabilized design and production/diagnostic diff each received independent read-only GO.
  After the real same-iteration output/EOF race was observed, one fresh reviewer returned GO for
  the single cancel-before-EOF correction and was closed without follow-up or reuse.
- Cheap checks passed: strict host tests, PowerShell parser, Node syntax, `git diff --check`, bounded
  callback/output/reference invariants and the generated Web asset consistency remained unchanged.
  No new route, Settings field, permission framework, background executor or UI asset exists.
- Final exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, resolved M5Stack ESP32 core
  `3.2.1`, 3,340,602 sketch bytes and 65,628 global bytes. The uploaded image is 3,340,784 bytes
  with SHA-256 `34EA26ED51B85DF7DBBCBDF5314448FDBD2AFF7CBC156A1E523813EB5562D589`; every flash segment
  reported hash verification and COM8 returned after reset.
- `SSHOUTPUTTEST` passed inline/spill/promotion/final-size/SD-failure/cleanup boundaries with free
  heap 122,500 bytes, largest block 60,404 bytes and stack margin 7,876 bytes. Against the retained
  P3 baseline this is -260 free-heap bytes, +2,048 largest-block bytes and +8 stack bytes.
- The final real foreground `SSHOUTPUTE2E` passed on the same device: the finite read-only command
  produced and retained exactly 157 bytes in collision-owned
  `ssh-command-42e5ea1879cfc4d4.log`, callback-triggered cancellation beat same-iteration EOF, and no
  background execution or retry occurred. The existing authenticated Web route downloaded exactly
  157 bytes with the required filename pattern while Web Console remained active; `EXIT` then
  returned `WEB_CONSOLE result=stopped`.
- Runner cleanup deleted only the returned command-log fixture. Two later cleanup calls both passed
  with `already_absent=yes`; the two failed hypotheses received the same failure-finalizer cleanup.
  Five exact-owned local log/sidecar files were removed from nine collision-checked candidate paths.
- Final `STATUS` remained responsive with microSD ready, two preserved chats/two history entries,
  reset reason 1, free heap 104,060 bytes, largest block 37,876 bytes, minimum heap 48,948 bytes and
  stack margin 5,972 bytes. Free heap is 12 bytes below the retained 104,072-byte active-Web/SSH
  comparison point, with bounded RAM, the 70-KiB general floor preserved and no reset or freeze.
- P4-05 completed at 2026-08-30 23:59:26 +03:00 after approximately 89 minutes. The mandatory
  material pivots were the independently closed P4-04 validator regression and then the proven
  same-iteration cancellation-order correction; no third patch/review loop was entered.

## P4-06 design gate

**Status:** completed
**Started:** 2026-08-31 00:15:29 +03:00.

### Locked clauses, adjacent ownership and non-goals

- Add exactly four model-facing tools: `sftp_list`, `sftp_read`, `sftp_write` and `sftp_move`.
  They use the existing single `ToolCapability::SftpReadWrite`, Phase 3 policy resolution,
  confirmation, pending preview, audit and cancellation boundaries.
- `Ask` requires confirmation for every model SFTP call. Under `Allow`, `sftp_write` and
  `sftp_move` still require mandatory confirmation when their explicit `overwrite` argument is
  true. No remote lookup, stat or mutation occurs before confirmation. The pending preview shows
  the exact destination and whether overwrite was requested.
- `overwrite` defaults to false. A write never opens the destination with `TRUNC` and a move never
  unlinks the destination. Replacement uses a collision-owned same-directory temporary file plus
  negotiated POSIX rename only after confirmation; a no-overwrite call uses the same completed-temp
  path and ordinary rename with zero flags.
- Listing is paged and bounded to at most 16 returned entries. Write content and each returned read
  chunk are bounded to 12,288 valid UTF-8 bytes, matching the existing model workspace chunk
  ceiling. Read offsets are raw byte offsets and `max_bytes` is 4..12,288 so a complete Unicode
  code point can always make progress. Remote paths are absolute, length-aware, NUL/CR/LF-free and
  at most 511 bytes.
- Every operation remains one foreground connection with a fixed 60-second total deadline from
  connection start through SFTP completion and cooperative cancellation. Existing trusted-host
  verification remains mandatory and mismatch or missing trust fails closed before authentication
  or SFTP mutation.
- Manual Device/Web SFTP retains direct-user authority and its existing methods. P4-07 owns
  workspace transfer, P4-11 owns standalone mismatch acceptance, P4-14 owns project/chat ceilings,
  and P4-16/P4-17 own consolidated UI journeys.
- Non-goals: a new capability or permission hierarchy, delete tool, shell classifier, remote
  action editor, background executor, retries, resumable transfer, persistent settings, remote
  transaction/recovery framework, UI assets, host-key-store changes or general SFTP abstraction.

### Existing producer, consumer, persistence and vendor inventory

- `tool_catalog` currently has seven schemas and an 8-bit mask. Four appended stable schema IDs
  require an 11-bit `uint16_t` mask; the plan itself is not persisted. The persisted policy codec
  already owns `SftpReadWrite` as `sf`, so no Settings or codec migration is required.
- `resolveChatToolPermissions()` currently leaves `SftpReadWrite` unavailable. The existing
  project route delegates all non-file tools to the generic audited route; enabling the existing
  capability and adding four dispatch cases is sufficient.
- Pending tool-call format v2 already stores canonical arguments and a generic SSH target. P4-06
  reuses that target: its authority hash is rebuilt from the selected public summary's stable
  opaque profile ID, public connection fields and trusted fingerprint without loading any profile
  secret; SFTP targets additionally store the canonical remote destination in `target.name`.
  Existing v2 `ssh_command` targets remain readable; a pre-change authority hash may become stale
  and fail closed, but is not treated as corrupt or migrated.
- Existing Device and Web confirmation renderers already display generic `targetName` and preview
  body. No Device/Web asset change is needed to show destination, source and overwrite intent.
- Existing manual SFTP list allocates and sorts the whole directory; model listing therefore needs
  a separate bounded page method. Existing manual upload opens the destination with `TRUNC` and is
  not reused for model writes. Existing manual download/transfer remains P4-07 ownership.
- The installed CardMind package identifies itself as version 1.11.1 in `library.properties`, while
  the compiled official-derived header is exact `LIBSSH2_VERSION` `1.11.1_DEV`. Its `CREAT|EXCL`
  open fails when a name exists. For SFTP v3, ordinary `rename_ex` does not transmit rename flags;
  therefore no-overwrite uses ordinary rename with zero flags, while confirmed overwrite uses only
  the negotiated `posix-rename@openssh.com` extension. If that extension is unsupported, overwrite
  fails explicitly with source and destination unchanged. There is no unlink/TRUNC fallback and no
  stronger portability claim than the server extension supplies.
- The completed read-only inventory agent was closed after one report and will not be reused for
  design, code, evidence or another roadmap row.

### Minimal contracts and state transitions

- `sftp_list`: exact `path`, `offset` and `max_entries`; `max_entries` is 1..16. It scans the
  foreground directory stream to the requested accepted-entry offset under the total deadline,
  retains at most one page plus one lookahead, and returns entries, `next_offset` and `eof`.
- `sftp_read`: exact `path`, raw byte `offset` and `max_bytes`; `max_bytes` is 4..12,288. A starting
  offset on a UTF-8 continuation byte fails explicitly. The reader returns the largest valid prefix
  not exceeding `max_bytes`; if the bound splits a multibyte code point, `next_offset` remains at
  that code point so the next call returns it once, without skip or duplication. NUL/binary/invalid
  UTF-8 fails with no partial model result.
- `sftp_write`: exact `path`, `content` and optional boolean `overwrite` normalized to false. It
  creates a bounded collision-owned same-directory temporary path with `CREAT|EXCL`, writes and
  closes the full content, then renames once to the destination. False uses ordinary rename with
  zero flags; true uses only `posix-rename@openssh.com` after mandatory confirmation and fails
  unchanged if the extension is unsupported.
- `sftp_move`: exact `source_path`, `destination_path` and optional boolean `overwrite` normalized
  to false. It performs one rename; false uses ordinary rename with zero flags, while true uses only
  `posix-rename@openssh.com` after mandatory confirmation and fails unchanged if unsupported.
- Pending build and claim bind the canonical call, project/chat revision, stable selected profile
  ID, host, port, username, auth mode, trusted fingerprint, private-key ID for private-key auth, and
  displayed destination. One narrow public-only authority lookup reads the selected non-secret ID
  and binding without password, passphrase or private-key bytes. Claim performs no remote operation.
  SFTP write/move preview body states overwrite yes/no; move also states the exact source.
- One outer start time and terminal latch governs connect, trust check, authentication, SFTP open,
  data I/O and rename. Every stage receives only the remaining part of the fixed 60-second budget;
  stages cannot accumulate fresh 60-second windows. Existing bounded channel/session close and an
  exact temporary cleanup attempt occur synchronously after the terminal outcome and outside that
  mutation deadline; no remote operation continues after return.
- Before the first rename call, cancellation/timeout leaves source and destination unchanged and
  attempts exact temporary cleanup. Once any rename attempt begins, cancellation, timeout, reset or
  connector loss before a final status has an explicitly unknown remote outcome and is never
  retried. A success status is committed remote state. A reset before rename may leave only the
  collision-owned temporary path; there is no boot scan/recovery owner. Cleanup failure reports that
  exact path without touching a destination or unrelated path.
- Pending state remains the existing atomic SD v2 owner. A rebooted pending call remains
  non-resumable under the existing boot boundary; no SFTP operation starts during recovery.

### Frozen minimal proof and forbidden effects

| Observation | Required result |
| --- | --- |
| Catalog/policy | Four appended schemas, 11-bit mask, existing `sf` codec unchanged, SSH group and Off/Deny/Unavailable fail closed |
| Confirmation | `Ask` persists all four calls; `Allow` executes list/read and no-overwrite write/move, but persists overwrite write/move; destination and overwrite are visible |
| Pending identity | Canonical exact fields round-trip; selected profile ID/public authority/private-key binding or destination change makes approval stale; no password/passphrase/private-key read or persisted value |
| Bounded read/list | Two list pages contain at most 16 entries with correct continuation; reads return at most 12,288 valid UTF-8 bytes, reject binary/NUL/mid-code-point offsets, and split one multibyte code point across the bound without skip/duplication |
| Safe write | Existing destination is unchanged before approval, on deny, on no-overwrite failure, temp-write failure and cancellation before the first rename call; a started rename without final status is unknown; confirmed replacement writes exact bytes and leaves no temp |
| Safe move | Existing destination blocks no-overwrite; confirmed overwrite uses negotiated POSIX rename or fails unchanged, with no fallback unlink; source/destination and pre-send/sent-unknown/success states are explicit |
| Trust/audit/cancel | Missing or changed host trust blocks before SFTP; every started call uses existing audit/cancel owner and no work continues after foreground return |
| Total deadline | A focused boundary proves connect/trust/auth/open/data/rename consume one shared 60-second budget while bounded teardown/temp cleanup is reported separately |
| Device/resources | One exact-core build/upload and smallest real-host exact-owned list/read/write/move scenario pass; active SSH heap/largest block/stack/latency do not regress or reset/freeze |
| Cleanup | Collision-checked remote fixtures and pending state are removed after success/failure, repeated cleanup is idempotent, and prior profile/selection/remote inventory is restored |

- Forbidden: destination `TRUNC` or pre-delete, remote preflight before confirmation, full-directory
  materialization, partial model output after a read failure, secret/key/path bytes in schema,
  result, pending JSON, preview, audit, serial or diagnostics, and any manual SFTP authority change.
- Forbidden: P4-07 transfer, P4-08 Safe Actions, P4-11 host-store UI, P4-14 ceilings, P4-16/P4-17
  UI integration, background work, retry/reconnect, temp-file boot scan or generic SFTP framework.

### Expected row-owned write set

- Production: `src/tool_catalog.{h,cpp}`, `src/tool_policy.cpp`, `src/api_client.cpp`,
  `src/pending_tool_call.cpp`, `src/tool_router.cpp`, `src/ssh_client.{h,cpp}` and one narrow
  `src/sftp_tool.{h,cpp}` execution owner.
- Proportional proof: `tests/host_tests.cpp` plus the existing serial diagnostic owner only if a
  direct real-host observation cannot be made through an existing command. No Web asset, Settings,
  policy codec, project/chat schema, known-host storage or manual SFTP UI file may change.

### Device-proof owner

- Read-only inventory after the first successful upload confirmed that existing `SFTPTEST` and
  `SSHDEMOTEST` exercise only the pre-P4 manual list/download methods; no existing serial command
  reaches the four model-SFTP executors. The optional existing serial owner is therefore activated
  for one proportional `MODELSFTPTEST` only in `SshTools.ino`, its forward declaration/header include
  in `CardputerAssistant.ino`, and command/result wiring in `SerialDiagnostics.ino`.
- The diagnostic uses the unchanged selected profile/trust, two random collision-checked exact-owned
  `/tmp` names and no SD/pending/profile mutation. It proves two one-entry list pages, UTF-8 raw-offset
  continuation, no-overwrite unchanged state, safe move/write overwrite, then removes only checked
  final paths it attempted and exact temporary paths reported by the connector, verifying every
  candidate absent twice. Any failure runs the same bounded cleanup.

### Diagnostic cleanup correction lock

- Architect health-check and the fresh read-only test review confirmed a P4-06 diagnostic defect,
  not a production-contract or adjacent-row defect: an initial write whose rename outcome is unknown
  can leave its final or reported collision-owned temporary path even though the model executor
  returns an ordinary failure, while the diagnostic previously marked a final path owned only after
  success and skipped cleanup when neither success flag was set. The serial result also derived
  `cleanup=yes` from overall operation success instead of observing cleanup independently.
- Before any mutation, the diagnostic must use the selected trusted profile to collision-check both
  random final paths. Immediately before each attempted write it marks that already-absent final path
  as an exact-owned cleanup candidate. A failed write contributes only a reported path matching the
  exact `/tmp/.cardmind-<16 lowercase hex>.tmp` production pattern; malformed or excess candidates
  fail the diagnostic rather than broadening cleanup. Success, failure and sent-unknown outcomes run
  one bounded cleanup over only these candidates and verify every candidate absent twice.
- After a successful overwrite move, the diagnostic must directly verify that the source path is
  absent before cleanup, so cleanup cannot hide a copy-like or partial move result. The serial result
  reports cleanup completion through a separate output value on both pass and failure paths.
- Frozen correction write set: only this evidence section plus the existing `MODELSFTPTEST`
  declaration, implementation and serial result wiring. No production SFTP contract/API, schema,
  permission, persistence, retry/recovery owner or general fixture framework may change.

### Design review outcome

- Fresh independent reviewer `01a05489-4289-7990-949a-5c03827deb62` returned five concrete STOP
  items: SFTP-v3 overwrite semantics, rename sent/unknown states, private-key binding in pending
  authority, one shared deadline and UTF-8 chunk boundaries. One combined follow-up confirmed that
  all substantive blockers were resolved and identified only two stale trace phrases; those exact
  phrases were corrected above. The reviewer is closed and will not review implementation,
  evidence, closure or another roadmap row.

### Code review correction gate

- Fresh read-only code review returned `STOP` for six concrete P4-06 defects: one duplicate local
  contract declaration; overwrite confirmation recomputed from the wrong canonical field; an
  indeterminate temporary-file open without explicit outcome/cleanup reporting; wrapper loss of
  exact cleanup detail; pre-send versus sent-unknown rename state collapse; and worst-case JSON
  expansion beyond existing pending/result bounds.
- Ownership is active-row P4-06. No completed or adjacent row is reopened. The correction is locked
  to those six findings, uses the existing pending/result limits and SFTP connector, and adds no
  schema, persistence owner, retry, recovery framework, UI or manual-SFTP change.
- Canonical `overwrite`, not remote target state, drives claim-time confirmation. A local
  first-call flag distinguishes cancellation/deadline before rename from a sent request whose
  final status is unavailable. An in-flight temporary create is resolved only through the same
  bounded libssh2 call; success is closed/unlinked, while unresolved status reports the exact
  collision-owned path and unknown outcome without retry.
- Model SFTP content is explicitly text: NUL and C0/DEL controls other than tab, CR and LF are
  rejected while valid UTF-8 up to 12,288 bytes remains accepted. Thus JSON escaping is at most
  twofold and both canonical pending arguments and read results fit the existing 32-KiB envelope;
  no larger buffer, storage side channel or new codec is introduced.
- After one coherent primary-agent correction and cheap checks, the same reviewer may receive its
  single combined follow-up for only these findings, then it is closed.
- The corrected diff passed `git diff --check` and the strict WSL host suite. The same reviewer used
  its single combined follow-up, returned `GO` for all six corrections and was then closed; it will
  not review evidence, closure or another row.
- The first exact firmware compile then failed before link on one P4-06-owned type mismatch while
  prefixing an SFTP move preview (`Arduino String` plus `std::string`). The locked correction is one
  `std::string` expression; no design, interface or adjacent-row behavior changes. This is recorded
  as one late compile escape and justifies one repeated exact build.

### Observed P4-06 evidence and closure

- Strict host regression, `git diff --check` and the focused catalog/policy/pending/parser checks
  passed. They proved four appended schemas with the existing `SftpReadWrite` capability, fail-closed
  Off/Deny/Unavailable behavior, `Ask` confirmation for every call, `Allow` confirmation for explicit
  overwrite, canonical destination/overwrite preview and stale approval after authority, key binding
  or destination change. No Settings codec, Web asset, new capability or manual-SFTP authority changed.
- The independent production code reviewer returned `GO` after its six concrete blockers were
  corrected and was closed. Architect health-check plus the fresh diagnostic reviewer independently
  confirmed the later fixture-cleanup and source-absence defects; the primary correction changed only
  the existing diagnostic declaration/implementation/result wiring and the trace lock above.
- Final exact pinned build passed with FQBN
  `m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=custom`, the unique resolved M5Stack
  ESP32 core `3.2.1`, 3,396,822 sketch bytes and 65,628 global bytes. The uploaded image is 3,397,008
  bytes with SHA-256 `BDEC90F28414C02EC1A4965A7123B1D38CA52AD57B53F18A8BD9420B41F158AE`;
  every flashed segment reported hash verification and COM8 returned after reset.
- Real-device `MODELSFTPTEST` passed in 57,728 ms through the four model executors. It observed two
  bounded one-entry list pages with advancing continuation, UTF-8 byte-offset split and continuation
  rejection, exact reads, no-overwrite preservation, confirmed overwrite move/write, source absence
  after the successful move and final content. The same selected trusted profile and existing manual
  SFTP owner were used; no profile, selection, pending state, SD file or user remote object changed.
- Both random `/tmp/cardmind-p4-06-<nonce>-*` paths were collision-checked before mutation. Every
  attempted final path and any exact connector-reported `/tmp/.cardmind-<16hex>.tmp` candidate used
  the bounded success/failure cleanup path, was verified absent twice, and serial independently
  returned `cleanup=yes`. No retry, boot scan or general fixture/recovery framework was added.
- Post-scenario `STATUS` remained responsive with microSD ready, two preserved chats and two history
  entries, reset reason 1, free heap 117,540 bytes, largest block 53,236 bytes, minimum heap 48,552
  bytes and stack margin 7,860 bytes. The diagnostic result itself reported free heap 117,076 bytes
  and the same minimum/largest/stack bounds. This remains above the retained active-SSH comparison
  points for free heap and largest block, with only a 396-byte lower global minimum than P4-05, and
  no reset, freeze or persistent session.
- P4-06 completed at 2026-08-31 01:37:01 +03:00. P4-07 and later behavior remain pending.
