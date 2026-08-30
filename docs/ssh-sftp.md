# SSH and SFTP

SSH is a full tool on the Cardputer and in the Web console. Both surfaces use the
same profiles and explicit connection lifecycle.

## Profiles and trust

A profile contains a name, host, port, username, authentication method, and optional
startup directory. CardMind stores up to eight profiles. Passwords, passphrases, and
private keys are write-only.

On first connection, compare the SHA-256 host fingerprint with a trusted source and
accept it explicitly. A changed fingerprint blocks the connection until it is reviewed
again; CardMind never accepts the replacement silently.

## On-device terminal

Open Tools → SSH. Arrow keys navigate without `Fn`; Enter opens or submits and Esc
returns or disconnects after confirmation. The terminal uses a rotating microSD log
for scrollback. Network and display work are serviced cooperatively so Esc remains
responsive during a session.

## Web terminal and SFTP

The Web console adds a desktop terminal, touch-keyboard command field, remote directory
browser, and transfers between the remote host and `/assistant/files`. Closing the
console or ending the session closes the SSH connection.

SSH still requires CardMind to have network reachability to the remote host. A future
USB console can carry browser control and terminal input, but it does not replace the
network path between CardMind and the SSH server.

## Model access

Capability settings govern model actions only; they do not disable the standalone
terminal or SFTP browser. Global **Master access**, project policy, and chat policy
contain separate rows for **SSH read**, **SSH mutate**, and **SFTP read/write**. A
narrower scope may reduce access but cannot grant more authority than its parent.

The current model SSH action is classified as SSH mutate. CardMind refuses it until a
complete profile exists and its current host fingerprint has already been trusted.
Before execution it shows the exact command and always requires **Allow once** or
**Deny**, even when SSH mutate is set to **Allow**; mandatory confirmation cannot be
saved for the chat. SSH read and model SFTP are currently unavailable, so their policy
rows cannot cause an operation to execute. Manual SFTP remains available through the
Device and Web console.

If power is lost or CardMind restarts while a decision is pending, the stored request
is shown as interrupted and cannot be approved or replayed. Imported project bundles
force each chat's SSH read and SSH mutate policies to **Off**.
