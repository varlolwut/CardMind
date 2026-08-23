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

Open a chat's details on the Cardputer or in the Web console to grant that chat
permission to run SSH commands. The permission is off by default, belongs only to
that chat, and is preserved when the chat is duplicated. Exported chat bundles do
not include it. CardMind refuses model-initiated SSH until a complete profile exists
and its current host fingerprint has already been trusted.
