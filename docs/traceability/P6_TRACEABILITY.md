# CardMind Phase 6 traceability

This is the only active Phase 6 matrix. `ROADMAP.md` defines product scope; this file records the
minimum atomic plan and only observed evidence. Phase 6 closes the remaining user-facing Web and
Device gaps through a stable, measured Wi-Fi baseline without introducing USB production work.

## Phase lock

- Phase source is authenticated remote `develop` at
  `fd9373aa93bd04e217b2e58c6a6ee75b80e3eaf8`, the reviewed merge of Phase 5 through PR #3.
  Remote `main` remains `681cc8ffa9b6d26897d4847001d5d57f17b5d340`.
- Work branch is `feature/phase-6-ui-stable-baseline`, created locally from that exact `develop`
  commit after the user-requested system reboot.
- The retained Phase 3 stash and the three approved Architect-owned untracked `.codex/agents`
  definitions are outside Phase 6 ownership and must remain untouched.
- Phase 3 owns policy, confirmation, audit, cancellation, projects, chats, files and the original
  Web/Device interaction boundaries. Phase 4 owns SSH profiles, the single terminal and controlled
  remote actions. Phase 5 owns the approved one-shot Python cycle and same-address handoff. Phase 6
  may extend only the explicit UI, profile, session, presence, reconnect and stable-baseline seams.
- The general-mode free-heap floor remains 70 KiB. Phase 4's missing numeric active-SSH resource
  sample and Phase 5's explicitly retained one-shot residuals are inherited facts, not Phase 6
  regressions or authority to broaden scope.

## Locked product contract

- Reconcile every non-deferred user-facing capability completed through Phase 5 across its required
  Device and Web surfaces. Preserve the established concepts, names, permissions and state.
- Complete responsive Web parity for desktop, tablet and phone without forcing identical layouts.
  Complete Cardputer navigation within the measured 240x135 display and existing input model.
- Add a bounded API-profile and model-preset set sized by measured NVS capacity; do not create a
  paginated profile framework.
- Offer authentication lifetimes of 15 minutes, 1 hour, 8 hours and until reboot. Authentication
  lifetime is independent from browser presence.
- Each visible Web tab sends a bounded heartbeat. Presence becomes waiting within 15-30 seconds
  after the last heartbeat, while the authentication session remains valid. `pagehide` and
  `sendBeacon` are only best-effort hints, and one closing tab cannot hide another live tab.
- Reuse the existing reconnect path while preserving the active project, active view and drafts.
  Exact scroll restoration is outside scope unless a measured defect changes `ROADMAP.md` first.
- Reuse existing diagnostics, export, clipboard, file/download and single-QR paths. Add only explicit
  missing-SD, provider-outage and unavailable-optional-API states required by the roadmap.
- Close with one exact, recoverable Wi-Fi baseline containing source, binary hash, build options,
  flash/RAM/heap/largest-block/stack, user-visible latency and exact-owned cleanup evidence.

## Explicit non-goals

- No USB/NCM production code, abstraction, experiment or preparation; no local HTTPS or installable
  PWA metadata before a trusted-origin delivery path exists.
- No new design-system dependency, front-end framework, route family, storage framework, second Web
  server, second dashboard, share center, temporary-link service or multi-frame QR.
- No encrypted secret export before the Phase 9 physical-access and recovery decision.
- No background job, queue, resume, generalized retry, tool-handle or recovery framework.
- No reopening of completed P3/P4/P5 ownership without Architect authorization and a canonical
  matrix update.

Status values are `pending`, `in_progress`, and `completed`.

## Execution matrix

| ID | Atomic boundary | Required observation | Status |
| --- | --- | --- | --- |
| P6-01 | Canonical phase transition and pre-edit freeze: confirm remote `develop`; inventory every required P3-P5 user-facing action; obtain Architect's consolidated replacement brief and independent visual red-team; create a materially different replacement artifact; freeze scope, non-goals, proof matrix, resource budget and first production-row write set for Architect GO | Exact baseline and branch evidence; complete action-to-surface inventory including project creation, Shared-workspace file creation/upload, chat creation/management and Settings; reviewed replacement brief; independent red-team verdict; Architect acceptance of the interactive artifact against complete functional coverage, current UX/design-system evidence, Web Console/ESP32 Cardputer feasibility and visual quality; reviewed first-row design/proof/write set before production | completed |
| P6-02 | Add the bounded API-profile and model-preset contract through existing settings, persistence, Device and Web owners | Measured NVS capacity; explicit count/length limits; selection/default behavior; malformed/full-storage failure; reboot persistence; Device/Web parity; exact cleanup and resources | pending |
| P6-03 | Add configurable authentication lifetime and independent multi-tab browser presence | Four exact lifetime choices; expiry and until-reboot semantics; visible-tab heartbeat; last-tab waiting within 30 seconds without auth loss; multi-tab correctness; reboot and stale-token behavior | pending |
| P6-04 | Preserve active project, active view and drafts through the existing Web reconnect path | Same-address reconnect after transient disconnect and Python handoff; active project/view/draft restored; stale or missing state fails explicitly; no exact-scroll claim | pending |
| P6-05 | Bring the Web Console to the Architect-reviewed replacement direction, add explicit discovered-network Wi-Fi selection through the smallest reviewed existing-owner backend mapping, and finish desktop/tablet/phone polish through the existing asset boundary | Every required Web capability reachable; explicit Wi-Fi scan/select/hidden-manual/connect states and other degraded states; stable-ID interaction checks; 1280, 900 and 390 px screenshots without overlap; soak and resource evidence | pending |
| P6-06 | Bring the 240x135 Device UI to the Architect-reviewed replacement direction through existing screen/input owners | Every required Device capability reachable; coherent navigation and compact states; provider/SD/optional-API degradation; no Web-only requirement leakage; device resources and latency | pending |
| P6-07 | Reconcile cross-surface behavior and run focused integration acceptance | Names/state/permissions consistent across Device and Web; profiles, sessions, presence, reconnect, SSH and Python boundaries interoperate; forbidden effects absent; exact-owned cleanup | pending |
| P6-08 | Establish and publish the stable Wi-Fi baseline and close Phase 6 | Full host/Device/Web regression; exact build options and binary hash; flash/RAM/heap/largest-block/stack/latency; screenshots; soak; cleanup; independent reviews; green CI; reviewed merge only to `develop` | pending |

## P6-01 locked boundary

P6-01 is a pre-production phase-transition row. The prior task-owned Concept C visual-polish
artifact is rejected evidence only and is frozen without further iteration. The replacement must
start from Architect's complete P3-P5 user-action inventory and a materially different visual and
compositional brief; it must not salvage the rejected palette or layout through incremental tweaks.
P6-01 does not own firmware, Web production assets, generated headers, schemas, persisted values,
routes, tests, builds, uploads or Device state.

Production work and the P6-02 design freeze remain stopped until Architect supplies the consolidated
replacement brief and independent visual red-team result. The replacement must cover every required
P3-P5 user-facing action, explicitly including workspace/project creation, chat creation and
management, and the complete applicable Settings surface. No replacement detail may be invented
before that brief.

The later visual replacement additionally fails closed on generic AI-dashboard styling: purple/blue
gradients, glassmorphism, blur, glow, repeated interchangeable rounded cards or pills, sparkle/AI
ornaments, oversized empty hero regions, decorative pseudo-metrics, ambient or looping motion and
other template-like AI-console treatment are prohibited. The accepted language must be specific to
CardMind and Cardputer: a professional working tool with a clear list-detail hierarchy, useful
density, semantic status, recognizable hardware continuity, and expression through composition,
typography, solid color and shape. This criterion does not unfreeze visual work before Architect's
separate functional `GO` and consolidated design brief.

Before P6-01 can close:

1. Receive Architect's consolidated inventory of every required P3-P5 user-facing action, replacement
   brief and independent visual red-team result.
2. Freeze the replacement artifact's functional coverage, visual direction and explicit non-goals.
3. Produce one new bounded interactive artifact without iterating the rejected Concept C polish.
4. Verify all inventoried actions at representative desktop, tablet, phone and 240x135 Device sizes.
5. Obtain the required independent review verdicts and Architect's independent, evidence-backed
   acceptance of complete functional coverage, current UX/design-system applicability, constrained
   Web Console/ESP32 Cardputer feasibility and sufficient visual quality. No user choice is required.
6. After Architect acceptance, close and publish P6-01. Only after exact remote-SHA verification,
   activate P6-02, freeze its smallest production design, proof matrix, measured resource budget,
   non-goals and exact write set, then obtain Architect's separate explicit pre-edit GO.

The P6-01 repository write set is exactly `AGENTS.md`, `ROADMAP.md` and this traceability file. The
interactive artifact is task-owned outside the repository. Any production edit before the final
Architect GO is out of scope.

## Observed P6-01 kickoff evidence

**Started:** 2026-09-02T22:34:32+03:00.

- Authenticated GitHub MCP resolved remote `develop` to
  `fd9373aa93bd04e217b2e58c6a6ee75b80e3eaf8`; the immutable commit is the Phase 5 PR #3 merge.
- The local remote-tracking ref was fetched to the same SHA, and the Phase 6 branch was created at
  that exact commit. The starting tree is `a737175feadb384e860bdefa503d880e5bc54f1c`.
- The starting tracked worktree was clean. Only the three Architect-owned `.codex/agents` files were
  untracked, and the retained stash remained present and untouched.

## Rejected P6-01 visual and feasibility checkpoint

**Observed:** 2026-09-02T23:09:28+03:00.

- The task-owned artifact is
  `C:\Users\84vs1\.codex\visualizations\2026\09\02\01a06386-22d5-7833-bb82-40a4f499f52e\concept-c-visual-polish.html`.
  A no-index comparison against the canonical Concept C prototype reports exactly 322 inserted CSS
  lines and zero deletions. Both artifacts retain one script with SHA-256
  `8863FF9A606396B00D0DE5F61C3494A3CD8C3AEED7304F55765DAE3CB32A383A` and the same 54 IDs.
- Browser checks produced exact 1280x720, 900x720 and 390x780 Web captures with no horizontal
  overflow. Visible control minima were 38 px on desktop/tablet and 44 px on phone; the phone bottom
  navigation remained flush with the product edge. The Device screen remained exactly 240x135.
- Mock-local interaction checks passed for all four Web destinations, active-project visibility,
  retained draft, disconnect-before-profile-switch, mismatch blocking, keep-blocked, exact-host
  forget without automatic reconnect, explicit reconnect, SFTP open/close and the single live
  terminal. All nine Device cards appeared in order; NETWORK opened the existing Wi-Fi picker,
  degraded WEB CONSOLE remained selectable, and TOOLS reached the complete SSH menu, manual-terminal
  detail and live terminal. The browser console reported no warning or error.
- The independent visual-quality review initially stopped on two 10 px helper labels, semantic-color
  reuse in ordinary ornament and inexact scrollbar-bearing captures. The same bounded pass restored
  the labels to 12 px, made the ornament neutral and produced exact-size captures; the one permitted
  blocker verification returned `GO`.
- A separate fresh functional-feasibility review mapped the artifact to the existing single Web
  asset/embed/root/route owner, P4 SSH profile/terminal/SFTP owners, P5 same-address handoff, current
  settings/NVS path, session token, URL-hash/draft state and the existing nine-card Device producer,
  renderer and input dispatcher. It returned `GO`: no second framework, server, route family,
  storage subsystem, terminal or dashboard is required.
- Architect independently checked the checkpoint and the exact desktop/tablet/phone renders and
  found no hidden technical or scope debt: the artifact remained presentation-only, behavior and
  IDs were preserved, both bounded reviews were `GO`, and no production boundary was touched.
  Architect explicitly kept P6-01 active pending the user's visual evaluation.
- Source reconciliation confirms the remaining Phase 6 gaps rather than hiding them: production has
  one API/model configuration, a fixed 15-minute session, no browser heartbeat/presence contract,
  only partial hash/draft reconnect state and incomplete explicit provider/optional-API degradation.
  These remain owned by P6-02 through P6-07. Production behavior was not changed by this checkpoint.
- These review verdicts and Architect's visual acceptance were invalidated by the later STOP because
  they preserved an incomplete prototype rather than checking the complete required user-action
  inventory. They are retained only as historical evidence and cannot support P6-01 closure.

## P6-01 Architect STOP

**Observed:** 2026-09-02T23:17:00+03:00.

- The user rejected both the palette and layout and identified concrete functional omissions:
  workspace/project creation, chat creation and management, and substantial Settings coverage.
- Architect issued `STOP`, invalidated both prior review `GO` verdicts and the earlier Architect
  visual acceptance, prohibited P6-01 closure, P6-02 freeze, production edits and further iteration
  of the existing polish artifact, and required a complete P3-P5 action inventory plus an independent
  visual red-team before issuing a consolidated replacement brief.
- Architect's source inventory found 188 unique element IDs in the shipped Web Console and 54 in the
  rejected mock, with zero exact overlap. The mock exposed only three unnamed chat selectors plus
  Send, represented Files as a static list and reduced Settings to session/browser/theme/device-status
  text; it provided no workspace/project creation, chat creation or project/chat CRUD and no
  substantial Settings controls. This disproves the checkpoint's stable-semantic-ID and
  functional-feasibility claims as production-parity evidence.
- The rejected artifact and its captures remain untouched as rejected evidence only. No production
  file, production test, build, upload or Device state was changed by the rejected checkpoint.
- P6-01 remains the sole `in_progress` row. Its active-work clock is paused while awaiting Architect's
  consolidated brief and red-team result; all production rows remain `pending`.

## P6-01 replacement coverage contract

**Received:** 2026-09-02 after the Architect STOP.

The rejected Concept C is not a base for iteration. Its mock-only information architecture, palette,
layout, 54-ID namespace and simulated behavior cannot be preserved as acceptance evidence. Production,
tests, builds and Device state remain frozen throughout this coverage gate.

Vocabulary is locked as follows:

- A **Project** is the user-created context with CRUD, settings, chats, shared-file links and bundle
  import/export.
- The **Shared workspace** is the single existing SD-backed `/assistant/files` area. It is not a
  named or separately created object; users create or upload files and link them to projects.
- The **Python workspace** is the existing startable manual runtime. It is not multi-workspace CRUD.
- Global creation is **New project**; Files exposes **New text file** and Upload; Python exposes Start
  when unavailable. A generic `New workspace` product object is forbidden.

Before visual styling, P6-01 must map each required function to Web (`W`), Device (`D`) or both
(`W+D`), an exact reachable screen/control, its states and its existing production semantic ID or an
explicit one-to-one mapping. The minimum functional inventory is:

1. **Global Web shell/session (`W`)** — login; authenticated state; lock/logout; explicit Console
   close/end; active-project selector and New project; persistent Chat, Workspace/Files, Terminal and
   Settings destinations; applicable Device, Wi-Fi, SD, provider/model, Python and pending/activity
   status refresh; the four exact P6 session lifetimes; multi-tab heartbeat/last-tab behavior;
   same-address reconnect preserving active project, active chat/view and drafts including Python
   handoff return; responsive desktop/tablet/phone navigation; no PWA.
2. **Projects (`W+D`)** — bounded list, create, select/open, rename, duplicate, archive/restore,
   delete, bundle export/import; instructions, model override, context budget, output-token override,
   auto-compact, optional SSH-profile ceiling and per-capability policy ceilings; shared-file inspect,
   link and unlink; explicit empty, loading, error and archive states.
3. **Chats and conversation (`W+D`)** — current and archived bounded lists, create/select and load
   more where supported; message history and streaming response; send, Stop and retry; model/context/
   capability and source/details visibility; compact/summary and full/archived history; chat
   instructions, model override, optional SSH ceiling and capability ceilings; supported rename,
   pin/unpin, archive/restore, duplicate, clear and delete; Markdown and project-bundle export/import;
   Auto, No tools, Web, Files, SSH and Python composer choices, next-output override and one-turn
   instructions; preserve draft and selected chat across destination changes and reconnect.
4. **Shared workspace/files/QR (`W+D` according to existing owners)** — real bounded SD directory
   listing/navigation with more/previous where supported; text open/read, bounded edit/window
   navigation, save, save copy/new text file, rename and delete; Web upload/download and project/chat
   bundle import; active-project link/unlink state; shipped Device find/bookmark/read-aloud/copy;
   text-or-file QR show/close; distinct missing, full, read-only, corrupt, removed-during-operation and
   replaced SD states plus replacement confirmation; no named-workspace CRUD.
5. **SSH profiles/security/terminal/SFTP (`W+D` according to shipped controls)** — at most five
   profiles with capacity, create, select/default, edit and delete; name/host/port/user/auth mode,
   write-only password/passphrase and private-key install without secret display/export; exactly one
   terminal with connect, unknown-host trust, mandatory changed-key block, exact-host forget, input,
   resize, streamed and bounded output, clear, disconnect/stop and supported fullscreen; existing SD/
   log/download behavior; existing SFTP navigation/file operations and controlled workspace-to-remote
   or remote-to-workspace transfers with explicit endpoints, overwrite denied by default,
   confirmation, cancel and normal error outcomes. Profile mutation remains locked while terminal,
   trust or mismatch state owns the selected profile; key-capacity failure preserves the prior
   binding. Automatic reconnect and reconnect schedulers, terminal tabs and a new SSH framework are
   forbidden, while explicit Connect after successful exact-host forget remains valid. The model
   surface exposes exactly `sftp_list`, `sftp_read`, `sftp_write` and `sftp_move`, plus the five fixed
   Safe Actions logs, service state, containers, disk and processes, through Chat, Pending and
   Activity rather than a separate application; arbitrary `ssh_command` remains mutating.
6. **Permissions/pending/audit (`W+D`)** — global master policy and defaults for new chats across Web
   search, Web fetch, Files read, Files write/delete, SSH read, SSH mutate, SFTP read/write and Python
   write/run; Project and Chat Inherit/Off/Ask/Allow ceilings plus selected SSH-profile availability/
   ceiling; composer intent cannot elevate broader policy and may select any W/F/S/P subset. Pending
   preview remains typed rather than exposing generic arguments: bounded file diff, exact SSH command,
   Python full-source route, or tool-and-reason-only generic projection, with stale authority explicit;
   rebooted pending exposes Acknowledge only. Allow once, Allow for chat only for an ordinary policy
   Ask, Deny and Acknowledge remain bounded by mandatory confirmation; activity/audit exposes tool,
   coarse target, status, duration, output bytes and optional SSH exit status plus cooperative cancel;
   raw/effective/source text accompanies compact W/F/S/P state. No duplicate policy engine,
   chain-of-thought view or configurable audit retention is allowed.
7. **Settings/providers/device controls (`W+D` according to existing owners)** — OpenAI-compatible
   base URL, secret key, default model and refresh; bounded P6 API profiles/model presets sized from
   measured NVS; global instructions and per-chat history quota; Wi-Fi SSID/password/connect state;
   STT, Web search and TTS endpoint/model/voice/key removal plus autoplay and volume; brightness,
   screen sleep, keyboard repeat and power profile; shipped Web surface style, compact density,
   reduced motion, terminal wake and diagnostic-metrics preferences. Controls require validation and
   save/apply/error states; telemetry is labelled status, never presented as a setting.
8. **Python (`W+D`)** — existing manual runtime readiness and Start; existing one-shot model path
   from Chat through mandatory path/size/SHA-256 and full-source approval, with an explicit privileged
   no-sandbox warning, then handoff to stdout/stderr/exit on the originating chat and same-address
   return/reconnect. The prototype must distinguish success, exception, normal reset, watchdog/power
   loss, effects-unknown, exactly-once attachment/no replay and canceled-before-runnable-staging. No
   second workspace, package manager, jobs, queue, scheduler or Python framework.
9. **Diagnostics/degraded states (`W+D`)** — live metrics and diagnostic export through existing
   owners; explicit provider/model, optional STT/TTS/search, Python image/runtime, Wi-Fi/session, SD
   and SSH-profile absence/unavailability; loading, empty, validation, connector failure,
   cancellation and timeout where existing contracts require them; distinct Browser connected,
   Waiting and authentication-expired states; SSH disconnected/failed/mismatch; and Python ready,
   unavailable and effects-unknown. No contradictory hard-coded ready/connected labels.
10. **Device 240x135 navigation (`D`)** — preserve all nine established destinations: Projects
    (projects, chats, settings/actions), AI (model/API, instructions, master/default policies,
    pending, activity), Voice (capture/transcription/playback and settings), Network (2.4 GHz scan,
    select, password, connect/save), Files (shared-area browser/editor/import/link), Web Console
    (start/address/session/Python/API and Wi-Fi entry), Device (display/power/quota/services/update/
    diagnostics), Tools (notes/checklist/timer/calculator/QR/SSH) and Help (controls/about/support).
    Tools remains the direct-user utility and SSH home rather than the LLM-permission home. Device
    keeps arrow navigation without requiring `Fn`, accessible state text, its own nested-screen model
    and explicit degraded states rather than cloning Web.

Canonical reconciliation narrows three ambiguous brief phrases without reducing required behavior:

- Device chat-bundle import remains removed because the retired control wrote legacy
  `/assistant/chats`; project-bundle import/export is the portable cross-surface path, while any
  retained chat import remains only on its existing owner.
- `No reconnect` means no automatic SSH reconnect or scheduler, not a ban on the explicit user
  Connect action after successful exact-host forget.
- `Full pending preview` means the complete safe typed preview applicable to that action, never a
  generic dump of arguments or secrets.

The ordered replacement gates are:

1. Deliver the complete coverage matrix and one neutral/grayscale low-fidelity IA prototype before
   any visual-polish layer.
2. Use actual production semantic IDs for existing Web controls or retain an explicit one-to-one
   mapping; add IDs only for exact P6 additions, never using the rejected `cm-*` namespace as proof.
3. Make prototype reachability and state changes interactive, label simulated data as prototype data,
   avoid contradictory readiness, and never use inert controls as evidence.
4. Independently red-team the matrix against ROADMAP, P3/P4/P5 traces, production routes/assets and
   Device menus; require Architect's personal coverage review before styling.
5. Only after coverage `GO`, create a materially new visual composition. The rejected dark teal,
   cyan and salmon palette, glow/gradient/grid, empty left rail, nested-card layout and mobile stacking
   are prohibited rather than polish targets.
6. Independently red-team the later visual hierarchy, responsiveness and accessibility at 1280x720,
   900x720, 390x780, 200% text zoom and Device 240x135.
7. Report omissions or conflicts exactly; never silently defer, invent or expand functions.

## P6-01 visual-direction STOP after coverage review

**Observed:** 2026-09-02T23:56:23+03:00.

- Architect personally reconciled the complete coverage matrix against the canonical sources and
  issued a coverage-matrix-only `GO`; this did not approve an artifact, palette, styling,
  production work, P6-02 or P6-01 closure.
- The task-owned `p6-01-neutral-coverage-ia.html` is frozen only as a functional coverage and
  wireframe reference. Its monochrome presentation was rejected as visually exhausting and must
  not be presented as the design, polished further or used as a base that is merely recolored.
- Architect issued `STOP` on further artifact work and required a bounded independent research pass
  over authoritative current sources for professional AI/chat, files, terminal and settings
  consoles, responsive navigation, information density, color hierarchy and compact 240x135 Device
  UI. The next deliverable is a source-backed set of patterns, anti-patterns and two or three
  materially distinct directions mapped to the verified CardMind IA.
- Production, tests, builds, uploads and Device state remain frozen. P6-01 remains the sole
  `in_progress` row.
- The user then clarified that the matrix and neutral IA are the immutable functional baseline:
  later visual work may change composition and presentation but may not drop or assume equivalence
  for any function. Before proposing visual directions, the task must independently validate every
  matrix row against the actual prototype for a reachable control or screen, required state variants,
  exact production ID or explicit P6-ID mapping, correct Web/Device ownership and absence of inert
  proof controls. Any omission is `STOP`; retain localhost or screenshot evidence usable by Architect.

## P6-01 functional-baseline audit and SD ownership correction

**Observed:** 2026-09-03T01:37:56+03:00.

- The earlier coverage-matrix-only `GO` and the subsequent candidate SHA-256
  `0A03AF411D4BC26915467DAC769303B585DDCB14C045A4F52A6A333F8054149B` are revoked as functional
  acceptance evidence. Architect found that their SD scenario text covered Projects, Chats and
  Shared files while the runtime guards covered only Shared-file handlers; Project/Chat reads and
  mutations therefore remained reachable in contradictory degraded states.
- The correction remains entirely inside the task-owned neutral artifact. One `sdMode` access owner
  now governs Project/Chat reads, writes, selection visibility and an active response stream in
  addition to the existing Shared-file guards. Missing, corrupt, removed and unconfirmed-replacement
  modes disable Project/Chat selectors, replace the chat content with an explicit unavailable state,
  and reject representative reads and writes. Full and read-only modes preserve selection, loading
  and export reads while rejecting Project/Chat creation, settings/metadata mutations, message send,
  compact, retry, destructive actions and project-bundle import before any dialog or state change.
- Browser observations verified the forbidden effects directly. In each read-unavailable mode,
  `Load more projects` and `New project` reported the exact SD reason, retained the three-item project
  count and opened no dialog. In both full and read-only modes, project switching and pagination
  remained available; an unsaved draft was not persisted across selection changes; New project,
  New chat, Send, Compact, Retry, Rename project and Clear chat retained their prior counts, messages,
  title and context meter; project/chat exports and text-file reads remained available; project-bundle
  import failed explicitly. Removing the SD during an active response replaced `Streaming` with a
  typed failure and never produced `Response saved`. Replacement confirmation and a return to Ready
  re-enabled selectors, project/chat creation and message streaming.
- The corrected functional candidate is
  `C:\Users\84vs1\.codex\visualizations\2026\09\02\01a06386-22d5-7833-bb82-40a4f499f52e\p6-01-neutral-coverage-ia.html`,
  SHA-256 `F2AD0CB39BFAB1424262997D2481C43F69838175029553FE999BECDACBE94EBD`, 164,423 bytes and
  1,983 lines. It retains 76 matrix rows across ten domains, 231 unique IDs, one syntax-valid script,
  no unresolved literal queried ID, no unhandled ID-bearing button and no generic Device fallback.
  The current review server is `http://127.0.0.1:57263/`.
- This evidence corrects the residual SD contradiction only; it does not approve visual styling,
  production work, P6-02 or P6-01 closure. The corrected hash still requires the bounded independent
  blocker verification and Architect's new personal browser `GO`.

## P6-01 narrowed blocker correction and acceptance-owner update

**Observed:** 2026-09-03T02:03:49+03:00.

- The user assigned visual-applicability acceptance to Architect without requiring another user
  choice. Architect may issue the later visual `GO` only when the same candidate proves complete
  functional coverage, applicability of current 2026 UX/design-system patterns, feasibility for the
  constrained Web Console and ESP32 Cardputer 240x135 surfaces, and sufficient visual quality.
- The independent narrowed reviewer issued `STOP` on candidate
  `F2AD0CB39BFAB1424262997D2481C43F69838175029553FE999BECDACBE94EBD`. Its 76-row structure and 71
  rows remained clear, but five reachable contradictions survived: the Project/Chat detail drawer
  leaked during unreadable SD states; global or project-bound provider unavailability did not block
  every model operation; Python unavailable did not own manual Start; Python return discarded a live
  unsent draft; and chat-list mutations leaked between projects through one global DOM inventory.
- The task-owned correction closes only those five owners. Unreadable SD states close and hide the
  detail drawer. One model-operation guard covers Send, Retry and Compact for both global-provider and
  saved-project-profile unavailability, and provider loss terminates an active response with an
  explicit failed outcome. Python has one availability state shared by its summary, detailed pane,
  manual Start and Python-intent Send; Start saves the originating context before handoff. Each
  project now owns its chat inventory and selected chat; create, pagination, rename, flags,
  duplication, deletion and project lifecycle synchronize only that project's inventory.
- Browser verification observed all five forbidden-effect boundaries. The four unreadable SD modes
  hide both chat content and the open detail drawer. Unavailable global and project API states retain
  message count, draft, one-turn controls and context meter while Send, Retry and Compact fail; a
  provider transition replaces `Streaming` with an explicit failure and never later saves it. Python
  unavailable retains the draft and history while both Start and Python-intent Send fail, whereas a
  ready Start restores the exact pre-handoff draft on return. Creating and loading chats in Field
  notes left Lab setup unchanged; creating then deleting a Lab setup chat left Field notes unchanged.
  New, duplicated and deleted projects initialized, cloned and removed only their owned chats.
- The corrected candidate is the same task-owned artifact, SHA-256
  `52DB1280015AFA0CCA152CA2695780976C02CE2A4C6386E0BB1F448C052F0310`, 171,507 bytes and 2,100
  lines. It retains 76 seven-cell matrix rows across ten domains, 231 unique markup IDs, 166 unique
  literal queried IDs with none missing, 85 handled ID-bearing buttons, one syntax-valid script and
  no generic Device fallback. Browser reflow remained contained with no horizontal offender at
  1280x720, 900x720 or 390x780. The live review server is `http://127.0.0.1:65428/`.
- Visual styling, production code/tests/build/device work, P6-02 and P6-01 closure remain frozen. This
  exact hash now awaits the reviewer's one blocker-verification verdict and Architect's personal
  browser `GO` or concrete `STOP`.
- The same independent reviewer used its one blocker-verification pass and returned `GO` on exact
  SHA-256 `52DB1280015AFA0CCA152CA2695780976C02CE2A4C6386E0BB1F448C052F0310`: all five blockers are
  resolved, the structural counts match, and no correction-caused regression was found in the 71
  rows cleared by the prior pass. Architect's personal functional browser verdict remains required.

## P6-01 functional GO and consolidated visual brief

**Observed:** 2026-09-03T02:16:35+03:00.

- Architect personally exercised the five corrected boundaries and representative previously
  cleared paths, then issued `FUNCTIONAL GO` on exact SHA-256
  `52DB1280015AFA0CCA152CA2695780976C02CE2A4C6386E0BB1F448C052F0310`. Its single script block has
  SHA-256 `0F0FD1D4E0F7D172288B44434371D76740E6C5860B7B8EED1F2F1B3EA4C162A6`; this is the frozen
  behavior baseline for the visual candidate. Architect additionally traversed all nine Device
  destinations, 67 items and 118 detail pages. The 76-row contract, IDs, controls, state graph and
  handlers are accepted and must survive visual work.
- Architect's consolidated direction is one product-specific `CardMind precision field console`,
  not a generic AI dashboard or retro-terminal pastiche. Expanded Web uses one labeled primary rail,
  a contextual list where needed, one dominant work canvas and only an owning contextual inspector;
  medium keeps the rail and dominant pane with temporary list/inspector panels; compact is one pane
  at a time with labeled navigation and an explicit Back path. Chat must foreground active Project
  and Chat, conversation and composer; Files remains list-detail; Terminal remains one terminal with
  profile/SFTP context; Settings remains a section list with coherent forms rather than a card wall.
- The locked visual system is a graphite shell, warm light working canvas, solid surface tiers and
  one measured burnt-orange hardware-derived accent. A restrained cool secondary may identify a
  distinct interaction role; green, amber and red are semantic only. Geometry mixes modest work
  surfaces, tighter controls and square technical output; typography is small and disciplined with
  monospace confined to code, paths, terminal and machine output. Purple/blue gradients,
  glass/blur/glow, equal rounded cards, pill soup, AI sparkle/robot/brain ornaments, empty hero areas,
  fake metrics, ambient motion and raster or external assets remain prohibited.
- The 240x135 Device frame is an independent handheld instrument with a stable title/status band,
  bounded task/list/detail region, conspicuous keyboard focus and a bottom key legend/Back affordance.
  It retains all nine destinations and representative deep screens using one RGB565 canvas, existing
  fonts, solid fills/borders, lines and optional 8x8 procedural icons, with at most five menu rows and
  event-driven redraw. It must not imply a framebuffer, widget tree, raster asset, blur, alpha,
  continuous animation or persistent allocation.
- Visual acceptance requires screenshots and interaction evidence at 1440x900, 1024x768, 390x844,
  320x568 and 200% text size; all nine Device homes plus representative Project, Chat, Files,
  SSH/SFTP, Pending, Settings, SD and fatal frames; contrast, targets, keyboard/focus, dialog return,
  reduced-motion and static external-resource/animation/blur checks; a bounded embedded-size estimate;
  and one fresh visual red-team with at most one consolidated correction pass. Production, P6-02 and
  row closure remain frozen until Architect issues a separate visual `GO`.

## P6-01 visual-source constraints

**Observed:** 2026-09-03T02:16:35+03:00.

- W3C WCAG 2.2 (`https://www.w3.org/TR/WCAG22/`) supplies the acceptance floor rather than an art
  direction: 4.5:1 normal-text and 3:1 large-text contrast, 3:1 meaningful non-text/focus contrast,
  200% text resize without lost content or function, 320 CSS px reflow without two-dimensional
  scrolling except genuinely two-dimensional content, keyboard operation without traps, visible and
  not-entirely-obscured focus, non-color state cues and 24x24 CSS px AA pointer targets or a listed
  exception. CardMind's approximately 44x44 touch target is a stricter product goal, not an AA claim.
- Material 3's current canonical list-detail guidance
  (`https://m3.material.io/foundations/layout/canonical-examples/list-detail`) directly supports one
  pane plus Back in compact layouts and simultaneous list/detail only when space permits. Its
  navigation-rail guidance (`https://m3.material.io/components/navigation-rail/guidelines`) supports
  a labeled rail at medium and larger widths and a navigation bar at compact widths. Material `dp`,
  Fluent breakpoint names, CSS pixels and Apple points are not interchangeable; the candidate uses
  its own content-driven Web ranges: compact through 719 CSS px, medium 720-1179 and expanded 1180+.
- Fluent 2 layout and color (`https://fluent2.microsoft.design/layout` and
  `https://fluent2.microsoft.design/color`) support adaptive spacing/grids, reserving the largest
  region for primary work, approximately 44x44 Web touch targets, neutral structural surfaces,
  sparse shared accents and semantic status colors paired with another indicator. They do not
  prescribe the rail, inspector, warm palette or solid-only CardMind art direction.
- Apple's Generative AI and accessibility guidance
  (`https://developer.apple.com/design/human-interface-guidelines/generative-ai` and
  `https://developer.apple.com/design/human-interface-guidelines/accessibility`) supports explicit
  progress/failure, retained user control, clear AI capability/limitation disclosure, enlarged text,
  non-color state cues and reduced repetitive/automatic motion. It does not require or prohibit
  sparkle, gradients or other generic AI motifs; those prohibitions remain the user's and
  Architect's product-specific decision.
- MDN responsive-design and `prefers-reduced-motion` guidance
  (`https://developer.mozilla.org/en-US/docs/Learn_web_development/Core/CSS_layout/Responsive_Design`
  and `https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/At-rules/@media/prefers-reduced-motion`)
  supports a narrow single-column base, adding columns at content-driven breakpoints and removing,
  reducing or replacing nonessential motion. The candidate therefore uses no animation or transition
  at all; reduced-motion behavior is identical rather than a separate visual mode.

## P6-01 first styled-pass visual STOP

**Observed:** 2026-09-03 after the functional GO.

- Architect issued `VISUAL STOP` on the first `CardMind precision field console` styling pass. This
  does not revoke the functional baseline or authorize changes to its script, IDs, handlers or state
  graph. The task must not request visual GO from a patch of only the reported screenshots; it must
  audit every screen and component family before freezing one corrected candidate.
- API-profile fields and their creation/destructive actions lacked vertical rhythm, grouping and
  action hierarchy; Delete was not safely separated. The Wi-Fi pane exposed only a manual SSID field,
  dropping the locked Device scan/select expectation from this Web surface. Before correction the
  task must inventory the existing scan, protected-portal and Web owners, then reuse the smallest
  existing boundary: visible discovered-network selection, an explicit separate hidden/manual SSID
  path, password and connection state, with no speculative route or framework.
- Master access rendered sixteen large native selects as a sparse, poorly scannable table without a
  strong Off/Ask/Allow representation. The correction must compare a compact segmented/radio pattern
  against deliberately styled native selects using keyboard operation, 320 CSS px reflow, 200% text
  size and embedded-byte cost. The composer/options family also failed: controls clipped visually,
  the output-token field detached from intent, axes/heights diverged and unexplained empty space
  remained below the conversation.
- The Device home carousel remains rejected without destination pictograms. The correction must
  compare (a) existing 8x8/line icon + short label + concise status with adjacent destination peek,
  (b) an icon-led compact destination card and (c) a compact icon/list alternative, then select one
  existing-primitive approach. All nine destinations, order and arrow/Enter/Back behavior remain
  immutable; no new bitmap, theme, widget or allocation owner is allowed.
- The required self-audit inventory is `defect → governing rule → chosen correction → affected
  screens`. It covers all forms, buttons, selects, destructive actions, tables/lists, composer,
  panels, dialogs, degraded/empty/error states and 1440x900, 1024x768, 390x844, 320x568 and 200% text
  layouts. Every one of the 76 required action/state rows must remain reachable behind a visible
  control. The restrained field-console surface language may remain; CRT/scanlines, all-monospace,
  low-contrast amber, fake-terminal labels and generic AI decoration remain prohibited.
- The task-owned artifact currently has SHA-256
  `1645F105B659EE887A9A5E254D0BC60A7CF61B75FE631636233844F26902AE5A`, 231 unique IDs and the
  unchanged accepted functional-script SHA-256
  `0F0FD1D4E0F7D172288B44434371D76740E6C5860B7B8EED1F2F1B3EA4C162A6`. This is unfinished STOP
  evidence, not a frozen candidate. Production, P6-02, tests, builds, uploads and Device state remain
  frozen.

## P6-01 functional-GO revocation and semantic re-audit

**Observed:** 2026-09-03 after the first styled-pass visual STOP.

- Architect revoked the earlier functional `GO`. Source inspection proved that `#fullSsh` only
  changes its own label and the status text; it does not enter a fullscreen layout, alter terminal
  geometry or update the displayed rows/columns. Coverage row 5 nevertheless claimed input, resize,
  output, clear and Web fullscreen. The earlier ID/listener and handled-button counts were therefore
  invalid behavioral proxies and cannot support any row-level acceptance.
- Further visual correction is frozen until all 76 rows receive a semantic re-audit. Each required
  observation and forbidden effect must be exercised or traced to a real state transition. Every
  visible control is classified as `real interactive simulation`, `cosmetic/no-op`,
  `incomplete-state` or `navigation-only`; only the first classification can prove a claimed effect.
  Authoritative production semantics govern any correction. The accepted script hash and ID/state
  inventory are reference points, not permission to preserve a false simulation.
- Terminal correction must keep exactly one terminal, actually enter and exit fullscreen, change its
  geometry and displayed row/column state coherently, and provide an explicit keyboard/touch resize
  alternative rather than depending only on a drag handle. SSH/SFTP visual correction, after the
  semantic audit, uses progressive disclosure without dropping existing fields or IDs: normal
  context is the selected-profile/trust/connection summary plus New/Edit; New/Edit replaces it with
  the full form and explicit Save/Cancel with secrets, key install and separated Delete; SFTP is a
  distinct related Files mode rather than content stacked under the profile editor. The terminal
  stays dominant; medium/compact context remains temporary with explicit Back.
- The Files correction belongs to the same later consolidated pass. It must distinguish collection
  creation/import from selected-file Open-or-Enter, Download, Rename and Link/Unlink; separate the
  confirmed destructive Delete; reconcile whether selection already opens before retaining an Open
  control; avoid arbitrary action wrapping; and remove phase/proof labels from product copy. IDs and
  behavior require one-to-one mapping, not deletion behind an overflow affordance.
- The visible plan returned P6-01c to `in_progress`; independent functional review and the entire
  visual pass are pending again. No production, P6-02, test, build, upload or Device action is
  authorized by this re-audit.

## P6-01 semantic-proof boundary and source inventories

**Observed:** 2026-09-03T02:47:28+03:00.

- Architect narrowed the corrective proof to UI-contract behavior, not a fake backend. The complete
  matrix plus exact reachable screen/control/state mapping may prove exhaustive coverage. Navigation,
  selection, create/edit disclosure, menus, dialogs/focus return, Wi-Fi scan/select/manual entry,
  terminal resizing/fullscreen, responsive pane transitions and representative confirmation flows
  must be genuinely interactive. Pending, SD, provider, SSH trust/mismatch, reconnect/draft and
  Python handoff must enforce their legal transitions and forbidden effects. A correctly placed
  control with a visible typed outcome is sufficient for a stateless external effect such as export,
  download or refresh; no file, network or firmware backend belongs in this artifact.
- Device `openWifiPicker` uses the shipped scan owner, sorting, protected/open indication,
  selection, password and connect/save states. The protected provisioning portal also owns a
  scanned-network select plus a separate hidden-SSID field and write-only password. Before the
  latest user feedback, the production main Web Console intentionally exposed only manual
  `wifiSsid` and write-only password; no main-Web discovered-network data route was found. The user
  has now explicitly added discovered-network selection to the main WebUI. P6-01 models explicit
  scan/select and a separate hidden/manual path locally; P6-05 owns proof of the smallest safe
  production backend mapping that reuses `wifi_networks` and provisioning logic. A narrow route or
  state field may be justified there, but no route family or background scanner is authorized.
- The Device artifact preserves the exact nine-destination order and all 67 items and 118 detail
  pages, and its six pointer buttons can reach every page. That proves the static catalog only. There
  is no keyboard dispatcher, no action/state identifier and no operation invocation: Enter on a
  detail page merely advances to another static page, including where the copy claims confirm,
  install, overwrite, pause or delete. SD, Pending, SSH, Python, browser-presence and provider states
  are separate from the Web-side state owners; a false firmware `VERIFIED` page is reachable without
  its prerequisites; fatal/status behavior is absent. Until corrected, Device operation claims are
  `navigation-only`, with informational About/Back the sole meaningful exception.
- Device visual implementation may reuse one static procedural line icon per destination, the full
  selected label, a restrained indexed/neighbor cue and the existing title/status/key bands. The
  shipped UI already has a complete nine-icon procedural line set and ordered position marks. The
  literal `no framebuffer` wording in the earlier visual brief is inaccurate: production owns one
  persistent full-display `LGFX_Sprite`, whose default RGB565 storage is about 64,800 bytes at
  240x135. The actual constraint is no additional framebuffer, bitmap asset, widget system,
  persistent allocation or animation owner. The existing animated rounded carousel is not the
  selected direction because it carries six intermediate redraws and visual language rejected by
  the field-console brief.

## P6-01 complete semantic/component audit and coherent rebuild freeze

**Observed:** 2026-09-03 after Architect's consolidated systemic `STOP`.

- Two fresh read-only audits independently rejected unfinished artifact SHA-256
  `1645F105B659EE887A9A5E254D0BC60A7CF61B75FE631636233844F26902AE5A`. Across Web rows 1-66,
  only rows 4, 7, 36 and 46 already supplied sufficient interactive semantics. Seven rows were
  status-only/no-op, six were Device-owned or non-goal navigation mappings, and the remaining 49
  were incomplete. The strongest false claims were policy persistence/effective precedence,
  visible QR presentation, terminal fullscreen/geometry and large-output ownership, main-Web
  Wi-Fi discovery, browser-local preferences, and Python approval/origin handoff. The Device audit
  separately classified the 9/67/118 catalog as exhaustive navigation evidence but not operational
  behavior.
- The action audit found a systemic ownership defect rather than isolated styling: collection,
  selected-object, operation, rare and destructive controls were repeatedly mixed; Stop/Cancel and
  inverse actions remained visible outside their legal state; Settings Save was placed inside the
  category navigation; dialogs closed after failed validation and native Escape bypassed their typed
  cancel path; compact inspectors could hide the focused control without returning focus. Required
  labels, state-specific status cues, inline errors, role-based messages, binary/bundle file states,
  44px targets, long-text wrapping and compact navigation also failed across component families.
- Architect therefore ordered one coherent rebuild of the same task-owned artifact, not another CSS
  override or a sequence of screenshot patches. The retained material is the exact 76-row coverage
  vocabulary, proven project/chat isolation and SD, SSH mismatch/forget and pending helpers, the
  one-terminal/four-destination/context-pane concepts, and the restrained graphite/warm/rust visual
  direction. The two conflicting style systems, permanently expanded forms, handled-ID proxy and
  static Device prose as behavioral proof are rejected implementation material.
- The frozen rebuild uses one DOM/CSS system and nine bounded patterns: persistent shell/navigation;
  collection-list-detail; selected-object action ownership; New/Edit/Save/Cancel disclosure;
  accessible confirmation/focus return; stored layered policy with effective/source summary;
  single-operation state; shared degraded/handoff identity; and an icon-led 240x135 Device catalog
  with only representative interactive risk/layout flows. Evidence levels are explicit: `A` is
  exhaustive reachable mapping, `B` is real layout/interaction behavior, `C` is real legal-state and
  forbidden-effect behavior, and `D` is a correctly placed external/stateless action with a typed
  outcome. The rebuild will not simulate storage, firmware, networking or every production action.
- The production and test write set remains empty. The artifact write is only
  `C:\Users\84vs1\.codex\visualizations\2026\09\02\01a06386-22d5-7833-bb82-40a4f499f52e\p6-01-neutral-coverage-ia.html`;
  canonical observations remain in this trace and the explicit main-Web Wi-Fi scope decision in
  `ROADMAP.md`. No P6-02 design or production owner is unfrozen.
- The repository-root untracked file literal `m[1])` is zero bytes and was created at
  2026-09-03T00:17:55+03:00, but no evidence binds it to this task or an exact-owned P6 action. It is
  preserved unchanged; cleanup is intentionally not claimed.

## P6-01 whole-artifact rejection and consolidated correction package

**Observed:** 2026-09-03 after the coherent-rebuild checkpoint.

- Architect issued an exact whole-artifact `STOP` on task-owned candidate SHA-256
  `BE0114D1D799F5342EFFE2ACBC5B0A4E79CEC78904EEA86B637C48C9FD0029EB`. This is a distinct rejected
  checkpoint from the earlier `1645F105B659EE887A9A5E254D0BC60A7CF61B75FE631636233844F26902AE5A`
  visual STOP. The rejected bytes are frozen as evidence; they cannot support functional, visual or
  P6-01 closure claims.
- Direct load proved that the candidate had no bootstrap: the Chat collection advertised two
  messages while `#messages` was empty; the Device screen, Device map and 76-row coverage body were
  empty; and the scenario selector plus Reset control had no handlers. The artifact was an HTML
  fragment without a doctype, UTF-8 declaration or viewport, so the browser selected Windows-1251
  and corrupted multiplication signs, middle dots, minus signs and arrows.
- The reviewer-state selector was decorative and its authentication, collection, SD, reconnect,
  SSH-mismatch, Python and diagnostics states were not reachable. Initial policy controls were not
  synchronized from the model before Save. Manually assigned A/B/C/D labels, static IDs and
  screenshots therefore did not prove the claimed state transitions.
- Architect reproduced shared semantic defects: SSH-profile availability did not ceiling effective
  policy; multi-intent evaluation stopped at the first `Ask`; pending consumption used an identity
  broader than one operation; SD gating missed applicable Project/Chat/File paths and did not always
  restore editability; several Settings/profile fields did not round-trip; destructive lifecycle
  paths could leave stale operation UI; final-file deletion could dereference a missing selection;
  QR output was a pseudo-random non-QR grid; and Device representative flows advanced storyboard
  pages while claiming legal-state proof.
- The compact composition failed as one system: Project and Chat settings were stacked into a
  3226-pixel undifferentiated inspector; responsive policy labels and selects became detached;
  action hierarchy collapsed; 1024-pixel Settings controls overlapped their heading; compact
  terminal context collided with its title and fullscreen sat beneath the bottom navigation;
  320-pixel composer intents clipped; and effective 720x450 rendering squeezed top actions into
  vertical letters.
- The approved correction is one full standalone UTF-8 HTML document at the existing task-owned
  artifact path, with a collapsed reviewer harness separate from the product, one coherent
  master-detail/single-pane Web composition, a dedicated Project-or-Chat settings workspace,
  accessible per-capability policy groups, owned action hierarchies, one terminal, the exact
  9/67/118 Device catalog, honest QR evidence, seven shared composite scenarios and direct-load
  bootstrap. The production/test write set remains empty and P6-02+ remain frozen.

## P6-01 accepted replacement artifact and closure

**Completed:** 2026-09-03T07:37:12+03:00.

- The accepted task-owned artifact is
  `C:\Users\84vs1\.codex\visualizations\2026\09\02\01a06386-22d5-7833-bb82-40a4f499f52e\p6-01-neutral-coverage-ia.html`,
  exact SHA-256 `3217A5068BE741A20BC369FFC9B13B664C798C299A65A4BAFE07F1169E7B309D`,
  282,214 bytes. It is one standalone UTF-8 document below the 1 MiB budget with no external
  resource dependency. Direct load populated two initial messages, seven composite scenarios,
  all 76 coverage rows and the exact Device catalog of 9 destinations, 67 items and 118 pages.
- The retained responsive checks covered 1440x900, 1024x768, 720x450, 390x844 and 320x568. At and
  above 720 CSS px the artifact presents simultaneous master/detail; below 720 it presents one pane
  with an explicit Back owner. The final correction changed no geometry, typography or composition,
  and Architect confirmed the accepted visual/responsive result remained unchanged.
- The effective-capability proof uses one shared owner for display and submit. WebSearch and WebFetch
  require writable storage, a ready search service, a configured search key and a valid absolute
  HTTP(S) search URL. A malformed nonempty URL is rejected with retained unsaved input and prior
  committed state; an empty URL or removed search key makes both capabilities visibly unavailable.
  A required-Web send kept the message count at 2, retained its draft and Web intent, opened no
  pending authority and reported both unavailable capabilities. Restoring valid configuration or
  reloading returned the prior `Allow` baseline. Search-key removal updated `Not configured`, the
  top `Web · Off` summary and both detailed unavailable states simultaneously without navigation.
- The same-address reconnect scenario now calls the existing capture/restore owners. It moved from
  `field-notes/launch-checklist/Chat` to the alternate context and restored the exact project, chat,
  view and draft once with two messages still present, no pending approval or notice replay and
  focus returned to the prompt. The duplicate scenario restore path and synthetic Python outcome
  are absent. The three review-surface, two Project/Chat-scope and two Terminal/Files native buttons
  expose `aria-pressed`, unique accessible names and unchanged click plus Enter/Space behavior;
  `aria-selected` is absent from these non-tab controls.
- A fresh proof red-team reproduced the changed reconnect and accessibility boundaries and found the
  malformed-URL defect before closure. Its permitted blocker recheck confirmed the source correction
  but lost its browser environment, so it made no unobserved runtime claim. Architect personally
  reproduced the exact-final artifact, including URL/service availability, immediate key-removal
  rendering, required-Web forbidden effects, reconnect/no replay and segmented-control behavior,
  then issued explicit `ARCHITECT CLOSURE GO` for the exact hash above. That verdict also resolved
  the row-ordering gate: publish P6-01 first, activate P6-02 only after exact remote-SHA verification,
  and require P6-02's separate design/proof/write-set `GO` before any production edit.
- No firmware, production Web asset, generated header, schema, persisted value, route, test, build,
  upload, Device state or secret changed. The disposable browser viewport was reset, the exact-owned
  local artifact server was stopped and TCP port 65428 was verified free. The unrelated zero-byte
  root file `m[1])`, Architect-owned `.codex/agents` files and retained stash remain untouched.
  Residual risk is limited to this artifact being a reviewed design/proof rather than production;
  P6-02 and all later production work remain separately gated.

## P6-01 reopened acceptance correction

**Reopened:** 2026-09-03 after Architect's explicit ownership verdict. The user's visual STOP
invalidated the recorded P6-01 closure. The accepted-path artifact at SHA-256
`3217A5068BE741A20BC369FFC9B13B664C798C299A65A4BAFE07F1169E7B309D` has independently and
personally reproduced interaction, responsive-layout, action-hierarchy and preset-semantics
defects. P6-01 is again the sole `in_progress` row; P6-02 is pending and any locally retained P6-02
design package is historical prepared input only, not an active or approved implementation contract.

The correction is limited to the existing task-owned P6-01 artifact and this trace. It must:

1. make Project settings navigate to its owning workspace from Chat, Shared files, Terminal and
   Settings rather than unhide only Chat descendants;
2. replace the desktop in-flow Other chat actions expansion with a bounded anchored disclosure or
   overlay while retaining a usable mobile in-flow disclosure;
3. give 720x450 a compact/single-pane treatment where needed, collapse project/session utilities
   behind one clear disclosure, compact healthy status while leaving warnings/errors immediate,
   and keep the main title plus first useful action inside each retained initial viewport;
4. keep destructive API-profile, model-preset and optional-service key-removal actions local,
   content-sized and behind their selected-resource/service edit or danger disclosure, with
   STT/Search/TTS visibly grouped as services;
5. fit Allow once, Allow for chat and Deny inside every retained narrow viewport with pointer and
   keyboard reachability and no horizontal scrolling;
6. add explicit Web and Device `Apply to active project` for the selected model preset. Apply copies
   only model and output tokens through the existing project-settings owner, selection/creation does
   not apply, failure preserves prior project values, and no preset reference is persisted. Update
   the coverage row and a scenario to prove both the effect and forbidden effects.

The correction preserves the graphite/warm/rust visual direction, Wi-Fi discovered/hidden paths,
separate Terminal/Files and SSH/SFTP disclosure, terminal fullscreen/resize, grouped Shared/project
settings actions, nine icon-led Device destinations, every prior function/stable ID and the already
corrected Project capability-grid association. It adds no production asset, route, schema,
framework, design-system module, build or Device action.

Before another closure request, clean-load evidence must cover 1440x900, 1024x768, 720x450,
390x844 and 320x568; every product action/decision without overlap or clipping; title/first-action
reachability; Project settings from every non-Chat view; unchanged conversation header/work-canvas
geometry when opening Other chat actions; Web and Device preset Apply behavior and forbidden
effects; all seven composite scenarios; the complete 76-row and 9-destination/67-item/118-page
catalogs or explicitly corrected counts; zero console errors; exact artifact hash/size; and fresh
independent visual and functional red-team verdicts. P6-01 may not be marked completed, staged or
committed and P6-02 may not resume before a new explicit Architect closure `GO`.

## P6-01 reopened correction evidence-ready checkpoint

**Evidence-ready:** 2026-09-03T09:37:04+03:00. P6-01 remains the sole `in_progress` row and
P6-02 remains pending while this exact checkpoint awaits Architect's personal closure verdict.

- The corrected task-owned artifact is
  `C:\Users\84vs1\.codex\visualizations\2026\09\02\01a06386-22d5-7833-bb82-40a4f499f52e\p6-01-neutral-coverage-ia.html`,
  exact SHA-256 `DA69622052F935FB5033E2A0EE7631FE56F71F3D8C68F995B9370FEBA50C1714`,
  291,978 bytes. It remains below the 1 MiB budget and contains no external script, Fetch, XHR,
  WebSocket or project-to-preset reference. Script parsing passed, all 355 IDs are unique, and every
  queried correction control exists.
- Clean direct loads at 1440x900, 1024x768, 720x450, 390x844 and 320x568 had document scroll width
  equal to client width, no horizontally overflowing visible control and no console warning or
  error. The compact initial views retained the main title and first useful action above the fixed
  navigation. At 1024 and below one Console-actions disclosure owns project/session utilities, one
  compact healthy summary replaces healthy detail, and a forced provider warning remained
  immediately visible as both the aggregate warning and detailed provider failure.
- Project settings navigated to the owning settings workspace from Chat, Shared files, Terminal and
  Settings at both 1440 and 1024 widths. Each Back action restored the exact source view and focus to
  the Project-settings opener. Opening Other chat actions at 1440 left the conversation header at
  `[444,184.35,980.8,154]` and work canvas at `[444,338.35,980.8,296.85]`; at 1024 their heights and
  boundary remained 147 and 331.35 CSS pixels. The menu stayed inside the viewport at both desktop
  widths and became an in-flow eight-action disclosure without overflow at 720, 390 and 320.
- API-profile and model-preset deletion were absent from the closed state and appeared only as
  local content-sized actions inside their selected-resource danger disclosures. STT, Search and
  TTS rendered as three distinct service groups, and each key-removal action was absent until its
  own local danger disclosure opened. At 390 every opened action remained inside its settings pane.
- At 390 the pending-decision dialog/content measured 338/298 CSS pixels in both client and scroll
  width; at 320 they measured 268/228. Allow once, Allow for chat and Deny were fully visible,
  stacked at both widths, and keyboard order was exactly once, chat, deny without duplicate or
  hidden stops.
- Web preset selection and creation left the active project's model and output tokens unchanged.
  Applying `Fast` copied only `gpt-5-mini` and `1024`; no preset reference was stored. A forced
  settings/storage failure preserved the exact prior project values. The Device representative
  flow separately reproduced select-only, explicit apply confirmation, successful two-field copy,
  no-reference result and failure preservation; reselection remained inert.
- Direct-load coverage remained exactly 76/76 rows. The exhaustive Device catalog remained nine
  destinations, 67 items and 118 pages with per-destination page totals
  `16,19,6,6,19,12,17,19,4`. Baseline, authority, storage, remote, session, settings and Device
  composite scenarios each completed a full cycle in its owning surface with zero console errors.
- A fresh functional proof red-team reproduced every correction, count, Web/Device preset success
  and forbidden effect, all seven scenario cycles and the empty console, then returned `GO` on the
  exact hash and size above. A separately dispatched fresh visual red-team returned `GO` on the same
  bytes after independently checking all five required viewports, responsive reachability and
  clearance, Project-settings routing, anchored-menu geometry, danger hierarchy, service grouping,
  pending-decision fit/order and the graphite/warm/rust visual hierarchy.
- No firmware, production Web asset, generated header, schema, persistence, route, test, build,
  upload, Device state or secret changed. The disposable browser viewport was reset, the
  exact-owned artifact tab and server were closed, and TCP port 65429 was verified free. The
  unrelated root file `m[1])`, `.codex/` state and retained stash remain untouched. Residual risk is
  limited to this being a reviewed interaction/design proof rather than production behavior; P6-02
  and every production boundary remain separately gated.

## P6-01 reopened correction closure

**Completed:** 2026-09-03T09:40:13+03:00.

- Architect personally reconciled the exact six-clause correction contract, row-owned artifact and
  trace diff, every affected producer/consumer, native browser semantics, all retained proof,
  cleanup, resource budget and residual risk. Architect issued explicit `ARCHITECT CLOSURE GO` for
  exact artifact SHA-256 `DA69622052F935FB5033E2A0EE7631FE56F71F3D8C68F995B9370FEBA50C1714`,
  291,978 bytes, after the fresh functional and visual red-team verdicts both returned `GO` on those
  same bytes.
- The publication boundary contains only the P6-01 reopened contract, evidence and closure record.
  The locally retained P6-02 activation/pre-edit package is not P6-01-owned and must remain unstaged
  until this correction commit is pushed and its exact remote SHA is verified. P6-02 remains pending and no
  production work is authorized during this zero-active-row publication window.
